// Speed Wheel Control - dynamic on-foot movement speed for MGSV:TPP.
//
// Both locomotion and the walk/jog/run blend are driven by Work+0x2C0 (move
// force, 0.1..0.95). PadImpl::Update gives us the game's wheel delta;
// UpdateMoveForceByPad is where we scale that force. Scaling it keeps animation
// speed and root motion in sync instead of sliding.
#include "speed.h"

#include "common.h"
#include "config.h"
#include "logger.h"
#include "offsets.h"
#include "scanner.h"

#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace sw {

namespace {

using PadUpdateFn = void(__fastcall*)(void* self);
using UpdateMoveForceByPadFn = void(__fastcall*)(void* self, unsigned playerIndex, void* work);
using PadWheelGetterFn = float(__fastcall*)(void* self);
using ZoomWheelHandlerFn = void(__fastcall*)(void* self, unsigned playerIndex, void* pad);

PadUpdateFn g_origPadUpdate = nullptr;
UpdateMoveForceByPadFn g_origMoveForce = nullptr;
PadWheelGetterFn g_origPadWheelGetter = nullptr;
ZoomWheelHandlerFn g_origZoomWheel = nullptr;

uintptr_t g_padUpdateTarget = 0;
uintptr_t g_moveForceTarget = 0;
uintptr_t g_padWheelGetterTarget = 0;
uintptr_t g_zoomWheelTarget = 0;

int   g_level = 7;
float g_targetMul = 1.0f;
float g_currentMul = 1.0f;
float g_wheelAccum = 0.0f;
float g_lastRawWheel = 0.0f;

// Zoom gating: the wheel delta in PadImpl+0x48C is shared by every consumer.
// A notch is held for one frame so the game's own zoom/UI readers can tag it
// first; tagged notches are dropped instead of moving the speed level.
double      g_lastZoomUse = 0.0;
const char* g_lastZoomWhy = "zoom";
float       g_pendingWheel = 0.0f;
double      g_pendingWheelAt = 0.0;
unsigned    g_pendingPadFlags = 0;
int         g_pendingBinoc = 0;
volatile LONG g_idroidOpen = 0;  // published by the IH Lua module (SpeedWheel.runtime)
double      g_lastGateLog = 0.0;
double      g_lastReaderLog = 0.0;

// Per-caller stats for the pad wheel accessor. A caller that also polls the
// accessor while the wheel is idle (the HUD) must not gate speed changes; an
// on-demand reader (map/UI zoom) does.
struct WheelReader {
    uintptr_t caller = 0;
    unsigned  calls = 0;
    unsigned  nonZero = 0;
};
WheelReader g_wheelReaders[16];

float g_forceMin = offsets::kStickForceMinDefault;
float g_forceMax = offsets::kStickForceMaxDefault;

float    g_lastForceIn = 0.0f;
float    g_lastForceOut = 0.0f;
float    g_lastWheel = 0.0f;
unsigned g_lastFlags = 0;
unsigned g_lastStance = 0;

volatile LONG g_frames = 0;
volatile LONG g_applyCount = 0;
int    g_localIndex = -1;
double g_lastLocalSeen = 0.0;
double g_lastSmoothTime = 0.0;
double g_lastHeartbeat = 0.0;
double g_lastWheelLog = 0.0;
unsigned g_nonLocalSeen = 0;

double NowSeconds() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

template <typename T>
bool SafeRead(uintptr_t addr, T& out) {
    if (!addr) return false;
    __try {
        out = *reinterpret_cast<const T*>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWrite(uintptr_t addr, const T& value) {
    if (!addr) return false;
    __try {
        *reinterpret_cast<T*>(addr) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ForegroundIsGame() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

bool CursorHidden() {
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci)) return true;
    return (ci.flags & CURSOR_SHOWING) == 0;
}

// The wheel may only drive the speed while the player is actually in gameplay.
bool GameplayInputAllowed() {
    const double now = NowSeconds();
    static double lastCheck = 0.0;
    static bool   cached = true;
    if (now - lastCheck < 0.05) return cached;
    lastCheck = now;

    cached = true;
    if (g_cfg.requireForeground && !ForegroundIsGame()) cached = false;
    if (cached && g_cfg.requireCursorHidden && !CursorHidden()) cached = false;
    return cached;
}

// Mirrors tpp::gm::player::impl::GetMoveSpeedBySpeedRate (stand/squat/crawl,
// normal move mode) so the log can show an approximate m/s figure.
float EstimateSpeed(unsigned stance, float rate) {
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;
    constexpr float kInvWalkRateMax = 1.22656f;
    constexpr float kWalkMax        = 2.74250f;
    constexpr float kRunMax         = 4.69661f;
    if (stance == 2) {  // squat
        return (1.0f - rate) * 0.7193f + rate * 3.19660f;
    }
    if (stance == 3) {  // crawl
        return (1.0f - rate) * 0.61341f + rate * 1.22705f;
    }
    if (rate >= 0.5f) {
        const float t = (rate - 0.5f) * 2.0f;
        return (1.0f - t) * kWalkMax + t * kRunMax;
    }
    return (1.0f - kInvWalkRateMax * rate) * kInvWalkRateMax +
           kInvWalkRateMax * rate * kWalkMax;
}

void RecomputeTarget() {
    if (g_cfg.levels.empty()) return;
    if (g_level < 0) g_level = 0;
    if (g_level >= (int)g_cfg.levels.size()) g_level = (int)g_cfg.levels.size() - 1;

    float rate = g_cfg.levels[(size_t)g_level];
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    const float forceTarget = g_forceMin + rate * (g_forceMax - g_forceMin);
    g_targetMul = forceTarget / g_forceMax;
    if (g_targetMul > 1.0f) g_targetMul = 1.0f;
    if (g_targetMul < 0.0f) g_targetMul = 0.0f;
}

void LogLevel(const char* why) {
    if (!g_cfg.logLevelChanges) return;
    const unsigned stance = g_lastStance ? g_lastStance : 1;
    const float rate = g_cfg.levels.empty() ? 1.0f : g_cfg.levels[(size_t)g_level];
    LogInfo("%s: level %d/%d  rate %.2f  force x%.2f  ~%.2f m/s (stance %u)  wheel %.2f",
            why, g_level + 1, (int)g_cfg.levels.size(), rate, g_targetMul,
            EstimateSpeed(stance, rate), stance, g_lastRawWheel);
}

void SetLevel(int level, const char* why) {
    const int maxLevel = (int)g_cfg.levels.size() - 1;
    if (level < 0) level = 0;
    if (level > maxLevel) level = maxLevel;
    if (level == g_level) return;
    g_level = level;
    RecomputeTarget();
    LogLevel(why);
}

void ProcessWheel(float raw) {
    g_lastWheel = raw;
    g_lastRawWheel = raw;
    if (raw == 0.0f) return;

    if (g_cfg.logWheel) {
        const double now = NowSeconds();
        if (now - g_lastWheelLog > 0.02) {
            g_lastWheelLog = now;
            LogInfo("wheel raw %.3f (pad 0x%X work 0x%X binoc %d)", raw, g_pendingPadFlags,
                    g_lastFlags, g_pendingBinoc);
        }
    }

    // MGSV reports the wheel normalized (about +-1 per notch). Normalize only
    // genuine Windows deltas (multiples of 120); a fast scroll arrives as one
    // large value, which must map to that many steps, not be divided down.
    float delta = raw;
    if (std::fabs(delta) >= 120.0f && std::fabs(std::fmod(delta, 120.0f)) < 0.5f) {
        delta /= 120.0f;
    }
    g_wheelAccum += delta;

    const float sign = g_cfg.invertWheel ? -1.0f : 1.0f;
    int steps = 0;
    while (g_wheelAccum * sign >= 0.5f) {
        ++steps;
        g_wheelAccum -= 1.0f * sign;
    }
    while (g_wheelAccum * sign <= -0.5f) {
        --steps;
        g_wheelAccum += 1.0f * sign;
    }
    if (steps == 0) return;

    // Keep the residual bounded so fast scrolling cannot bank notches forever.
    if (g_wheelAccum > 2.0f) g_wheelAccum = 2.0f;
    if (g_wheelAccum < -2.0f) g_wheelAccum = -2.0f;

    const int mult = (g_cfg.wheelStep >= 1) ? g_cfg.wheelStep : 1;
    SetLevel(g_level + steps * mult, steps > 0 ? "wheel up" : "wheel down");
}

void UpdateSmoothing(double now) {
    float dt = (float)(now - g_lastSmoothTime);
    g_lastSmoothTime = now;
    if (dt <= 0.0f) return;
    if (dt > 0.10f) dt = 0.10f;

    const float tau = g_cfg.smoothSeconds;
    if (tau <= 0.0001f || !g_cfg.enabled) {
        g_currentMul = g_cfg.enabled ? g_targetMul : 1.0f;
        return;
    }
    const float a = 1.0f - std::exp(-dt / tau);
    g_currentMul += (g_targetMul - g_currentMul) * a;
    if (std::fabs(g_currentMul - g_targetMul) < 0.001f) g_currentMul = g_targetMul;
}

// The Infinite Heaven module SpeedWheel_Core writes this file whenever a knob
// changes; we mirror it into the live state. The menu is the only writer, so only a
// content change is applied: polling must not undo wheel/hotkey changes.
void PollRuntimeFile() {
    const std::string path = g_dllDir + "\\SpeedWheel.runtime";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f) return;

    char buf[512] = {};
    const size_t read = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    static std::string lastContent;
    std::string content(buf, read);
    if (content == lastContent) return;
    lastContent = content;

    int   enabled = -1;
    int   level = -1;
    int   invert = -1;
    int   step = -1;
    int   idroid = -1;
    float smooth = -1.0f;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        if (eol == std::string::npos) eol = content.size();

        char line[256] = {};
        size_t len = eol - pos;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, content.data() + pos, len);
        pos = eol + 1;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = line;
        char* val = eq + 1;
        while (*key == ' ' || *key == '\t') ++key;
        char* keyEnd = key + strlen(key);
        while (keyEnd > key && (keyEnd[-1] == ' ' || keyEnd[-1] == '\t')) *--keyEnd = 0;
        while (*val == ' ' || *val == '\t') ++val;

        if      (_stricmp(key, "enabled") == 0) enabled = atoi(val);
        else if (_stricmp(key, "level") == 0)   level = atoi(val);
        else if (_stricmp(key, "smooth") == 0)  smooth = (float)atof(val);
        else if (_stricmp(key, "invert") == 0)  invert = atoi(val);
        else if (_stricmp(key, "step") == 0)    step = atoi(val);
        else if (_stricmp(key, "idroid") == 0)  idroid = atoi(val);
    }

