#pragma once

#include "bedrock_insight/core/event.hpp"
#include <memory>
#include <string>

class Player;
class Level;

namespace bedrock_insight::levilamina {

class TravelTracker;

// Poll-based movement sampler. It deliberately avoids Player::checkMovementStats
// and other symbols the pinned prelink runtime does not export: position and pose
// are read each server tick through resolvable inline/checked accessors.
using TravelToken = std::shared_ptr<TravelTracker>;

struct TravelEventSink {
    void* context;
    void (*emit)(void*, AnalyticsEvent);
};

TravelToken make_travel_tracker();

// Records the starting position for a joining player (or a re-observed one).
void track_travel(TravelTracker&, Player&, std::int64_t utc_ms, TravelEventSink);
// Flushes pending distance at its last observed movement, not disconnect time.
void forget_travel(TravelTracker&, std::string const& player_id, std::int64_t utc_ms, TravelEventSink);
// Samples every tracked player on the level; emits coalesced distance events.
void poll_travel(TravelTracker&, Level&, std::int64_t utc_ms, TravelEventSink);
// Flushes at the actual last movement timestamp (never last-flush time), then clears.
void finish_travel(TravelTracker&, TravelEventSink);

} // namespace bedrock_insight::levilamina
