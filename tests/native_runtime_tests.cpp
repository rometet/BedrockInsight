#include "bedrock_insight/levilamina/runtime.hpp"
#include "fake_sdk.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <cstdlib>
#include <new>
#include <thread>

// Test binary ONLY: fail the next C++ allocation to verify noexcept admission.
// This file never participates in a real native mod DLL build.
std::atomic<bool> fail_next_new{false};
void* operator new(std::size_t size) {
    if (fail_next_new.exchange(false)) throw std::bad_alloc();
    if (auto p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace bedrock_insight::levilamina {
extern bool test_busy_install_failure;
extern bool test_drop_install_failure;
extern bool test_death_install_failure;
extern bool test_pickup_install_failure;
extern bool test_jump_install_failure;
extern bool test_kill_install_failure;
extern bool test_use_install_failure;
extern bool test_hurt_install_failure;
extern bool test_block_install_failure;
DeathSink test_death_callback();
PickupSink test_pickup_callback();
JumpSink test_jump_callback();
KillSink test_kill_callback();
UseSink test_use_callback();
HurtSink test_hurt_callback();
void test_drop_observation(AnalyticsEvent);
void test_busy_window(Level&,BusyTickWindow const&,bool valid=true);
}

void require(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}

long long scalar(bedrock_insight::sqlite_api::Library& library,
    bedrock_insight::sqlite_api::sqlite3* db, char const* sql) {
    bedrock_insight::sqlite_api::sqlite3_stmt* statement = nullptr;
    require(library.prepare_v2(db, sql, -1, &statement, nullptr) == 0, "prepare runtime query");
    require(library.step(statement) == bedrock_insight::sqlite_api::kRow, "read runtime query");
    auto value = library.column_int64(statement, 0);
    library.finalize(statement);
    return value;
}

bool can_switch_journal(std::filesystem::path const& path) {
    using namespace bedrock_insight;
    sqlite_api::Library library;
    sqlite_api::sqlite3* db = nullptr;
    require(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadWrite, nullptr) == 0, "open journal probe");
    auto const status = library.exec(db, "PRAGMA journal_mode=DELETE;", nullptr, nullptr, nullptr);
    require(library.close(db) == 0, "close journal probe");
    return status == sqlite_api::kOk;
}

int main() {
    using namespace bedrock_insight;
    try {
        auto root = std::filesystem::current_path() / "build/manual/tmp" /
            ("adapter-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto& bus = ll::event::EventBus::getInstance();
        auto mod = ll::mod::NativeMod::current();
        mod->data_dir = root / "unsupported-loader";
        for (int failure=0; failure<4; ++failure) {
            ll::test_is_client=(failure==0);
            ll::test_loader_version.minor=(failure==1 ? 51u : 60u);
            ll::test_loader_version.preRelease=(failure==2);
            ll::test_version_throws=(failure==3);
            levilamina::Runtime runtime;
            require(!runtime.load(), "unsupported loader must fail before capture initialization");
            require(!std::filesystem::exists(mod->data_dir) && bus.registrations.empty(), "loader rejection must not open DB or hooks");
            require(runtime.unload(), "rejected loader cleanup");
        }
        ll::test_is_client=false; ll::test_loader_version={}; ll::test_version_throws=false;
        {
            mod->data_dir = root / "never-enabled";
            levilamina::Runtime runtime;
            require(runtime.load(), "load metadata only");
            require(!std::filesystem::exists(mod->data_dir), "load must not open SQLite or start its worker");
            require(runtime.disable() && runtime.unload(), "never-enabled lifecycle cleanup");
        }
        {
            mod->data_dir = root / "activation-oom";
            levilamina::Runtime runtime;
            require(runtime.load(), "load before injected allocation failure");
            fail_next_new=true;
            auto const enabled=runtime.enable();
            require(!fail_next_new.exchange(false), "allocation failure was actually triggered");
            require(!enabled, "activation OOM returns false instead of terminating BDS");
            require(!std::filesystem::exists(mod->data_dir) && bus.registrations.empty(), "OOM has no worker/hooks/data side effects");
            require(runtime.unload(), "OOM lifecycle cleanup");
        }
        {
            mod->data_dir = root / "bad-start";
            // An owned directory at the database filename forces SQLite open
            // failure without touching an external file or a running server.
            std::filesystem::create_directories(mod->data_dir / "bedrock_insight.sqlite3");
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(), "failed writer startup must fail activation");
            require(bus.registrations.empty(), "failed storage must not install game hooks");
            require(runtime.disable() && runtime.unload(), "failed startup cleanup");
            std::filesystem::remove(mod->data_dir / "bedrock_insight.sqlite3");
            std::filesystem::remove(mod->data_dir);
        }
        {
            mod->data_dir=root / "hook-failure";
            levilamina::test_busy_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"busy hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"hook failure closes worker");
            require(runtime.unload() && bus.registrations.empty(),"hook failure cleanup removes listeners");
            levilamina::test_busy_install_failure=false;
        }
        for (int fail_at = 1; fail_at <= 11; ++fail_at) {
            mod->data_dir = root / std::to_string(fail_at);
            bus.add_calls = 0;
            bus.fail_add = fail_at;
            levilamina::Runtime runtime;
            require(runtime.load(), "load before activation");
            require(!runtime.enable(), "registration failure must fail enable");
            require(bus.registrations.empty(), "partial registration must leave no listeners");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"), "failed activation must close SQLite");
            require(runtime.unload(), "failed enable cleanup is idempotent");
        }
        {
            mod->data_dir=root / "drop-hook-failure";
            levilamina::test_drop_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"drop hook install failure rejects enable");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"drop failure stops SQLite worker");
            require(runtime.unload() && bus.registrations.empty(),"drop failure removes listeners and busy state");
            levilamina::test_drop_install_failure=false;
        }
        mod->data_dir = root / "capture";
        {
            mod->data_dir=root / "death-hook-failure";
            levilamina::test_death_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"death hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"death failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"death failure closes earlier drop/busy bridges");
            levilamina::test_death_install_failure=false;
        }
        {
            mod->data_dir=root / "pickup-hook-failure";
            levilamina::test_pickup_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"pickup hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"pickup failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"pickup failure closes prior bridges/listeners");
            levilamina::test_pickup_install_failure=false;
        }
        {
            mod->data_dir=root / "jump-hook-failure";
            levilamina::test_jump_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"jump hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"jump failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"jump failure closes prior bridges/listeners");
            levilamina::test_jump_install_failure=false;
        }
        {
            mod->data_dir=root / "use-hook-failure";
            levilamina::test_use_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"use hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"use failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"use failure closes prior bridges/listeners");
            levilamina::test_use_install_failure=false;
        }
        {
            mod->data_dir=root / "hurt-hook-failure";
            levilamina::test_hurt_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"hurt hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"hurt failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"hurt failure closes previous bridges/listeners");
            levilamina::test_hurt_install_failure=false;
        }
        {
            mod->data_dir=root / "block-hook-failure";
            levilamina::test_block_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"block hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"block failure closes writer");
            require(runtime.unload() && bus.registrations.empty(),"block failure closes previous bridges/listeners");
            levilamina::test_block_install_failure=false;
        }
        {
            mod->data_dir=root / "kill-hook-failure";
            levilamina::test_kill_install_failure=true;
            levilamina::Runtime runtime;
            require(runtime.load() && !runtime.enable(),"kill hook failure rejects activation");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"),"kill failure closes worker");
            require(runtime.unload() && bus.registrations.empty(),"kill failure closes prior bridges/listeners");
            levilamina::test_kill_install_failure=false;
        }
        mod->data_dir = root / "capture";
        bus.fail_add = 0;
        bus.add_calls = 0;
        Player player;
        auto join = ll::event::PlayerJoinEvent{player};
        auto leave = ll::event::PlayerDisconnectEvent{player};
        auto destroy = ll::event::PlayerDestroyBlockEvent{player, {-2,64,3}};
        auto placed = ll::event::PlayerPlacedBlockEvent{player, {-2,64,3}};
        std::vector<ll::event::EventBus::Registration> delayed_snapshot;
        {
            levilamina::Runtime runtime;
            require(runtime.load() && bus.registrations.empty(), "load must not install game hooks");
            require(runtime.enable(), "runtime startup");
            require(!can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"), "journal probe must detect active SQLite worker");
            require(bus.registrations.size() == 11, "eleven typed adapters including deferred modes");
            require(runtime.enable() && bus.registrations.size() == 11, "enable is idempotent");
            Level level;
            player.level=&level;
            auto tick = ll::event::ServerLevelTickEvent{level};
            bus.publish(tick); // first callback is only warmup, never a guessed 20 TPS
            join.cancelled = true;
            bus.publish(join);
            join.cancelled = false;
            bus.publish(join);
            bus.publish(join); // Duplicate notification must not count two sessions.
            player.name = "Renamed"; // Identity remains UUID based.
            destroy.cancelled = true;
            bus.publish(destroy);
            destroy.cancelled = false;
            bus.publish(destroy);
            bus.publish(placed);
            auto death = ll::event::PlayerDieEvent{player};
            auto use = ll::event::PlayerUseItemEvent{player};
            bus.publish(death);
            use.cancelled=true;bus.publish(use);
            use.cancelled=false;bus.publish(use);bus.publish(use);
            bus.publish(leave);
            AnalyticsEvent dropped;
            dropped.kind=EventKind::ItemDropResult;dropped.actor={ActorKind::Player,"uuid64:1:2","Renamed"};
            dropped.occurred_at_ms=1;dropped.amount=64;dropped.primary_type="minecraft:stone";
            dropped.cause="ll26.60.player_drop_returned_true.v1";
            levilamina::test_drop_observation(dropped);
            auto confirmed=dropped;confirmed.kind=EventKind::PlayerDeath;confirmed.amount=1;
            confirmed.primary_type="minecraft:player";confirmed.secondary_type="cause:5";
            confirmed.cause="ll26.60.player_dead_transition.v1";
            confirmed.details_json=R"({"hook":"Player::$die","phase":"post_dead_transition","before_dead":false,"after_dead":true,"killer_attribution":"unmeasured"})";
            auto delayed_death=levilamina::test_death_callback();
            require(static_cast<bool>(delayed_death),"death bridge installed");
            delayed_death(confirmed);
            AnalyticsEvent pickup=confirmed;pickup.kind=EventKind::ItemPickupDelta;
            pickup.primary_type="minecraft:stone";pickup.secondary_type="";pickup.amount=3;
            pickup.cause="ll26.60.take_inventory_delta.v1";
            pickup.details_json=R"({"hook":"Player::take","phase":"return_true_inventory_delta","scope":"main_inventory_type_total","entry_units":64,"slots":36,"before_units":63,"after_units":66,"nested":false,"durable_transfer_verified":false})";
            auto delayed_pickup=levilamina::test_pickup_callback();
            require(static_cast<bool>(delayed_pickup),"pickup bridge installed");delayed_pickup(pickup);
            AnalyticsEvent jump=confirmed;jump.kind=EventKind::PlayerJumpImpulse;
            jump.secondary_type="";jump.cause="ll26.60.jump_pos_delta.v1";
            jump.details_json=R"({"hook":"Mob::jumpFromGround","phase":"post_upward_impulse","measurement":"pos_delta_y_x1e6","grounded_before":true,"vehicle_before":false,"vehicle_after":false,"before_y_micro":-80000,"after_y_micro":420000,"dimension":0,"world_jump_verified":false})";
            auto delayed_jump=levilamina::test_jump_callback();
            require(static_cast<bool>(delayed_jump),"jump bridge installed");delayed_jump(jump);
            AnalyticsEvent use_result=confirmed;use_result.kind=EventKind::ItemUseResult;
            use_result.primary_type="minecraft:snowball";use_result.secondary_type="main_hand";
            use_result.cause="ll26.60.gamemode_use_return.v1";
            use_result.details_json=R"({"hook":"GameMode::$useItem","phase":"return_true_inventory_snapshot","scope":"main_inventory_type_total","entry_units":16,"slots":36,"before_units":16,"after_units":15,"loss_units":1,"nested":false,"effect_completed_verified":false,"consumption_verified":false})";
            auto delayed_use=levilamina::test_use_callback();
            require(static_cast<bool>(delayed_use),"use bridge installed");delayed_use(use_result);
            auto hurt=use_result;hurt.kind=EventKind::ItemHurtSlotLoss;hurt.primary_type="minecraft:wooden_pickaxe";
            hurt.secondary_type="";hurt.cause="ll26.60.hurt_slot_empty.v1";
            hurt.details_json=R"({"hook":"ItemStackBase::hurtAndBreak","phase":"return_true_slot_empty","scope":"main_inventory_same_slot","delta_damage":1,"slots":36,"slot":0,"entry_units":1,"after_units":0,"nested":false,"break_completed_verified":false})";
            auto delayed_hurt=levilamina::test_hurt_callback();
            require(static_cast<bool>(delayed_hurt),"hurt bridge installed");delayed_hurt(hurt);
            AnalyticsEvent kill=confirmed;kill.kind=EventKind::DirectKillObservation;
            kill.primary_type="minecraft:zombie";kill.secondary_type="mob";
            kill.cause="ll26.60.direct_attack_dead_transition.v1";
            kill.details_json=R"({"hook":"Mob::$die","phase":"post_dead_transition","damage_cause":2,"source_entity":true,"source_child":false,"source_block":false,"source_ids_equal":true,"resolved_player":true,"self":false,"before_dead":false,"after_dead":true,"nested":false,"world_death_verified":false})";
            auto delayed_kill=levilamina::test_kill_callback();
            require(static_cast<bool>(delayed_kill),"kill bridge installed");delayed_kill(kill);
            auto player_kill=kill;player_kill.primary_type="minecraft:player";player_kill.secondary_type="player";delayed_kill(player_kill);
            player.uuid = {0,0};
            bus.publish(join);
            bus.publish(leave); // A display name must not substitute for UUID.
            // All submissions fit the native queue. Disable must drain them
            // before returning, even without a subsequent unload callback.
            player.uuid = {1,2};
            for (int index = 0; index < 128; ++index) bus.publish(placed);
            level.online=2;
            std::this_thread::sleep_for(std::chrono::milliseconds(5050));
            auto sprint=ll::event::PlayerSprintingEvent{player};
            auto sneak=ll::event::PlayerSneakingEvent{player,true};
            bus.publish(sneak); // cancelled notification must not create a pending observation
            bus.publish(sprint);player.sprinting=true;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            bus.publish(tick); // real adapter clock/serialization -> queue -> SQLite
            levilamina::test_busy_window(level,BusyTickWindow{100,5000000000,200000000,2000000,100});
            levilamina::test_busy_window(level,{},false);
            levilamina::test_busy_window(level,BusyTickWindow{100,5000000000,200000000,2000000,100});
            require(runtime.disable(), "disable");
            delayed_death(confirmed); // already in flight: disabled admission
            delayed_pickup(pickup);
            delayed_jump(jump);
            delayed_kill(kill);
            delayed_use(use_result);
            delayed_hurt(hurt);
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"), "disable must close worker connection before unload");
            bus.publish(join); // Must not change session or database state.
            delayed_snapshot = bus.registrations;
            // The official 26.60 EventBus removes mod listeners after disable.
            bus.registrations.clear();
            bus.fail_add = bus.add_calls + 2;
            require(!runtime.enable() && bus.registrations.empty(), "partial re-enable must unwind");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"), "failed re-enable must stop its new worker");
            for (auto const& value : delayed_snapshot) if (value.type == typeid(ll::event::PlayerJoinEvent)) {
                std::static_pointer_cast<ll::event::Listener<ll::event::PlayerJoinEvent>>(value.listener)->callback(join);
            }
            bus.fail_add = 0;
            require(runtime.enable() && bus.registrations.size() == 11, "restore listeners after LL disable cleanup");
            delayed_death(confirmed); // old activation must not enter new writer
            delayed_pickup(pickup);
            delayed_jump(jump);
            delayed_kill(kill);
            delayed_use(use_result);
            delayed_hurt(hurt);
            for (auto const& value : delayed_snapshot) if (value.type == typeid(ll::event::PlayerJoinEvent)) {
                std::static_pointer_cast<ll::event::Listener<ll::event::PlayerJoinEvent>>(value.listener)->callback(join);
            }
            player.uuid = {1,2};
            bus.publish(join);
            bus.publish(leave);
            require(runtime.disable(), "disable after re-enable");
            // Also exercise re-enable when registry cleanup has not run yet.
            require(runtime.enable() && bus.registrations.size() == 11, "replace rather than duplicate retained handles");
            bus.fail_remove = true;
            require(!runtime.unload(), "failed removal must deny logical unload");
            require(can_switch_journal(mod->data_dir / "bedrock_insight.sqlite3"), "failed unload must still stop active storage");
            bus.publish(join); // Unload failure must leave admission disabled.
            require(runtime.disable(), "disable after failed unload is idempotent");
            require(bus.registrations.size() == 11, "failed removal retains callback handles");
            bus.fail_remove = false;
            require(runtime.unload() && bus.registrations.empty(), "retry removes remaining handles");
        }
        // EventBus snapshots may outlive removal and Runtime. Callbacks must
        // retain disabled state, not dereference a destroyed Runtime.
        for (auto const& value : delayed_snapshot) if (value.type == typeid(ll::event::PlayerJoinEvent)) {
            std::static_pointer_cast<ll::event::Listener<ll::event::PlayerJoinEvent>>(value.listener)->callback(join);
        }
        delayed_snapshot.clear();

        sqlite_api::Library library;
        sqlite_api::sqlite3* db = nullptr;
        auto path = mod->data_dir / "bedrock_insight.sqlite3";
        require(library.open_v2(path.string().c_str(), &db, sqlite_api::kReadOnly, nullptr) == 0, "open captured data");
        require(scalar(library, db, "SELECT COUNT(*) FROM events;") == 163, "disable drains accepted events across activations");
        require(scalar(library, db, "SELECT MAX(event_id) FROM events;") == 163, "re-enable resumes durable event IDs");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='item_hurt_slot_loss' AND amount=1 AND session_id LIKE 'lltick-%';")==1,"hurt slot drains exactly once");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='item_use_result' AND amount=1 AND session_id LIKE 'lltick-%';")==1,"use result drains once; stale callbacks rejected");
        require(scalar(library, db, "SELECT COALESCE(SUM(items_used),0) FROM player_daily;")==0,"use result must not inflate legacy full-use totals");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='direct_kill_observation' AND amount=1 AND session_id LIKE 'lltick-%';")==2,"direct player/mob observations drain once; stale callbacks rejected");
        require(scalar(library, db, "SELECT COALESCE(SUM(kills),0) FROM player_daily;")==0,"scoped kill observations do not inflate legacy full-kill totals");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='player_jump_impulse' AND amount=1 AND session_id LIKE 'lltick-%';")==1,"jump impulse drains once, disabled/old generation rejected");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='item_pickup_delta' AND amount=3 AND session_id LIKE 'lltick-%';")==1,"pickup delta drains once; stale callbacks rejected");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='player_mode_observation' AND primary_type='sprint' AND secondary_type='start' AND amount=1 AND session_id LIKE 'lltick-%';") == 1,"confirmed flag reaches draining writer, notification itself is not a count");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='player_mode_observation' AND primary_type='sneak';") == 0,"cancelled sneak not observed");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.player_dead_transition.v1' AND amount=1 AND session_id LIKE 'lltick-%';") == 1,"confirmed transition only once, old generation rejected");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='item_drop_result' AND amount=64 AND primary_type='minecraft:stone' AND session_id LIKE 'lltick-%';") == 1,"drop observation follows single writer and activation identity");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.busytick.invalid.v1';") == 1, "sticky invalidation cannot be overtaken by a delayed valid sample");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.busytick.start.v1';") == 3, "busy start precedes admission each activation");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.busytick.end.v1';") == 3, "busy end drains even empty warmup activation");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.busytick.v1' AND amount=100 AND tick_mspt=0 AND details_json LIKE '%200000000%';") == 1, "busy callback copies typed processing timing into writer");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.interval.start.v1';") == 3, "every successful activation starts new telemetry provenance");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.interval.end.v1';") == 3, "disable ends even empty warmup activations");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE cause='ll26.60.interval.v1' AND amount=1 AND online_players=2 AND tick_mspt=0;") == 1, "actual adapter serializes real clock interval not MSPT");
        require(scalar(library, db, "SELECT COUNT(*) FROM server_tick_samples;") == 0, "no interval masquerades as processing time");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='player_death_notification' AND cause='ll26.60.player_die_notification.v1';") == 1, "death notification not confirmed death");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind='item_use_attempt' AND cause='ll26.60.item_use_attempt.v1';") == 2, "cancelled observation excluded, other uses are attempts only");
        require(scalar(library, db, "SELECT COALESCE(SUM(deaths),0) FROM player_daily;") == 1, "only postcondition event increments deaths, not pre-notification");
        require(scalar(library, db, "SELECT COALESCE(SUM(items_used),0) FROM player_daily;") == 0, "use attempts cannot inflate success aggregates");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE actor_kind='unknown' AND actor_id='';") == 2,
            "unavailable UUID must preserve unknown attribution");
        require(scalar(library, db, "SELECT COUNT(*) FROM sessions WHERE ended_at_ms IS NOT NULL;") == 2,
            "duplicate join and display rename preserve one session per activation");
        require(scalar(library, db, "SELECT COUNT(*) FROM players WHERE player_id='uuid64:1:2';") == 1,
            "stable identity through LL accessor");
        require(scalar(library, db, "SELECT COUNT(*) FROM events WHERE event_kind IN ('block_break','block_place');") == 0,
            "attempts cannot count completed block changes");
        require(scalar(library, db, "SELECT COUNT(*) FROM audit_events WHERE before_type='' AND after_type='';") == 130,
            "attempts cannot become reversible snapshots");
        library.close(db);
        // Only explicit files created by this test are removed.
        for (auto const& folder : {"1","2","3","4","5","6","7","8","9","10","11","capture","hook-failure","drop-hook-failure","death-hook-failure","pickup-hook-failure","jump-hook-failure","kill-hook-failure","use-hook-failure","hurt-hook-failure","block-hook-failure"}) {
            auto directory = root / folder;
            for (auto const& suffix : {"","-wal","-shm"}) {
                std::filesystem::remove(directory / (std::string("bedrock_insight.sqlite3") + suffix));
            }
            std::filesystem::remove(directory);
        }
        std::filesystem::remove(root);
        std::cout << "BedrockInsight native adapter safety tests: PASS (fake SDK; no BDS)\n";
        return 0;
    } catch (std::exception const& exception) {
        std::cerr << "BedrockInsight native adapter safety tests: FAIL: " << exception.what() << '\n';
        return 1;
    }
}
