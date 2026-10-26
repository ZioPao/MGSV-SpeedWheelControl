// Wheel ownership for Speed Wheel Control.
//
// PadImpl+0x48C is one per-frame delta shared by every wheel consumer, so a
// notch meant for zoom would also move the speed level. This unit hooks the
// native consumers it can see and reads the game state for the ones it cannot
// (binocular status, iDroid open), then decides who owns each notch.
#include "gate.h"

#include "common.h"
#include "config.h"
#include "logger.h"
#include "offsets.h"

#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

namespace sw {
namespace {

using PadWheelGetterFn = float(__fastcall*)(void* self);
using ZoomWheelHandlerFn = void(__fastcall*)(void* self, unsigned playerIndex, void* pad);

PadWheelGetterFn g_origPadWheelGetter = nullptr;
ZoomWheelHandlerFn g_origZoomWheel = nullptr;

// Set when the game used the wheel for camera zoom this frame.
double g_lastZoomUse = 0.0;

// Published by the IH module; 1 while the iDroid map is open.
volatile LONG g_idroidOpen = 0;

// Pad wheel accessor diagnostics (see WheelGateTick).
struct WheelReader {
    uintptr_t caller = 0;
    unsigned  calls = 0;
    unsigned  nonZero = 0;
};
WheelReader g_wheelReaders[16];
double g_lastReaderLog = 0.0;

void StampWheelUse() {
    g_lastZoomUse = NowSeconds();
}

// Binoculars and scopes zoom with the wheel while the aim button is held; that
// path does not read the pad wheel through any hookable accessor, so it needs a
// state gate. Off by default: aiming must keep the wheel working.
bool AimHeld() {
    if (g_cfg.aimKey <= 0) return false;
    return (GetAsyncKeyState(g_cfg.aimKey) & 0x8000) != 0;
}

// Local player status bits (same table PlayerInfo.OrCheckStatus reads).
bool PlayerStatusActive(unsigned status) {
    if (!g_base) return false;
    int block = 0;
    int index = 0;
    if (!SafeRead<int>(g_base + offsets::kStatusBlockRva, block)) return false;
    if (!SafeRead<int>(g_base + offsets::kStatusIndexRva, index)) return false;
    if (block < 0 || block > 1) return false;
    if (index < 0 || index >= 16) return false;

    const uintptr_t obj = g_base + offsets::kStatusBaseRva +
                          static_cast<uintptr_t>(block * 16 + index) * offsets::kStatusStride;
    uint32_t word = 0;
    if (!SafeRead<uint32_t>(obj + offsets::kStatusBits + (status >> 5) * 4, word)) return false;
    return ((word >> (status & 31)) & 1u) != 0;
}

bool BinocularsUp() {
    return PlayerStatusActive(offsets::kStatusBinocle);
}

void NoteWheelReader(uintptr_t caller, bool nonZero) {
    for (auto& r : g_wheelReaders) {
        if (r.caller == caller) {
            ++r.calls;
            if (nonZero) ++r.nonZero;
            return;
        }
    }
    for (auto& r : g_wheelReaders) {
        if (r.caller == 0) {
            r.caller = caller;
            r.calls = 1;
            r.nonZero = nonZero ? 1u : 0u;
            return;
        }
    }
}

const char* ReaderKind(uintptr_t caller) {
    for (const auto& r : g_wheelReaders) {
        if (r.caller != caller) continue;
        if (r.calls < 24) return "on demand";
        const uint64_t ratio = static_cast<uint64_t>(r.nonZero) * 100u;
        return (ratio >= static_cast<uint64_t>(r.calls) * 25u) ? "on demand" : "polls";
    }
    return "on demand";
}

float __fastcall PadWheelGetterHook(void* self) {
    const float value = g_origPadWheelGetter ? g_origPadWheelGetter(self) : 0.0f;
    if (g_cfg.gateZoom && value != 0.0f) {
        // Any consumer that asks for the wheel owns this notch.
        NoteWheelReader(reinterpret_cast<uintptr_t>(_ReturnAddress()), true);
        StampWheelUse();
    }
    return value;
}

void __fastcall ZoomWheelHandlerHook(void* self, unsigned playerIndex, void* pad) {
    if (g_cfg.gateZoom && pad) {
        unsigned flags = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(pad) + offsets::kPadInputFlags, flags) &&
            (flags & offsets::kPadFlagWheelActive)) {
            StampWheelUse();
        }
    }
    g_origZoomWheel(self, playerIndex, pad);
}

} // namespace

