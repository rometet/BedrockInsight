#pragma once

#include "bedrock_insight/core/storage.hpp"
#include "bedrock_insight/core/tick_interval.hpp"
#include "bedrock_insight/levilamina/busy_tick.hpp"
#include "bedrock_insight/levilamina/player_actions.hpp"
#include "bedrock_insight/levilamina/player_block.hpp"
#include "bedrock_insight/levilamina/player_death.hpp"
#include "bedrock_insight/levilamina/player_modes.hpp"
#include "bedrock_insight/levilamina/player_pickup.hpp"
#include "bedrock_insight/levilamina/player_jump.hpp"
#include "bedrock_insight/levilamina/player_kills.hpp"
#include "bedrock_insight/levilamina/player_hurt.hpp"
#ifndef BEDROCKINSIGHT_FAKE_SDK
#include "bedrock_insight/levilamina/player_travel.hpp"
#include "bedrock_insight/levilamina/player_item_use.hpp"
#include "bedrock_insight/levilamina/player_item_transfer.hpp"
#include "bedrock_insight/levilamina/player_trade.hpp"
#include "bedrock_insight/levilamina/player_breed.hpp"
#include "bedrock_insight/levilamina/player_target.hpp"
#include "bedrock_insight/levilamina/player_block_interaction.hpp"

#include "ll/api/event/entity/ActorHurtEvent.h"
#endif
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/event/player/PlayerSneakEvent.h"
#include "ll/api/event/player/PlayerSprintEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef BEDROCKINSIGHT_FAKE_SDK
#include "mc/deps/ecs/WeakEntityRef.h"
#endif

namespace ll::event {
class ListenerBase;
using ListenerPtr = std::shared_ptr<ListenerBase>;
}

namespace bedrock_insight::levilamina {

#ifndef BEDROCKINSIGHT_FAKE_SDK
// Minimal identity/state record for tracked players. WeakEntityRef is resolved
// on the server tick; no raw Actor pointer survives across ticks by itself.
struct TrackedPlayer final {
    WeakEntityRef weak;
    ::Actor const* identity = nullptr;
    std::string actor_id;
    std::string display_name;
    std::int64_t unique_id = 0;
    bool sleeping = false;
};

struct SneakState final {
    std::int64_t start_us = 0;
    std::string actor_id;
    std::string display_name;
};
#endif

// The runtime owns the only game-facing adapter state. Event callbacks copy
// primitive/string values into AnalyticsEvent and immediately hand them to
// the asynchronous SQLite writer. Callbacks share owned state instead of a
// raw Runtime pointer, and serialize admission against disable/unload.
class Runtime final {
public:
    bool load() noexcept;
    bool enable() noexcept;
    bool disable() noexcept;
    bool unload() noexcept;

private:
    bool install_listeners() noexcept;
    bool remove_listeners() noexcept;
    bool stop_storage() noexcept;

    struct Capture final {
        std::mutex mutex;
        bool enabled = false;
        std::uint64_t activation_generation = 0;
        StorageConfig storage_config;
        std::unique_ptr<StorageWriter> storage;
        bool persistence_healthy = true;
        std::unordered_map<std::string, std::string> sessions;
        std::uint64_t next_session_nonce = 1;
        std::uint64_t callback_failures = 0;
        TickIntervalSampler tick_sampler;
        std::string tick_run_id;
        bool telemetry_started = false;
        bool tick_invalidated = false;
        std::int32_t sampled_peak_online = -1;
        BusyTickToken busy_ticks;
        DropToken drops;
        DeathToken deaths;
        ModeToken modes;
        PickupToken pickups;
        JumpToken jumps;
        KillToken kills;
        HurtToken hurts;
        BlockToken blocks;
#ifndef BEDROCKINSIGHT_FAKE_SDK
        TravelToken travels;
        ItemUseToken item_uses;
        ItemTransferToken item_transfers;
        TradeToken trades;
        BreedToken breeds;
        TargetToken targets;
        BlockInteractionToken block_interactions;
        std::unordered_map<std::int64_t, TrackedPlayer> players_by_uid;
        std::unordered_map<std::string, std::int64_t> uid_by_player;
        std::unordered_map<std::string, SneakState> sneak_started_us;
#endif
        bool busy_invalidated = false;
    };
    static bool submit(Capture&, AnalyticsEvent event);

    static void on_join(Capture&, ll::event::PlayerJoinEvent&);
    static void on_disconnect(Capture&, ll::event::PlayerDisconnectEvent&);
    static void on_destroy(Capture&, ll::event::PlayerDestroyBlockEvent&);
    static void on_placed(Capture&, ll::event::PlayerPlacedBlockEvent&);
    static void on_tick(Capture&, ll::event::ServerLevelTickEvent&);
    static void on_death(Capture&, ll::event::PlayerDieEvent&);
    static void on_use_item(Capture&, ll::event::PlayerUseItemEvent&);
#ifndef BEDROCKINSIGHT_FAKE_SDK
    static void on_actor_hurt(Capture&, ll::event::ActorHurtEvent&);
    static void track_player(Capture&, Player&);
    static void forget_player_state(Capture&, std::string const& player_id);
#endif
    static void on_busy(Capture&, Level&, BusyTickWindow const&, bool valid);
    static void mode_emit(void*, AnalyticsEvent) noexcept;
    static void travel_emit(void*, AnalyticsEvent) noexcept;
    static void on_sprint_start(Capture&, ll::event::PlayerSprintingEvent&);
    static void on_sprint_stop(Capture&, ll::event::PlayerSprintedEvent&);
    static void on_sneak_start(Capture&, ll::event::PlayerSneakingEvent&);
    static void on_sneak_stop(Capture&, ll::event::PlayerSneakedEvent&);

    std::mutex lifecycle_mutex_;
    bool loaded_ = false;
    std::shared_ptr<Capture> capture_;
    std::vector<ll::event::ListenerPtr> listeners_;
};

} // namespace bedrock_insight::levilamina
