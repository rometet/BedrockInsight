// Companion-facing C ABI. This is an independent equivalent of the four
// LK-Stats RemoteCall functions, exported by this DLL so another native mod can
// resolve them with GetProcAddress. Each function returns JSON in a
// caller-owned buffer and never exposes raw storage handles or SQL. No AGPL
// code is copied.
#define BEDROCKINSIGHT_EXPORTING_STATS_API
#include "bedrock_insight/core/remote_api.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"
#include "bedrock_insight/core/stats_reader.hpp"

#include <cstring>
#include <memory>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bedrock_insight {
namespace {
std::mutex g_mutex;
std::filesystem::path g_database_path;

void append_json_string(std::string& out, std::string_view value) {
    out += '"';
    for (char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) out += ' ';
            else out += c;
        }
    }
    out += '"';
}

int respond(std::string const& payload, char* buffer, int capacity) {
    if (buffer == nullptr || capacity <= 0) return -2;
    if(payload.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()-1))return -3;
    auto const length = static_cast<int>(payload.size());
    if (length + 1 > capacity) return -3;
    std::memcpy(buffer, payload.data(), static_cast<std::size_t>(length));
    buffer[length] = '\0';
    return length;
}

std::string error_json(std::string_view message) {
    std::string out = "{\"ok\":false,\"error\":";
    append_json_string(out, message);
    out += '}';
    return out;
}

std::unique_ptr<StatsReader> open_reader(std::string& error) {
    std::filesystem::path path;
    {
        std::lock_guard lock(g_mutex);
        if (g_database_path.empty()) {
            error = "statistics database is not configured";
            return {};
        }
        path = g_database_path;
    }
    auto reader = std::make_unique<StatsReader>(path);
    if (!reader->open(error)) return {};
    return reader;
}

std::string summary_json(PlayerStatSummary const& summary, int page, int limit) {
    // Never expose the raw player id/XUID; the display name is the public key.
    std::string out = "{\"ok\":true,\"player_name\":";
    append_json_string(out, summary.display_name);
    out += ",\"category\":";
    append_json_string(out, category_info(summary.category).id);
    out += ",\"category_label\":";
    append_json_string(out, category_info(summary.category).label);
    out += ",\"total\":" + (summary.total_available?std::to_string(summary.total):std::string("null"))
        + ",\"time_basis\":\"closed_session_lower_bound_50ms_ticks\",\"page\":" + std::to_string(page)
        + ",\"limit\":" + std::to_string(limit) + ",\"total_entries\":"+std::to_string(summary.row_count)
        + ",\"has_more\":"+(summary.offset+summary.rows.size()<summary.row_count?"true":"false")
        + ",\"collection_complete\":false,\"entries\":[";
    bool first = true;
    for (auto const& row : summary.rows) {
        if (!first) out += ',';
        first = false;
        out += "{\"key\":";
        append_json_string(out, row.key);
        out += ",\"label\":";
        append_json_string(out, custom_key_label(row.key));
        out += ",\"value\":" + std::to_string(row.value) + '}';
    }
    out += "]}";
    return out;
}

int run_player(char const* player, char const* category, int page, int limit, char* buffer, int capacity) {
    if (player == nullptr || category == nullptr) return -1;
    if (page < 0 || page > 1000 || limit < 1 || limit > 100) return -2;
    auto parsed = category_from_id(category);
    if (parsed == StatsCategory::Unknown) return -4;
    std::string error;
    auto reader = open_reader(error);
    if (!reader) return respond(error_json(error), buffer, capacity);
    std::string player_id, display_name;
    if (!reader->find_player(player, player_id, display_name, error)) {
        return respond(error_json(error), buffer, capacity);
    }
    PlayerStatSummary summary;
    if (!reader->player_summary(player_id, parsed, summary, error,
            static_cast<std::size_t>(page)*static_cast<std::size_t>(limit),static_cast<std::size_t>(limit))) {
        return respond(error_json(error), buffer, capacity);
    }
    summary.display_name = display_name;
    return respond(summary_json(summary, page, limit), buffer, capacity);
}

