#include "bedrock_insight/levilamina/player_rest.hpp"
#include "bedrock_insight/core/rest_counter.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/server/ServerPlayer.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace bedrock_insight::levilamina {
class RestCapture {
public:
    explicit RestCapture(RestSink fn):sink(std::move(fn)){}
    struct Entry {WeakEntityRef weak;Player const* identity;Level const* level;RestCounter counter;};
    RestSink sink;std::atomic<bool> active{true};std::mutex mutex;
    std::unordered_map<std::string,Entry> players;
    std::thread::id owner;bool unsupported=false;
};
namespace {
std::mutex current_mutex,install_mutex;RestToken current;bool installed=false,failed=false;
RestToken capture() noexcept {try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}}
thread_local bool* outer=nullptr;
struct Scope {bool* saved;explicit Scope(bool& nested):saved(outer){outer=&nested;}~Scope(){outer=saved;}};
std::int64_t steady_us(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
LL_TYPE_INSTANCE_HOOK(RestTickHook,ll::memory::HookPriority::Lowest,ServerPlayer,&ServerPlayer::$normalTick,void) {
    if(outer){*outer=true;return origin();}
    auto state=capture();if(!state || !state->active.load(std::memory_order_acquire))return origin();
    // Weak ref is captured before origin; never dereference a destroyed this.
    std::optional<WeakEntityRef> weak;
    try{weak=getEntityContext().getWeakRef();}catch(...){}
    bool nested=false;Scope scope(nested);
    observe_rest_tick([&]{origin();},[&]{
        if(!weak || !state->active.load(std::memory_order_acquire))return;
        auto player=weak->tryUnwrap<Player>();if(!player || player.as_ptr()!=static_cast<Player*>(this))return;
        auto const& uuid=player->getUuid();if((uuid.a==0 && uuid.b==0) || player->isDead() || player->mRemoved)return;
        std::string id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        auto name=player->getRealName();if(name.size()>256)name.resize(256);
        auto us=steady_us();std::optional<AnalyticsEvent> event;
        {
            std::lock_guard lock(state->mutex);
            if(state->unsupported)return;
            if(state->owner==std::thread::id{})state->owner=std::this_thread::get_id();
            if(state->owner!=std::this_thread::get_id()){state->unsupported=true;state->players.clear();return;}
            auto it=state->players.find(id);
            if(it!=state->players.end()) {
                auto old=it->second.weak.tryUnwrap<Player>();
                if(!old || old.as_ptr()!=player.as_ptr() || it->second.level!=&player->getLevel()) {
                    state->players.erase(it);it=state->players.end();
                }
            }
            if(it==state->players.end()) {
                if(state->players.size()>=256) {
                    for(auto p=state->players.begin();p!=state->players.end();) {
                        if(us>=p->second.counter.last_us && us-p->second.counter.last_us>30000000)p=state->players.erase(p);else ++p;
                    }
                    if(state->players.size()>=256)return;
                }
                it=state->players.emplace(id,RestCapture::Entry{*weak,player.as_ptr(),&player->getLevel(),{}}).first;
            }
            auto& c=it->second.counter;
            if(nested){c={};} // no ambiguous completed tick is counted
            if(c.observe(!nested && player->isSleeping(),us)) {
                auto at=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                event=rest_snapshot({ActorKind::Player,id,std::move(name)},c,at);
            }
        }
        // No capture/state lock is held across the bounded writer submission.
        if(event && state->active.load(std::memory_order_acquire))state->sink(std::move(*event));
    });
}
}
RestToken start_rest_capture(RestSink sink) noexcept {
    try{std::lock_guard lock(install_mutex);if(failed || capture())return {};
        if(!installed){if(RestTickHook::hook()!=0){failed=true;return {};}installed=true;}
        auto token=std::make_shared<RestCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_rest_capture(RestToken const& token) noexcept {
    if(!token)return;token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
void forget_rest(RestToken const& token,std::string const& id) noexcept {
    if(!token)return;try{std::lock_guard lock(token->mutex);token->players.erase(id);}catch(...){}
}
}
