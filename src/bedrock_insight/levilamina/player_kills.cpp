#include "bedrock_insight/levilamina/player_kills.hpp"
#include "bedrock_insight/core/direct_kill.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/state_transition.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
static_assert(static_cast<int>(SharedTypes::Legacy::ActorDamageCause::EntityAttack)==2);
static_assert(static_cast<int>(SharedTypes::Legacy::ActorDamageCause::Projectile)==3);
class KillCapture final {
public:
    explicit KillCapture(KillSink fn):sink(std::move(fn)){}
    KillSink sink;std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
KillToken current;
bool installed=false,install_failed=false;
thread_local bool inside_kill=false,nested_kill=false;
KillToken current_capture() noexcept {
    try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}
}
struct KillScope final {
    KillScope(){inside_kill=true;nested_kill=false;}
    ~KillScope(){inside_kill=false;nested_kill=false;}
};
struct KillEntry {WeakEntityRef victim;::Actor const* identity;AnalyticsEvent value;};
LL_TYPE_INSTANCE_HOOK(DirectKillHook,ll::memory::HookPriority::Lowest,Mob,
    &Mob::$die,void,ActorDamageSource const& source) {
    if(inside_kill){nested_kill=true;origin(source);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin(source);return;}
    KillScope scope;
    observe_state_transition([&]() -> std::optional<KillEntry> {
        int const cause=static_cast<int>(source.mCause);
        bool const child=source.isChildEntitySource();
        if(!state->active.load(std::memory_order_acquire) || isDead() ||
           !(cause==2 || cause==3) || !source.isEntitySource() || source.isBlockSource())return {};
        // Official const virtual declarations, not unchecked MCNAPI thunks.
        // getEntityUniqueID() is the attributable attacker/owner for both a
        // direct hit and a child projectile; getDamagingEntityUniqueID() is the
        // projectile itself in the child case, so equal ids are NOT required.
        auto const source_id=source.getEntityUniqueID(),damaging_id=source.getDamagingEntityUniqueID();
        if(source_id.rawID==0 || (!child && source_id!=damaging_id))return {};
        auto& level=getLevel();
        auto* attacker=level.fetchEntity(source_id,false);
        if(!attacker || attacker==static_cast<::Actor*>(this) || &attacker->getLevel()!=&level)return {};
        auto* player=Player::tryGetFromEntity(attacker->getEntityContext(),false);
        if(!player || static_cast<::Actor*>(player)!=attacker)return {};
        if(!credible_direct_attack({cause,true,child,false,source_id.rawID,damaging_id.rawID,true,false}))return {};
        auto const& uuid=player->getUuid();if(uuid.a==0 && uuid.b==0)return {};
        auto type=getTypeName();if(!valid_item_type(type))return {};
        auto const victim_player=Player::tryGetFromEntity(getEntityContext(),false)!=nullptr;
        if(victim_player!=(type=="minecraft:player"))return {}; // no ambiguous class/type label
        KillEntry entry{getEntityContext().getWeakRef(),static_cast<::Actor*>(this),{}};
        entry.value.kind=EventKind::DirectKillObservation;
        entry.value.actor.kind=ActorKind::Player;
        entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.value.actor.display_name=player->getRealName();
        if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
        entry.value.primary_type=std::move(type);
        entry.value.secondary_type=victim_player ? "player" : "mob";
        // Preserve v1's direct-only meaning. A projectile must never be read
        // under the older direct-source validator/label.
        entry.value.cause=child?"ll26.60.projectile_owner_dead_transition.v1":"ll26.60.direct_attack_dead_transition.v1";
        entry.value.details_json="{\"hook\":\"Mob::$die\",\"phase\":\"post_dead_transition\",\"damage_cause\":"+std::to_string(cause)
            +",\"source_entity\":true,\"source_child\":"+(child?"true":"false")
            +",\"source_block\":false,\"source_id\":"+std::to_string(source_id.rawID)
            +",\"damaging_id\":"+std::to_string(damaging_id.rawID)
            +",\"source_ids_equal\":"+(source_id==damaging_id?"true":"false")
            +",\"resolved_player\":true,\"resolved_actor\":true,\"same_level\":true,\"self\":false,\"before_dead\":false,\"after_dead\":true,\"nested\":false,\"world_death_verified\":false}";
        return entry;
    },[&]{origin(source);},[&](KillEntry& entry) {
        if(nested_kill || !state->active.load(std::memory_order_acquire))return false;
        // No damage source, attacker, or old victim read after origin.
        auto victim=entry.victim.tryUnwrap<::Actor>();
        if(!victim || victim.as_ptr()!=entry.identity)return false;
        return direct_death_transition(false,victim->isDead(),true,
            nested_kill,state->active.load(std::memory_order_acquire));
    },[&](KillEntry entry) {
        if(!state->active.load(std::memory_order_acquire))return;
        entry.value.occurred_at_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state->sink(std::move(entry.value));
    });
}
}
KillToken start_kill_capture(KillSink sink) noexcept {
    try{
        std::lock_guard lock(install_mutex);if(install_failed || current_capture())return {};
        if(!installed){if(DirectKillHook::hook()!=0){install_failed=true;return {};}installed=true;}
        auto token=std::make_shared<KillCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_kill_capture(KillToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
