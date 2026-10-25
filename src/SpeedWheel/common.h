#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace sw {

extern uintptr_t g_base;      // mgsvtpp.exe base address
extern size_t    g_size;      // size of image in memory
extern std::string g_dllDir;  // folder the plugin DLL lives in (MGS_TPP\plugins)

bool InitModuleInfo();
std::string GetExeVersionString();
std::string GetGameDir();

} // namespace sw