    if (enabled >= 0) {
        const bool e = enabled != 0;
        if (e != g_cfg.enabled) {
            g_cfg.enabled = e;
            if (!e) g_currentMul = 1.0f;
            LogInfo("IH menu: system %s", e ? "ENABLED" : "DISABLED");
        }
    }
    if (level >= 1) {
        SetLevel(level - 1, "IH menu");
    }
    if (step >= 1) {
        const int s = (step > 3) ? 3 : step;
        if (s != g_cfg.wheelStep) {
            g_cfg.wheelStep = s;
            LogInfo("IH menu: wheel step %d", s);
        }
    }
    if (smooth >= 0.0f && std::fabs(smooth - g_cfg.smoothSeconds) > 0.0005f) {
        g_cfg.smoothSeconds = smooth;
        LogInfo("IH menu: smooth %.3f s", smooth);
    }
    if (invert >= 0) {
        const bool i = invert != 0;
        if (i != g_cfg.invertWheel) {
            g_cfg.invertWheel = i;
            LogInfo("IH menu: invert %s", i ? "ON" : "OFF");
        }
    }
    if (idroid >= 0 && idroid != (int)g_idroidOpen) {
        g_idroidOpen = idroid;
        LogInfo("iDroid %s - speed wheel %s", idroid ? "OPEN" : "closed",
                idroid ? "paused" : "active");
    }
}

