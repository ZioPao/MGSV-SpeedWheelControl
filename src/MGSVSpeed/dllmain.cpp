// MGSVSpeed - dynamic on-foot movement speed for MGSV:TPP, loaded as an IHHook
// plugin (MGS_TPP/plugins/MGSVSpeed.dll, requires enable_dll_loader = true in
// ihhook_config.lua).
#include <windows.h>

#include <string>

#include "common.h"
#include "config.h"
#include "logger.h"
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
    mgsvspeed::g_dllDir = DirOfModule(g_self);
    mgsvspeed::LogInit(mgsvspeed::g_dllDir + "\\MGSVSpeed.log");

    mgsvspeed::LogRaw("=== MGSVSpeed v1.0.0 (mouse wheel dynamic movement speed) ===");
    mgsvspeed::LogInfo("dll dir: %s", mgsvspeed::g_dllDir.c_str());

    if (!mgsvspeed::InitModuleInfo()) {
        mgsvspeed::LogError("could not read module info - aborting");
        return 0;
    }
    mgsvspeed::LogInfo("target: %s  version %s  base 0x%llX  size 0x%zX",
                       mgsvspeed::GetGameDir().c_str(),
                       mgsvspeed::GetExeVersionString().c_str(),
                       (unsigned long long)mgsvspeed::g_base,
                       mgsvspeed::g_size);

    const std::string ini = mgsvspeed::ConfigPathFor(mgsvspeed::g_dllDir);
    mgsvspeed::WriteDefaultConfigIfMissing(ini);
    mgsvspeed::LoadConfig(ini);

    if (!mgsvspeed::InstallSpeedHook()) {
        mgsvspeed::LogError("speed hook installation failed - plugin is inert");
    }

    for (;;) {
        Sleep(1000);
        mgsvspeed::SpeedTick();
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
