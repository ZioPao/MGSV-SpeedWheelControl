#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <excpt.h>

namespace sw {

extern uintptr_t g_base;      // mgsvtpp.exe base address
extern size_t    g_size;      // size of image in memory
extern std::string g_dllDir;  // folder the plugin DLL lives in (MGS_TPP\plugins)

bool InitModuleInfo();
std::string GetExeVersionString();
std::string GetGameDir();

// Monotonic seconds (QPC) shared by the hooks and the gate.
double NowSeconds();

template <typename T>
inline bool SafeRead(uintptr_t addr, T& out) {
    if (!addr) return false;
    __try {
        out = *reinterpret_cast<const T*>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
inline bool SafeWrite(uintptr_t addr, const T& value) {
    if (!addr) return false;
    __try {
        *reinterpret_cast<T*>(addr) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace sw
