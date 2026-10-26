// Speed Wheel Control - dynamic on-foot movement speed for MGSV:TPP, loaded as an IHHook
// plugin (MGS_TPP/plugins/SpeedWheel.dll, requires enable_dll_loader = true in
// ihhook_config.lua).
#include <windows.h>

#include <string>

#include "common.h"
#include "config.h"
#include "logger.h"
#include "sw_version.h"
#include "speed.h"

namespace {
HMODULE g_self = nullptr;

std::string DirOfModule(HMODULE mod) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(mod, path, MAX_PATH);
    std::wstring ws(path);
    size_t pos = ws.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return {};
    std::wstring dir = ws.substr(0, pos);
    int len = WideCharToMultiByte(CP_ACP, 0, dir.c_str(), (int)dir.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_ACP, 0, dir.c_str(), (int)dir.size(), &out[0], len, nullptr, nullptr);
    return out;
}

DWORD WINAPI InitThread(LPVOID) {
    sw::g_dllDir = DirOfModule(g_self);
    sw::LogInit(sw::g_dllDir + "\\SpeedWheel.log");

    sw::LogRaw(std::string("=== Speed Wheel Control v") + SW_VERSION +
                " (mouse wheel dynamic movement speed) ===");
    sw::LogInfo("dll dir: %s", sw::g_dllDir.c_str());

    if (!sw::InitModuleInfo()) {
        sw::LogError("could not read module info - aborting");
        return 0;
    }
    sw::LogInfo("target: %s  version %s  base 0x%llX  size 0x%zX",
                       sw::GetGameDir().c_str(),
                       sw::GetExeVersionString().c_str(),
                       (unsigned long long)sw::g_base,
                       sw::g_size);

    const std::string ini = sw::ConfigPathFor(sw::g_dllDir);
    sw::WriteDefaultConfigIfMissing(ini);
    sw::LoadConfig(ini);

    if (!sw::InstallSpeedHook()) {
        sw::LogError("speed hook installation failed - plugin is inert");
    }

    for (;;) {
        Sleep(100);
        sw::SpeedTick();
    }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    } else if (reason == DLL_PROCESS_DETACH) {
        // Never block shutdown on the game's own teardown.
    }
    return TRUE;
}
