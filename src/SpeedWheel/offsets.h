#pragma once
#include <cstdint>

// ===========================================================================
// mgsvtpp.exe 1.0.15.4 EN  (image base 0x140000000)
//
// All RVAs below were verified against the retail executable in this repo.
// The retail code section (.xtext) is plaintext, so the patterns match the
// file bytes exactly.
// ===========================================================================
namespace sw::offsets {

// Game build this plugin's RVAs/patterns were verified against.
constexpr const char* kTargetGameVersion = "1.0.15.4";

// tpp::gm::player::impl::BasicActionImpl::UpdateMoveForceByPad
//   rcx = BasicActionImpl*, edx = player index, r8 = Work*
// Reads the pad move force and stores it into Work::moveForce (+0x2C0).
// 51-byte prologue signature (unique in .xtext).
constexpr uintptr_t kUpdateMoveForceByPadRva = 0x10BCD90;
const char* const   kUpdateMoveForceByPadPat =
    "48 89 5C 24 10 48 89 6C 24 18 56 48 83 EC 40 4C 8B 49 08 "
    "48 89 7C 24 50 33 F6 4D 8B 51 78 49 8B 81 38 01 00 00 "
    "44 8B CA 45 2B 4A 08 48 8B B8 B0 00 00 00";

// tpp::gm::player::impl::BasicActionImpl::UpdateMoveSpeed
// Turns Work::moveForce into the motion-graph move-speed rate.
constexpr uintptr_t kUpdateMoveSpeedRva = 0x10BD010;

// tpp::gm::player::impl::PadImpl::Update
// Writes the mouse wheel delta into PadImpl +0x48C every frame.
constexpr uintptr_t kPadUpdateRva = 0x1206A00;
const char* const   kPadUpdatePat =
    "48 8B C4 56 41 54 48 81 EC B8 00 00 00 0F 29 70 C8 "
    "48 89 58 08 48 89 68 10 48 89 78 18";

// ---------------------------------------------------------------------------
// BasicActionImpl::Work (per player, stride 0x340)
// ---------------------------------------------------------------------------
constexpr uintptr_t kWorkMoveForce  = 0x2C0;  // float, 0.1 .. 0.95
constexpr uintptr_t kWorkMoveAngle  = 0x2C4;  // float, radians
constexpr uintptr_t kWorkFlags      = 0x2AC;  // uint32
constexpr uint32_t  kWorkFlagDash   = 0x20;   // dash / sprint active
constexpr uintptr_t kWorkStance     = 0x2A2;  // byte: 1 = stand, 2 = squat, 3 = crawl
constexpr uintptr_t kWorkDeltaTime  = 0x2B8;  // float, frame delta seconds

// ---------------------------------------------------------------------------
// PadImpl (retail layout, unchanged from the debug build)
// ---------------------------------------------------------------------------
constexpr uintptr_t kPadWheel     = 0x48C;  // float, per-frame mouse wheel delta
constexpr uintptr_t kPadMoveForce = 0x10;   // float, 0..1
constexpr uintptr_t kPadMoveAngle = 0x14;   // float, radians

// Raw pad input bits (wheel event, per frame). The wheel delta at +0x48C is
// shared by every consumer: our speed wheel, the scope/binocular zoom and the
// iDroid map/UI. These bits say whether the wheel moved this frame.
constexpr uintptr_t kPadInputFlags     = 0x564;  // uint32
constexpr uint32_t  kPadFlagWheelActive = 0x00080000;  // wheel produced a delta
constexpr uint32_t  kPadFlagWheelDown   = 0x00100000;  // negative direction

// ---------------------------------------------------------------------------
// Wheel consumers. The wheel is a single shared per-frame delta, so a notch
// meant for zoom would also move the speed level. Both native consumers are
// hooked to tag the notch as "used by the game" and drop it.
// ---------------------------------------------------------------------------

// PadImpl::GetWheel - vtable-only accessor returning PadImpl+0x48C. UI/map
// code reads the wheel through this, never through a direct call.
constexpr uintptr_t kPadWheelGetterRva = 0x1205E60;
const char* const   kPadWheelGetterPat =
    "F3 0F 10 81 8C 04 00 00 C3 CC CC CC CC CC CC CC";

// Wheel-driven camera zoom (scope/binoculars). Method of the player camera
// manager: (self, playerIndex, PadImpl*), reads PadImpl+0x48C.
constexpr uintptr_t kZoomWheelHandlerRva = 0xB5D560;
const char* const   kZoomWheelHandlerPat =
    "40 55 48 8B EC 48 83 EC 60 48 8B 05 C0 BD FE 01";

// ---------------------------------------------------------------------------
// Player status bits (tpp::PlayerInfo status table).
//
// PlayerInfo::OrCheckStatus (rva 0x554520) resolves the local player's status
// object with the getter at rva 0x53CA40:
//   obj = 0x2BD0FF0 + (block * 16 + playerIndex) * 0xE0
// and tests status bits at obj + 0xC0 (256 bits). block/playerIndex live at
// 0x2BD0FD0 / 0x2BD8FD4 (rva 0x53C990 returns the same index).
// ---------------------------------------------------------------------------
constexpr uintptr_t kStatusBaseRva   = 0x2BD0FF0;  // status object array
constexpr uintptr_t kStatusBlockRva  = 0x2BD0FD0;  // int32, 0 or 1 (double buffer)
constexpr uintptr_t kStatusIndexRva  = 0x2BD8FD4;  // int32, local player index
constexpr uintptr_t kStatusStride    = 0xE0;
constexpr uintptr_t kStatusBits      = 0xC0;
constexpr unsigned  kStatusBinocle   = 58;         // "using int-scope" (binoculars)
constexpr unsigned  kStatusGunReady  = 10;         // aiming a weapon

// Pointer chain used inside UpdateMoveForceByPad itself:
//   pad = [[[self + 0x8] + 0x138] + 0xB0]
constexpr uintptr_t kSelfModule     = 0x8;
constexpr uintptr_t kModulePadOwner = 0x138;
constexpr uintptr_t kPadOwnerPad    = 0xB0;

// ---------------------------------------------------------------------------
// Force limits (globals read by UpdateMoveForceByPad / UpdateMoveSpeed)
// ---------------------------------------------------------------------------
constexpr uintptr_t kStickForceMinRva = 0x2AD04BC;  // 0.1f
constexpr uintptr_t kStickForceMaxRva = 0x2AD04C0;  // 0.95f
constexpr float     kStickForceMinDefault = 0.1f;
constexpr float     kStickForceMaxDefault = 0.95f;

} // namespace sw::offsets
