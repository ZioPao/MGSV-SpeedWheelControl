#pragma once

namespace sw {

// Resolves UpdateMoveForceByPad, installs the detour and starts the speed state.
bool InstallSpeedHook();

// Called from the init thread; currently just keeps the heartbeat alive when the
// hook is not receiving frames (menus, loading screens).
void SpeedTick();

} // namespace sw
