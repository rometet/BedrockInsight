#include "bedrock_insight/core/event_queue.hpp"
#include "bedrock_insight/core/api_payload.hpp"
#include "bedrock_insight/core/api_json.hpp"
#include "bedrock_insight/core/admin_api.hpp"
#include "bedrock_insight/core/analytics_query.hpp"
#include "bedrock_insight/core/http_transport.hpp"
#include "bedrock_insight/core/http_server.hpp"
#include "bedrock_insight/core/json.hpp"
#include "bedrock_insight/core/metrics.hpp"
#include "bedrock_insight/core/rollback.hpp"
#include "bedrock_insight/core/schema.hpp"
#include "bedrock_insight/core/storage.hpp"

#include "bedrock_insight/core/sqlite_api.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

void check(bool condition, char const* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

#ifdef _WIN32
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
#else
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;
#endif

void close_test_socket(TestSocket socket) noexcept {
    if (socket == kInvalidTestSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

std::string test_http_roundtrip(std::uint16_t port, std::string const& request) {
    auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(socket != kInvalidTestSocket, "test client socket creation");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    check(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1, "test client loopback address");
    check(::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "test client connect");

    std::size_t offset = 0;
    while (offset < request.size()) {
        auto chunk = static_cast<int>(std::min<std::size_t>(request.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
#ifdef _WIN32
        auto sent = ::send(socket, request.data() + offset, chunk, 0);
#else
        auto sent = static_cast<int>(::send(socket, request.data() + offset, static_cast<std::size_t>(chunk), 0));
#endif
        check(sent > 0, "test client send");
        offset += static_cast<std::size_t>(sent);
    }

    std::string response;
    std::array<char, 1024> buffer{};
    for (;;) {
#ifdef _WIN32
        auto received = ::recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        auto received = static_cast<int>(::recv(socket, buffer.data(), buffer.size(), 0));
#endif
        if (received <= 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(received));
    }
    close_test_socket(socket);
    return response;
}

std::int64_t scalar(bedrock_insight::sqlite_api::Library& library, bedrock_insight::sqlite_api::sqlite3* db, char const* sql) {
    bedrock_insight::sqlite_api::sqlite3_stmt* statement = nullptr;
    check(library.prepare_v2(db, sql, -1, &statement, nullptr) == bedrock_insight::sqlite_api::kOk, library.errmsg(db));
    auto result = library.step(statement);
    check(result == bedrock_insight::sqlite_api::kRow, library.errmsg(db));
    auto value = library.column_int64(statement, 0);
    library.finalize(statement);
    return value;
}

std::string text_scalar(bedrock_insight::sqlite_api::Library& library, bedrock_insight::sqlite_api::sqlite3* db, char const* sql) {
    bedrock_insight::sqlite_api::sqlite3_stmt* statement = nullptr;
    check(library.prepare_v2(db, sql, -1, &statement, nullptr) == bedrock_insight::sqlite_api::kOk, library.errmsg(db));
    auto result = library.step(statement);
    check(result == bedrock_insight::sqlite_api::kRow, library.errmsg(db));
    auto* value = library.column_text(statement, 0);
    check(value != nullptr, "expected text result");
    std::string copy(value);
    library.finalize(statement);
    return copy;
}

void queue_test() {
    using namespace bedrock_insight;
    EventQueue queue(2);

    AnalyticsEvent first;
    first.kind = EventKind::BlockBreak;
    first.primary_type = "minecraft:stone";
    auto accepted_first = queue.try_push(first);
    AnalyticsEvent second{};
    second.kind = EventKind::BlockPlace;
    auto accepted_second = queue.try_push(second);
    AnalyticsEvent rejected_event{};
    rejected_event.kind = EventKind::ItemUse;
    auto rejected = queue.try_push(rejected_event);
    check(accepted_first.accepted && accepted_first.sequence == 1, "first queue push");
    check(accepted_second.accepted && accepted_second.sequence == 2, "second queue push");
    check(!rejected.accepted, "bounded queue must reject when full");

    EventQueue::EventPtr value;
    check(queue.wait_pop(value) && value->primary_type == "minecraft:stone", "FIFO order");
    check(queue.wait_pop(value) && value->kind == EventKind::BlockPlace, "second FIFO value");
    queue.close();
    check(!queue.wait_pop(value), "closed empty queue must stop consumer");
}

void load_safety_regression_test() {
    using namespace bedrock_insight;
    auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("bedrock-insight-reopen-" + std::to_string(stamp) + ".db");
    AnalyticsEvent event;
    event.kind = EventKind::BlockPlace;
    event.actor = {ActorKind::Player, "uuid:test", "Test"};
    event.occurred_at_ms = 1'735'689'600'000;
    event.primary_type = "minecraft:stone";
    for (int restart = 0; restart < 3; ++restart) {
        StorageWriter writer({path, 4096});
        check(!writer.submit(event), "reject event before storage startup");
        check(writer.start(), writer.last_error().c_str());
        std::atomic<int> rejected{0};
        std::vector<std::thread> producers;
        for (int producer = 0; producer < 4; ++producer) {
            producers.emplace_back([&] {
                for (int index = 0; index < 100; ++index) {
                    if (!writer.submit(event)) ++rejected;
                }
            });
        }
        for (auto& producer : producers) producer.join();
        check(rejected == 0 && writer.flush(), "concurrent producer flush after reopen");
        check(writer.metrics().committed_events == 400, "all concurrent events committed");
        writer.stop();
        writer.stop();
        check(!writer.start() && !writer.submit(event), "closed writer cannot report a successful restart");
    }
    sqlite_api::Library library;
    sqlite_api::sqlite3* db = nullptr;
    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadWrite, nullptr) == sqlite_api::kOk,
        "open restarted database");
    check(scalar(library, db, "SELECT COUNT(*) FROM events;") == 1200, "restart preserves all event rows");
    check(scalar(library, db, "SELECT MAX(event_id) FROM events;") == 1200, "event IDs resume after restart");
    check(scalar(library, db, "SELECT SUM(blocks_placed) FROM player_daily;") == 1200,
        "no duplicate aggregate updates on reopen");
    check(library.exec(db, "CREATE TRIGGER fail_events BEFORE INSERT ON events BEGIN SELECT RAISE(ABORT,'injected failure'); END;",
        nullptr, nullptr, nullptr) == sqlite_api::kOk, "install worker failure fixture");
    library.close(db);
    {
        StorageWriter writer({path, 4});
        check(writer.start(), writer.last_error().c_str());
        check(writer.submit(event), "submit event before injected worker failure");
        check(!writer.flush() && writer.metrics().failed, "worker write failure reported to flush");
        check(!writer.submit(event), "failed worker rejects later events");
        // Before the review fix, this early-exited joinable thread caused
        // std::terminate in the StorageWriter destructor.
    }
    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadWrite, nullptr) == sqlite_api::kOk,
        "open after worker failure");
    check(scalar(library, db, "SELECT COUNT(*) FROM events;") == 1200, "failed event transaction rolled back");
    check(scalar(library, db, "SELECT SUM(blocks_placed) FROM player_daily;") == 1200,
        "failed event leaves aggregate unchanged");
    check(library.exec(db, "PRAGMA user_version=999;", nullptr, nullptr, nullptr) == sqlite_api::kOk,
        "install unsupported schema fixture");
    library.close(db);
    {
        StorageWriter writer({path, 4});
        check(!writer.start(), "newer schema fails closed");
        check(!writer.submit(event) && !writer.flush(), "startup failure rejects writes and flush");
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");

    std::string error;
    ApiTokenStore missing_hasher({});
    check(!missing_hasher.add_token("t", "s", ApiRole::Admin, "a", 0, error)
        && !missing_hasher.authenticate("s"), "unconfigured hasher fails closed");
    ApiTokenStore throwing_hasher([](std::string_view) -> std::string { throw std::runtime_error("hash failure"); });
    check(!throwing_hasher.authenticate("s"), "hasher failure does not escape authentication");
    ApiTokenStore invalid_role([](std::string_view) { return std::string("test-only-hash"); });
    check(!invalid_role.add_hashed_token({"t", "h", "a", static_cast<ApiRole>(255), 0, true}, error)
        && !role_allows(static_cast<ApiRole>(255), ApiPermission::ApplyRollback), "invalid roles cannot escalate");
}

void storage_test() {
    using namespace bedrock_insight;
    auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("bedrock-insight-test-" + std::to_string(stamp) + ".db");
    std::filesystem::remove(path);

    StorageWriter writer({path, 32});
    check(writer.start(), writer.last_error().c_str());

    auto player = Actor{ActorKind::Player, "xuid-42", "Ada"};
    auto start = AnalyticsEvent{
        .occurred_at_ms = 1'735'689'600'000,
        .kind = EventKind::SessionStart,
        .actor = player,
        .world_id = "world",
        .position = {},
        .primary_type = {},
        .secondary_type = {},
        .session_id = "session-1",
        .cause = {},
        .amount = 1,
        .details_json = {},
    };
    check(writer.submit(start), "session event submit");

    auto break_event = start;
    break_event.kind = EventKind::BlockBreak;
    break_event.primary_type = "minecraft:stone";
    break_event.amount = 3;
    check(writer.submit(break_event), "break event submit");

    auto place_event = start;
    place_event.kind = EventKind::BlockPlace;
    place_event.primary_type = "minecraft:torch";
    check(writer.submit(place_event), "place event submit");

    auto unknown = start;
    unknown.kind = EventKind::AuditBlockChange;
    unknown.actor = Actor{ActorKind::Unknown, "", "Unknown"};
    unknown.primary_type = "minecraft:air";
    unknown.secondary_type = "minecraft:chest";
    unknown.cause = "redstone";
    unknown.details_json = R"({"source":"test"})";
    check(writer.submit(unknown), "unknown event submit");

    auto earlier_unknown = unknown;
    earlier_unknown.occurred_at_ms -= 10'000;
    earlier_unknown.primary_type = "minecraft:dirt";
    earlier_unknown.secondary_type = "minecraft:grass_block";
    earlier_unknown.details_json = R"({"source":"earlier-test"})";
    check(writer.submit(earlier_unknown), "earlier unknown audit submit");

    auto tick = start;
    tick.kind = EventKind::ServerTick;
    tick.actor = Actor{ActorKind::System, "", "server"};
    tick.session_id.clear();
    tick.tick_mspt = 40;
    tick.online_players = 2;
    check(writer.submit(tick), "server tick submit");

    check(writer.flush(), writer.last_error().c_str());
    auto metrics = writer.metrics();
    check(metrics.accepted_events == 6 && metrics.committed_events == 6, "writer metrics");
    writer.stop();

    sqlite_api::Library library;
    std::string error;
    sqlite_api::sqlite3* db = nullptr;
    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadOnly, nullptr) == sqlite_api::kOk, "reopen database");
    check(text_scalar(library, db, "PRAGMA journal_mode;") == "wal", "WAL journal mode");
    check(scalar(library, db, "SELECT user_version FROM pragma_user_version;") == schema::kVersion, "schema version");
    check(scalar(library, db, "SELECT COUNT(*) FROM events;") == 6, "event count");
    check(scalar(library, db, "SELECT COUNT(*) FROM audit_events;") == 2, "audit count");
    check(scalar(library, db, "SELECT blocks_broken FROM player_daily WHERE player_id='xuid-42';") == 3, "daily break aggregate");
    check(scalar(library, db, "SELECT blocks_placed FROM player_daily WHERE player_id='xuid-42';") == 1, "daily place aggregate");
    check(scalar(library, db, "SELECT amount FROM player_daily_types WHERE player_id='xuid-42' AND metric='blocks_broken' AND value_type='minecraft:stone';") == 3, "typed daily aggregate");
    check(scalar(library, db, "SELECT COUNT(*) FROM server_tick_samples;") == 1, "server sample count");
    check(scalar(library, db, "SELECT COUNT(*) FROM events WHERE actor_kind='unknown' AND actor_id='';") == 2, "unknown actor retention");
    library.close(db);

    AnalyticsQuery query(path);
    std::string query_error;
    check(query.open(query_error), query_error.c_str());
    std::vector<RankingEntry> rankings;
    check(query.ranking(
              RankingMetric::BlocksBroken,
              RankingPeriod::Day,
              "2025-01-01",
              10,
              rankings,
              query_error), query_error.c_str());
    check(rankings.size() == 1 && rankings[0].player_id == "xuid-42" && rankings[0].value == 3, "durable daily ranking");
    check(query.ranking(
              RankingMetric::BlocksBroken,
              RankingPeriod::Month,
              "2025-01",
              10,
              rankings,
              query_error), query_error.c_str());
    check(rankings.size() == 1 && rankings[0].value == 3, "durable monthly ranking");
    check(query.ranking(
              RankingMetric::BlocksBroken,
              RankingPeriod::AllTime,
              {},
              10,
              rankings,
              query_error), query_error.c_str());
    check(rankings.size() == 1 && rankings[0].value == 3, "durable all-time ranking");

    ServerMetrics server_metrics;
    check(query.server_metrics(1'735'689'599'999, 1'735'689'600'001, server_metrics, query_error), query_error.c_str());
    check(server_metrics.tick_samples == 1 && server_metrics.total_mspt == 40 && server_metrics.current_online == 2, "durable server metrics");

    std::int64_t active_players = 0;
    check(query.active_players(RankingPeriod::Day, "2025-01-01", active_players, query_error), query_error.c_str());
    check(active_players == 1, "daily active players");
    check(query.active_players(RankingPeriod::Month, "2025-01", active_players, query_error), query_error.c_str());
    check(active_players == 1, "monthly active players");

    AuditFilter audit_filter;
    audit_filter.start_ms = 1'735'689'599'999;
    audit_filter.end_ms = 1'735'689'600'001;
    audit_filter.event_kind = "audit_block_change";
    audit_filter.actor_kind = "unknown";
    audit_filter.world_id = "world";
    audit_filter.dimension = 0;
    audit_filter.min_x = 0;
    audit_filter.max_x = 0;
    audit_filter.min_y = 0;
    audit_filter.max_y = 0;
    audit_filter.min_z = 0;
    audit_filter.max_z = 0;
    audit_filter.before_type = "minecraft:chest";
    audit_filter.after_type = "minecraft:air";
    audit_filter.primary_type = "minecraft:air";
    audit_filter.secondary_type = "minecraft:chest";
    audit_filter.cause = "redstone";
    std::vector<AuditRecord> audits;
    check(query.lookup_audit(audit_filter, audits, query_error), query_error.c_str());
    check(audits.size() == 1 && audits[0].actor_id.empty() && audits[0].before_type == "minecraft:chest"
              && audits[0].after_type == "minecraft:air",
        "filtered audit lookup");
    auto inspected_event_id = audits[0].event_id;

    AuditFilter newest_filter;
    newest_filter.event_kind = "audit_block_change";
    newest_filter.actor_kind = "unknown";
    newest_filter.cause = "redstone";
    newest_filter.limit = 1;
    check(query.lookup_audit(newest_filter, audits, query_error), query_error.c_str());
    check(audits.size() == 1 && audits[0].occurred_at_ms == 1'735'689'600'000, "newest audit ordering");
    newest_filter.newest_first = false;
    check(query.lookup_audit(newest_filter, audits, query_error), query_error.c_str());
    check(audits.size() == 1 && audits[0].occurred_at_ms == 1'735'689'590'000, "oldest audit ordering");

    AuditRecord inspected;
    check(query.inspect_audit(inspected_event_id, inspected, query_error), query_error.c_str());
    check(inspected.details_json == R"({"source":"test"})" && inspected.cause == "redstone", "audit inspect");
    check(!query.inspect_audit(999'999, inspected, query_error) && query_error == "audit event not found", "missing audit inspect");
    query.close();
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}

void schema_upgrade_test() {
    using namespace bedrock_insight;
    auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("bedrock-insight-v1-test-" + std::to_string(stamp) + ".db");
    std::filesystem::remove(path);

    sqlite_api::Library library;
    sqlite_api::sqlite3* db = nullptr;
    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadWrite | sqlite_api::kCreate, nullptr) == sqlite_api::kOk, "create v1 database");
    check(library.exec(db, schema::kMigrationV1.data(), nullptr, nullptr, nullptr) == sqlite_api::kOk, "create v1 schema");
    check(library.exec(db, "PRAGMA user_version=1;", nullptr, nullptr, nullptr) == sqlite_api::kOk, "set v1 version");
    library.close(db);

    StorageWriter writer({path, 4});
    check(writer.start(), writer.last_error().c_str());
    AnalyticsEvent event{};
    event.occurred_at_ms = 1'735'689'600'000;
    event.kind = EventKind::BlockBreak;
    event.actor = Actor{ActorKind::Player, "xuid-upgrade", "Upgrade"};
    event.primary_type = "minecraft:stone";
    event.amount = 2;
    check(writer.submit(event), "upgrade event submit");
    check(writer.flush(), writer.last_error().c_str());
    writer.stop();

    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadOnly, nullptr) == sqlite_api::kOk, "reopen upgraded database");
    check(scalar(library, db, "SELECT user_version FROM pragma_user_version;") == schema::kVersion, "upgraded schema version");
    check(scalar(library, db, "SELECT COUNT(*) FROM player_daily_types;") == 1, "upgraded typed table");
    check(scalar(library, db, "SELECT COUNT(*) FROM api_tokens;") == 0, "upgraded API token table");
    check(scalar(library, db, "SELECT COUNT(*) FROM admin_action_log;") == 0, "upgraded admin log table");
    library.close(db);
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}

