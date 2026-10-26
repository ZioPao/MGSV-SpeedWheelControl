#pragma once
#include <cstdint>

namespace sw {

// The wheel delta at PadImpl+0x48C is shared by every consumer. A notch the
// game used for zoom belongs to the game, not to the speed system.
enum class WheelGate {
    None,
    CameraZoom,   // native camera zoom handler consumed the wheel
    Aim,          // optional aimKey held (off by default)
    Binoculars,   // PlayerStatus BINOCLE ("using int-scope")
    Idroid,       // Infinite Heaven reports the iDroid map open
};

// Hooks the native wheel consumers and reads their state. Either target may be
// 0 (pattern miss); the state gates still work without the hooks.
void InstallWheelGate(uintptr_t padWheelGetter, uintptr_t zoomHandler);

// Published by the IH module through SpeedWheel.runtime.
void SetIdroidOpen(bool open);

// Gate that owns the current notch, or None when the speed wheel may take it.
WheelGate WheelGateReason(double now);
const char* WheelGateName(WheelGate gate);

// Periodic pad-wheel-reader diagnostics (logWheel only).
void WheelGateTick();

} // namespace sw
