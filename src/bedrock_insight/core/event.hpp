#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bedrock_insight {

enum class EventKind : std::uint8_t {
    SessionStart,
    SessionEnd,
    BlockBreak,
    BlockPlace,
    ItemCraft,
    ItemUse,
    EntityKill,
    PlayerDeath,
    Distance,
    ServerTick,
    AuditBlockChange,
    AuditContainerChange,
    AuditFluidChange,
    AuditPiston,
    AuditExplosion,
    AuditEntityChange,
    // Deliberately separate from PlayerDeath/ItemUse success-oriented counters.
    // Append only: preserve existing enum ordinals and persisted text meanings.
    PlayerDeathNotification,
    ItemUseAttempt,
    ItemDropResult,
    PlayerModeObservation,
    ItemPickupDelta,
    PlayerJumpImpulse,
    DirectKillObservation,
    ItemUseResult,
    ItemHurtSlotLoss,
    // Proven post-outcome block transitions (state change at the same position).
    // Appended after all prior ordinals; attempt events remain separate.
    BlockMinedResult,
    BlockPlacedResult,
    // Completed-craft outcome from a successful craft request (item + count).
    ItemCraftResult,
    // Per-tick movement distance, keyed by vanilla movement mode.
    PlayerTravelDistance,
    // Successful right-click interaction with a mapped block type.
    PlayerInteractionResult,
    // Damage events, amounts in vanilla tenths; source cause in secondary_type.
    PlayerDamageTaken,
    PlayerDamageDealt,
    // Genuine state-transition/duration counters.
    PlayerSleepResult,
    PlayerSneakTime,
    // Durability-driven item destruction (isDamageable + engine removal).
    ItemBrokenResult,
    // Resolved source actor type for a Player dead-flag transition; NOT cause:N.
    PlayerKilledByEntity,
    // Opt-in raw shield investigation. Never a successful shield statistic.
    PlayerShieldEvidence,
};

enum class ActorKind : std::uint8_t {
    Player,
    Console,
    System,
    Unknown,
};

struct Position final {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int8_t dimension = 0;
};

struct Actor final {
    ActorKind kind = ActorKind::Unknown;
    std::string id;
    std::string display_name;
};

// This is the immutable value crossing from a game-thread adapter to the
// persistence queue. Strings are copied at the adapter boundary so the
// worker never observes a mutable Bedrock object.
struct AnalyticsEvent final {
    std::uint64_t sequence = 0;
    std::int64_t occurred_at_ms = 0;
    EventKind kind = EventKind::ServerTick;
    Actor actor;
    std::string world_id;
    Position position;
    std::string primary_type;
    std::string secondary_type;
    std::string session_id;
    std::string cause;
    std::int64_t amount = 1;
    std::string details_json;
    // These fields are populated only for EventKind::ServerTick. Keeping them
    // typed avoids making adapters encode metrics into JSON or coordinates.
    std::int64_t tick_mspt = 0;
    std::int32_t online_players = -1;

    [[nodiscard]] bool has_player_actor() const noexcept {
        return actor.kind == ActorKind::Player && !actor.id.empty();
    }
};

[[nodiscard]] std::string_view to_string(EventKind kind) noexcept;
[[nodiscard]] std::string_view to_string(ActorKind kind) noexcept;

} // namespace bedrock_insight