void admin_persistence_test() {
    using namespace bedrock_insight;
    auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("bedrock-insight-admin-test-" + std::to_string(stamp) + ".db");
    std::filesystem::remove(path);

    StorageWriter writer({path, 4});
    std::string error;
    check(writer.start(), writer.last_error().c_str());
    ApiTokenMetadata token{"admin-token", "hash-secret", "admin-1", ApiRole::Admin, 500, true};
    AdminActionLogEntry action;
    action.occurred_at_ms = 501;
    action.actor_id = "admin-1";
    action.role = ApiRole::Admin;
    action.action = "rollback.apply";
    action.method = "POST";
    action.path = "/api/v1/rollback/apply";
    action.remote_address = "127.0.0.1";
    action.allowed = true;
    action.status = 200;
    action.detail = "applied";
    bool token_sink_ok = true;
    ApiTokenStore token_store([](std::string_view secret) {
        return std::string("hash-") + std::string(secret);
    });
    token_store.set_change_sink([&](ApiTokenMetadata const& metadata) {
        token_sink_ok = writer.submit_api_token(metadata);
    });
    check(token_store.add_token(token.token_id, "secret", token.role, token.actor_id, token.created_at_ms, error),
        error.c_str());
    check(token_sink_ok, "API token persistence sink");

    bool action_sink_ok = true;
    AdminActionLog action_log(8);
    action_log.set_action_sink([&](AdminActionLogEntry const& entry) {
        action_sink_ok = action_sink_ok && writer.submit_admin_action(entry);
    });
    action_log.append(action);
    action.allowed = false;
    action.status = 403;
    action.detail = "role denied";
    action_log.append(action);
    check(action_sink_ok, "admin action persistence sink");
    check(writer.flush(), writer.last_error().c_str());
    auto metrics = writer.metrics();
    check(metrics.accepted_api_tokens == 1 && metrics.committed_api_tokens == 1
              && metrics.accepted_admin_actions == 2 && metrics.committed_admin_actions == 2,
        "API administration writer metrics");
    writer.stop();

    sqlite_api::Library library;
    sqlite_api::sqlite3* db = nullptr;
    check(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadOnly, nullptr) == sqlite_api::kOk,
        "open persisted API administration database");
    check(scalar(library, db, "SELECT user_version FROM pragma_user_version;") == schema::kVersion,
        "API administration schema version");
    check(scalar(library, db, "SELECT COUNT(*) FROM api_tokens;") == 1, "persisted API token count");
    check(text_scalar(library, db, "SELECT secret_hash FROM api_tokens WHERE token_id='admin-token';") == "hash-secret",
        "persisted token hash only");
    check(scalar(library, db, "SELECT COUNT(*) FROM admin_action_log;") == 2, "persisted admin action count");
    library.close(db);

    AnalyticsQuery query(path);
    check(query.open(error), error.c_str());
    std::vector<ApiTokenMetadata> tokens;
    check(query.load_api_tokens(tokens, error), error.c_str());
    check(tokens.size() == 1 && tokens[0].token_id == "admin-token" && tokens[0].secret_hash == "hash-secret"
              && tokens[0].role == ApiRole::Admin && tokens[0].enabled,
        "API token query readback");

    std::vector<AdminActionLogEntry> actions;
    check(query.lookup_admin_actions(1, actions, error), error.c_str());
    check(actions.size() == 1 && actions[0].status == 403 && !actions[0].allowed
              && actions[0].actor_id == "admin-1" && actions[0].action == "rollback.apply",
        "admin action query readback");
    query.close();

    ApiTokenStore restored([](std::string_view secret) {
        return std::string("hash-") + std::string(secret);
    });
    check(restored.add_hashed_token(token, error), error.c_str());
    auto principal = restored.authenticate("secret");
    check(principal.has_value() && principal->actor_id == "admin-1" && principal->role == ApiRole::Admin,
        "hashed token restore authentication");

    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}

