#pragma once
#include <cstdint>
#include <cstddef>

namespace sw {

// Byte pattern scanner with '?' wildcards, e.g. "4C 8B DC 49 89 5B ? 55".
// Returns the first match address (absolute) inside [start, start+size), or 0.
uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern);

// Compare memory against a wildcard pattern (used to validate hardcoded addrs).
bool MatchPattern(uintptr_t address, const char* pattern);

} // namespace sw
