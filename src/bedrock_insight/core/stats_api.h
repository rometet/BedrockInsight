#ifndef BEDROCK_INSIGHT_STATS_API_H
#define BEDROCK_INSIGHT_STATS_API_H

/* Independent, trusted in-process C ABI. NOT LegacyRemoteCall compatible.
 * UTF-8 NUL-terminated inputs; caller-owned writable output. JSON contains
 * display names, never storage player ids. This API opens SQLite read-only.
 * A native caller already has full process privileges; there is no HTTP token.
 * Positive result: JSON byte length excluding NUL (inspect ok, too).
 * -1 null input, -2 page/buffer bounds, -3 capacity, -4 category, -5 exception.
 * Negative results clear out[0] when out != NULL and capacity > 0.
 * No buffer allocation or C++ object crosses the boundary.
 */
#define BI_STATS_API_CONTRACT "bedrockinsight.stats.c.v1"
#define BI_STATS_MAX_PAGE 1000
#define BI_STATS_MAX_LIMIT 100

#if defined(_WIN32)
#define BI_STATS_CALL __cdecl
#if defined(BEDROCKINSIGHT_EXPORTING_STATS_API)
#define BI_STATS_EXPORT __declspec(dllexport)
#else
#define BI_STATS_EXPORT
#endif
#else
#define BI_STATS_CALL
#define BI_STATS_EXPORT
#endif

#if defined(__cplusplus)
#define BI_STATS_NOEXCEPT noexcept
extern "C" {
#else
#define BI_STATS_NOEXCEPT
#endif

BI_STATS_EXPORT int BI_STATS_CALL getPlayerStats(
    const char* player, const char* category, char* out, int capacity) BI_STATS_NOEXCEPT;
BI_STATS_EXPORT int BI_STATS_CALL getRankStats(
    const char* category, int page, int limit, char* out, int capacity) BI_STATS_NOEXCEPT;
BI_STATS_EXPORT int BI_STATS_CALL getPlayerStatsPage(
    const char* player, const char* category, int page, int limit,
    char* out, int capacity) BI_STATS_NOEXCEPT;
BI_STATS_EXPORT int BI_STATS_CALL getRankStatsPage(
    const char* category, int page, int limit, char* out, int capacity) BI_STATS_NOEXCEPT;

/* For GetProcAddress on the already-loaded, enabled BedrockInsight module.
 * Resolve all four symbols; never load/reload a server mod to query it.
 * Keep pointers only for the provider's enabled lifetime. */
typedef int (BI_STATS_CALL *bi_get_player_stats_fn)(
    const char*, const char*, char*, int) BI_STATS_NOEXCEPT;
typedef int (BI_STATS_CALL *bi_get_rank_stats_fn)(
    const char*, int, int, char*, int) BI_STATS_NOEXCEPT;
typedef int (BI_STATS_CALL *bi_get_player_stats_page_fn)(
    const char*, const char*, int, int, char*, int) BI_STATS_NOEXCEPT;
typedef int (BI_STATS_CALL *bi_get_rank_stats_page_fn)(
    const char*, int, int, char*, int) BI_STATS_NOEXCEPT;

#if defined(__cplusplus)
}
#endif
#undef BI_STATS_EXPORT
#undef BI_STATS_NOEXCEPT
#endif
