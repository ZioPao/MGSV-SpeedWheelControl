// Wheel ownership for Speed Wheel Control.
//
// PadImpl+0x48C is one per-frame delta shared by every wheel consumer, so a
// notch meant for zoom would also move the speed level. This unit hooks the one
// native consumer that reads it (the camera zoom handler) and reads the game
// state for the paths that cannot be hooked (binocular status, iDroid open),
// then decides who owns each notch. The weapon/item ring reads the wheel through
// the pad service, so its update is hooked only to observe its open state.
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
using EquipCrossUpdateFn = void(__fastcall*)(void* self);

ZoomWheelHandlerFn g_origZoomWheel = nullptr;
EquipCrossUpdateFn g_origEquipCrossUpdate = nullptr;

// Set when the game used the wheel for camera zoom this frame.
double g_lastZoomUse = 0.0;

// Last time the weapon/item ring updated while open. A timestamp rather than a
// flag, so a ring that stops updating (unloaded HUD) cannot hold the gate.
double   g_lastEquipOpen = 0.0;
uint32_t g_lastEquipState = 0;

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
    if (pad) {
        unsigned flags = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(pad) + offsets::kPadInputFlags, flags) &&
            (flags & offsets::kPadFlagWheelActive)) {
            g_lastZoomUse = NowSeconds();
        }
    }
    g_origZoomWheel(self, playerIndex, pad);
}

void __fastcall EquipCrossUpdateHook(void* self) {
    g_origEquipCrossUpdate(self);
    if (!self) return;

    uint32_t state = 0;
    if (!SafeRead(reinterpret_cast<uintptr_t>(self) + offsets::kEquipCrossState, state)) return;
    if (state >= offsets::kEquipCrossOpenFirst && state <= offsets::kEquipCrossOpenLast) {
        g_lastEquipOpen = NowSeconds();
    }
    if (state != g_lastEquipState) {
        if (g_cfg.logWheel) {
            LogInfo("equip ring state %u -> %u", g_lastEquipState, state);
        }
        g_lastEquipState = state;
    }
}

bool HookTarget(uintptr_t target, LPVOID detour, LPVOID* original) {
    return target &&
           MH_CreateHook(reinterpret_cast<LPVOID>(target), detour, original) == MH_OK &&
           MH_EnableHook(reinterpret_cast<LPVOID>(target)) == MH_OK;
}

} // namespace

void InstallWheelGate(uintptr_t zoomHandler, uintptr_t equipCrossUpdate) {
    const bool zoomHooked = HookTarget(zoomHandler,
                                       reinterpret_cast<LPVOID>(&ZoomWheelHandlerHook),
                                       reinterpret_cast<LPVOID*>(&g_origZoomWheel));
    if (zoomHooked) {
        LogInfo("hooked ZoomWheelHandler @ 0x%llX (wheel gate)",
                (unsigned long long)zoomHandler);
    } else if (zoomHandler) {
        LogWarn("ZoomWheelHandler hook failed - scope zoom is not gated");
    }

    const bool equipHooked = HookTarget(equipCrossUpdate,
                                        reinterpret_cast<LPVOID>(&EquipCrossUpdateHook),
                                        reinterpret_cast<LPVOID*>(&g_origEquipCrossUpdate));
    if (equipHooked) {
        LogInfo("hooked EquipCrossEvCall::Update @ 0x%llX (wheel gate)",
                (unsigned long long)equipCrossUpdate);
    } else if (equipCrossUpdate) {
        LogWarn("EquipCrossEvCall::Update hook failed - weapon/item ring is not gated");
    }

    LogInfo("wheel gate: camera zoom %s, equip ring %s, binoculars on, iDroid on",
            zoomHooked ? "hooked" : "off", equipHooked ? "hooked" : "off");
}

void SetIdroidOpen(bool open) {
    const LONG value = open ? 1 : 0;
    if (value == g_idroidOpen) return;
    g_idroidOpen = value;
    LogInfo("iDroid %s - speed wheel %s", open ? "OPEN" : "closed",
            open ? "paused" : "active");
}

WheelGate WheelGateReason(double now) {
    if (g_idroidOpen != 0) return WheelGate::Idroid;
    if (g_lastEquipOpen > 0.0 && (now - g_lastEquipOpen) <= 0.10) return WheelGate::EquipMenu;
    if (BinocularsUp()) return WheelGate::Binoculars;
    if (g_lastZoomUse > 0.0 && (now - g_lastZoomUse) <= 0.06) return WheelGate::CameraZoom;
    return WheelGate::None;
}

const char* WheelGateName(WheelGate gate) {
    switch (gate) {
        case WheelGate::CameraZoom: return "camera zoom";
        case WheelGate::Binoculars: return "binoculars up";
        case WheelGate::Idroid:     return "iDroid map open";
        case WheelGate::EquipMenu:  return "weapon/item ring open";
        default:                    return "none";
    }
}

} // namespace sw