void metrics_test() {
    using namespace bedrock_insight;
    MetricsAggregator aggregator;
    auto player_a = Actor{ActorKind::Player, "xuid-a", "Ada"};
    auto player_b = Actor{ActorKind::Player, "xuid-b", "Bea"};

    AnalyticsEvent event{};
    event.occurred_at_ms = 1'735'689'600'000;
    event.kind = EventKind::BlockBreak;
    event.actor = player_a;
    event.primary_type = "minecraft:stone";
    event.amount = 3;
    aggregator.observe(event);
    event.primary_type = "minecraft:dirt";
    event.amount = 1;
    aggregator.observe(event);

    event.actor = player_b;
    event.primary_type = "minecraft:stone";
    event.amount = 2;
    aggregator.observe(event);

    event.actor = player_a;
    event.occurred_at_ms = 1'735'776'000'000;
    event.kind = EventKind::SessionStart;
    event.primary_type.clear();
    event.amount = 1;
    aggregator.observe(event);
    event.kind = EventKind::SessionEnd;
    aggregator.observe(event);

    event.actor = player_b;
    event.occurred_at_ms = 1'738'368'000'000;
    event.kind = EventKind::ItemUse;
    event.primary_type = "minecraft:apple";
    event.amount = 5;
    aggregator.observe(event);

    aggregator.observe_server_tick({event.occurred_at_ms, 40, 2});
    aggregator.observe_server_tick({event.occurred_at_ms + 50, 60, 3});

    auto snapshot = aggregator.snapshot();
    check(snapshot.players.at("xuid-a").blocks_broken == 4, "player all-time blocks");
    check(snapshot.players.at("xuid-a").blocks_broken_by_type.at("minecraft:stone") == 3, "typed block counter");
    check(snapshot.players.at("xuid-a").blocks_broken_by_type.at("minecraft:dirt") == 1, "second typed block counter");
    check(snapshot.players.at("xuid-a").sessions == 1 && !snapshot.players.at("xuid-a").online, "session state");
    check(snapshot.server.tick_samples == 2 && snapshot.server.peak_online == 3, "server sample counters");
    check(snapshot.server.average_mspt() == 50.0, "average MSPT");
    check(snapshot.server.tps() == 20.0, "TPS cap and calculation");

    auto daily = aggregator.ranking(RankingMetric::BlocksBroken, RankingPeriod::Day, 1'735'689'600'000, 10);
    check(daily.size() == 2 && daily[0].player_id == "xuid-a" && daily[0].value == 4, "daily ranking");
    auto monthly = aggregator.ranking(RankingMetric::BlocksBroken, RankingPeriod::Month, 1'735'689'600'000, 1);
    check(monthly.size() == 1 && monthly[0].player_id == "xuid-a" && monthly[0].value == 4, "monthly ranking limit");
    auto next_month = aggregator.ranking(RankingMetric::ItemsUsed, RankingPeriod::Month, 1'738'368'000'000, 10);
    check(next_month.size() == 1 && next_month[0].player_id == "xuid-b" && next_month[0].value == 5, "monthly bucket separation");
    auto all_time = aggregator.ranking(RankingMetric::BlocksBroken, RankingPeriod::AllTime, 0, 10);
    check(all_time.size() == 2 && all_time[0].player_id == "xuid-a" && all_time[1].player_id == "xuid-b", "all-time ranking");
}

