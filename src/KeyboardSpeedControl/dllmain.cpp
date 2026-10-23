// Keyboard Speed Control - dynamic on-foot movement speed for MGSV:TPP, loaded as an IHHook
// plugin (MGS_TPP/plugins/KeyboardSpeedControl.dll, requires enable_dll_loader = true in
// ihhook_config.lua).
#include <windows.h>

#include <string>

#include "common.h"
#include "config.h"
#include "logger.h"
#include "ksc_version.h"
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
    ksc::g_dllDir = DirOfModule(g_self);
    ksc::LogInit(ksc::g_dllDir + "\\KeyboardSpeedControl.log");

    ksc::LogRaw(std::string("=== Keyboard Speed Control v") + KSC_VERSION +
                " (mouse wheel dynamic movement speed) ===");
    ksc::LogInfo("dll dir: %s", ksc::g_dllDir.c_str());

    if (!ksc::InitModuleInfo()) {
        ksc::LogError("could not read module info - aborting");
        return 0;
    }
    ksc::LogInfo("target: %s  version %s  base 0x%llX  size 0x%zX",
                       ksc::GetGameDir().c_str(),
                       ksc::GetExeVersionString().c_str(),
                       (unsigned long long)ksc::g_base,
                       ksc::g_size);

    const std::string ini = ksc::ConfigPathFor(ksc::g_dllDir);
    ksc::WriteDefaultConfigIfMissing(ini);
    ksc::LoadConfig(ini);

    if (!ksc::InstallSpeedHook()) {
        ksc::LogError("speed hook installation failed - plugin is inert");
    }

    for (;;) {
        Sleep(250);
        ksc::SpeedTick();
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
