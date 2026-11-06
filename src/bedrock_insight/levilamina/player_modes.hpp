#pragma once
#include "bedrock_insight/core/event.hpp"
#include <memory>

class Player;
class Level;
namespace bedrock_insight::levilamina {
enum class PlayerMode {Sprint, Sneak};
class ModeTracker;
using ModeToken=std::shared_ptr<ModeTracker>;
struct ModeEventSink {
    void* context;
    void (*emit)(void*,AnalyticsEvent);
};
// Caller serializes on Capture::mutex. Sink is synchronous and never retained;
// no mutex is reacquired in the sink, no delayed raw Capture/MC pointer task.
ModeToken make_mode_tracker();
void note_mode(ModeTracker&, Player&, PlayerMode, bool wanted, std::int64_t now_us);
void poll_modes(ModeTracker&, Level&, std::int64_t now_us, std::int64_t utc_ms, ModeEventSink);
void forget_modes(ModeTracker&, std::string const& player_id, ModeEventSink);
void finish_modes(ModeTracker&, ModeEventSink);
}
