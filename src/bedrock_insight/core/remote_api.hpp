#pragma once

#include <filesystem>

namespace bedrock_insight {
// Publishes the database path used by the exported companion statistics API.
// Called once during mod load; it does not open the database.
void set_remote_api_database(std::filesystem::path database_path);
} // namespace bedrock_insight

// Independent native C ABI, NOT LegacyRemoteCall wire compatibility. Trusted
// in-process callers own valid NUL-terminated UTF-8 input strings and writable
// output buffers. No player UUID/XUID is returned. Page is zero-based,0..1000;
// limit1..100. Positive return = JSON byte count without NUL (ok may be false).
// -1 null input,-2 invalid page/buffer,-3 insufficient capacity,-4 unknown
// category,-5 contained C++ exception. Output is cleared on negative return.
// Empty/unmeasured category total and mixed-unit CUSTOM total are JSON null.
#ifdef BEDROCKINSIGHT_EXPORTING_STATS_API
#define BI_STATS_API __declspec(dllexport)
#else
#define BI_STATS_API
#endif
extern "C" {
BI_STATS_API int getPlayerStats(char const*,char const*,char*,int) noexcept;
BI_STATS_API int getRankStats(char const*,int,int,char*,int) noexcept;
BI_STATS_API int getPlayerStatsPage(char const*,char const*,int,int,char*,int) noexcept;
BI_STATS_API int getRankStatsPage(char const*,int,int,char*,int) noexcept;
}
#undef BI_STATS_API