void InstallWheelGate(uintptr_t padWheelGetter, uintptr_t zoomHandler) {
    bool getterHooked = false;
    if (padWheelGetter &&
        MH_CreateHook(reinterpret_cast<LPVOID>(padWheelGetter),
                      reinterpret_cast<LPVOID>(&PadWheelGetterHook),
                      reinterpret_cast<LPVOID*>(&g_origPadWheelGetter)) == MH_OK &&
        MH_EnableHook(reinterpret_cast<LPVOID>(padWheelGetter)) == MH_OK) {
        getterHooked = true;
        LogInfo("hooked PadImpl::GetWheel @ 0x%llX (wheel gate)",
                (unsigned long long)padWheelGetter);
    } else if (padWheelGetter) {
        LogWarn("PadImpl::GetWheel hook failed - UI wheel reads are not gated");
    }

    bool zoomHooked = false;
    if (zoomHandler &&
        MH_CreateHook(reinterpret_cast<LPVOID>(zoomHandler),
                      reinterpret_cast<LPVOID>(&ZoomWheelHandlerHook),
                      reinterpret_cast<LPVOID*>(&g_origZoomWheel)) == MH_OK &&
        MH_EnableHook(reinterpret_cast<LPVOID>(zoomHandler)) == MH_OK) {
        zoomHooked = true;
        LogInfo("hooked ZoomWheelHandler @ 0x%llX (wheel gate)",
                (unsigned long long)zoomHandler);
    } else if (zoomHandler) {
        LogWarn("ZoomWheelHandler hook failed - scope zoom is not gated");
    }

    LogInfo("wheel gate: %s (accessor %s, camera zoom %s, binoculars %s, iDroid %s)",
            g_cfg.gateZoom ? "on" : "off",
            getterHooked ? "hooked" : "off",
            zoomHooked ? "hooked" : "off",
            g_cfg.gateBinoc ? "on" : "off",
            g_cfg.gateIdroid ? "on" : "off");
}

void SetIdroidOpen(bool open) {
    const LONG value = open ? 1 : 0;
    if (value == g_idroidOpen) return;
    g_idroidOpen = value;
    LogInfo("iDroid %s - speed wheel %s", open ? "OPEN" : "closed",
            open ? "paused" : "active");
}

WheelGate WheelGateReason(double now) {
    if (!g_cfg.gateZoom) return WheelGate::None;
    if (g_cfg.gateIdroid && g_idroidOpen != 0) return WheelGate::Idroid;
    if (g_cfg.gateBinoc && BinocularsUp()) return WheelGate::Binoculars;
    if (AimHeld()) return WheelGate::Aim;
    if (g_lastZoomUse > 0.0 && (now - g_lastZoomUse) <= 0.06) return WheelGate::CameraZoom;
    return WheelGate::None;
}

const char* WheelGateName(WheelGate gate) {
    switch (gate) {
        case WheelGate::CameraZoom: return "camera zoom";
        case WheelGate::Aim:        return "aim held";
        case WheelGate::Binoculars: return "binoculars up";
        case WheelGate::Idroid:     return "iDroid map open";
        default:                    return "none";
    }
}

void WheelGateTick() {
    if (!g_cfg.logWheel) return;
    const double now = NowSeconds();
    if (now - g_lastReaderLog < 5.0) return;

    bool any = false;
    for (const auto& r : g_wheelReaders) {
        if (r.caller) { any = true; break; }
    }
    if (!any) return;

    g_lastReaderLog = now;
    for (const auto& r : g_wheelReaders) {
        if (!r.caller) continue;
        LogInfo("wheel reader rva 0x%llX: %u calls, %u with wheel (%s)",
                (unsigned long long)(r.caller - g_base), r.calls, r.nonZero,
                ReaderKind(r.caller));
    }
}

} // namespace sw
