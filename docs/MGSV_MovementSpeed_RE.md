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

## Walk key (reset trigger)

`Work + 0x2B0` bit `0x100` is the walk-pace state:

- `UpdateMoveForceByPad` (rva `0x10BCD90`) sets it every frame from pad button
  `0x19` when the pad is valid, and clears it when the button is not down
  (`0x10BCF76` / `0x10BCF82`). That button is the in-game walk binding, so the
  plugin never needs a key code and rebinds work untouched.
- `GetMoveForceLimit` (rva `0x109F6F0`) tests the bit and clamps the move force
  to `STICK_FORCE_MIN` while it is set — the reason keyboard walking is so slow.
- `Work + 0x2FC` is a separate stand-speed limit (`SetStandMoveSpeedLimit`); it
  defaults to `-1` and is unrelated to this flag.

The plugin exposes the flag as the reset input: a rising edge sets the speed
level back to `defaultLevel`.

`GetMoveForceLimit` is hooked (rva `0x109F6F0`, signature
`float (this, playerIndex, Work*)`) to implement `walkKey=1`: for that call the
walk bit is masked in `Work + 0x2B0` and restored afterwards, so the walk clamp
never reaches the move force while every other clamp (aim, status `0x7a`/`0x7e`,
stand speed limit) still applies.

## Wheel ownership

`PadImpl + 0x48C` is one per-frame delta shared by every wheel consumer, so a
notch meant for zoom would also move the speed level:

| Consumer | RVA | Notes |
|---|---|---|
| Wheel zoom handler | `0xB5D560` | camera/binocular zoom; invoked from `0xB5D750` when the pad wheel flag is set, handles both directions |
| Pad wheel flags | `+0x564` | `0x80000` = wheel produced a delta, `0x100000` = negative direction |

`PadImpl::GetWheel` (`0x1205E60`) exists but the game never calls it, so it is
not hooked.

`SpeedWheel` defers each notch by one frame and drops it when the zoom handler
read the wheel in that frame; the scope/binocular path is additionally gated by
the player status bit `BINOCLE` (58) and the iDroid map by IH's `inIdroid`.

## Player status bits

`PlayerInfo::OrCheckStatus` (RVA `0x554520`) resolves the local player's status
object with `0x53CA40`:

```
obj = 0x2BD0FF0 + (block * 16 + playerIndex) * 0xE0
bits at obj + 0xC0 (256 statuses), block @ 0x2BD0FD0, playerIndex @ 0x2BD8FD4
```

Status `58` = `BINOCLE` ("using int-scope"), `10` = `GUN_READY` (aiming a
weapon). `SpeedWheel` reads bit 58 to gate the wheel while the binoculars are
up without blocking speed changes while merely aiming.

## iDroid / map state

The map zoom does **not** read `PadImpl+0x48C` through `PadImpl::GetWheel` (that
accessor is never called by the game) and does not use the camera zoom handler,
so it cannot be detected from the wheel side. The IH module
(`SpeedWheel_Core.lua`) publishes IH's own `currentChecks.inIdroid` to
`SpeedWheel.runtime` (`idroid=0|1`) only when it flips open/closed; the DLL polls
that file every 100 ms and drops notches while it is open.
