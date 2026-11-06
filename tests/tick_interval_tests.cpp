#include "bedrock_insight/core/tick_interval.hpp"
#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/sqlite_api.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace bedrock_insight;
void check(bool value, char const* what) { if (!value) throw std::runtime_error(what); }
int main() {
    try {
        int primary = 0, other = 0;
        TickIntervalSampler sampler;
        check(!sampler.observe(&primary, 0), "first callback is warmup");
        for (int i = 1; i < 100; ++i) check(!sampler.observe(&primary, i * 50000), "bounded cadence");
        auto window = sampler.observe(&primary, 5000000);
        check(window && window->intervals == 100 && window->elapsed_us == 5000000, "100 intervals not 101 callbacks");
        check(window->effective_tps() == 20.0, "20 measured TPS");
        check(!sampler.observe(&primary, 5000000), "duplicate timestamp ignored");
        check(!sampler.observe(&primary, 4999999), "backward timestamp ignored");
        for (int i = 1; i < 50; ++i) check(!sampler.observe(&primary, 5000000 + i * 100000), "second window");
        window = sampler.observe(&primary, 10000000);
        check(window && window->intervals == 50 && window->effective_tps() == 10.0, "measured slowdown");
        check(!sampler.observe(&primary, 41000000), "long gap discarded not counted as live");
        for (int i = 1; i < 100; ++i) check(!sampler.observe(&primary, 41000000 + i * 50000), "fresh warmup after gap");
        check(sampler.observe(&primary, 46000000)->effective_tps() == 20.0, "resumes fresh");
        check(!sampler.observe(&other, 46050000) && !sampler.supported(), "multiple Level fail closed");
        check(!sampler.observe(&primary, 52000000), "cannot resume after mixed Level");
        check(TickIntervalWindow{5000000, 150}.effective_tps() == 20.0, "upper clamp");
        check(TickIntervalWindow{}.effective_tps() == 0.0, "no samples not guessed");

        // Real single-writer SQLite pipeline, not a BDS/game fixture.
        auto root = std::filesystem::current_path() / ("fixture-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        StorageWriter writer({root / "telemetry.sqlite3", 32});
        check(writer.start(), "writer start with historical migrations");
        AnalyticsEvent value;
        value.kind = EventKind::ServerTick; value.actor.kind = ActorKind::System;
        value.occurred_at_ms = 1793916000000; value.cause = "ll26.60.interval.v1";
        value.session_id = "lltick-native-fixture"; value.amount = 100; value.online_players = 2;
        value.details_json = R"({"clock":"steady","window_us":5000000,"sampled_peak_online":2})";
        check(writer.submit(value) && writer.flush(), "interval uses immutable queue and writer");
        writer.stop();
        check(writer.metrics().committed_events == 1 && !writer.metrics().failed, "one window one write");
        sqlite_api::Library library;
        sqlite_api::sqlite3* db = nullptr;
        check(library.open_v2((root / "telemetry.sqlite3").string().c_str(), &db, sqlite_api::kReadOnly, nullptr) == 0, "read persisted window");
        auto scalar = [&](char const* sql) {
            sqlite_api::sqlite3_stmt* stmt = nullptr;
            check(library.prepare_v2(db, sql, -1, &stmt, nullptr) == 0, "prepare");
            check(library.step(stmt) == sqlite_api::kRow, "row");
            auto n = library.column_int64(stmt, 0); library.finalize(stmt); return n;
        };
        check(scalar("PRAGMA user_version") == 4, "no destructive schema change");
        check(scalar("SELECT COUNT(*) FROM events WHERE cause='ll26.60.interval.v1' AND tick_mspt=0 AND amount=100 AND online_players=2") == 1, "typed provenance persisted");
        check(scalar("SELECT COUNT(*) FROM server_tick_samples") == 0, "interval must never masquerade as MSPT");
        library.close(db);
        std::cout << "Tick interval formulas/windowing/multi-Level/storage: PASS\nFixture: " << root.string() << '\n';
        return 0;
    } catch (std::exception const& e) { std::cerr << e.what() << '\n'; return 1; }
}