void CheckHotkeys() {
    if (!g_cfg.hotkeyEnabled) return;
    static bool lastF7 = false;
    static bool lastF8 = false;

    const bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7 && !lastF7) {
        g_cfg.enabled = !g_cfg.enabled;
        if (!g_cfg.enabled) g_currentMul = 1.0f;
        LogInfo("hotkey F7: system %s", g_cfg.enabled ? "ENABLED" : "DISABLED");
    }
    lastF7 = f7;

    const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (f8 && !lastF8) {
        SetLevel((int)g_cfg.levels.size() - 1, "hotkey F8 (reset)");
        LogInfo("hotkey F8: level reset to top speed");
    }
    lastF8 = f8;
}

// The game shares PadImpl+0x48C between every wheel consumer. These hooks only
// record that a consumer used the wheel this frame; the notch is dropped later.
void StampWheelUse(const char* why) {
    g_lastZoomUse = NowSeconds();
    g_lastZoomWhy = why;
}

// Binoculars and scopes zoom with the wheel while the aim button is held, and
// that path does not read the pad wheel through any hookable accessor.
bool AimHeld() {
    if (g_cfg.aimKey <= 0) return false;
    return (GetAsyncKeyState(g_cfg.aimKey) & 0x8000) != 0;
}

// Reads the local player's status bits (the same table PlayerInfo.OrCheckStatus
// uses). BINOCLE is set only while the binoculars/int-scope is up, so the wheel
// stays a speed control while merely aiming a weapon.
bool PlayerStatusActive(unsigned status) {
    if (!g_base) return false;
    int block = 0;
    int index = 0;
    if (!SafeRead<int>(g_base + offsets::kStatusBlockRva, block)) return false;
    if (!SafeRead<int>(g_base + offsets::kStatusIndexRva, index)) return false;
    if (block < 0 || block > 1) return false;
    if (index < 0 || index >= 16) return false;

    const uintptr_t obj = g_base + offsets::kStatusBaseRva +
                          (uintptr_t)(block * 16 + index) * offsets::kStatusStride;
    uint32_t word = 0;
    if (!SafeRead<uint32_t>(obj + offsets::kStatusBits + (status >> 5) * 4, word)) return false;
    return ((word >> (status & 31)) & 1u) != 0;
}

