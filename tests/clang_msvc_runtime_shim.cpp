// Only used by the local MinGW/clang verification command when it consumes
// the cached MSVC-built SQLite static library. A normal MSVC LeviLamina build
// supplies these symbols from the Microsoft CRT and does not compile this file.
#include <cstdint>

#ifndef _MSC_VER
extern "C" {
std::uintptr_t __security_cookie = 0x2B992DDFA232ULL;
void __security_check_cookie(std::uintptr_t) {}
void __chkstk() {}
int _fltused = 0;
}
#endif
