#include "bedrock_insight/levilamina/runtime.hpp"

#include "bedrock_insight/core/remote_api.hpp"
#ifndef BEDROCKINSIGHT_FAKE_SDK
#include "bedrock_insight/levilamina/player_damage.hpp"
#include "bedrock_insight/levilamina/player_block_interaction.hpp"
#include "bedrock_insight/levilamina/stats_command.hpp"
#endif
#include "ll/api/event/EventBus.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/Versions.h"
#include "mc/world/actor/player/Player.h"
#ifndef BEDROCKINSIGHT_FAKE_SDK
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#endif
#include "mc/world/level/BlockPos.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace bedrock_insight::levilamina {
namespace {

void module_anchor() {}

bool pin_callback_module() noexcept {
#ifdef _WIN32
    // This SDK snapshots ListenerPtr before dispatch; removeListener does not
    // wait for snapshots. Keep callback code mapped until process exit even
    // if LL later unloads the logical mod. Hot DLL replacement is unsupported.
    HMODULE module = nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&module_anchor), &module) != 0;
#else
    return false;
#endif
}

std::int64_t now_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t steady_us() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

AnalyticsEvent tick_marker(std::string const& run_id, char const* cause) {
    AnalyticsEvent value;
    value.kind = EventKind::ServerTick;
    value.actor.kind = ActorKind::System;
    value.occurred_at_ms = now_ms();
    value.cause = cause;
    value.session_id = run_id;
    return value;
}

Actor actor_for(Player& player) {
    // Use the LL accessor, never generated Player field offsets or a mutable
    // display name as identity. Preserve all 128 UUID bits without MCAPI.
    auto const& uuid = player.getUuid();
    Actor actor;
    if (uuid.a != 0 || uuid.b != 0) {
        actor.kind = ActorKind::Player;
        actor.id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
    }
    actor.display_name = player.getRealName();
    return actor;
}

AnalyticsEvent player_event(Player& player, EventKind kind) {
    AnalyticsEvent event;
    event.occurred_at_ms = now_ms();
    event.kind = kind;
    event.actor = actor_for(player);
    // Session events do not provide a verified world/position snapshot.
    event.cause = "levilamina_player_event";
    return event;
}

} // namespace

bool Runtime::load() noexcept {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    if (loaded_) return true;
    if (!listeners_.empty()) return false;
    try {
        // The preloaded LL manager is deliberately absent from the ordinary
        // mod dependency graph. Gate its real exported version/platform here,
        // before opening data or registering hooks, not with a missing "mod".
        auto const version = ll::getLoaderVersion();
        if (ll::isClient() || version.major != 26 || version.minor != 60 ||
            version.patch != 0 || version.preRelease) return false;
        auto mod = ll::mod::NativeMod::current();
        if (!mod) return false;
        if (!pin_callback_module()) return false;
        capture_ = std::make_shared<Capture>();
        capture_->storage_config.database_path = mod->getDataDir() / "bedrock_insight.sqlite3";
        capture_->storage_config.queue_capacity = 8192;
#ifndef BEDROCKINSIGHT_FAKE_SDK
        set_remote_api_database(capture_->storage_config.database_path);
#endif
        // A loaded but never-enabled mod does not receive disable at shutdown.
        // Do not create a worker whose cleanup would depend on DLL detach.
        mod->getLogger().info("BedrockInsight loaded: capture only; HTTP and rollback disabled");
        loaded_ = true;
        return true;
    } catch (...) {
        remove_listeners();
        stop_storage();
        if (listeners_.empty()) capture_.reset();
        return false;
    }
}

