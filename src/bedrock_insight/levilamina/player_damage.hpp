#pragma once

#include "bedrock_insight/core/event.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace ll::event::inline entity {
class ActorHurtEvent;
}

namespace bedrock_insight::levilamina {
using DamageSink = std::function<void(AnalyticsEvent)>;

struct PlayerIdentity final {
    std::string actor_id;
    std::string display_name;
};

// Resolves a Bedrock ActorUniqueID to a currently tracked player. Returning
// false means "unknown", in which case damage is never attributed.
using PlayerByUniqueId = std::function<bool(std::int64_t, PlayerIdentity&)>;

// Reads the real pre-origin damage amount from ActorHurtEvent.
//  - Victim is a Player -> PlayerDamageTaken.
//  - Damaging entity resolves to another tracked Player -> PlayerDamageDealt
//    with the same real amount (no attack-action substitution).
// Unresolvable sources are recorded only as un-attributed damage_taken.
void observe_hurt(
    ll::event::entity::ActorHurtEvent& event,
    PlayerByUniqueId const& lookup,
    DamageSink sink
) noexcept;
} // namespace bedrock_insight::levilamina
