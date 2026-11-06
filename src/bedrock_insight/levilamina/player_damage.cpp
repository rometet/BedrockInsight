#include "bedrock_insight/levilamina/player_damage.hpp"
#include "bedrock_insight/core/damage_observation.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/player/Player.h"
#include <chrono>
#include <string>

namespace bedrock_insight::levilamina {
namespace {
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
void fill_player(AnalyticsEvent& value, Player& player) {
    auto const& uuid = player.getUuid();
    if (uuid.a == 0 && uuid.b == 0) return;
    value.actor.kind = ActorKind::Player;
    value.actor.id = "uuid64:" + std::to_string(uuid.a) + ":" + std::to_string(uuid.b);
    value.actor.display_name = player.getRealName();
    if (value.actor.display_name.size() > 256) value.actor.display_name.resize(256);
}
} // namespace

void observe_hurt(
    ll::event::entity::ActorHurtEvent& event,
    PlayerByUniqueId const& lookup,
    DamageSink sink
) noexcept {
    if (!sink || event.isCancelled()) return;
    try {
        auto const tenths = damage_tenths(event.damage());
        if (tenths <= 0) return;
        auto& victim = event.self();
        auto const& source = event.source();
        auto const cause = normalized_damage_cause(static_cast<int>(source.mCause));
        auto const victim_uid = victim.mLegacyUniqueID->rawID;
        auto* victim_player = Player::tryGetFromEntity(victim.getEntityContext(), false);

        // Real damage dealt: a tracked player is the damaging entity and is not
        // the victim. Amount is the same pre-origin float as damage_taken.
        if (lookup && source.isEntitySource()) {
            auto const attacker_uid = source.getDamagingEntityUniqueID().rawID;
            PlayerIdentity attacker;
            if (attacker_uid != victim_uid && lookup(attacker_uid, attacker) && !attacker.actor_id.empty()) {
                AnalyticsEvent value;
                value.kind = EventKind::PlayerDamageDealt;
                value.actor.kind = ActorKind::Player;
                value.actor.id = attacker.actor_id;
                value.actor.display_name = attacker.display_name;
                value.primary_type = victim.getTypeName();
                value.secondary_type = "cause:" + std::to_string(cause);
                value.amount = tenths;
                value.occurred_at_ms = now_ms();
                value.position = {0, 0, 0,
                    static_cast<std::int8_t>(static_cast<int>(victim.getDimensionId()))};
                value.cause = "ll26.60.actor_hurt_real_damage.v1";
                value.details_json = "{\"adapter\":\"ActorHurtEvent\",\"phase\":\"pre_origin_amount\","
                    "\"cause_code\":" + std::to_string(cause) + ",\"units\":\"tenths\","
                    "\"attribution\":\"damaging_entity_unique_id\",\"attack_action_substituted\":false,"
                    "\"absorbed_or_resisted_measured\":false}";
                sink(std::move(value));
            }
        }

        if (victim_player == nullptr) return;
        AnalyticsEvent value;
        value.kind = EventKind::PlayerDamageTaken;
        fill_player(value, *victim_player);
        if (value.actor.id.empty()) return;
        value.occurred_at_ms = now_ms();
        value.primary_type = std::string("cause:") + std::to_string(cause);
        value.secondary_type = source.isEntitySource() ? "entity" : "environment";
        value.amount = tenths;
        value.position = {0, 0, 0,
            static_cast<std::int8_t>(static_cast<int>(victim_player->getDimensionId()))};
        value.cause = "ll26.60.actor_hurt_event.v1";
        value.details_json = "{\"adapter\":\"ActorHurtEvent\",\"phase\":\"pre_origin_amount\",\"cause_code\":"
            + std::to_string(cause) + ",\"entity_source\":" + (source.isEntitySource() ? "true" : "false")
            + ",\"damaging_entity_type\":" + std::to_string(static_cast<int>(source.getDamagingEntityType()))
            + ",\"units\":\"tenths\",\"absorbed_or_resisted_measured\":false}";
        sink(std::move(value));
    } catch (...) {}
}
} // namespace bedrock_insight::levilamina