bool Runtime::enable() noexcept {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    if (!loaded_ || !capture_) return false;
    try {
        std::lock_guard lock(capture_->mutex);
        if (!capture_->persistence_healthy || capture_->callback_failures != 0) return false;
        if (capture_->storage && capture_->storage->metrics().failed) return false;
        if (capture_->enabled) return true;
        if (capture_->activation_generation == std::numeric_limits<std::uint64_t>::max()) return false;
        ++capture_->activation_generation;
        capture_->tick_sampler = {};
        capture_->tick_run_id = "lltick-" + std::to_string(now_ms()) + "-" + std::to_string(steady_us())
            + "-" + std::to_string(capture_->activation_generation);
        capture_->telemetry_started = false;
        capture_->tick_invalidated = false;
        capture_->sampled_peak_online = -1;
        capture_->busy_invalidated = false;
        capture_->modes=make_mode_tracker();
    } catch (...) {
        // Building the activation ID allocates. enable() is noexcept: resource
        // exhaustion must reject activation, never terminate the BDS process.
        return false;
    }
    // LL removes mod listeners after disable. Re-register on every activation,
    // first cleaning handles from a previous activation or a partial failure.
    // Do not hold Capture::mutex across SDK hook registration/removal.
    if (!remove_listeners()) return false;
    try {
        // StorageWriter is intentionally one-shot. Each activation opens a
        // fresh writer and resumes durable event IDs from the same database.
        auto storage = std::make_unique<StorageWriter>(capture_->storage_config);
        if (!storage->start()) return false;
        std::lock_guard lock(capture_->mutex);
        capture_->storage = std::move(storage);
    } catch (...) {
        stop_storage();
        return false;
    }
    if (!install_listeners()) {
        stop_storage();
        return false;
    }
    BusyTickToken busy;
    try {
        auto const generation=capture_->activation_generation;
        busy=start_busy_ticks([weak=std::weak_ptr<Capture>(capture_),generation](
            Level& level, BusyTickWindow const& window, bool valid) noexcept {
            if (auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if (!capture->enabled || !capture->storage || capture->activation_generation!=generation) return;
                try { on_busy(*capture,level,window,valid); }
                catch (...) { ++capture->callback_failures; }
            }
        });
    } catch (...) { stop_storage(); return false; }
    if (!busy) { stop_storage(); return false; }
    DropToken drops;
    try {
        auto const generation=capture_->activation_generation;
        drops=start_drop_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if (auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if (!capture->enabled || !capture->storage || capture->activation_generation!=generation) return;
                try {
                    value.session_id=capture->tick_run_id;
                    static_cast<void>(submit(*capture,std::move(value)));
                } catch (...) {++capture->callback_failures;}
            }
        });
    } catch (...) {}
    if (!drops) { stop_busy_ticks(busy); stop_storage(); return false; }
    DeathToken deaths;
    try {
        auto const generation=capture_->activation_generation;
        deaths=start_death_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if (auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if (!capture->enabled || !capture->storage || capture->activation_generation!=generation) return;
                try {
                    value.session_id=capture->tick_run_id;
                    static_cast<void>(submit(*capture,std::move(value)));
                } catch (...) {++capture->callback_failures;}
            }
        });
    } catch (...) {}
    if (!deaths) {stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    PickupToken pickups;
    try {
        auto const generation=capture_->activation_generation;
        pickups=start_pickup_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try {value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!pickups){stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    JumpToken jumps;
    try {
        auto const generation=capture_->activation_generation;
        jumps=start_jump_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try {value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!jumps){stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    KillToken kills;
    try {
        auto const generation=capture_->activation_generation;
        kills=start_kill_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!kills){stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    HurtToken hurts;
    try {
        auto const generation=capture_->activation_generation;
        hurts=start_hurt_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!hurts){stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    BlockToken blocks;
    try {
        auto const generation=capture_->activation_generation;
        blocks=start_block_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!blocks){stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
#ifndef BEDROCKINSIGHT_FAKE_SDK
    TravelToken travels;
    try {
        travels=make_travel_tracker();
    }catch(...){}
    if(!travels){stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    ItemUseToken item_uses;
    try {
        auto const generation=capture_->activation_generation;
        item_uses=start_item_use_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!item_uses){stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    ItemTransferToken item_transfers;
    try {
        auto const generation=capture_->activation_generation;
        item_transfers=start_item_transfer_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!item_transfers){stop_item_use_capture(item_uses);stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    TradeToken trades;
    try {
        auto const generation=capture_->activation_generation;
        trades=start_trade_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!trades){stop_item_transfer_capture(item_transfers);stop_item_use_capture(item_uses);stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    BreedToken breeds;
    try {
        auto const generation=capture_->activation_generation;
        breeds=start_breed_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!breeds){stop_trade_capture(trades);stop_item_transfer_capture(item_transfers);stop_item_use_capture(item_uses);stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    TargetToken targets;
    try {
        auto const generation=capture_->activation_generation;
        targets=start_target_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!targets){stop_breed_capture(breeds);stop_trade_capture(trades);stop_item_transfer_capture(item_transfers);stop_item_use_capture(item_uses);stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
    BlockInteractionToken block_interactions;
    try {
        auto const generation=capture_->activation_generation;
        block_interactions=start_block_interaction_capture([weak=std::weak_ptr<Capture>(capture_),generation](AnalyticsEvent value) noexcept {
            if(auto capture=weak.lock()) {
                std::lock_guard lock(capture->mutex);
                if(!capture->enabled || !capture->storage || capture->activation_generation!=generation)return;
                try{value.session_id=capture->tick_run_id;static_cast<void>(submit(*capture,std::move(value)));}
                catch(...){++capture->callback_failures;}
            }
        });
    }catch(...){}
    if(!block_interactions){stop_target_capture(targets);stop_breed_capture(breeds);stop_trade_capture(trades);stop_item_transfer_capture(item_transfers);stop_item_use_capture(item_uses);stop_block_capture(blocks);stop_hurt_capture(hurts);stop_kill_capture(kills);stop_jump_capture(jumps);stop_pickup_capture(pickups);stop_death_capture(deaths);stop_drop_capture(drops);stop_busy_ticks(busy);stop_storage();return false;}
#endif
    bool admitted = false;
    {
        std::lock_guard lock(capture_->mutex);
        capture_->busy_ticks=std::move(busy);
        capture_->drops=std::move(drops);
        capture_->deaths=std::move(deaths);
        capture_->pickups=std::move(pickups);
        capture_->jumps=std::move(jumps);
        capture_->kills=std::move(kills);
        capture_->hurts=std::move(hurts);
        capture_->blocks=std::move(blocks);
#ifndef BEDROCKINSIGHT_FAKE_SDK
        capture_->travels=std::move(travels);
        capture_->item_uses=std::move(item_uses);
        capture_->item_transfers=std::move(item_transfers);
        capture_->trades=std::move(trades);
        capture_->breeds=std::move(breeds);
        capture_->targets=std::move(targets);
        capture_->block_interactions=std::move(block_interactions);
#endif
        try {
            // Invalidate any recent crash-left sample BEFORE this activation's
            // first tick. A new run is warming up, not the previous run's TPS.
            auto const marked=submit(*capture_, tick_marker(capture_->tick_run_id, "ll26.60.busytick.start.v1"));
            capture_->telemetry_started=marked;
            admitted=marked && submit(*capture_, tick_marker(capture_->tick_run_id, "ll26.60.interval.start.v1"));
            capture_->enabled = admitted;
        } catch (...) { ++capture_->callback_failures; }
    }
    if (!admitted) { stop_storage(); return false; }
#ifndef BEDROCKINSIGHT_FAKE_SDK
    // Commands are registered after the capture is fully admitted. Failure to
    // register is non-fatal: capture must keep working without the GUI.
    static_cast<void>(register_stats_commands(capture_->storage_config.database_path));
#endif
    try {
        if (auto mod = ll::mod::NativeMod::current()) {
            mod->getLogger().info("BedrockInsight enabled: join/leave, block attempts, busy timer v1, death/use notifications; drop, dead-flag/mode, take delta, jump impulse, direct-attack death, hurt-slot and proven mined/placed block observations v1; block-use/cake/cauldron/flower-pot/noteblock interaction results v1; item use/durability-use, created-output craft/trade/enchant and furnace output v1");
        }
    } catch (...) {}
    return true;
}

bool Runtime::disable() noexcept {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    // LL's normal server shutdown disables mods, but does not unload them.
    // Drain/join here, not from a global destructor under the DLL loader lock.
    return stop_storage();
}

bool Runtime::unload() noexcept {
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    bool healthy = stop_storage();
    // Do not discard handles or authorize DLL unload while registered.
    // Storage has stopped even if removal fails and unload must be denied.
    if (!remove_listeners()) return false;
    capture_.reset();
    loaded_ = false;
    return healthy;
}

bool Runtime::stop_storage() noexcept {
    if (!capture_) return true;
    std::unique_ptr<StorageWriter> storage;
    BusyTickToken busy;
    DropToken drops;
    DeathToken deaths;
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
#endif
    {
        std::lock_guard lock(capture_->mutex);
        capture_->enabled = false;
        if(capture_->modes) {
            try {
                if(capture_->storage)finish_modes(*capture_->modes,{capture_.get(),&Runtime::mode_emit});
            } catch (...) {++capture_->callback_failures;}
            capture_->modes.reset();
        }
        busy=std::move(capture_->busy_ticks);
        drops=std::move(capture_->drops);
        deaths=std::move(capture_->deaths);
        pickups=std::move(capture_->pickups);
        jumps=std::move(capture_->jumps);
        kills=std::move(capture_->kills);
        hurts=std::move(capture_->hurts);
        blocks=std::move(capture_->blocks);
#ifndef BEDROCKINSIGHT_FAKE_SDK
        travels=std::move(capture_->travels);
        item_uses=std::move(capture_->item_uses);
        item_transfers=std::move(capture_->item_transfers);
        trades=std::move(capture_->trades);
        breeds=std::move(capture_->breeds);
        targets=std::move(capture_->targets);
        block_interactions=std::move(capture_->block_interactions);
        // Flush pending travel distance while capture_->storage is still alive.
        // The emit callback routes through submit(), so the writer must not be
        // moved out before this point or pending distances are lost/crash.
        if (travels) {
            try { finish_travel(*travels, {capture_.get(), &Runtime::travel_emit}); }
            catch (...) { ++capture_->callback_failures; }
            travels.reset();
        }
#endif
        // Queued BEFORE draining; a clean disable invalidates live telemetry.
        // Exceptions must not escape this noexcept lifecycle or skip worker join.
        if (capture_->storage && capture_->telemetry_started) {
            try {
                if (!capture_->tick_invalidated)
                    static_cast<void>(submit(*capture_, tick_marker(capture_->tick_run_id, "ll26.60.interval.end.v1")));
                static_cast<void>(submit(*capture_, tick_marker(capture_->tick_run_id, "ll26.60.busytick.end.v1")));
            }
            catch (...) { ++capture_->callback_failures; }
        }
        capture_->telemetry_started = false;
#ifndef BEDROCKINSIGHT_FAKE_SDK
        // Finalize open sneak intervals before the writer is drained. This is a
        // real wall-clock duration for an observed start with no stop event yet.
        for (auto& [id, sneak] : capture_->sneak_started_us) {
            (void)id;
            try {
                auto const elapsed = steady_us() - sneak.start_us;
                if (elapsed > 0 && elapsed <= 3600000000LL) {
                    AnalyticsEvent value;
                    value.kind = EventKind::PlayerSneakTime;
                    value.actor.kind = ActorKind::Player;
                    value.actor.id = sneak.actor_id;
                    value.actor.display_name = sneak.display_name;
                    value.primary_type = "sneak";
                    value.amount = std::max<std::int64_t>(1, elapsed / 50000);
                    value.occurred_at_ms = now_ms();
                    value.cause = "ll26.60.sneak_duration.v1";
                    value.details_json = R"({"phase":"shutdown_finalize","units":"ticks","wall_clock":true})";
                    static_cast<void>(submit(*capture_, std::move(value)));
                }
            } catch (...) { ++capture_->callback_failures; }
        }
        capture_->sneak_started_us.clear();
        capture_->players_by_uid.clear();
        capture_->uid_by_player.clear();
#endif
        // No synthetic disconnect is asserted for players still connected.
        capture_->sessions.clear();
        storage = std::move(capture_->storage);
    }
    stop_busy_ticks(busy); // code remains pinned, hooks become pass-through
    stop_drop_capture(drops);
    stop_death_capture(deaths);
    stop_pickup_capture(pickups);
    stop_jump_capture(jumps);
    stop_kill_capture(kills);
    stop_hurt_capture(hurts);
    stop_block_capture(blocks);
#ifndef BEDROCKINSIGHT_FAKE_SDK
    stop_target_capture(targets);
    stop_block_interaction_capture(block_interactions);
    stop_breed_capture(breeds);
    stop_trade_capture(trades);
    stop_item_transfer_capture(item_transfers);
    stop_item_use_capture(item_uses);
#endif
    if (storage) {
        // stop() closes admission, drains accepted events and joins the worker.
        // Never hold Capture::mutex across SQLite shutdown or a thread join.
        storage->stop();
        auto const metrics = storage->metrics();
        std::lock_guard lock(capture_->mutex);
        capture_->persistence_healthy = capture_->persistence_healthy && !metrics.failed
            && metrics.dropped_events == 0 && metrics.accepted_events == metrics.committed_events;
    }
    std::lock_guard lock(capture_->mutex);
    return capture_->persistence_healthy && capture_->callback_failures == 0;
}

bool Runtime::install_listeners() noexcept {
    try {
        auto& bus = ll::event::EventBus::getInstance();
        std::uint64_t generation;
        {
            std::lock_guard lock(capture_->mutex);
            generation = capture_->activation_generation;
        }
        // Own every handle before registration, including on partial failure.
        listeners_.reserve(14);
        auto add = [&]<class Event>(void (*handler)(Capture&, Event&)) {
            auto callback = [capture = capture_, handler, generation](Event& event) noexcept {
                std::lock_guard lock(capture->mutex);
                if (!capture->enabled || !capture->storage || capture->activation_generation != generation) return;
                try { handler(*capture, event); }
                catch (...) { ++capture->callback_failures; }
            };
            auto listener = ll::event::Listener<Event>::create(
                std::move(callback), ll::event::EventPriority::Lowest);
            listeners_.push_back(listener);
            if (!bus.addListener<Event>(listener)) throw std::runtime_error("event registration failed");
        };
        add(&Runtime::on_join);
        add(&Runtime::on_disconnect);
        add(&Runtime::on_destroy);
        add(&Runtime::on_placed);
        add(&Runtime::on_tick);
        add(&Runtime::on_death);
        add(&Runtime::on_use_item);
#ifndef BEDROCKINSIGHT_FAKE_SDK
        add(&Runtime::on_actor_hurt);
#endif
        add(&Runtime::on_sprint_start);
        add(&Runtime::on_sprint_stop);
        add(&Runtime::on_sneak_start);
        add(&Runtime::on_sneak_stop);
        return true;
    } catch (...) {
        remove_listeners();
        return false;
    }
}

bool Runtime::remove_listeners() noexcept {
    if (listeners_.empty()) return true;
    try {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto iterator = listeners_.begin(); iterator != listeners_.end();) {
            if (!bus.hasListener(*iterator) || bus.removeListener(*iterator)) iterator = listeners_.erase(iterator);
            else ++iterator;
        }
    } catch (...) {
        return false;
    }
    return listeners_.empty();
}

bool Runtime::submit(Capture& capture, AnalyticsEvent event) {
    // Retain every rejection in StorageMetrics; no uncommitted metric copy.
    return capture.storage->submit(std::move(event));
}

void Runtime::on_join(Capture& capture, ll::event::PlayerJoinEvent& event) {
    if (event.isCancelled()) return;
    auto value = player_event(event.self(), EventKind::SessionStart);
    if (value.has_player_actor()) {
        if (capture.sessions.contains(value.actor.id)) return;
        if(capture.modes)forget_modes(*capture.modes,value.actor.id,{&capture,&Runtime::mode_emit});
#ifndef BEDROCKINSIGHT_FAKE_SDK
        if(capture.travels)track_travel(*capture.travels,event.self(),value.occurred_at_ms,
            {&capture,&Runtime::travel_emit});
        track_player(capture,event.self());
#endif
        value.session_id = "session-" + value.actor.id + "-" + std::to_string(value.occurred_at_ms)
            + "-" + std::to_string(capture.next_session_nonce++);
        auto [session, inserted] = capture.sessions.emplace(value.actor.id, value.session_id);
        if (inserted && !submit(capture, std::move(value))) capture.sessions.erase(session);
    } else {
        static_cast<void>(submit(capture, std::move(value)));
    }
}

void Runtime::on_disconnect(Capture& capture, ll::event::PlayerDisconnectEvent& event) {
    auto value = player_event(event.self(), EventKind::SessionEnd);
    if(capture.modes)forget_modes(*capture.modes,value.actor.id,{&capture,&Runtime::mode_emit});
#ifndef BEDROCKINSIGHT_FAKE_SDK
    if(capture.travels)forget_travel(*capture.travels,value.actor.id,value.occurred_at_ms,{&capture,&Runtime::travel_emit});
    forget_player_state(capture,value.actor.id);
#endif
    auto session = capture.sessions.find(value.actor.id);
    if (session != capture.sessions.end()) value.session_id = session->second;
    static_cast<void>(submit(capture, std::move(value)));
    if (session != capture.sessions.end()) capture.sessions.erase(session);
}

void Runtime::on_destroy(Capture& capture, ll::event::PlayerDestroyBlockEvent& event) {
    if (event.isCancelled()) return;
    auto value = player_event(event.self(), EventKind::AuditBlockChange);
    auto const& pos = event.pos();
    value.position = {pos.x, pos.y, pos.z, static_cast<std::int8_t>(static_cast<int>(event.self().getDimensionId()))};
    value.cause = "player_destroy_attempt";
    value.details_json = R"({"adapter":"PlayerDestroyBlockEvent","phase":"attempt","state_complete":false})";
    // The SDK publishes before origin(); it does not prove a committed break.
    static_cast<void>(submit(capture, std::move(value)));
}

void Runtime::on_placed(Capture& capture, ll::event::PlayerPlacedBlockEvent& event) {
    auto value = player_event(event.self(), EventKind::AuditBlockChange);
    auto const& pos = event.pos();
    value.position = {pos.x, pos.y, pos.z, static_cast<std::int8_t>(static_cast<int>(event.self().getDimensionId()))};
    value.cause = "player_place_attempt";
    value.details_json = R"({"adapter":"PlayerPlacedBlockEvent","phase":"attempt","state_complete":false})";
    // Despite its name, this SDK publishes before origin(). Both type columns
    // stay empty so an attempt cannot become a reversible state snapshot.
    static_cast<void>(submit(capture, std::move(value)));
}

void Runtime::on_tick(Capture& capture, ll::event::ServerLevelTickEvent& event) {
    auto const window = capture.tick_sampler.observe(&event.level(), steady_us());
    if(capture.modes && capture.tick_sampler.supported())
        poll_modes(*capture.modes,event.level(),steady_us(),now_ms(),{&capture,&Runtime::mode_emit});
#ifndef BEDROCKINSIGHT_FAKE_SDK
    if(capture.travels && capture.tick_sampler.supported())
        poll_travel(*capture.travels,event.level(),now_ms(),{&capture,&Runtime::travel_emit});
    if(capture.tick_sampler.supported()) {
        // Sleep is a real state transition: only false -> true is a success.
        for(auto it=capture.players_by_uid.begin();it!=capture.players_by_uid.end();) {
            auto& tracked=it->second;
            auto player=tracked.weak.tryUnwrap<Player>();
            if(!player || player.as_ptr()!=tracked.identity) { it=capture.players_by_uid.erase(it); continue; }
            try {
                bool const sleeping=player->isSleeping();
                if(sleeping && !tracked.sleeping) {
                    AnalyticsEvent value;
                    value.kind=EventKind::PlayerSleepResult;
                    value.actor.kind=ActorKind::Player;
                    value.actor.id=tracked.actor_id;
                    value.actor.display_name=tracked.display_name;
                    value.primary_type="minecraft:sleep_in_bed";
                    value.amount=1;
                    value.occurred_at_ms=now_ms();
                    value.position={0,0,0,static_cast<std::int8_t>(static_cast<int>(player->getDimensionId()))};
                    value.cause="ll26.60.sleep_flag_transition.v1";
                    value.details_json=R"({"adapter":"ServerLevelTickEvent","phase":"sleep_flag_false_to_true","success_verified":true,"wake_time_measured":false})";
                    static_cast<void>(submit(capture,std::move(value)));
                }
                tracked.sleeping=sleeping;
            } catch (...) { ++capture.callback_failures; }
            ++it;
        }
    }
#endif
    if (!capture.tick_sampler.supported() && !capture.tick_invalidated) {
        capture.tick_invalidated = true;
        static_cast<void>(submit(capture, tick_marker(capture.tick_run_id, "ll26.60.interval.end.v1")));
    }
    if (!window) return;
    // Official 26.60 Level MCAPI, not a guessed field offset/virtual slot.
    // One count read and one queue submission per >=5s, never SQLite per tick.
    auto const online = event.level().$getActivePlayerCount();
    auto value = AnalyticsEvent{};
    value.kind = EventKind::ServerTick;
    value.actor.kind = ActorKind::System;
    value.occurred_at_ms = now_ms();
    value.cause = "ll26.60.interval.v1";
    value.session_id = capture.tick_run_id;
    value.amount = window->intervals;
    value.online_players = online >= 0 && online <= 100000 ? online : -1;
    capture.sampled_peak_online = std::max(capture.sampled_peak_online, value.online_players);
    value.details_json = "{\"clock\":\"steady\",\"window_us\":" + std::to_string(window->elapsed_us)
        + ",\"sampled_peak_online\":" + std::to_string(capture.sampled_peak_online) + "}";
    // tick_mspt deliberately stays ZERO. Existing legacy server_tick_samples
    // requires positive processing mspt and MUST NOT receive interval telemetry.
    static_cast<void>(submit(capture, std::move(value)));
}

void Runtime::on_death(Capture& capture, ll::event::PlayerDieEvent& event) {
    auto value = player_event(event.self(), EventKind::PlayerDeathNotification);
    value.cause = "ll26.60.player_die_notification.v1";
    value.details_json = R"({"adapter":"PlayerDieEvent","phase":"before_origin","confirmed_death":false})";
    // The official publisher precedes Player::$die. Count notifications, NOT
    // distinct confirmed deaths; do not guess attacker identity/damage offsets.
    static_cast<void>(submit(capture, std::move(value)));
}

void Runtime::on_use_item(Capture& capture, ll::event::PlayerUseItemEvent& event) {
    if (event.isCancelled()) return;
    auto value = player_event(event.self(), EventKind::ItemUseAttempt);
    value.cause = "ll26.60.item_use_attempt.v1";
    value.details_json = R"({"adapter":"PlayerUseItemEvent","phase":"before_origin","confirmed_use":false})";
    // No ItemStack/NBT access or guessed subject offsets. Other listeners may
    // still cancel later; origin's bool result has NOT been observed here.
    static_cast<void>(submit(capture, std::move(value)));
}

#ifndef BEDROCKINSIGHT_FAKE_SDK
void Runtime::on_actor_hurt(Capture& capture, ll::event::ActorHurtEvent& event) {
    try {
        PlayerByUniqueId lookup = [&capture](std::int64_t unique_id, PlayerIdentity& out) {
            auto it = capture.players_by_uid.find(unique_id);
            if (it == capture.players_by_uid.end()) return false;
            out.actor_id = it->second.actor_id;
            out.display_name = it->second.display_name;
            return !out.actor_id.empty();
        };
        observe_hurt(event, lookup, [&capture](AnalyticsEvent value) {
            try { static_cast<void>(submit(capture, std::move(value))); }
            catch (...) { ++capture.callback_failures; }
        });
    } catch (...) { ++capture.callback_failures; }
}

void Runtime::track_player(Capture& capture, Player& player) {
    try {
        auto const& uuid = player.getUuid();
        if (uuid.a == 0 && uuid.b == 0) return;
        auto const unique_id = player.mLegacyUniqueID->rawID;
        if (unique_id == 0) return;
        auto id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        auto name = player.getRealName();
        if (name.size() > 256) name.resize(256);
        TrackedPlayer record{player.getEntityContext().getWeakRef(), static_cast<::Actor*>(&player),
            id, std::move(name), unique_id, player.isSleeping()};
        capture.players_by_uid[unique_id] = std::move(record);
        capture.uid_by_player[id] = unique_id;
    } catch (...) { ++capture.callback_failures; }
}

void Runtime::forget_player_state(Capture& capture, std::string const& player_id) {
    auto sneak = capture.sneak_started_us.find(player_id);
    if (sneak != capture.sneak_started_us.end()) {
        try {
            auto const elapsed = steady_us() - sneak->second.start_us;
            if (elapsed > 0 && elapsed <= 3600000000LL) {
                AnalyticsEvent value;
                value.kind = EventKind::PlayerSneakTime;
                value.actor.kind = ActorKind::Player;
                value.actor.id = sneak->second.actor_id;
                value.actor.display_name = sneak->second.display_name;
                value.primary_type = "sneak";
                value.amount = std::max<std::int64_t>(1, elapsed / 50000);
                value.occurred_at_ms = now_ms();
                value.cause = "ll26.60.sneak_duration.v1";
                value.details_json = R"({"phase":"disconnect_finalize","units":"ticks","wall_clock":true})";
                static_cast<void>(submit(capture, std::move(value)));
            }
        } catch (...) { ++capture.callback_failures; }
        capture.sneak_started_us.erase(sneak);
    }
    auto uid = capture.uid_by_player.find(player_id);
    if (uid != capture.uid_by_player.end()) {
        capture.players_by_uid.erase(uid->second);
        capture.uid_by_player.erase(uid);
    }
}
#endif

void Runtime::mode_emit(void* context, AnalyticsEvent value) noexcept {
    auto& capture=*static_cast<Capture*>(context); // synchronous, caller holds Capture::mutex
    try {value.session_id=capture.tick_run_id;static_cast<void>(submit(capture,std::move(value)));}
    catch (...) {++capture.callback_failures;}
}
void Runtime::travel_emit(void* context, AnalyticsEvent value) noexcept {
#ifndef BEDROCKINSIGHT_FAKE_SDK
    auto& capture = *static_cast<Capture*>(context); // synchronous, caller holds Capture::mutex
    try { value.session_id = capture.tick_run_id; static_cast<void>(submit(capture, std::move(value))); }
    catch (...) { ++capture.callback_failures; }
#else
    (void)context;
    (void)value;
#endif
}
void Runtime::on_sprint_start(Capture& c, ll::event::PlayerSprintingEvent& e) {
    if(c.modes)note_mode(*c.modes,e.self(),PlayerMode::Sprint,true,steady_us());
}
void Runtime::on_sprint_stop(Capture& c, ll::event::PlayerSprintedEvent& e) {
    if(c.modes)note_mode(*c.modes,e.self(),PlayerMode::Sprint,false,steady_us());
}
void Runtime::on_sneak_start(Capture& c, ll::event::PlayerSneakingEvent& e) {
    if(e.isCancelled()) return;
    if(c.modes)note_mode(*c.modes,e.self(),PlayerMode::Sneak,true,steady_us());
#ifndef BEDROCKINSIGHT_FAKE_SDK
    try {
        auto const& uuid = e.self().getUuid();
        if (uuid.a == 0 && uuid.b == 0) return;
        auto id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        if (c.sneak_started_us.contains(id)) return;
        auto name = e.self().getRealName();
        if (name.size() > 256) name.resize(256);
        c.sneak_started_us.emplace(id, SneakState{steady_us(), id, std::move(name)});
    } catch (...) { ++c.callback_failures; }
#endif
}
void Runtime::on_sneak_stop(Capture& c, ll::event::PlayerSneakedEvent& e) {
    if(e.isCancelled()) return;
    if(c.modes)note_mode(*c.modes,e.self(),PlayerMode::Sneak,false,steady_us());
#ifndef BEDROCKINSIGHT_FAKE_SDK
    try {
        auto const& uuid = e.self().getUuid();
        if (uuid.a == 0 && uuid.b == 0) return;
        auto id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
        auto it = c.sneak_started_us.find(id);
        if (it == c.sneak_started_us.end()) return;
        auto const elapsed = steady_us() - it->second.start_us;
        if (elapsed > 0 && elapsed <= 3600000000LL) {
            AnalyticsEvent value;
            value.kind = EventKind::PlayerSneakTime;
            value.actor.kind = ActorKind::Player;
            value.actor.id = it->second.actor_id;
            value.actor.display_name = it->second.display_name;
            value.primary_type = "sneak";
            value.amount = std::max<std::int64_t>(1, elapsed / 50000);
            value.occurred_at_ms = now_ms();
            value.position = {0, 0, 0, static_cast<std::int8_t>(static_cast<int>(e.self().getDimensionId()))};
            value.cause = "ll26.60.sneak_duration.v1";
            value.details_json = "{\"phase\":\"sneak_stop_to_start\",\"units\":\"ticks\",\"wall_clock\":true,"
                "\"elapsed_us\":" + std::to_string(elapsed) + "}";
            static_cast<void>(submit(c, std::move(value)));
        }
        c.sneak_started_us.erase(it);
    } catch (...) { ++c.callback_failures; }
#endif
}

void Runtime::on_busy(Capture& capture, Level& level, BusyTickWindow const& window, bool valid) {
    if (!valid) {
        if (!capture.busy_invalidated) {
            capture.busy_invalidated=true;
            static_cast<void>(submit(capture, tick_marker(capture.tick_run_id, "ll26.60.busytick.invalid.v1")));
        }
        return;
    }
    // A second thread may have invalidated the bridge after this report was
    // prepared but before this Capture lock was acquired. Never resurrect it.
    if (capture.busy_invalidated) return;
    AnalyticsEvent value;
    value.kind=EventKind::ServerTick; value.actor.kind=ActorKind::System;
    value.occurred_at_ms=now_ms(); value.cause="ll26.60.busytick.v1";
    value.session_id=capture.tick_run_id; value.amount=window.ticks;
    auto const online=level.$getActivePlayerCount();
    value.online_players=online>=0 && online<=100000 ? online : -1;
    capture.sampled_peak_online=std::max(capture.sampled_peak_online,value.online_players);
    value.details_json="{\"clock\":\"steady\",\"hook\":\"Level::$tick/Highest+Lowest\",\"wall_ns\":"
        + std::to_string(window.wall_ns) + ",\"busy_ns\":" + std::to_string(window.busy_ns)
        + ",\"max_inner_ns\":" + std::to_string(window.max_inner_ns)
        + ",\"frames\":" + std::to_string(window.frames)
        + ",\"sampled_peak_online\":" + std::to_string(capture.sampled_peak_online) + "}";
    // Integer legacy mspt is deliberately unused: preserve sub-ms precision and
    // explicit actual-tick provenance; writer still performs all SQLite work.
    static_cast<void>(submit(capture,std::move(value)));
}

} // namespace bedrock_insight::levilamina
