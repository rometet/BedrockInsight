#pragma once

#include "bedrock_insight/core/metrics.hpp"
#include "bedrock_insight/core/sqlite_api.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

struct StatRow final {
    std::string key;
    std::int64_t value = 0;
};

struct PlayerStatSummary final {
    std::string player_id;
    std::string display_name;
    StatsCategory category = StatsCategory::Unknown;
    std::vector<StatRow> rows;
    std::int64_t total = 0;
    // CUSTOM has incompatible units; an absent total must not become score 0.
    bool total_available = true;
    std::size_t row_count = 0;
    std::size_t offset = 0;
    std::size_t limit = 200;
};

// Read-only, per-request SQLite access for the in-game /stats commands and the
// exported companion API. It never writes, never shares the writer connection,
// and only executes fixed SQL with bound identifiers.
class StatsReader final {
public:
    explicit StatsReader(std::filesystem::path database_path);
    ~StatsReader();

    StatsReader(StatsReader const&) = delete;
    StatsReader& operator=(StatsReader const&) = delete;

    [[nodiscard]] bool open(std::string& error);
    void close();

    // Resolves a raw player id or an exact display name to a stored id.
    [[nodiscard]] bool find_player(
        std::string_view query,
        std::string& player_id,
        std::string& display_name,
        std::string& error) const;

    [[nodiscard]] bool player_summary(
        std::string_view player_id,
        StatsCategory category,
        PlayerStatSummary& result,
        std::string& error,
        std::size_t offset = 0,
        std::size_t limit = 200) const;

    [[nodiscard]] bool ranking(
        StatsCategory category,
        std::string_view custom_key,
        std::size_t offset,
        std::size_t limit,
        std::vector<RankingEntry>& result,
        std::string& error) const;

private:
    std::filesystem::path database_path_;
    mutable std::mutex mutex_;
    mutable sqlite_api::Library sqlite_;
    mutable sqlite_api::sqlite3* db_ = nullptr;
};

} // namespace bedrock_insight