bool BinocularsUp() {
    return g_cfg.gateBinoc && PlayerStatusActive(offsets::kStatusBinocle);
}

void NoteWheelReader(uintptr_t caller, bool nonZero) {
    for (auto& r : g_wheelReaders) {
        if (r.caller == caller) {
            ++r.calls;
            if (nonZero) ++r.nonZero;
            return;
        }
    }
    for (auto& r : g_wheelReaders) {
        if (r.caller == 0) {
            r.caller = caller;
            r.calls = 1;
            r.nonZero = nonZero ? 1u : 0u;
            return;
        }
    }
}

// An on-demand reader only calls the accessor when the wheel actually moved
// (map/UI zoom). Kept for diagnostics only: any non-zero read means the game
// consumed the wheel, so the notch is dropped.
bool ReaderIsOnDemand(uintptr_t caller) {
    for (const auto& r : g_wheelReaders) {
        if (r.caller != caller) continue;
        if (r.calls < 24) return true;
        return static_cast<uint64_t>(r.nonZero) * 100u >= static_cast<uint64_t>(r.calls) * 25u;
    }
    return true;
}

void MaybeLogWheelReaders() {
    if (!g_cfg.logWheel) return;
    const double now = NowSeconds();
    if (now - g_lastReaderLog < 5.0) return;
    bool any = false;
    for (const auto& r : g_wheelReaders) {
        if (r.caller) { any = true; break; }
    }
    if (!any) return;
    g_lastReaderLog = now;
    for (const auto& r : g_wheelReaders) {
        if (!r.caller) continue;
        LogInfo("wheel reader rva 0x%llX: %u calls, %u with wheel (%s)",
                (unsigned long long)(r.caller - g_base), r.calls, r.nonZero,
                ReaderIsOnDemand(r.caller) ? "on demand" : "polls");
    }
}

