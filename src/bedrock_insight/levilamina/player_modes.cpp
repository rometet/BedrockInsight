#include "bedrock_insight/levilamina/player_modes.hpp"
#include "bedrock_insight/core/mode_observation.hpp"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include <array>
#include <unordered_map>
#include <algorithm>

namespace bedrock_insight::levilamina {
namespace {
struct Counter {std::int64_t count=0,first=0,last=0;};
struct Entry {
    WeakEntityRef weak;
    ::Actor const* identity;
    Level const* level;
    bedrock_insight::Actor actor;
    std::array<PendingMode,2> pending;
    std::array<Counter,4> counts;
    std::int64_t day=-1;
};
void flush(Entry& entry, ModeEventSink sink) {
    for(std::size_t i=0;i<entry.counts.size();++i) {
        auto const c=entry.counts[i];
        entry.counts[i]={}; // no replay after queue rejection; writer records loss
        if(!c.count)continue;
        AnalyticsEvent value;
        value.kind=EventKind::PlayerModeObservation;value.actor=entry.actor;
        value.cause="ll26.60.mode_flag_observed.v1";
        value.primary_type=i<2 ? "sprint" : "sneak";
        value.secondary_type=i%2==0 ? "start" : "stop";
        value.amount=c.count;value.occurred_at_ms=c.last;
        value.details_json="{\"phase\":\"delayed_flag_observation\",\"poll_us\":250000,\"flush_us\":5000000,\"coalesced\":true,\"duration_measured\":false,\"first_ms\":"
            +std::to_string(c.first)+",\"last_ms\":"+std::to_string(c.last)+",\"jst_day\":"+std::to_string(entry.day)+"}";
        sink.emit(sink.context,std::move(value));
    }
}
}
class ModeTracker final {
public:
    static constexpr std::size_t kCapacity=256;
    std::unordered_map<std::string,Entry> entries;
    std::int64_t last_poll=-1,last_flush=-1;
};
ModeToken make_mode_tracker() {return std::make_shared<ModeTracker>();}
void note_mode(ModeTracker& tracker, Player& player, PlayerMode mode, bool wanted, std::int64_t now) {
    auto const& uuid=player.getUuid();
    if(uuid.a==0 && uuid.b==0)return;
    auto id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
    auto it=tracker.entries.find(id);
    if(it==tracker.entries.end()) {
        if(tracker.entries.size()>=ModeTracker::kCapacity)return; // unknown, never fabricated zero
        auto name=player.getRealName();if(name.size()>256)name.resize(256);
        Entry entry{player.getEntityContext().getWeakRef(),static_cast<::Actor*>(&player),
            &player.getLevel(),{ActorKind::Player,id,std::move(name)},{},{},-1};
        it=tracker.entries.emplace(id,std::move(entry)).first;
    }
    // A different entity with the same UUID must not inherit pending observations.
    if(it->second.identity!=static_cast<::Actor*>(&player))return;
    auto const before=mode==PlayerMode::Sprint ? player.isSprinting() : player.isSneaking();
    it->second.pending[mode==PlayerMode::Sprint ? 0 : 1].note(before,wanted,now);
}
void poll_modes(ModeTracker& tracker, Level& level, std::int64_t now, std::int64_t stamp, ModeEventSink sink) {
    if(now<0)return;
    if(tracker.last_poll>=0 && now<tracker.last_poll) {
        for(auto& [id,e]:tracker.entries){(void)id;for(auto& p:e.pending)p.clear();}
        tracker.last_poll=now;tracker.last_flush=now;return;
    }
    if(tracker.last_poll>=0 && now-tracker.last_poll<PendingMode::kDelayUs)return;
    tracker.last_poll=now;
    if(tracker.last_flush<0)tracker.last_flush=now;
    auto day=jst_observation_day(stamp);if(!day)return;
    for(auto it=tracker.entries.begin();it!=tracker.entries.end();) {
        auto& e=it->second;
        if(e.day!=*day){flush(e,sink);e.day=*day;} // never put two JST days in one row
        if(e.level!=&level){++it;continue;}
        bool const sprint=e.pending[0].ready(now),sneak=e.pending[1].ready(now);
        if(sprint || sneak) {
            auto actor=e.weak.tryUnwrap<::Actor>();
            if(!actor || actor.as_ptr()!=e.identity) {
                flush(e,sink);it=tracker.entries.erase(it);continue;
            }
            for(std::size_t i=0;i<2;++i) {
                if(!(i==0 ? sprint : sneak))continue;
                bool flag=i==0 ? actor->isSprinting() : actor->isSneaking();
                if(auto observation=e.pending[i].observe(flag,now)) {
                    auto& c=e.counts[i*2+(observation->after ? 0 : 1)];
                    if(c.count<1000000) {
                        if(!c.count)c.first=stamp;
                        c.first=std::min(c.first,stamp);c.last=std::max(c.last,stamp);++c.count;
                    }
                }
            }
        }
        ++it;
    }
    if(now-tracker.last_flush>=5000000) {
        for(auto& [id,e]:tracker.entries){(void)id;flush(e,sink);}
        tracker.last_flush=now;
    }
}
void forget_modes(ModeTracker& tracker, std::string const& id, ModeEventSink sink) {
    auto it=tracker.entries.find(id);if(it==tracker.entries.end())return;
    flush(it->second,sink);tracker.entries.erase(it); // pending input is NOT resolved at disconnect
}
void finish_modes(ModeTracker& tracker, ModeEventSink sink) {
    for(auto& [id,e]:tracker.entries){(void)id;flush(e,sink);}
    tracker.entries.clear();
}
} // namespace bedrock_insight::levilamina
