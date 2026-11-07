#include "bedrock_insight/levilamina/player_shield_evidence.hpp"
#include "bedrock_insight/core/shield_evidence.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/ActorFlags.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/ActorHurtResult.h"
#include "mc/world/actor/HurtParameters.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>

namespace bedrock_insight::levilamina {
class ShieldEvidenceCapture {
public:
    ShieldEvidenceCapture(ShieldEvidenceSink fn,bool enabled):sink(std::move(fn)),active(enabled){}
    ShieldEvidenceSink sink;std::atomic<bool> active;std::mutex mutex;ShieldEvidenceRate rate;
};
namespace {
static_assert(static_cast<int>(ActorFlags::BlockedUsingShield)==74);
std::mutex current_mutex,install_mutex;ShieldEvidenceToken current;bool installed=false,failed=false;
ShieldEvidenceToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
thread_local bool* outer=nullptr;
struct Scope {bool* saved;explicit Scope(bool& nested):saved(outer){outer=&nested;}~Scope(){outer=saved;}};
ShieldFields fields(Player const& p) {
    return {p.isBlocking(),p.getStatusFlag(ActorFlags::BlockedUsingShield),p.mPrevBlockedUsingShield,
        p.mPrevBlockedUsingDamagedShield,p.mInvulnerable,p.isDead(),p.mBlockedUsingShieldTimeStamp,
        p.mBlockedUsingDamagedShieldTimeStamp,p.mInvulnerableTime};
}
struct Entry {WeakEntityRef weak;Player const* identity;Level const* level;int dimension;Actor actor;ShieldEvidence evidence;};
LL_TYPE_INSTANCE_HOOK(ShieldEvidenceHook,ll::memory::HookPriority::Normal,Player,&Player::$_hurt,ActorHurtResult,
    ActorDamageSource const& source,float damage,HurtParameters const& params) {
    if(outer){*outer=true;return origin(source,damage,params);}
    auto state=capture();if(!state || !state->active.load(std::memory_order_acquire))return origin(source,damage,params);
    bool nested=false;Scope scope(nested);
    return observe_shield_evidence([&]()->std::optional<Entry>{
        if(mRemoved)return {};auto const& uuid=getUuid();if(uuid.a==0 && uuid.b==0)return {};
        std::string id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        {std::lock_guard lock(state->mutex);if(!state->rate.admit(id,us))return {};}
        auto name=getRealName();if(name.size()>256)name.resize(256);
        ShieldEvidence e;e.before=fields(*this);e.input=absorption_milli(damage);
        e.may_block=isDamageBlocked(source);e.invulnerable_to_source=isInvulnerableTo(source);e.cause=static_cast<int>(source.mCause);
        return Entry{getEntityContext().getWeakRef(),this,&getLevel(),static_cast<int>(getDimensionId()),
            {ActorKind::Player,std::move(id),std::move(name)},e};
    },[&]{return origin(source,damage,params);},[&](Entry& entry,ActorHurtResult const& result){
        if(!state->active.load(std::memory_order_acquire))return;
        auto p=entry.weak.tryUnwrap<Player>();if(!p || p.as_ptr()!=entry.identity || p->mRemoved
            || &p->getLevel()!=entry.level || static_cast<int>(p->getDimensionId())!=entry.dimension)return;
        auto e=entry.evidence;e.after=fields(*p);e.result_type=static_cast<int>(result.mType);e.ambiguous=nested;
        auto const& returned=*result.mDamageAmount;e.returned_present=returned.has_value();
        if(returned)e.returned=absorption_milli(*returned);
        auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if(auto v=shield_evidence_event(entry.actor,e,at))if(state->active.load(std::memory_order_acquire))state->sink(std::move(*v));
    });
}
}
ShieldEvidenceToken start_shield_evidence(ShieldEvidenceSink sink) noexcept {
    try{std::lock_guard lock(install_mutex);if(capture())return {};
        bool enabled=shield_evidence_opt_in(std::getenv("BEDROCKINSIGHT_SHIELD_EVIDENCE"));
        if(enabled){if(failed)return {};if(!installed){if(ShieldEvidenceHook::hook()!=0){failed=true;return {};}installed=true;}}
        auto token=std::make_shared<ShieldEvidenceCapture>(std::move(sink),enabled);
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_shield_evidence(ShieldEvidenceToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
bool shield_evidence_enabled(ShieldEvidenceToken const& token) noexcept {return token && token->active.load(std::memory_order_acquire);}
}
