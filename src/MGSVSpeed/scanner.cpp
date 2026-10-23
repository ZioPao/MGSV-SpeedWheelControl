#include "scanner.h"

#include <cstring>
#include <vector>

namespace mgsvspeed {

namespace {

struct PatternByte { uint8_t value; bool wildcard; };

std::vector<PatternByte> ParsePattern(const char* pattern) {
    std::vector<PatternByte> out;
    const char* p = pattern;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (p[0] == '?') {                       // "?" or "??"
            out.push_back({0, true});
            ++p;
            if (*p == '?') ++p;
            continue;
        }
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = hexVal(p[0]);
        int lo = p[1] ? hexVal(p[1]) : -1;
        if (hi < 0 || lo < 0) break;             // malformed -> stop
        out.push_back({(uint8_t)((hi << 4) | lo), false});
        p += 2;
    }
    return out;
}

} // namespace

uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern) {
    std::vector<PatternByte> pat = ParsePattern(pattern);
    if (pat.empty() || size < pat.size()) return 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(start);
    const size_t last = size - pat.size();
    for (size_t i = 0; i <= last; ++i) {
        size_t j = 0;
        for (; j < pat.size(); ++j) {
            if (!pat[j].wildcard && bytes[i + j] != pat[j].value) break;
        }
        if (j == pat.size()) return start + i;
    }
    return 0;
}

size_t FindPatternAll(uintptr_t start, size_t size, const char* pattern,
                      uintptr_t* out, size_t maxOut) {
    std::vector<PatternByte> pat = ParsePattern(pattern);
    size_t found = 0;
    if (pat.empty() || size < pat.size()) return 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(start);
    const size_t last = size - pat.size();
    for (size_t i = 0; i <= last && found < maxOut; ++i) {
        size_t j = 0;
        for (; j < pat.size(); ++j) {
            if (!pat[j].wildcard && bytes[i + j] != pat[j].value) break;
        }
        if (j == pat.size()) out[found++] = start + i;
    }
    return found;
}

bool MatchPattern(uintptr_t address, const char* pattern) {
    std::vector<PatternByte> pat = ParsePattern(pattern);
    if (pat.empty()) return false;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(address);
    for (size_t j = 0; j < pat.size(); ++j) {
        if (!pat[j].wildcard && bytes[j] != pat[j].value) return false;
    }
    return true;
}

} // namespace mgsvspeed