float __fastcall PadWheelGetterHook(void* self) {
    const float value = g_origPadWheelGetter ? g_origPadWheelGetter(self) : 0.0f;
    if (g_cfg.gateZoom) {
        const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const bool nonZero = value != 0.0f;
        NoteWheelReader(caller, nonZero);
        // Any consumer that asks for the wheel owns this notch, including the
        // iDroid map UI, which polls the accessor while the map is open.
        if (nonZero) StampWheelUse("pad wheel read");
    }
    return value;
}

void __fastcall ZoomWheelHandlerHook(void* self, unsigned playerIndex, void* pad) {
    if (g_cfg.gateZoom && pad) {
        unsigned flags = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(pad) + offsets::kPadInputFlags, flags) &&
            (flags & offsets::kPadFlagWheelActive)) {
            StampWheelUse("camera zoom");
        }
    }
    g_origZoomWheel(self, playerIndex, pad);
}

// Apply the previous frame's notch unless the game used the wheel for zoom.
// One frame of latency (~16 ms) lets the game's own readers run first.
void FlushPendingWheel(double now) {
    if (g_pendingWheel == 0.0f) return;
    if (now - g_pendingWheelAt < 0.005) return;  // same-frame pad update, wait

    const float wheel = g_pendingWheel;
    g_pendingWheel = 0.0f;

    if (g_cfg.gateZoom) {
        const bool tagged = g_lastZoomUse > 0.0 && (now - g_lastZoomUse) <= 0.06;
        const bool aiming = AimHeld();
        const bool binoculars = BinocularsUp();
        const bool idroid = g_cfg.gateIdroid && g_idroidOpen != 0;
        if (tagged || aiming || binoculars || idroid) {
            g_wheelAccum = 0.0f;
            if (g_cfg.logWheel) {
                static double loggedAt = 0.0;
                if (now - loggedAt > 0.5) {
                    loggedAt = now;
                    if (idroid) {
                        LogInfo("wheel %.2f ignored (iDroid map open)", wheel);
                    } else if (binoculars) {
                        LogInfo("wheel %.2f ignored (binoculars up, pad flags 0x%X, work flags 0x%X)",
                                wheel, g_pendingPadFlags, g_lastFlags);
                    } else if (aiming) {
                        LogInfo("wheel %.2f ignored (aim held, pad flags 0x%X, work flags 0x%X)", wheel,
                                g_pendingPadFlags, g_lastFlags);
                    } else {
                        LogInfo("wheel %.2f ignored (%s %.0f ms ago, pad flags 0x%X, work flags 0x%X)", wheel,
                                g_lastZoomWhy, (now - g_lastZoomUse) * 1000.0,
                                g_pendingPadFlags, g_lastFlags);
                    }
                }
            }
            return;
        }
    }
    ProcessWheel(wheel);
}

void __fastcall PadUpdateHook(void* self) {
    g_origPadUpdate(self);
    ++g_frames;
    if (!self) return;

    const double now = NowSeconds();
    UpdateSmoothing(now);
    CheckHotkeys();

    if (g_frames == 1) {
        float wheel = 0.0f;
        SafeRead(reinterpret_cast<uintptr_t>(self) + offsets::kPadWheel, wheel);
        LogInfo("first PadImpl::Update: pad 0x%llX wheel %.3f",
                (unsigned long long)(uintptr_t)self, wheel);
    }

    if (!g_cfg.enabled || !GameplayInputAllowed()) {
        g_pendingWheel = 0.0f;
        return;
    }

    float wheel = 0.0f;
    if (!SafeRead(reinterpret_cast<uintptr_t>(self) + offsets::kPadWheel, wheel)) return;

    FlushPendingWheel(now);
    MaybeLogWheelReaders();
    if (wheel != 0.0f) {
        g_pendingWheel = wheel;
        g_pendingWheelAt = now;
        unsigned flags = 0;
        SafeRead(reinterpret_cast<uintptr_t>(self) + offsets::kPadInputFlags, flags);
        g_pendingPadFlags = flags;
        g_pendingBinoc = BinocularsUp() ? 1 : 0;
    }
}

