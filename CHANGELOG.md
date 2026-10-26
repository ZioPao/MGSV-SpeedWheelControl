# Changelog

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
