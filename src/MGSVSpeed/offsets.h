#pragma once
#include <cstdint>

// ===========================================================================
// mgsvtpp.exe 1.0.15.4 EN  (image base 0x140000000)
//
// All RVAs below were verified against the retail executable in this repo.
// The retail code section (.xtext) is plaintext, so the patterns match the
// file bytes exactly.
// ===========================================================================
namespace mgsvspeed::offsets {

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

} // namespace mgsvspeed::offsets
