#include "common.h"

#include <windows.h>
#include <cstdio>
#include <string>

namespace mgsvspeed {

uintptr_t   g_base = 0;
size_t      g_size = 0;
std::string g_dllDir;

bool InitModuleInfo() {
    HMODULE mod = GetModuleHandleW(nullptr);
    if (!mod) return false;
    g_base = reinterpret_cast<uintptr_t>(mod);

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    g_size = nt->OptionalHeader.SizeOfImage;
    return true;
}

std::string GetExeVersionString() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (!size) return "unknown";

    std::string version = "unknown";
    void* data = malloc(size);
    if (data && GetFileVersionInfoW(path, 0, size, data)) {
        VS_FIXEDFILEINFO* info = nullptr;
        UINT len = 0;
        if (VerQueryValueW(data, L"\\", reinterpret_cast<LPVOID*>(&info), &len) && info) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                     HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                     HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
            version = buf;
        }
    }
    free(data);
    return version;
}

std::string Narrow(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_ACP, 0, ws.c_str(), (int)ws.size(), &out[0], len, nullptr, nullptr);
    return out;
}

std::string GetGameDir() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring ws(path);
    size_t pos = ws.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return {};
    return Narrow(ws.substr(0, pos));
}

} // namespace mgsvspeed
