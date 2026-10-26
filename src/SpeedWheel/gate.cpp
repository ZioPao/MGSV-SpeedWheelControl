// Wheel ownership for Speed Wheel Control.
//
// PadImpl+0x48C is one per-frame delta shared by every wheel consumer, so a
// notch meant for zoom would also move the speed level. This unit hooks the one
// native consumer that reads it (the camera zoom handler) and reads the game
// state for the paths that cannot be hooked (binocular status, iDroid open),
// then decides who owns each notch.
//
// PadImpl::GetWheel (rva 0x1205E60) is not used: the game never calls it, so it
// is not hooked.
#include "gate.h"

#include "common.h"
#include "config.h"
#include "logger.h"
#include "offsets.h"

#include <windows.h>
#include <MinHook.h>

namespace sw {
namespace {

using ZoomWheelHandlerFn = void(__fastcall*)(void* self, unsigned playerIndex, void* pad);

ZoomWheelHandlerFn g_origZoomWheel = nullptr;

// Set when the game used the wheel for camera zoom this frame.
double g_lastZoomUse = 0.0;

// Published by the IH module; 1 while the iDroid map is open.
volatile LONG g_idroidOpen = 0;

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

void __fastcall ZoomWheelHandlerHook(void* self, unsigned playerIndex, void* pad) {
    if (g_cfg.gateZoom && pad) {
        unsigned flags = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(pad) + offsets::kPadInputFlags, flags) &&
            (flags & offsets::kPadFlagWheelActive)) {
            g_lastZoomUse = NowSeconds();
        }
    }
    g_origZoomWheel(self, playerIndex, pad);
}

} // namespace

void InstallWheelGate(uintptr_t zoomHandler) {
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

    LogInfo("wheel gate: %s (camera zoom %s, binoculars %s, iDroid %s)",
            g_cfg.gateZoom ? "on" : "off",
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
    if (g_lastZoomUse > 0.0 && (now - g_lastZoomUse) <= 0.06) return WheelGate::CameraZoom;
    return WheelGate::None;
}

const char* WheelGateName(WheelGate gate) {
    switch (gate) {
        case WheelGate::CameraZoom: return "camera zoom";
        case WheelGate::Binoculars: return "binoculars up";
        case WheelGate::Idroid:     return "iDroid map open";
        default:                    return "none";
    }
}

} // namespace sw
