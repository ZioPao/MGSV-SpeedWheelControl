#include "config.h"
#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace sw {

Config g_cfg;

std::string ConfigPathFor(const std::string& dllDir) {
    return dllDir + "\\SpeedWheel.ini";
}

namespace {

bool ParseBool(const char* v, bool def) {
    if (!v) return def;
    return atoi(v) != 0 || _stricmp(v, "true") == 0 || _stricmp(v, "yes") == 0;
}

std::vector<float> ParseFloatList(const char* v, const std::vector<float>& def) {
    std::vector<float> out;
    if (!v) return def;
    const char* p = v;
    while (*p) {
        char* end = nullptr;
        float f = strtof(p, &end);
        if (end == p) break;
        out.push_back(f);
        p = end;
        while (*p == ',' || *p == ' ' || *p == '\t') ++p;
    }
    if (out.size() < 2) return def;
    return out;
}

} // namespace

bool LoadConfig(const std::string& iniPath) {
    FILE* f = nullptr;
    if (fopen_s(&f, iniPath.c_str(), "r") != 0 || !f) {
        LogWarn("config not found (%s) - using defaults", iniPath.c_str());
        return false;
    }

    char line[1024];
    int applied = 0;
    while (fgets(line, sizeof(line), f)) {
        char* hash = strchr(line, ';');
        if (hash) *hash = 0;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = line;
        char* val = eq + 1;
        auto trim = [](char* s) {
            while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
            char* end = s + strlen(s);
            while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
            return s;
        };
        key = trim(key);
        val = trim(val);
        if (!*key) continue;

        if      (!_stricmp(key, "enabled"))             g_cfg.enabled = ParseBool(val, g_cfg.enabled);
        else if (!_stricmp(key, "hotkeyEnabled"))       g_cfg.hotkeyEnabled = ParseBool(val, g_cfg.hotkeyEnabled);
        else if (!_stricmp(key, "logLevelChanges"))     g_cfg.logLevelChanges = ParseBool(val, g_cfg.logLevelChanges);
        else if (!_stricmp(key, "logWheel"))            g_cfg.logWheel = ParseBool(val, g_cfg.logWheel);
        else if (!_stricmp(key, "logEveryFrame"))       g_cfg.logEveryFrame = ParseBool(val, g_cfg.logEveryFrame);
        else if (!_stricmp(key, "heartbeatSeconds"))    g_cfg.heartbeatSeconds = (int)strtol(val, nullptr, 0);
        else if (!_stricmp(key, "invertWheel"))         g_cfg.invertWheel = ParseBool(val, g_cfg.invertWheel);
        else if (!_stricmp(key, "wheelStep"))           g_cfg.wheelStep = (int)strtol(val, nullptr, 0);
        else if (!_stricmp(key, "defaultLevel"))        g_cfg.defaultLevel = (int)strtol(val, nullptr, 0);
        else if (!_stricmp(key, "walkKey"))             g_cfg.walkKey = (int)strtol(val, nullptr, 0);
        else if (!_stricmp(key, "levels"))              g_cfg.levels = ParseFloatList(val, g_cfg.levels);
        else if (!_stricmp(key, "smoothSeconds"))       g_cfg.smoothSeconds = (float)atof(val);
        else if (!_stricmp(key, "forceMin"))            g_cfg.forceMin = (float)atof(val);
        else if (!_stricmp(key, "forceMax"))            g_cfg.forceMax = (float)atof(val);
        else if (!_stricmp(key, "requireForeground"))   g_cfg.requireForeground = ParseBool(val, g_cfg.requireForeground);
        else if (!_stricmp(key, "requireCursorHidden")) g_cfg.requireCursorHidden = ParseBool(val, g_cfg.requireCursorHidden);
        else if (!_stricmp(key, "playerIndex"))         g_cfg.playerIndex = (int)strtol(val, nullptr, 0);
        else {
            LogWarn("config: unknown key '%s'", key);
            continue;
        }
        ++applied;
    }
    fclose(f);

    if (g_cfg.defaultLevel < 0) g_cfg.defaultLevel = 0;
    if (g_cfg.defaultLevel >= (int)g_cfg.levels.size()) g_cfg.defaultLevel = (int)g_cfg.levels.size() - 1;
    if (g_cfg.walkKey < 0) g_cfg.walkKey = 0;
    if (g_cfg.walkKey > 2) g_cfg.walkKey = 2;
    if (g_cfg.smoothSeconds < 0.0f) g_cfg.smoothSeconds = 0.0f;

    LogInfo("config loaded: %d keys from %s", applied, iniPath.c_str());
    return true;
}

void WriteDefaultConfigIfMissing(const std::string& iniPath) {
    FILE* probe = nullptr;
    if (fopen_s(&probe, iniPath.c_str(), "r") == 0 && probe) {
        fclose(probe);
        return;
    }
    FILE* f = nullptr;
    if (fopen_s(&f, iniPath.c_str(), "w") != 0 || !f) return;

    fprintf(f,
        "; Speed Wheel Control - dynamic on-foot movement speed (mouse wheel)\n"
        "; 1.0 speed rate = vanilla keyboard run. The wheel can only slow you down\n"
        "; from vanilla; wheel up returns to normal speed.\n"
        "\n"
        "enabled=1\n"
        "hotkeyEnabled=1          ; F7 toggle, F8 reset to top speed\n"
        "\n"
        "logLevelChanges=1\n"
        "logWheel=1               ; raw wheel deltas, useful to verify the input\n"
        "logEveryFrame=0\n"
        "heartbeatSeconds=5       ; 0 disables\n"
        "\n"
        "invertWheel=0            ; 1 = wheel up slows down\n"
        "wheelStep=1              ; levels moved per wheel notch (1..3)\n"
        "defaultLevel=7           ; index into levels (7 = vanilla run)\n"
        "walkKey=1                ; walk key: 0 off, 1 reset + no walk, 2 reset + walk\n"
        "\n"
        "; target move-speed rate per notch (0..1), 1.0 = vanilla run\n"
        "levels=0.10,0.23,0.36,0.49,0.62,0.75,0.88,1.00\n"
        "\n"
        "smoothSeconds=0.12       ; gait transition smoothing\n"
        "forceMin=0               ; 0 = read the game's STICK_FORCE_MIN (0.1)\n"
        "forceMax=0               ; 0 = read the game's STICK_FORCE_MAX (0.95)\n"
        "\n"
        "requireForeground=1\n"
        "requireCursorHidden=1    ; ignore the wheel while a menu shows the cursor\n"
        "\n"
        "playerIndex=-1           ; -1 = auto-detect the local player's index\n");
    fclose(f);
    LogInfo("wrote default config to %s", iniPath.c_str());
}

} // namespace sw