int run_rank(char const* category, int page, int limit, char* buffer, int capacity) {
    if (category == nullptr) return -1;
    if (page < 0 || page > 1000 || limit < 1 || limit > 100) return -2;
    std::string_view input{category};
    std::string_view custom_key;
    auto parsed = StatsCategory::Unknown;
    // "custom:<explicit key>" is required because CUSTOM mixes cm, tenths and
    // counts; a bare "custom" ranking would compare incompatible dimensions.
    if (input.rfind("custom:", 0) == 0) {
        parsed = StatsCategory::Custom;
        custom_key = input.substr(7);
    } else {
        parsed = category_from_id(input);
    }
    if (parsed == StatsCategory::Unknown) return -4;
    std::string error;
    auto reader = open_reader(error);
    if (!reader) return respond(error_json(error), buffer, capacity);
    std::vector<RankingEntry> rows;
    if (!reader->ranking(parsed, custom_key, static_cast<std::size_t>(page) * static_cast<std::size_t>(limit),
            static_cast<std::size_t>(limit), rows, error)) {
        return respond(error_json(error), buffer, capacity);
    }
    std::string out = "{\"ok\":true,\"category\":";
    if (parsed == StatsCategory::Custom) {
        append_json_string(out, std::string("custom:") + std::string(custom_key));
    } else {
        append_json_string(out, category_info(parsed).id);
    }
    out += ",\"category_label\":";
    append_json_string(out, category_info(parsed).label);
    out += ",\"page\":" + std::to_string(page) + ",\"limit\":" + std::to_string(limit) + ",\"entries\":[";
    bool first = true;
    int rank = page * limit + 1;
    for (auto const& row : rows) {
        if (!first) out += ',';
        first = false;
        out += "{\"rank\":" + std::to_string(rank++) + ",\"name\":";
        append_json_string(out, row.display_name);
        out += ",\"value\":" + std::to_string(row.value) + '}';
    }
    out += "]}";
    return respond(out, buffer, capacity);
}
} // namespace

void set_remote_api_database(std::filesystem::path database_path) {
    std::lock_guard lock(g_mutex);
    g_database_path = std::move(database_path);
}

} // namespace bedrock_insight

namespace {
template<class F> int api_boundary(char* buffer,int capacity,F&& run) noexcept {
    if(buffer==nullptr || capacity<=0)return -2;
    buffer[0]='\0';
    try{return run();}catch(...){buffer[0]='\0';return -5;}
}
}

extern "C" {
// getPlayerStats(player, category, buffer, capacity)
__declspec(dllexport) int getPlayerStats(
    char const* player, char const* category, char* buffer, int capacity) noexcept {
    return api_boundary(buffer,capacity,[&]{return bedrock_insight::run_player(player, category, 0, 100, buffer, capacity);});
}

// getRankStats(category, page, limit, buffer, capacity)
__declspec(dllexport) int getRankStats(
    char const* category, int page, int limit, char* buffer, int capacity) noexcept {
    return api_boundary(buffer,capacity,[&]{return bedrock_insight::run_rank(category, page, limit, buffer, capacity);});
}

// getPlayerStatsPage(player, category, page, limit, buffer, capacity)
__declspec(dllexport) int getPlayerStatsPage(
    char const* player, char const* category, int page, int limit, char* buffer, int capacity) noexcept {
    return api_boundary(buffer,capacity,[&]{return bedrock_insight::run_player(player, category, page, limit, buffer, capacity);});
}

// getRankStatsPage(category, page, limit, buffer, capacity)
__declspec(dllexport) int getRankStatsPage(
    char const* category, int page, int limit, char* buffer, int capacity) noexcept {
    return api_boundary(buffer,capacity,[&]{return bedrock_insight::run_rank(category, page, limit, buffer, capacity);});
}
}