void rollback_test() {
    using namespace bedrock_insight;
    auto location = Position{1, 64, 1, 0};
    AuditRecord first{};
    first.event_id = 10;
    first.occurred_at_ms = 100;
    first.event_kind = "audit_block_change";
    first.world_id = "world";
    first.position = location;
    first.before_type = "minecraft:air";
    first.after_type = "minecraft:stone";
    auto second = first;
    second.event_id = 11;
    second.occurred_at_ms = 200;
    second.before_type = "minecraft:stone";
    second.after_type = "minecraft:dirt";

    std::string current_type = "minecraft:dirt";
    RollbackStateReader reader = [&](RollbackLocation const& target) -> std::optional<RollbackState> {
        if (target.world_id != "world" || target.position.x != 1) {
            return std::nullopt;
        }
        return RollbackState{current_type};
    };

    RollbackRequest request;
    request.operation = RollbackOperation::Undo;
    request.transaction_id = "rollback-test";
    request.actor_id = "admin-1";
    request.reason = "test undo";
    request.audit_records = {second, first};

    RollbackPlanner planner;
    auto preview = planner.preview(request, reader);
    check(preview.can_apply() && preview.changes.size() == 1, "rollback preview");
    check(preview.changes[0].world_id == "world" && preview.changes[0].expected_type == "minecraft:dirt"
              && preview.changes[0].target_type == "minecraft:air"
              && preview.changes[0].source_event_ids.size() == 2,
        "coalesced rollback change");

    RollbackJournal journal;
    std::string transaction_id;
    std::string error;
    check(journal.begin(preview, "admin-1", "test undo", 300, transaction_id, error), error.c_str());
    check(transaction_id == "rollback-test", "rollback transaction id");

    int writes = 0;
    RollbackStateWriter writer = [&](RollbackLocation const& target, std::string_view type, std::string&) {
        check(target.world_id == "world" && target.position.x == 1, "rollback world location");
        current_type = std::string(type);
        ++writes;
        return true;
    };
    check(!journal.apply(transaction_id, reader, writer, error) && writes == 0,
        "rollback apply must be disabled before any world write");
    RollbackTransaction transaction;
    check(journal.inspect(transaction_id, transaction, error), error.c_str());
    check(transaction.status == RollbackTransactionStatus::Prepared && transaction.entries.size() == 1
              && transaction.entries[0].status == RollbackEntryStatus::Prepared
              && transaction.entries[0].source_event_ids.size() == 2 && current_type == "minecraft:dirt",
        "disabled rollback preserves preview and world");
    check(!journal.apply(transaction_id, reader, writer, error) && writes == 0, "repeated apply stays disabled");

    auto multi_preview = preview;
    multi_preview.transaction_id = "rollback-partial-write-regression";
    multi_preview.changes.push_back(preview.changes.front());
    multi_preview.changes.back().position.x = 2;
    std::string multi_id;
    check(journal.begin(multi_preview, "admin-1", "partial write regression", 300, multi_id, error), error.c_str());
    auto dangerous_writer = [&](RollbackLocation const&, std::string_view, std::string&) {
        return ++writes < 2; // Previously left the first location changed.
    };
    check(!journal.apply(multi_id, reader, dangerous_writer, error) && writes == 0,
        "two-location failure scenario must invoke no world writer");

    current_type = "minecraft:dirt";
    auto stale_preview = planner.preview(request, reader);
    RollbackJournal stale_journal;
    std::string stale_transaction_id;
    check(stale_preview.can_apply()
              && stale_journal.begin(stale_preview, "admin-1", "stale preview", 301, stale_transaction_id, error),
        "stale preview journal");
    auto writes_before_conflict = writes;
    current_type = "minecraft:grass_block";
    check(!stale_journal.apply(stale_transaction_id, reader, writer, error) && writes == writes_before_conflict,
        "preflight prevents partial rollback");
    check(stale_journal.inspect(stale_transaction_id, transaction, error)
              && transaction.status == RollbackTransactionStatus::Prepared,
        "disabled apply does not mutate journal state");

    auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto journal_path = std::filesystem::temp_directory_path()
        / ("bedrock-insight-rollback-journal-" + std::to_string(stamp) + ".db");
    std::filesystem::remove(journal_path);
    StorageWriter journal_writer({journal_path, 4});
    check(journal_writer.start(), journal_writer.last_error().c_str());
    check(journal_writer.submit_rollback_transaction(transaction), "prepared journal submit");
    check(journal_writer.flush(), journal_writer.last_error().c_str());

    check(journal.inspect(transaction_id, transaction, error), error.c_str());
    check(journal_writer.submit_rollback_transaction(transaction), "applied journal submit");
    check(journal_writer.flush(), journal_writer.last_error().c_str());
    auto journal_metrics = journal_writer.metrics();
    check(journal_metrics.accepted_rollback_transactions == 2 && journal_metrics.committed_rollback_transactions == 2,
        "journal writer metrics");
    journal_writer.stop();

    sqlite_api::Library journal_library;
    sqlite_api::sqlite3* journal_db = nullptr;
    auto journal_open_result = journal_library.open_v2(journal_path.string().c_str(), &journal_db, sqlite_api::kReadOnly, nullptr);
    check(journal_open_result == sqlite_api::kOk, journal_db == nullptr ? "open journal database" : journal_library.errmsg(journal_db));
    check(scalar(journal_library, journal_db, "SELECT user_version FROM pragma_user_version;") == schema::kVersion, "journal schema version");
    check(scalar(journal_library, journal_db, "SELECT COUNT(*) FROM rollback_transactions WHERE status='prepared';") == 1, "durable prepared journal transaction");
    check(scalar(journal_library, journal_db, "SELECT COUNT(*) FROM rollback_entries WHERE status='prepared';") == 1, "durable prepared journal entry");
    check(text_scalar(journal_library, journal_db, "SELECT source_event_ids_json FROM rollback_entries;") == "[10,11]", "durable source event ids");
    journal_library.close(journal_db);
    AnalyticsQuery journal_query(journal_path);
    check(journal_query.open(error), error.c_str());
    RollbackSummary rollback_summary;
    check(journal_query.inspect_rollback(transaction_id, rollback_summary, error), error.c_str());
    check(rollback_summary.status == "prepared" && rollback_summary.entry_count == 1
              && rollback_summary.applied_count == 0 && rollback_summary.actor_id == "admin-1",
        "rollback journal query");
    journal_query.close();
    std::filesystem::remove(journal_path);
    std::filesystem::remove(journal_path.string() + "-wal");
    std::filesystem::remove(journal_path.string() + "-shm");

    current_type = "minecraft:grass_block";
    auto conflict_preview = planner.preview(request, reader);
    check(!conflict_preview.can_apply() && conflict_preview.conflict_count == 1, "rollback conflict detection");
    check(!journal.begin(conflict_preview, "admin-1", "conflict", 301, transaction_id, error), "conflict preview rejected");

    auto unsupported = first;
    unsupported.event_id = 12;
    unsupported.event_kind = "audit_container_change";
    request.audit_records.push_back(unsupported);
    auto unsupported_preview = planner.preview(request, reader);
    check(!unsupported_preview.can_apply() && unsupported_preview.unsupported_event_ids.size() == 1, "unsupported rollback rejected");
}

