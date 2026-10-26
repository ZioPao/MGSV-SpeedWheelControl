# Changelog

## 1.1.1 — 2026-10-27

The walk key now doubles as the emergency reset.

- **New:** pressing the walk key resets the speed level to `defaultLevel`
  (vanilla run unless you changed it). No new ini keys.
- The key is read from the game, not from a VK: the plugin watches
  `Work + 0x2B0` bit `0x100`, the flag `UpdateMoveForceByPad` writes from the
  walk binding, so rebinding the walk key in-game is picked up automatically.
- Holding the key still walks natively; only the press edge resets the level.
- F7 (toggle) and F8 (reset) still work as before.
- DLL-only update: `plugins/SpeedWheel.dll`.

## 1.1.0 — 2026-10-26

The mouse wheel no longer does two things at once.

- **Fixed:** scrolling while zooming no longer changes your speed level:
  - binoculars / int-scope
  - iDroid map
- **Still works:** aiming a weapon and moving keeps the wheel on speed control.
  Only an actual zoom takes it.
- When the game uses a notch for zoom, the notch is discarded — the speed level
  never drifts while you zoom.
- The log now says why a notch was ignored (`binoculars up`, `iDroid map open`,
  `camera zoom`).
- Update both files: `plugins/SpeedWheel.dll` and `mod/modules/SpeedWheel_Core.lua`.
  The iDroid gate needs the Lua module.
- Requirements unchanged: MGSV 1.0.15.4 EN, IHHook with `enable_dll_loader=true`,
  Infinite Heaven.

## 1.0.0

First release as Speed Wheel Control.

- Mouse wheel picks the on-foot walk/run speed in 8 levels; top level is vanilla run.
- Smooth gait transitions, invert wheel, and wheel-step options.
- Infinite Heaven knobs (F3 → *Speed Wheel Control*), F7 toggle, F8 reset.
