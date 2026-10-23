#pragma once
#include <string>

namespace ksc {

void LogInit(const std::string& filePath);
void LogRaw(const std::string& line);
void LogInfo(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogBeat(const char* fmt, ...);

} // namespace ksc
