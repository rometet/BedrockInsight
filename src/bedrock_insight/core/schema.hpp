#pragma once

#include <string_view>

namespace bedrock_insight::schema {

inline constexpr int kVersion = 4;

inline constexpr std::string_view kMigrationV1 = R"sql(
CREATE TABLE IF NOT EXISTS schema_meta (
    version INTEGER PRIMARY KEY,
    applied_at_ms INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS players (
    player_id TEXT PRIMARY KEY,
    last_name TEXT NOT NULL,
    first_seen_ms INTEGER NOT NULL,
    last_seen_ms INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    session_id TEXT PRIMARY KEY,
    player_id TEXT NOT NULL,
    started_at_ms INTEGER NOT NULL,
    ended_at_ms INTEGER,
    duration_ms INTEGER,
    FOREIGN KEY (player_id) REFERENCES players(player_id)
);

CREATE TABLE IF NOT EXISTS events (
    event_id INTEGER PRIMARY KEY,
    occurred_at_ms INTEGER NOT NULL,
    event_kind TEXT NOT NULL,
    actor_kind TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    actor_name TEXT NOT NULL,
    world_id TEXT NOT NULL,
    dimension INTEGER NOT NULL,
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    z INTEGER NOT NULL,
    primary_type TEXT NOT NULL,
    secondary_type TEXT NOT NULL,
    session_id TEXT NOT NULL,
    cause TEXT NOT NULL,
    amount INTEGER NOT NULL,
    details_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_events (
    event_id INTEGER PRIMARY KEY,
    action TEXT NOT NULL,
    cause TEXT NOT NULL,
    before_type TEXT NOT NULL,
    after_type TEXT NOT NULL,
    FOREIGN KEY (event_id) REFERENCES events(event_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS player_daily (
    player_id TEXT NOT NULL,
    day_utc TEXT NOT NULL,
    blocks_broken INTEGER NOT NULL DEFAULT 0,
    blocks_placed INTEGER NOT NULL DEFAULT 0,
    items_crafted INTEGER NOT NULL DEFAULT 0,
    items_used INTEGER NOT NULL DEFAULT 0,
    kills INTEGER NOT NULL DEFAULT 0,
    deaths INTEGER NOT NULL DEFAULT 0,
    distance_m REAL NOT NULL DEFAULT 0,
    sessions INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, day_utc),
    FOREIGN KEY (player_id) REFERENCES players(player_id)
);

CREATE INDEX IF NOT EXISTS events_time_idx ON events(occurred_at_ms);
CREATE INDEX IF NOT EXISTS events_actor_idx ON events(actor_id, occurred_at_ms);
CREATE INDEX IF NOT EXISTS events_position_idx ON events(world_id, dimension, x, y, z);
CREATE INDEX IF NOT EXISTS audit_cause_idx ON events(cause, occurred_at_ms);
)sql";

inline constexpr std::string_view kMigrationV2 = R"sql(
ALTER TABLE events ADD COLUMN tick_mspt INTEGER NOT NULL DEFAULT 0;
ALTER TABLE events ADD COLUMN online_players INTEGER NOT NULL DEFAULT -1;

CREATE TABLE IF NOT EXISTS player_daily_types (
    player_id TEXT NOT NULL,
    day_utc TEXT NOT NULL,
    metric TEXT NOT NULL,
    value_type TEXT NOT NULL,
    amount INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, day_utc, metric, value_type),
    FOREIGN KEY (player_id) REFERENCES players(player_id)
);

CREATE TABLE IF NOT EXISTS server_tick_samples (
    event_id INTEGER PRIMARY KEY,
    occurred_at_ms INTEGER NOT NULL,
    mspt INTEGER NOT NULL,
    online_players INTEGER NOT NULL,
    FOREIGN KEY (event_id) REFERENCES events(event_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS player_daily_types_metric_idx
    ON player_daily_types(metric, day_utc, amount);
CREATE INDEX IF NOT EXISTS server_tick_samples_time_idx
    ON server_tick_samples(occurred_at_ms);
)sql";

inline constexpr std::string_view kMigrationV3 = R"sql(
CREATE TABLE IF NOT EXISTS rollback_transactions (
    transaction_id TEXT PRIMARY KEY,
    operation TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    reason TEXT NOT NULL,
    created_at_ms INTEGER NOT NULL,
    status TEXT NOT NULL,
    entry_count INTEGER NOT NULL DEFAULT 0,
    applied_count INTEGER NOT NULL DEFAULT 0,
    error TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS rollback_entries (
    transaction_id TEXT NOT NULL,
    entry_index INTEGER NOT NULL,
    event_id INTEGER NOT NULL,
    source_event_ids_json TEXT NOT NULL,
    world_id TEXT NOT NULL,
    dimension INTEGER NOT NULL,
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    z INTEGER NOT NULL,
    expected_type TEXT NOT NULL,
    target_type TEXT NOT NULL,
    observed_type TEXT NOT NULL,
    status TEXT NOT NULL,
    error TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (transaction_id, entry_index),
    FOREIGN KEY (transaction_id) REFERENCES rollback_transactions(transaction_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS rollback_transactions_created_idx
    ON rollback_transactions(created_at_ms, status);
CREATE INDEX IF NOT EXISTS rollback_entries_event_idx
    ON rollback_entries(event_id);
)sql";

inline constexpr std::string_view kMigrationV4 = R"sql(
CREATE TABLE IF NOT EXISTS api_tokens (
    token_id TEXT PRIMARY KEY,
    secret_hash TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    role TEXT NOT NULL,
    created_at_ms INTEGER NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS admin_action_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    occurred_at_ms INTEGER NOT NULL,
    actor_id TEXT NOT NULL,
    role TEXT NOT NULL,
    action TEXT NOT NULL,
    method TEXT NOT NULL,
    path TEXT NOT NULL,
    remote_address TEXT NOT NULL,
    allowed INTEGER NOT NULL,
    status INTEGER NOT NULL,
    detail TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS admin_action_log_time_idx
    ON admin_action_log(occurred_at_ms, id);
CREATE INDEX IF NOT EXISTS admin_action_log_actor_idx
    ON admin_action_log(actor_id, occurred_at_ms);
)sql";

} // namespace bedrock_insight::schema