void __fastcall UpdateMoveForceByPadHook(void* self, unsigned playerIndex, void* work) {
    g_origMoveForce(self, playerIndex, work);

    if (!work) return;
    const uintptr_t workAddr = reinterpret_cast<uintptr_t>(work);

    unsigned flags = 0;
    unsigned char stance = 0;
    SafeRead(workAddr + offsets::kWorkFlags, flags);
    SafeRead(workAddr + offsets::kWorkStance, stance);
    g_lastFlags = flags;
    g_lastStance = stance;

    if (g_cfg.playerIndex >= 0) {
        if ((int)playerIndex != g_cfg.playerIndex) return;
    } else {
        const double now = NowSeconds();
        if (g_localIndex < 0) {
            g_localIndex = (int)playerIndex;
            g_lastLocalSeen = now;
            LogInfo("scaling player index %u (auto)", playerIndex);
        } else if ((int)playerIndex != g_localIndex) {
            // A mission reload can hand the player a different entity index.
            if (now - g_lastLocalSeen > 2.0) {
                LogInfo("player index changed %d -> %u (reload?), adopting", g_localIndex, playerIndex);
                g_localIndex = (int)playerIndex;
                g_lastLocalSeen = now;
            } else {
                if (g_nonLocalSeen++ == 0) {
                    LogInfo("ignoring other player index %u (local is %d)", playerIndex, g_localIndex);
                }
                return;
            }
        } else {
            g_lastLocalSeen = now;
        }
    }
    if (!g_cfg.enabled) return;

    float force = 0.0f;
    if (!SafeRead(workAddr + offsets::kWorkMoveForce, force)) return;
    g_lastForceIn = force;
    g_lastForceOut = force;

    // Only scale actual movement input. Idle (force < min) is left to the game;
    // keyboard force is 1.0, while STICK_FORCE_MAX (0.95) is only the clamp used
    // by the speed-rate curve, so there is no upper bound here.
    if (!(force >= g_forceMin)) return;
    if (g_currentMul >= 0.9999f) return;

    float scaled = force * g_currentMul;
    if (scaled < g_forceMin) scaled = g_forceMin;
    if (scaled > force) scaled = force;

    if (SafeWrite(workAddr + offsets::kWorkMoveForce, scaled)) {
        g_lastForceOut = scaled;
        ++g_applyCount;
    }

    if (g_cfg.logEveryFrame && (g_applyCount % 30) == 0) {
        LogInfo("frame: force %.3f->%.3f mul %.2f stance %u flags 0x%X",
                g_lastForceIn, g_lastForceOut, g_currentMul, g_lastStance, g_lastFlags);
    }
}

uintptr_t FindCodeSection(uintptr_t& sizeOut) {
    sizeOut = 0;
    if (!g_base) return 0;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            size_t vsize = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
            if (sec->VirtualAddress + vsize > nt->OptionalHeader.SizeOfImage) {
                vsize = nt->OptionalHeader.SizeOfImage - sec->VirtualAddress;
            }
            sizeOut = vsize;
            return g_base + sec->VirtualAddress;
        }
    }
    return 0;
}

uintptr_t ResolveTarget(uintptr_t codeStart, size_t codeSize, const char* name,
                        uintptr_t rva, const char* pattern) {
    const uintptr_t found = FindPattern(codeStart, codeSize, pattern);
    if (found) {
        LogInfo("%s: pattern match at rva 0x%llX", name, (unsigned long long)(found - g_base));
        return found;
    }
    const uintptr_t fallback = g_base + rva;
    if (MatchPattern(fallback, pattern)) {
        LogWarn("%s: pattern not found, verified rva 0x%llX used", name, (unsigned long long)rva);
        return fallback;
    }
    LogError("%s: neither pattern nor rva 0x%llX matched", name, (unsigned long long)rva);
    return 0;
}

