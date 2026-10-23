# MGSV movement speed — retail RE notes

Target: `mgsvtpp.exe` 1.0.15.4 EN (SHA256 `085C2F82...E9BB45`), image base `0x140000000`.

The retail executable is **not** encrypted at rest: `.xtext` (RVA `0x1000`, size `0x208C000`) is plaintext x64 code, so function RVAs and byte patterns can be derived statically from the file.

## Pipeline

`BasicActionImpl::UpdateMoveForceByPad` → `Work + 0x2C0` (move force, 0.1–0.95)
→ `BasicActionImpl::UpdateMoveSpeed` → `SetMoveSpeedByPadForceWithInterp`
→ `CommonBlendControlValues + 0x30` (move-speed rate 0..1)
→ motion graph: animation blend **and** root motion.

Both locomotion and the walk/jog/run blend read the same rate, so scaling the force keeps feet and animation in sync.

## Retail addresses

| Function | RVA | Notes |
|---|---|---|
| `PadImpl::Update` | `0x1206A00` | writes the mouse wheel delta to `PadImpl + 0x48C`; first statement is `addsd xmm0,[rcx+0x270]` |
| `BasicActionImpl::UpdateMoveForceByPad` | `0x10BCD90` | signature: `rcx` = BasicActionImpl, `edx` = player index, `r8` = Work |
| `BasicActionImpl::UpdateMoveSpeed` | `0x10BD010` | converts force → rate; dash flag (`Work + 0x2AC & 0x20`) forces 1.0 |
| `BasicActionImpl::StateStandMove` | `0x10B52F0` | confirms phase order: force update (phase 4), speed update (phase 6) |

## Offsets

```
Work (stride 0x340):
  +0x2A2  u8    stance (1 stand, 2 squat, 3 crawl)
  +0x2AC  u32   flags (bit 0x20 = dash)
  +0x2B8  f32   frame delta seconds
  +0x2C0  f32   move force (0.1 .. 0.95)
  +0x2C4  f32   move angle (radians)

PadImpl:
  +0x10   f32   move force
  +0x14   f32   move angle
  +0x48C  f32   mouse wheel delta (per frame)
  pointer chain from BasicActionImpl: [[[self+8]+0x138]+0xB0]

Globals:
  0x2AD04BC  f32  STICK_FORCE_MIN = 0.1
  0x2AD04C0  f32  STICK_FORCE_MAX = 0.95
```

## Speed curve (stand, normal move mode)

From `GetMoveSpeedBySpeedRate`:

- rate `< 0.5`: walk band, ~1.23 → 2.74 m/s
- rate `>= 0.5`: jog/run band, 2.74 → 4.70 m/s
- squat: 0.72 → 3.20 m/s
- crawl: 0.61 → 1.23 m/s

The plugin maps each wheel notch to a target rate and converts it back to a force:
`force = STICK_FORCE_MIN + rate * (STICK_FORCE_MAX - STICK_FORCE_MIN)`.
Keyboard input reports a force of **1.0**; `STICK_FORCE_MAX` (0.95) is only the upper
clamp of the rate curve inside `UpdateMoveSpeed`, so the scaling guard has no upper
bound. Scaling is floored at `STICK_FORCE_MIN` so the state machine still sees
movement input.
