#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <share.h>
#include <string>

namespace sw {

namespace {
std::mutex g_mutex;
FILE*      g_file = nullptr;

std::string Timestamp() {
    time_t t = time(nullptr);
    tm tmv{};
    localtime_s(&tmv, &t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

void WriteLine(const char* level, const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) return;
    char msg[2048];
    vsnprintf(msg, sizeof(msg), fmt, args);
    fprintf(g_file, "[%s][%s] %s\n", Timestamp().c_str(), level, msg);
    fflush(g_file);
}
} // namespace

void LogInit(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) return;
    // _SH_DENYNO keeps the log readable while the game is running.
    g_file = _fsopen(filePath.c_str(), "w", _SH_DENYNO);
    if (!g_file) fopen_s(&g_file, filePath.c_str(), "w");
    if (g_file) {
        fprintf(g_file, "=== Speed Wheel Control log (%s) ===\n", Timestamp().c_str());
        fflush(g_file);
    }
}

void LogRaw(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) return;
    fprintf(g_file, "%s\n", line.c_str());
    fflush(g_file);
}

void LogInfo(const char* fmt, ...)  { va_list a; va_start(a, fmt); WriteLine("info",  fmt, a); va_end(a); }
void LogWarn(const char* fmt, ...)  { va_list a; va_start(a, fmt); WriteLine("warn",  fmt, a); va_end(a); }
void LogError(const char* fmt, ...) { va_list a; va_start(a, fmt); WriteLine("error", fmt, a); va_end(a); }
void LogBeat(const char* fmt, ...)  { va_list a; va_start(a, fmt); WriteLine("beat",  fmt, a); va_end(a); }

} // namespace sw
