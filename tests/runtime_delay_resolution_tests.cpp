// A standalone EXE, NOT engine instrumentation or a fake native runtime PASS.
// Uses the real pinned SymbolProvider static dependency and one synthetic import.
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" __declspec(dllimport) int bedrock_insight_delay_probe();
namespace {
int resolutions=0; // constant-initialized before SymbolProvider CRT preload
int probe() { return 42; }
}
namespace ll::memory {
void* resolveSymbol(char const* name) {
    if (std::strcmp(name,"bedrock_insight_delay_probe")!=0) std::abort();
    ++resolutions;
    return reinterpret_cast<void*>(&probe);
}
}
int main() {
#ifdef BEDROCKINSIGHT_TEST_MSVC_DELAY_CONTROL
    // A caught standalone negative control, no crash dialog/engine process.
    __try { static_cast<void>(bedrock_insight_delay_probe()); }
    __except (GetExceptionCode()==0xC06D007E ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        std::cout << "Regular delayimp negative control PASS (caught C06D007E, same host-trial code)\n";
        return 0;
    }
    return 3; // a real bedrock_runtime.dll would invalidate this control
#else
    // SymbolProvider's CRT initializer resolves imports before main. Subsequent
    // calls must hit the resolved IAT, never LoadLibrary("bedrock_runtime.dll").
    if (resolutions!=1 || GetModuleHandleW(L"bedrock_runtime.dll")!=nullptr) return 1;
    if (bedrock_insight_delay_probe()!=42 || bedrock_insight_delay_probe()!=42 || resolutions!=1) return 2;
    std::cout << "Pinned SymbolProvider synthetic delay resolution PASS (preloaded once, two calls, no runtime DLL)\n";
#endif
}
