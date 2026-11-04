#pragma once
#include <cstdint>

namespace sw {

// The wheel delta at PadImpl+0x48C is shared by every consumer. A notch the
// game used for zoom belongs to the game, not to the speed system.
enum class WheelGate {
    None,
    CameraZoom,   // native camera zoom handler consumed the wheel
    Binoculars,   // PlayerStatus BINOCLE ("using int-scope")
    Idroid,       // Infinite Heaven reports the iDroid map open
    EquipMenu,    // weapon/item selection ring is open
};

// Hooks the native camera zoom consumer and the weapon/item ring update (0 is a
// pattern miss) and enables the state gates. A missed hook only costs its gate.
void InstallWheelGate(uintptr_t zoomHandler, uintptr_t equipCrossUpdate);

// Published by the IH module through SpeedWheel.runtime.
void SetIdroidOpen(bool open);

// Gate that owns the current notch, or None when the speed wheel may take it.
WheelGate WheelGateReason(double now);
const char* WheelGateName(WheelGate gate);

} // namespace sw
