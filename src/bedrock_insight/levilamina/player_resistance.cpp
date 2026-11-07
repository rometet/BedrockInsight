#include "bedrock_insight/levilamina/player_resistance.hpp"
#include "bedrock_insight/core/resistance_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/HurtParameters.h"
#include "mc/world/effect/MobEffectIds.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/Level.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class ResistanceCapture {
public:explicit ResistanceCapture(ResistanceSink fn):sink(std::move(fn)){}
    ResistanceSink sink;std::atomic<bool> active{true};
};
namespace {
static_assert(static_cast<int>(MobEffectIds::DamageResistance)==11);
static_assert(static_cast<int>(ActorHurtResult::Type::DamageProcessed)==2);
std::mutex current_mutex,install_mutex;ResistanceToken current;bool installed=false,failed=false;
ResistanceToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
struct Frame {AbsorptionCall chain;ResistanceStageGate stage;ResistanceOutcome outcome;};
thread_local Frame* frame=nullptr;
struct RootScope {Frame* saved;explicit RootScope(Frame& f):saved(frame){frame=&f;}~RootScope(){frame=saved;}};
struct ParentScope {AbsorptionCall& chain;explicit ParentScope(AbsorptionCall& c):chain(c){c.in_parent=true;}~ParentScope(){chain.in_parent=false;}};
struct StageScope {ResistanceStageGate& gate;~StageScope(){gate.active=false;}};
Actor identity(Player& p){auto const& u=p.getUuid();if(u.a==0 && u.b==0)return {};
    auto n=p.getRealName();if(n.size()>256)n.resize(256);
    return {ActorKind::Player,"uuid64:"+std::to_string(u.a)+":"+std::to_string(u.b),std::move(n)};}
struct Entry {
    WeakEntityRef weak;::Actor const* actor;Level const* level;int dimension;
    std::string type;Actor victim,attacker;WeakEntityRef attacker_weak;Player const* attacker_actor=nullptr;
    DamageAttributionFacts facts;
};
std::optional<Entry> prepare(::Actor& actor,ActorDamageSource const& source) {
    if(actor.isDead() || actor.mRemoved)return {};
    auto type=actor.getTypeName();if(!valid_item_type(type))return {};
    Entry e{actor.getEntityContext().getWeakRef(),&actor,&actor.getLevel(),static_cast<int>(actor.getDimensionId()),
        std::move(type),{},{},{},nullptr,{}};
    if(auto p=Player::tryGetFromEntity(actor.getEntityContext(),false))e.victim=identity(*p);
    try {
        auto& f=e.facts;f.cause=static_cast<int>(source.mCause);if(f.cause< -1 || f.cause>34)f.cause=-1;
        f.entity=source.isEntitySource();f.child=source.isChildEntitySource();f.block=source.isBlockSource();
        if(f.entity && !f.block && (f.cause==2 || f.cause==3)) {
            auto uid=source.getEntityUniqueID();f.source_id=uid.rawID;f.damaging_id=source.getDamagingEntityUniqueID().rawID;
            auto* target=f.source_id ? actor.getLevel().fetchEntity(uid,false) : nullptr;
            f.resolved=target!=nullptr;f.same_level=target && &target->getLevel()==e.level;f.self=target==&actor;
            if(!resolved_damage_path(f).empty())if(auto p=Player::tryGetFromEntity(target->getEntityContext(),false)) {
                if(!p->mRemoved && !p->isDead()){e.attacker=identity(*p);e.attacker_weak=p->getEntityContext().getWeakRef();e.attacker_actor=p;}
            }
        }
    }catch(...){e.attacker={};}
    if(e.victim.id.empty() && e.attacker.id.empty())return {};return e;
}
struct EffectState {int amplifier,duration;};
std::optional<EffectState> effect_of(::Actor const& actor) {
    auto* effect=actor.getEffect(static_cast<uint>(MobEffectIds::DamageResistance));
    if(!effect || static_cast<int>(effect->mId)!=11 || effect->mAmplifier<0 || effect->mAmplifier>255
        || effect->mDuration->mValue<=0)return {};
    return EffectState{effect->mAmplifier,effect->mDuration->mValue};
}
bool same_effect(::Actor const& actor,ResistanceOutcome const& o) {
    auto effect=effect_of(actor);return effect && effect->amplifier==o.amplifier && effect->duration==o.duration;
}
template<class Origin>
ActorHurtResult run(::Actor& self,AbsorptionHook hook,ActorDamageSource const& source,float damage,Origin origin) {
    if(frame) {
        if(frame->stage.active){frame->chain.ambiguous=true;return origin();}
        if(frame->stage.source!=&source)frame->chain.ambiguous=true;
        if(frame->chain.admit_parent(&self,hook)){ParentScope scope(frame->chain);return origin();}
        return origin();
    }
    auto state=capture();if(!state || !state->active.load(std::memory_order_acquire))return origin();
    Frame context;context.chain={&self,hook};context.stage.actor=&self;context.stage.source=&source;
    context.outcome.hook=hook;auto input=absorption_milli(damage);if(input)context.outcome.input=*input;
    RootScope scope(context);
    return observe_resistance([&]{return prepare(self,source);},origin,[&](Entry& e,ActorHurtResult const& result){
        if(context.chain.ambiguous || context.stage.ambiguous || context.stage.active || !state->active.load(std::memory_order_acquire))return;
        auto actor=e.weak.tryUnwrap<::Actor>();
        if(!actor || actor.as_ptr()!=e.actor || actor->mRemoved || &actor->getLevel()!=e.level
            || static_cast<int>(actor->getDimensionId())!=e.dimension)return;
        auto o=context.outcome;if(!same_effect(*actor,o))return;
        auto const& returned=*result.mDamageAmount;if(!returned)return;
        auto amount=absorption_milli(*returned);if(!amount)return;
        o.returned=*amount;o.result_type=static_cast<int>(result.mType);o.parent_calls=context.chain.parent_calls;
        o.stage_calls=context.stage.calls;o.same_actor=true;o.same_level=true;o.same_effect=true;
        auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto emit=[&](AnalyticsEvent v){if(state->active.load(std::memory_order_acquire))state->sink(std::move(v));};
        if(auto v=resistance_event(e.victim,e.type,false,o,e.facts,at))try{emit(std::move(*v));}catch(...){}
        if(!e.attacker.id.empty()) {
            auto p=e.attacker_weak.tryUnwrap<Player>();
            if(p && p.as_ptr()==e.attacker_actor && !p->mRemoved && !p->isDead() && &p->getLevel()==e.level)
                if(auto v=resistance_event(e.attacker,e.type,true,o,e.facts,at))emit(std::move(*v));
        }
    });
}
LL_TYPE_INSTANCE_HOOK(PlayerResistanceHook,ll::memory::HookPriority::Normal,Player,&Player::$_hurt,ActorHurtResult,
    ActorDamageSource const& source,float damage,HurtParameters const& params) {
    return run(*this,AbsorptionHook::Player,source,damage,[&]{return origin(source,damage,params);});
}
LL_TYPE_INSTANCE_HOOK(MobResistanceHook,ll::memory::HookPriority::Normal,Mob,&Mob::$_hurt,ActorHurtResult,
    ActorDamageSource const& source,float damage,HurtParameters const& params) {
    return run(*this,AbsorptionHook::Mob,source,damage,[&]{return origin(source,damage,params);});
}
struct StageEntry {WeakEntityRef weak;::Actor const* actor;Level const* level;int dimension;EffectState effect;std::int64_t input;};
LL_TYPE_INSTANCE_HOOK(ResistanceBasisHook,ll::memory::HookPriority::Lowest,Mob,&Mob::$getDamageAfterEnchantReduction,float,
    ActorDamageSource const& source,float damage) {
    auto* context=frame;if(!context)return origin(source,damage);
    auto const* actor=static_cast<::Actor const*>(this);
    if(!context->stage.enter(actor,&source))return origin(source,damage);
    StageScope stage{context->stage};
    return observe_resistance([&]()->std::optional<StageEntry>{
        if(context->chain.ambiguous || !source.isReducedByResistanceEffect())return {};
        auto input=absorption_milli(damage);auto effect=effect_of(*actor);
        if(!input || *input<=0 || !effect || actor->mRemoved || actor->isDead())return {};
        return StageEntry{actor->getEntityContext().getWeakRef(),actor,&actor->getLevel(),
            static_cast<int>(actor->getDimensionId()),*effect,*input};
    },[&]{return origin(source,damage);},[&](StageEntry& e,float result){
        auto resolved=e.weak.tryUnwrap<::Actor>();auto basis=absorption_milli(result);
        if(!resolved || resolved.as_ptr()!=e.actor || resolved->mRemoved || &resolved->getLevel()!=e.level
            || static_cast<int>(resolved->getDimensionId())!=e.dimension || !basis)return;
        auto& o=context->outcome;o.stage_input=e.input;o.basis=*basis;o.amplifier=e.effect.amplifier;o.duration=e.effect.duration;
        if(!same_effect(*resolved,o))return;
        o.same_source=true;
    });
}
}
ResistanceToken start_resistance_capture(ResistanceSink sink) noexcept {
    try{std::lock_guard lock(install_mutex);if(failed || capture())return {};
        if(!installed){if(PlayerResistanceHook::hook()!=0 || MobResistanceHook::hook()!=0 || ResistanceBasisHook::hook()!=0){failed=true;return {};}installed=true;}
        auto token=std::make_shared<ResistanceCapture>(std::move(sink));{std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_resistance_capture(ResistanceToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
}
