#pragma once
#include <string>
#include <vector>

namespace ksc {

struct Config {
    bool  enabled           = true;   // master switch (F7 toggles)
    bool  hotkeyEnabled     = true;   // F7 = toggle, F8 = reset to top speed
    bool  logLevelChanges   = true;   // one line per wheel notch
    bool  logWheel          = true;   // log every raw wheel delta (probe aid)
    bool  logEveryFrame     = false;  // very noisy; for debugging only
    int   heartbeatSeconds  = 5;      // 0 disables the periodic state line

    bool  invertWheel       = false;  // wheel up = slower
    int   defaultLevel      = 7;      // index into levels (top = vanilla run)
    int   wheelStep         = 1;      // levels moved per wheel notch (1..3)

    // Each entry is a target move-speed *rate* (0..1) mapped through the game's
    // own force curve. 1.0 = vanilla keyboard run, lower = slower.
    std::vector<float> levels = { 0.10f, 0.23f, 0.36f, 0.49f, 0.62f, 0.75f, 0.88f, 1.00f };

    float smoothSeconds     = 0.12f;  // wheel notch -> force interpolation time
    float forceMin          = 0.0f;   // 0 = read the game's STICK_FORCE_MIN
    float forceMax          = 0.0f;   // 0 = read the game's STICK_FORCE_MAX

    bool  requireForeground = true;   // ignore the wheel when the game is not focused
    bool  requireCursorHidden = true; // ignore the wheel while a menu shows the cursor

    int   playerIndex       = -1;     // -1 = auto (first index seen); else force one
};

extern Config g_cfg;

bool LoadConfig(const std::string& iniPath);
void WriteDefaultConfigIfMissing(const std::string& iniPath);
std::string ConfigPathFor(const std::string& dllDir);

} // namespace ksc
