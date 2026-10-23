# MGSVSpeed

Dynamic on-foot movement speed for **Metal Gear Solid V: The Phantom Pain** (1.0.15.4 EN), in the spirit of Splinter Cell: scroll the mouse wheel to pick a speed, and Snake moves at that speed with matching walk/jog/run animation.

- **Wheel up** = faster, **wheel down** = slower.
- **Top speed is vanilla keyboard run** (the plugin never makes you faster than normal).
- Works in all stances (stand / squat / crawl), on foot only. Vehicles and cutscenes are untouched.
- Logs only — no overlay, no HUD.

## Requirements

- MGSV:TPP 1.0.15.4 (Steam, EN)
- [IHHook](https://github.com/kapuragu/IHHook) with `enable_dll_loader=true` in `ihhook_config.lua`

## Install

1. Copy `MGSVSpeed.dll` into `MGS_TPP/plugins/`.
2. Launch the game. On first run the plugin writes `MGSVSpeed.ini` next to the DLL and creates `MGSVSpeed.log`.
3. In gameplay, scroll the mouse wheel.

## Controls

| Input | Action |
|---|---|
| Wheel up | one step faster |
| Wheel down | one step slower |
| F7 | enable / disable the system |
| F8 | reset to top speed (vanilla run) |

The wheel is ignored while the game is not focused or while a menu shows the cursor.

## Config (`MGSVSpeed.ini`)

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `1` | master switch |
| `levels` | `0.10,0.23,...,1.00` | target move-speed rate per notch; `1.0` = vanilla run |
| `defaultLevel` | `7` | starting notch (7 = top speed) |
| `invertWheel` | `0` | `1` = wheel up slows down |
| `smoothSeconds` | `0.12` | gait transition smoothing |
| `forceMin` / `forceMax` | `0` | `0` = read the game's own force limits (0.1 / 0.95) |
| `playerIndex` | `-1` | `-1` = auto-detect the local player; force a value only if the log shows the wrong index |
| `logLevelChanges` | `1` | one log line per notch |
| `logWheel` | `1` | log raw wheel deltas |
| `heartbeatSeconds` | `5` | periodic state line; `0` disables |

## How it works

MGSV drives both locomotion and the walk/jog/run animation blend from a single analog **move force** (`BasicActionImpl::Work + 0x2C0`, range 0.1–0.95). Keyboard input is binary (0.95 = run), which is why the game only offers CTRL = walk.

The plugin hooks two engine functions:

| Function | Retail RVA | Role |
|---|---|---|
| `PadImpl::Update` | `0x1206A00` | reads the game's own mouse wheel delta (`pad + 0x48C`) |
| `BasicActionImpl::UpdateMoveForceByPad` | `0x10BCD90` | scales the move force before it reaches `UpdateMoveSpeed` |

Because the force feeds the motion graph, animation playback and root motion stay in sync — the character does not slide. Scaling is floored at the game's `STICK_FORCE_MIN` so the state machine still sees movement input and never freezes in idle.

Detailed reverse-engineering notes (RVAs, offsets, speed curve) are in [`docs/MGSV_MovementSpeed_RE.md`](docs/MGSV_MovementSpeed_RE.md).

## Build

Requires CMake ≥ 3.20 and MSVC (VS 2022/2026, x64).

```powershell
.\build.ps1            # configure + build
.\build.ps1 -Deploy    # build and copy to the game's plugins folder
```

Output: `build/Release/MGSVSpeed.dll`.

## Notes

- Single-player only. Do not use on MGO/FOB — client-side speed changes are not replicated and may be treated as cheating.
- The plugin is inert if the executable version or the byte patterns do not match; the log says so.
- Log: `MGS_TPP/plugins/MGSVSpeed.log`.