void admin_api_test() {
    using namespace bedrock_insight;

    auto test_hasher = [](std::string_view secret) {
        return std::string("test-hash:") + std::string(secret);
    };
    ApiTokenStore tokens(test_hasher);
    std::string error;
    check(tokens.add_token("viewer-token", "viewer-secret", ApiRole::Viewer, "viewer-1", 100, error), error.c_str());
    check(tokens.add_token("moderator-token", "moderator-secret", ApiRole::Moderator, "moderator-1", 101, error), error.c_str());
    check(tokens.add_token("admin-token", "admin-secret", ApiRole::Admin, "admin-1", 102, error), error.c_str());
    check(tokens.size() == 3, "API token count");
    check(!tokens.add_token("admin-token", "other-secret", ApiRole::Admin, "admin-2", 103, error)
              && error == "token id already exists",
        "duplicate API token rejected");

    AdminActionLog action_log(32);
    AdminApi api(ApiPolicy{}, tokens, action_log);
    ApiDataProvider provider;
    provider.health_json = [] { return std::string(R"({"ok":true})"); };
    provider.metrics_json = [] { return std::string(R"({"tps":20})"); };
    provider.rankings_json = [] { return std::string(R"({"period_key":"2026-11-05","entries":[]})"); };
    provider.player_json = [](std::string_view target) {
        return std::string(R"({"target":)") + json_quote(target) + "}";
    };
    provider.rankings_json = [] { return std::string(R"({"entries":[{"rank":1,"value":20}]})"); };
    provider.player_json = [](std::string_view target) {
        return std::string(R"({"target":)") + json_quote(target) + "}";
    };
    provider.audit_json = [](std::string_view body) {
        return std::string(R"({"audit_filter":)") + (body.empty() ? std::string("null") : std::string(body)) + "}";
    };
    provider.rollback_preview_json = [](std::string_view body) {
        return std::string(R"({"preview":true,"request":)") + std::string(body) + "}";
    };
    provider.rollback_apply_json = [](std::string_view body) {
        return std::string(R"({"applied":true,"request":)") + std::string(body) + "}";
    };
    provider.token_admin_json = [](std::string_view body) {
        return std::string(R"({"token_management":true,"request":)") + std::string(body) + "}";
    };
    provider.admin_actions_json = [] { return std::string(R"({"entries":[]})"); };

    ApiRequest request;
    request.method = "GET";
    request.path = "/api/v1/health";
    request.remote_address = "127.0.0.1";
    request.received_at_ms = 200;
    auto response = api.handle(request, provider);
    check(response.status == 401 && response.body == R"({"error":"authentication required"})", "missing API auth");

    request.bearer_token = "viewer-secret";
    request.remote_address = "192.0.2.10";
    response = api.handle(request, provider);
    check(response.status == 403 && response.body == R"({"error":"localhost access required"})", "remote API rejection");

    request.remote_address = "127.0.0.1";
    request.bearer_token = "wrong-secret";
    response = api.handle(request, provider);
    check(response.status == 401, "invalid API token");

    request.bearer_token = "viewer-secret";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"ok":true})", "viewer health endpoint");

    request.path = "/api/v1/metrics";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"tps":20})", "viewer metrics endpoint");

    request.path = "/api/v1/audit";
    request.body = R"({"actor_id":"xuid-a"})";
    response = api.handle(request, provider);
    check(response.status == 403 && response.body == R"({"error":"insufficient role"})", "viewer audit denial");

    request.bearer_token = "moderator-secret";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"audit_filter":{"actor_id":"xuid-a"}})", "moderator audit endpoint");

    request.method = "POST";
    request.path = "/api/v1/rollback/preview";
    request.body = R"({"transaction_id":"preview-1"})";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"preview":true,"request":{"transaction_id":"preview-1"}})",
        "moderator rollback preview endpoint");

    request.path = "/api/v1/rollback/apply";
    response = api.handle(request, provider);
    check(response.status == 403, "moderator rollback apply denial");

    request.bearer_token = "admin-secret";
    response = api.handle(request, provider);
    check(response.status == 501 && response.body.find("rollback apply is disabled") != std::string::npos,
        "admin rollback apply provider cannot bypass disabled mutation");

    request.path = "/api/v1/tokens";
    request.body = R"({"token_id":"new-token"})";
    response = api.handle(request, provider);
    check(response.status == 200
              && response.body == R"({"token_management":true,"request":{"token_id":"new-token"}})",
        "admin token-management endpoint");

    check(tokens.set_enabled("admin-token", false, error), error.c_str());
    response = api.handle(request, provider);
    check(response.status == 401, "disabled API token");
    check(tokens.set_enabled("admin-token", true, error), error.c_str());

    request.method = "GET";
    request.path = "/api/v1/rankings";
    request.bearer_token = "viewer-secret";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"entries":[{"rank":1,"value":20}]})",
        "viewer rankings endpoint");

    request.path = "/api/v1/player?id=xuid-a";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"target":"/api/v1/player?id=xuid-a"})",
        "viewer player endpoint");

    request.path = "/api/v1/admin/actions";
    response = api.handle(request, provider);
    check(response.status == 403, "viewer admin-log denial");
    request.bearer_token = "admin-secret";
    response = api.handle(request, provider);
    check(response.status == 200 && response.body == R"({"entries":[]})", "admin action-log endpoint");

    request.path = "/api/v1/missing";
    response = api.handle(request, provider);
    check(response.status == 404 && response.body == R"({"error":"route not found"})", "unknown API route");

    auto entries = action_log.snapshot();
    check(entries.size() == 17, "API action log count");
    check(entries.front().status == 401 && !entries.front().allowed, "denied API action log");
    check(entries[6].actor_id == "moderator-1" && entries[6].allowed && entries[6].action == "audit.read",
        "allowed API action log");
    for (auto const& entry : entries) {
        check(entry.detail.find("secret") == std::string::npos, "API action log must not contain token secret");
    }

    AdminActionLog bounded_log(2);
    bounded_log.append(AdminActionLogEntry{0, 1, "a", ApiRole::Viewer, "one", "GET", "/one", "127.0.0.1", true, 200, "ok"});
    bounded_log.append(AdminActionLogEntry{0, 2, "b", ApiRole::Viewer, "two", "GET", "/two", "127.0.0.1", true, 200, "ok"});
    auto bounded_id = bounded_log.append(AdminActionLogEntry{0, 3, "c", ApiRole::Admin, "three", "POST", "/three", "127.0.0.1", true, 200, "ok"});
    auto bounded_entries = bounded_log.snapshot();
    check(bounded_entries.size() == 2 && bounded_entries.front().actor_id == "b"
              && bounded_entries.back().id == bounded_id,
        "bounded API action log");
}

