#include "ll/api/mod/RegisterHelper.h"

// Required by the 26.60 NativeModManager. Use the SDK's actual allocation
// overrides in exactly one translation unit, not a stand-alone marker export.
#define LL_MEMORY_OPERATORS
#include "ll/api/memory/MemoryOperators.h"
#undef LL_MEMORY_OPERATORS

#include "bedrock_insight/levilamina/runtime.hpp"

namespace bedrock_insight::levilamina {

class BedrockInsightMod final {
public:
    bool load() noexcept {
        return runtime_.load();
    }

    bool enable() noexcept {
        return runtime_.enable();
    }

    bool disable() noexcept {
        return runtime_.disable();
    }

    bool unload() noexcept {
        return runtime_.unload();
    }

private:
    bedrock_insight::levilamina::Runtime runtime_;
};

} // namespace bedrock_insight::levilamina

namespace {
bedrock_insight::levilamina::BedrockInsightMod g_bedrock_insight_mod;
}

LL_REGISTER_MOD(bedrock_insight::levilamina::BedrockInsightMod, g_bedrock_insight_mod);