void ReadForceLimits() {
    if (g_cfg.forceMin > 0.0f) g_forceMin = g_cfg.forceMin;
    else {
        float v = 0.0f;
        if (SafeRead(g_base + offsets::kStickForceMinRva, v) && v > 0.0f && v < 1.0f) g_forceMin = v;
    }
    if (g_cfg.forceMax > 0.0f) g_forceMax = g_cfg.forceMax;
    else {
        float v = 0.0f;
        if (SafeRead(g_base + offsets::kStickForceMaxRva, v) && v > 0.0f && v <= 1.0f) g_forceMax = v;
    }
    if (g_forceMax <= g_forceMin) {
        g_forceMin = offsets::kStickForceMinDefault;
        g_forceMax = offsets::kStickForceMaxDefault;
        LogWarn("force limits looked invalid - using defaults %.2f..%.2f", g_forceMin, g_forceMax);
    }
}

} // namespace

bool InstallSpeedHook() {
    if (!g_base || !g_size) {
        LogError("module base not initialised");
        return false;
    }
    const std::string version = GetExeVersionString();
    if (version != offsets::kTargetGameVersion) {
        LogWarn("executable version is %s, this build targets %s",
                version.c_str(), offsets::kTargetGameVersion);
    }

    size_t codeSize = 0;
    const uintptr_t codeStart = FindCodeSection(codeSize);
    if (!codeStart) {
        LogError("no executable section found");
        return false;
    }

    g_padUpdateTarget = ResolveTarget(codeStart, codeSize, "PadImpl::Update",
                                      offsets::kPadUpdateRva, offsets::kPadUpdatePat);
    g_moveForceTarget = ResolveTarget(codeStart, codeSize, "UpdateMoveForceByPad",
                                      offsets::kUpdateMoveForceByPadRva,
                                      offsets::kUpdateMoveForceByPadPat);
    if (!g_padUpdateTarget || !g_moveForceTarget) return false;

    // Optional: wheel consumers used for scope/map zoom. Missing hooks only
    // cost the zoom gate, never the speed system itself.
    if (g_cfg.gateZoom) {
        g_padWheelGetterTarget = ResolveTarget(codeStart, codeSize, "PadImpl::GetWheel",
                                               offsets::kPadWheelGetterRva,
                                               offsets::kPadWheelGetterPat);
        g_zoomWheelTarget = ResolveTarget(codeStart, codeSize, "ZoomWheelHandler",
                                          offsets::kZoomWheelHandlerRva,
                                          offsets::kZoomWheelHandlerPat);
    }

    ReadForceLimits();
    if (g_cfg.levels.empty()) {
        LogError("config has no levels - aborting");
        return false;
    }
    g_level = g_cfg.defaultLevel;
    RecomputeTarget();
    g_currentMul = g_targetMul;
    g_lastSmoothTime = NowSeconds();
    g_lastHeartbeat = g_lastSmoothTime;

    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        LogError("MH_Initialize failed (%d)", (int)st);
        return false;
    }

    st = MH_CreateHook(reinterpret_cast<LPVOID>(g_padUpdateTarget),
                       reinterpret_cast<LPVOID>(&PadUpdateHook),
                       reinterpret_cast<LPVOID*>(&g_origPadUpdate));
    if (st != MH_OK) {
        LogError("MH_CreateHook(PadImpl::Update) failed (%d)", (int)st);
        return false;
    }
    st = MH_CreateHook(reinterpret_cast<LPVOID>(g_moveForceTarget),
                       reinterpret_cast<LPVOID>(&UpdateMoveForceByPadHook),
                       reinterpret_cast<LPVOID*>(&g_origMoveForce));
    if (st != MH_OK) {
        LogError("MH_CreateHook(UpdateMoveForceByPad) failed (%d)", (int)st);
        return false;
    }
    st = MH_EnableHook(reinterpret_cast<LPVOID>(g_padUpdateTarget));
    if (st != MH_OK) {
        LogError("MH_EnableHook(PadImpl::Update) failed (%d)", (int)st);
        return false;
    }
    st = MH_EnableHook(reinterpret_cast<LPVOID>(g_moveForceTarget));
    if (st != MH_OK) {
        LogError("MH_EnableHook(UpdateMoveForceByPad) failed (%d)", (int)st);
        return false;
    }

    if (g_padWheelGetterTarget &&
        MH_CreateHook(reinterpret_cast<LPVOID>(g_padWheelGetterTarget),
                      reinterpret_cast<LPVOID>(&PadWheelGetterHook),
                      reinterpret_cast<LPVOID*>(&g_origPadWheelGetter)) == MH_OK &&
        MH_EnableHook(reinterpret_cast<LPVOID>(g_padWheelGetterTarget)) == MH_OK) {
        LogInfo("hooked PadImpl::GetWheel @ 0x%llX (zoom gate)",
                (unsigned long long)g_padWheelGetterTarget);
    } else {
        LogWarn("PadImpl::GetWheel hook failed - UI wheel reads are not gated");
        g_padWheelGetterTarget = 0;
    }

    if (g_zoomWheelTarget &&
        MH_CreateHook(reinterpret_cast<LPVOID>(g_zoomWheelTarget),
                      reinterpret_cast<LPVOID>(&ZoomWheelHandlerHook),
                      reinterpret_cast<LPVOID*>(&g_origZoomWheel)) == MH_OK &&
        MH_EnableHook(reinterpret_cast<LPVOID>(g_zoomWheelTarget)) == MH_OK) {
        LogInfo("hooked ZoomWheelHandler @ 0x%llX (zoom gate)",
                (unsigned long long)g_zoomWheelTarget);
    } else {
        LogWarn("ZoomWheelHandler hook failed - scope zoom still moves the speed level");
        g_zoomWheelTarget = 0;
    }

    LogInfo("hooked PadImpl::Update @ 0x%llX (wheel +0x%X)",
            (unsigned long long)g_padUpdateTarget, (unsigned)offsets::kPadWheel);
    LogInfo("hooked UpdateMoveForceByPad @ 0x%llX (force +0x%X, min %.2f max %.2f)",
            (unsigned long long)g_moveForceTarget, (unsigned)offsets::kWorkMoveForce,
            g_forceMin, g_forceMax);
    LogInfo("speed levels: %d, default %d, smooth %.2fs, top speed = vanilla run",
            (int)g_cfg.levels.size(), g_level + 1, g_cfg.smoothSeconds);
    LogInfo("zoom gate: %s (wheel accessor %s, camera zoom %s, binocular status %s)",
            g_cfg.gateZoom ? "on" : "off",
            g_padWheelGetterTarget ? "hooked" : "missing",
            g_zoomWheelTarget ? "hooked" : "missing",
            g_cfg.gateBinoc ? "on" : "off");
    LogLevel("initial level");
    return true;
}

void SpeedTick() {
    PollRuntimeFile();

    const double now = NowSeconds();
    if (g_cfg.heartbeatSeconds <= 0) return;
    if (now - g_lastHeartbeat < (double)g_cfg.heartbeatSeconds) return;
    g_lastHeartbeat = now;
    if (!g_origPadUpdate) return;
    if (g_frames == 0) {
        LogWarn("hook installed but no PadImpl::Update frames yet (still loading, or not in gameplay)");
        return;
    }
    LogBeat("level %d/%d mul %.2f (cur %.2f) force %.3f->%.3f stance %u flags 0x%X wheel %.2f frames %ld applies %ld",
            g_level + 1, (int)g_cfg.levels.size(), g_targetMul, g_currentMul,
            g_lastForceIn, g_lastForceOut, g_lastStance, g_lastFlags, g_lastWheel,
            (long)g_frames, (long)g_applyCount);
}

} // namespace sw