void http_transport_test() {
    using namespace bedrock_insight;

    ApiTokenStore tokens([](std::string_view secret) {
        return std::string("test-hash:") + std::string(secret);
    });
    std::string error;
    check(tokens.add_token("viewer-token", "viewer-secret", ApiRole::Viewer, "viewer-1", 100, error), error.c_str());
    check(tokens.add_token("moderator-token", "moderator-secret", ApiRole::Moderator, "moderator-1", 101, error), error.c_str());
    AdminActionLog action_log;
    AdminApi api(ApiPolicy{}, tokens, action_log);
    ApiDataProvider provider;
    provider.metrics_json = [] { return std::string(R"({"tps":20})"); };
    provider.rollback_preview_json = [](std::string_view body) {
        return std::string(R"({"preview":true,"request":)") + std::string(body) + "}";
    };

    auto metrics_wire = std::string("GET /api/v1/metrics HTTP/1.1\r\n")
        + "hOsT: localhost\r\nAuthorization: bEaReR viewer-secret\r\n\r\n";
    HttpRequest parsed;
    std::size_t consumed = 0;
    auto parse_result = parse_http_request(metrics_wire, parsed, consumed, error);
    check(parse_result == HttpParseResult::Complete && consumed == metrics_wire.size()
              && parsed.method == "GET" && parsed.target == "/api/v1/metrics"
              && http_header_value(parsed, "HOST") == "localhost",
        "HTTP request parse");

    auto response_wire = handle_http_request(metrics_wire, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Complete
              && response_wire == "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 10\r\n"
                                   "Connection: close\r\n\r\n{\"tps\":20}",
        "HTTP metrics dispatch and serialization");

    auto preview_body = std::string(R"({"transaction_id":"x"})");
    auto preview_wire = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer moderator-secret\r\nContent-Type: application/json\r\nContent-Length: "
        + std::to_string(preview_body.size()) + "\r\n\r\n" + preview_body;
    response_wire = handle_http_request(preview_wire, "::1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Complete && response_wire.find("HTTP/1.1 200 OK\r\n") == 0
              && response_wire.find("{\"preview\":true,\"request\":{\"transaction_id\":\"x\"}}") != std::string::npos,
        "HTTP body dispatch");

    auto incomplete = std::string("GET /api/v1/metrics HTTP/1.1\r\nHost: localhost\r\n");
    response_wire = handle_http_request(incomplete, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Incomplete && response_wire.empty(), "incomplete HTTP headers");

    auto incomplete_body = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Content-Length: 5\r\n\r\nabc";
    parse_result = parse_http_request(incomplete_body, parsed, consumed, error);
    check(parse_result == HttpParseResult::Incomplete && consumed == 0, "incomplete HTTP body");

    auto missing_host = std::string("GET /api/v1/metrics HTTP/1.1\r\nAuthorization: Bearer viewer-secret\r\n\r\n");
    response_wire = handle_http_request(missing_host, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::BadRequest
              && response_wire.find("HTTP/1.1 400 Bad Request\r\n") == 0
              && response_wire.find("{\"error\":\"bad request\"}") != std::string::npos,
        "HTTP malformed request response");

    auto oversized = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Content-Length: 5\r\n\r\nabcde";
    HttpLimits small_limits;
    small_limits.max_body_bytes = 4;
    response_wire = handle_http_request(oversized, "127.0.0.1", api, provider, parse_result, error, small_limits);
    check(parse_result == HttpParseResult::TooLarge
              && response_wire.find("HTTP/1.1 413 Payload Too Large\r\n") == 0,
        "HTTP body limit");

    auto chunked = std::string("GET /api/v1/metrics HTTP/1.1\r\nHost: localhost\r\n")
        + "Transfer-Encoding: chunked\r\n\r\n";
    response_wire = handle_http_request(chunked, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::BadRequest
              && response_wire.find("HTTP/1.1 400 Bad Request\r\n") == 0,
        "HTTP transfer encoding rejection");

    auto duplicate_length = std::string("GET /api/v1/metrics HTTP/1.1\r\nHost: localhost\r\n")
        + "Content-Length: 0\r\nContent-Length: 0\r\n\r\n";
    parse_result = parse_http_request(duplicate_length, parsed, consumed, error);
    check(parse_result == HttpParseResult::BadRequest, "duplicate HTTP header rejection");

    auto invalid_json = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer moderator-secret\r\nContent-Type: application/json\r\nContent-Length: 4\r\n\r\n{bad";
    response_wire = handle_http_request(invalid_json, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Complete
              && response_wire.find("HTTP/1.1 400 Bad Request\r\n") == 0
              && response_wire.find("{\"error\":\"valid JSON object required\"}") != std::string::npos,
        "invalid JSON body rejection");

    auto array_json = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer moderator-secret\r\nContent-Type: application/json\r\nContent-Length: 3\r\n\r\n[1]";
    response_wire = handle_http_request(array_json, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Complete
              && response_wire.find("{\"error\":\"valid JSON object required\"}") != std::string::npos,
        "JSON object root enforcement");

    auto no_content_type = std::string("POST /api/v1/rollback/preview HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer moderator-secret\r\nContent-Length: " + std::to_string(preview_body.size())
        + "\r\n\r\n" + preview_body;
    response_wire = handle_http_request(no_content_type, "127.0.0.1", api, provider, parse_result, error);
    check(parse_result == HttpParseResult::Complete
              && response_wire.find("{\"error\":\"JSON content type required\"}") != std::string::npos,
        "JSON content type enforcement");
}

void http_server_test() {
    using namespace bedrock_insight;

    ApiTokenStore tokens([](std::string_view secret) {
        return std::string("listener-hash:") + std::string(secret);
    });
    std::string error;
    check(tokens.add_token("listener-token", "listener-secret", ApiRole::Viewer, "listener-1", 200, error),
        error.c_str());
    AdminActionLog action_log;
    AdminApi api(ApiPolicy{}, tokens, action_log);
    ApiDataProvider provider;
    provider.health_json = []() -> std::string { throw std::runtime_error("injected provider failure"); };
    provider.metrics_json = [] { return std::string(R"({"tps":20})"); };

    HttpServerConfig rejected_config;
    rejected_config.bind_address = "0.0.0.0";
    HttpServer rejected(api, provider, rejected_config);
    check(!rejected.start(error) && error == "HTTP server bind address must be IPv4 loopback",
        "non-loopback listener rejection");

    HttpServerConfig config;
    config.port = 0;
    config.receive_timeout_ms = 500;
    HttpServer server(api, provider, config);
    check(server.start(error), error.c_str());
    check(server.running() && server.port() != 0, "loopback listener start");

    auto request = std::string("GET /api/v1/metrics HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer listener-secret\r\n\r\n";
    auto response = test_http_roundtrip(server.port(), request);
    check(response.find("HTTP/1.1 200 OK\r\n") == 0
              && response.find("Content-Length: 10\r\n") != std::string::npos
              && response.find("{\"tps\":20}") != std::string::npos,
        "loopback listener request/response");

    auto dashboard_request = std::string("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    response = test_http_roundtrip(server.port(), dashboard_request);
    check(response.find("HTTP/1.1 200 OK\r\n") == 0
              && response.find("Content-Type: text/html; charset=utf-8\r\n") != std::string::npos
              && response.find("BedrockInsight") != std::string::npos,
        "embedded dashboard HTML");

    auto script_request = std::string("GET /app.js HTTP/1.1\r\nHost: localhost\r\n\r\n");
    response = test_http_roundtrip(server.port(), script_request);
    check(response.find("Content-Type: text/javascript; charset=utf-8\r\n") != std::string::npos
              && response.find("/api/v1/rankings") != std::string::npos
              && response.find("/api/v1/admin/actions") != std::string::npos,
        "embedded dashboard script");

    auto unauthorized_request = std::string("GET /api/v1/health HTTP/1.1\r\nHost: localhost\r\n\r\n");
    response = test_http_roundtrip(server.port(), unauthorized_request);
    check(response.find("HTTP/1.1 401 Unauthorized\r\n") == 0
              && response.find("authentication required") != std::string::npos,
        "loopback listener authentication");

    auto throwing_request = std::string("GET /api/v1/health HTTP/1.1\r\nHost: localhost\r\n")
        + "Authorization: Bearer listener-secret\r\n\r\n";
    check(test_http_roundtrip(server.port(), throwing_request).empty(), "provider failure closes connection");
    check(test_http_roundtrip(server.port(), request).find("HTTP/1.1 200 OK") == 0,
        "listener survives provider exception");

    server.stop();
    check(!server.running() && server.port() == 0, "loopback listener stop");
}

void json_parser_test() {
    using namespace bedrock_insight;
    JsonValue document;
    std::string error;
    auto result = parse_json(
        R"({"operation":"undo","count":42,"nested":{"ok":true},"items":[null,false],"unicode":"\u03a9"})",
        document, error);
    check(result == JsonParseResult::Complete && document.type() == JsonValue::Type::Object, "JSON object parse");

    std::string operation;
    check(json_string_field(document, "operation", operation, true, error) && operation == "undo", "JSON string field");
    std::int64_t count = 0;
    check(json_integer_field(document, "count", count, true, error) && count == 42, "JSON integer field");
    std::string optional;
    check(json_string_field(document, "missing", optional, false, error) && optional.empty(), "optional JSON field");
    check(document.find("nested") != nullptr && document.find("nested")->type() == JsonValue::Type::Object,
        "nested JSON object");
    check(document.find("items") != nullptr && document.find("items")->array().size() == 2, "JSON array parse");
    check(document.find("unicode") != nullptr && document.find("unicode")->string() == "Ω", "JSON Unicode escape");

    result = parse_json(R"({"key":1,"key":2})", document, error);
    check(result == JsonParseResult::Invalid, "duplicate JSON key rejection");
    result = parse_json(R"({"count":1.5})", document, error);
    check(result == JsonParseResult::Complete && !json_integer_field(document, "count", count, true, error),
        "non-integral JSON number rejection");
    result = parse_json(R"({"count":01})", document, error);
    check(result == JsonParseResult::Invalid, "leading-zero JSON number rejection");
    result = parse_json(R"({"bad":"\q"})", document, error);
    check(result == JsonParseResult::Invalid, "invalid JSON escape rejection");
    result = parse_json("[1]", document, error);
    check(result == JsonParseResult::Complete && !json_require_object(document, error), "JSON object validator");

    JsonLimits small_limits;
    small_limits.max_values = 2;
    result = parse_json(R"([1,2,3])", document, error, small_limits);
    check(result == JsonParseResult::TooLarge, "JSON value limit");
    small_limits = {};
    small_limits.max_depth = 2;
    result = parse_json(R"({"a":{"b":{"c":1}}})", document, error, small_limits);
    check(result == JsonParseResult::TooLarge, "JSON depth limit");
}

void api_payload_test() {
    using namespace bedrock_insight;
    std::string error;
    RollbackApiPayload rollback_payload;
    auto result = parse_rollback_api_payload(
        R"({"operation":"restore","transaction_id":"tx-1","reason":"repair","event_ids":[10,11]})",
        rollback_payload, error);
    check(result == JsonParseResult::Complete && rollback_payload.operation == RollbackOperation::Restore
              && rollback_payload.transaction_id == "tx-1" && rollback_payload.reason == "repair"
              && rollback_payload.event_ids.size() == 2 && rollback_payload.event_ids[1] == 11,
        "typed rollback API payload");

    result = parse_rollback_api_payload(
        R"({"operation":"delete","transaction_id":"tx-1","reason":"repair","event_ids":[10]})",
        rollback_payload, error);
    check(result == JsonParseResult::Invalid, "rollback operation validation");
    result = parse_rollback_api_payload(
        R"({"operation":"undo","transaction_id":"tx-1","reason":"repair","event_ids":[-1]})",
        rollback_payload, error);
    check(result == JsonParseResult::Invalid, "rollback event id validation");

    AuditFilter filter;
    result = parse_audit_filter_api_payload(
        R"({"event_id":12,"start_ms":100,"end_ms":200,"event_kind":"audit_block_change","actor_kind":"unknown","actor_id":"x","world_id":"world","dimension":1,"min_x":-2,"max_x":3,"before_type":"minecraft:air","after_type":"minecraft:stone","cause":"redstone","limit":25,"newest_first":false})",
        filter, error);
    check(result == JsonParseResult::Complete && filter.event_id == 12 && filter.start_ms == 100
              && filter.end_ms == 200 && filter.event_kind == "audit_block_change"
              && filter.actor_kind == "unknown" && filter.actor_id == "x" && filter.world_id == "world"
              && filter.dimension == 1 && filter.min_x == -2 && filter.max_x == 3
              && filter.before_type == "minecraft:air" && filter.after_type == "minecraft:stone"
              && filter.cause == "redstone" && filter.limit == 25 && !filter.newest_first,
        "typed audit filter payload");
    result = parse_audit_filter_api_payload(R"({"dimension":999})", filter, error);
    check(result == JsonParseResult::Invalid, "audit dimension validation");
    result = parse_audit_filter_api_payload(R"({"limit":0})", filter, error);
    check(result == JsonParseResult::Invalid, "audit limit validation");
}

void api_json_test() {
    using namespace bedrock_insight;
    JsonValue document;
    std::string error;
    ServerMetrics metrics;
    metrics.tick_samples = 3;
    metrics.total_mspt = 120;
    metrics.max_mspt = 60;
    metrics.current_online = 2;
    metrics.peak_online = 4;
    metrics.last_sample_ms = 500;
    auto serialized = serialize_server_metrics_json(metrics);
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("tick_samples") != nullptr && document.find("tps") != nullptr,
        "server metrics JSON serialization");

    std::vector<RankingEntry> rankings = {{"xuid-a", "Ada \"A\"", 7}, {"xuid-b", "Bea", 3}};
    serialized = serialize_rankings_json(RankingMetric::BlocksBroken, RankingPeriod::Day, "2026-11-05", rankings);
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("metric")->string() == "blocks_broken"
              && document.find("entries")->array().size() == 2
              && document.find("entries")->array()[0].find("rank")->number() == "1"
              && document.find("entries")->array()[0].find("display_name")->string() == "Ada \"A\"",
        "ranking JSON serialization");

    PlayerStats stats;
    stats.player_id = "xuid-a";
    stats.display_name = "Ada";
    stats.online = true;
    stats.blocks_broken = 7;
    stats.blocks_broken_by_type["minecraft:stone"] = 7;
    serialized = serialize_player_stats_json(stats);
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("online")->boolean()
              && document.find("blocks_broken_by_type")->find("minecraft:stone")->number() == "7",
        "player profile JSON serialization");

    AuditRecord audit;
    audit.event_id = 8;
    audit.occurred_at_ms = 900;
    audit.event_kind = "audit_block_change";
    audit.actor_kind = "unknown";
    audit.actor_id = "system";
    audit.world_id = "world\"one";
    audit.position = Position{1, 64, -2, 0};
    audit.details_json = R"({"source":"test"})";
    serialized = serialize_audit_records_json({audit});
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("records")->array().size() == 1
              && document.find("records")->array()[0].find("world_id")->string() == "world\"one"
              && document.find("records")->array()[0].find("position")->find("z")->number() == "-2"
              && document.find("records")->array()[0].find("details_json")->string() == R"({"source":"test"})",
        "audit JSON serialization");

    RollbackSummary summary;
    summary.transaction_id = "tx-1";
    summary.operation = "undo";
    summary.actor_id = "admin-1";
    summary.status = "applied";
    summary.entry_count = 2;
    summary.applied_count = 2;
    serialized = serialize_rollback_summary_json(summary);
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("status")->string() == "applied"
              && document.find("applied_count")->number() == "2",
        "rollback JSON serialization");

    AdminActionLogEntry log_entry;
    log_entry.id = 3;
    log_entry.actor_id = "admin-1";
    log_entry.role = ApiRole::Admin;
    log_entry.action = "rollback.apply";
    log_entry.method = "POST";
    log_entry.path = "/api/v1/rollback/apply";
    log_entry.remote_address = "127.0.0.1";
    log_entry.allowed = true;
    log_entry.status = 200;
    serialized = serialize_admin_action_log_json({log_entry});
    check(parse_json(serialized, document, error) == JsonParseResult::Complete
              && document.find("entries")->array()[0].find("role")->string() == "admin"
              && document.find("entries")->array()[0].find("allowed")->boolean(),
        "admin log JSON serialization");
}

} // namespace

int main() {
    try {
        queue_test();
        storage_test();
        load_safety_regression_test();
        schema_upgrade_test();
        admin_persistence_test();
        metrics_test();
        rollback_test();
        admin_api_test();
        http_transport_test();
        http_server_test();
        json_parser_test();
        api_payload_test();
        api_json_test();
        std::cout << "BedrockInsight core tests: PASS\n";
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "BedrockInsight core tests: FAIL: " << error.what() << '\n';
        return 1;
    }
}
