#include "bedrock_insight/levilamina/player_absorption.hpp"
#include "bedrock_insight/core/absorption_observation.hpp"
#include "bedrock_insight/core/health_loss_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/HurtParameters.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class AbsorptionCapture {
public:
    explicit AbsorptionCapture(AbsorptionSink fn):sink(std::move(fn)){}
    AbsorptionSink sink;std::atomic<bool> active{true};
};
namespace {
static_assert(static_cast<int>(ActorHurtResult::Type::DamageProcessed)==2);
std::mutex current_mutex,install_mutex;AbsorptionToken current;bool installed=false,failed=false;
AbsorptionToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
thread_local AbsorptionCall* call=nullptr;
struct RootScope {AbsorptionCall* saved;explicit RootScope(AbsorptionCall& c):saved(call){call=&c;}~RootScope(){call=saved;}};
struct ParentScope {AbsorptionCall& c;explicit ParentScope(AbsorptionCall& v):c(v){c.in_parent=true;}~ParentScope(){c.in_parent=false;}};
Actor player_identity(Player& player) {
    auto const& u=player.getUuid();if(u.a==0 && u.b==0)return {};
    auto name=player.getRealName();if(name.size()>256)name.resize(256);
    return {ActorKind::Player,"uuid64:"+std::to_string(u.a)+":"+std::to_string(u.b),std::move(name)};
}
struct Entry {
    WeakEntityRef weak;::Actor const* identity;Level const* level;int dimension;
    AttributeInstance const* attribute;std::int64_t before,input;
    std::string type;Actor victim,attacker;WeakEntityRef attacker_weak;Player const* attacker_identity=nullptr;
    DamageAttributionFacts facts;
    AttributeInstance const* health_attribute=nullptr;
    std::optional<std::int64_t> health_before;
};
std::optional<Entry> prepare(::Actor& victim,float damage,ActorDamageSource const& source) {
    auto input=absorption_milli(damage);if(!input || *input<=0 || victim.isDead() || victim.mRemoved)return {};
    auto const& definition=SharedAttributes::ABSORPTION();auto attribute=victim.getAttribute(definition);
    auto* p=attribute.mPtr;
    auto before=p && p->mAttribute==&definition ? absorption_milli(p->mCurrentValue) : std::nullopt;
    auto const& health_definition=SharedAttributes::HEALTH();
    auto health=victim.getAttribute(health_definition);
    auto* hp=health.mPtr;
    auto health_before=hp && hp->mAttribute==&health_definition ? absorption_milli(hp->mCurrentValue) : std::nullopt;
    if((!before || *before<=0) && (!health_before || *health_before<=0))return {};
    auto type=victim.getTypeName();if(!valid_item_type(type))return {};
    Entry e{victim.getEntityContext().getWeakRef(),&victim,&victim.getLevel(),static_cast<int>(victim.getDimensionId()),
        p,before.value_or(0),*input,std::move(type),{},{},{},nullptr,{},hp,health_before};
    if(auto player=Player::tryGetFromEntity(victim.getEntityContext(),false))e.victim=player_identity(*player);
    try {
        auto& f=e.facts;f.cause=static_cast<int>(source.mCause);if(f.cause< -1 || f.cause>34)f.cause=-1;
        f.entity=source.isEntitySource();f.child=source.isChildEntitySource();f.block=source.isBlockSource();
        if(f.entity && !f.block && (f.cause==2 || f.cause==3)) {
            auto uid=source.getEntityUniqueID();f.source_id=uid.rawID;f.damaging_id=source.getDamagingEntityUniqueID().rawID;
            auto* actor=f.source_id ? victim.getLevel().fetchEntity(uid,false) : nullptr;
            f.resolved=actor!=nullptr;f.same_level=actor && &actor->getLevel()==e.level;f.self=actor==&victim;
            if(!resolved_damage_path(f).empty())if(auto player=Player::tryGetFromEntity(actor->getEntityContext(),false)) {
                if(!player->isDead() && !player->mRemoved) {
                    e.attacker=player_identity(*player);e.attacker_weak=player->getEntityContext().getWeakRef();e.attacker_identity=player;
                }
            }
        }
    }catch(...){e.attacker={};}
    if(e.victim.id.empty() && e.attacker.id.empty())return {};
    return e;
}
template<class Origin>
ActorHurtResult run(::Actor& self,AbsorptionHook hook,ActorDamageSource const& source,float damage,Origin origin) {
    if(call) {
        if(call->admit_parent(&self,hook)){ParentScope parent(*call);return origin();}
        return origin();
    }
    auto state=capture();if(!state || !state->active.load(std::memory_order_acquire))return origin();
    AbsorptionCall context{&self,hook};RootScope scope(context);
    return observe_absorption([&]{return prepare(self,damage,source);},origin,
        [&](Entry& e,ActorHurtResult const& result,auto& emit) {
            if(context.ambiguous || !state->active.load(std::memory_order_acquire))return;
            auto actor=e.weak.tryUnwrap<::Actor>();
            if(!actor || actor.as_ptr()!=e.identity || actor->mRemoved || &actor->getLevel()!=e.level
                || static_cast<int>(actor->getDimensionId())!=e.dimension)return;
            auto const& returned=*result.mDamageAmount;if(!returned)return;
            auto returned_milli=absorption_milli(*returned);if(!returned_milli)return;
            auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            // Resolve the credited attacker again after origin. No source/old
            // Actor pointer dereference after it may have been destroyed.
            bool attacker_valid=false;
            if(!e.attacker.id.empty()) {
                auto attacker=e.attacker_weak.tryUnwrap<Player>();
                attacker_valid=attacker && attacker.as_ptr()==e.attacker_identity && !attacker->mRemoved
                    && !attacker->isDead() && &attacker->getLevel()==e.level;
            }
            auto sample=[&](auto const& definition,AttributeInstance const* expected,std::optional<std::int64_t> before,
                auto builder) {
                if(!expected || !before || *before<=0)return;
                auto attribute=actor->getAttribute(definition);auto* p=attribute.mPtr;
                if(!p || p!=expected || p->mAttribute!=&definition)return;
                auto after=absorption_milli(p->mCurrentValue);if(!after)return;
                AbsorptionOutcome o{hook,*before,*after,e.input,*returned_milli,static_cast<int>(result.mType),context.parent_calls,
                    true,true,true,context.ambiguous};
                if(auto v=builder(e.victim,e.type,false,o,e.facts,at))try{emit(std::move(*v));}catch(...){}
                if(attacker_valid)if(auto v=builder(e.attacker,e.type,true,o,e.facts,at))try{emit(std::move(*v));}catch(...){}
            };
            // Independent attribute reads/events, not one combined damage sum.
            // Queue admission for victim/attacker/absorption/health is non-atomic.
            sample(SharedAttributes::ABSORPTION(),e.attribute,e.before,absorption_event);
            sample(SharedAttributes::HEALTH(),e.health_attribute,e.health_before,health_loss_event);
        },[&](AnalyticsEvent v){if(state->active.load(std::memory_order_acquire))state->sink(std::move(v));});
}
LL_TYPE_INSTANCE_HOOK(PlayerAbsorptionHook,ll::memory::HookPriority::Lowest,Player,&Player::$_hurt,ActorHurtResult,
    ActorDamageSource const& source,float damage,HurtParameters const& params) {
    return run(*this,AbsorptionHook::Player,source,damage,[&]{return origin(source,damage,params);});
}
LL_TYPE_INSTANCE_HOOK(MobAbsorptionHook,ll::memory::HookPriority::Lowest,Mob,&Mob::$_hurt,ActorHurtResult,
    ActorDamageSource const& source,float damage,HurtParameters const& params) {
    return run(*this,AbsorptionHook::Mob,source,damage,[&]{return origin(source,damage,params);});
}
}
AbsorptionToken start_absorption_capture(AbsorptionSink sink) noexcept {
    try{std::lock_guard lock(install_mutex);if(failed || capture())return {};
        if(!installed){if(PlayerAbsorptionHook::hook()!=0 || MobAbsorptionHook::hook()!=0){failed=true;return {};}installed=true;}
        auto token=std::make_shared<AbsorptionCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_absorption_capture(AbsorptionToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
