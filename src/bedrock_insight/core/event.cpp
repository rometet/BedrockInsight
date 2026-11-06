#include "bedrock_insight/core/event.hpp"

namespace bedrock_insight {

std::string_view to_string(EventKind kind) noexcept {
    switch (kind) {
    case EventKind::SessionStart: return "session_start";
    case EventKind::SessionEnd: return "session_end";
    case EventKind::BlockBreak: return "block_break";
    case EventKind::BlockPlace: return "block_place";
    case EventKind::ItemCraft: return "item_craft";
    case EventKind::ItemUse: return "item_use";
    case EventKind::EntityKill: return "entity_kill";
    case EventKind::PlayerDeath: return "player_death";
    case EventKind::Distance: return "distance";
    case EventKind::ServerTick: return "server_tick";
    case EventKind::AuditBlockChange: return "audit_block_change";
    case EventKind::AuditContainerChange: return "audit_container_change";
    case EventKind::AuditFluidChange: return "audit_fluid_change";
    case EventKind::AuditPiston: return "audit_piston";
    case EventKind::AuditExplosion: return "audit_explosion";
    case EventKind::AuditEntityChange: return "audit_entity_change";
    case EventKind::PlayerDeathNotification: return "player_death_notification";
    case EventKind::ItemUseAttempt: return "item_use_attempt";
    case EventKind::ItemDropResult: return "item_drop_result";
    case EventKind::PlayerModeObservation: return "player_mode_observation";
    case EventKind::ItemPickupDelta: return "item_pickup_delta";
    case EventKind::PlayerJumpImpulse: return "player_jump_impulse";
    case EventKind::DirectKillObservation: return "direct_kill_observation";
    case EventKind::ItemUseResult: return "item_use_result";
    case EventKind::ItemHurtSlotLoss: return "item_hurt_slot_loss";
    case EventKind::BlockMinedResult: return "block_mined_result";
    case EventKind::BlockPlacedResult: return "block_placed_result";
    case EventKind::ItemCraftResult: return "item_craft_result";
    case EventKind::PlayerTravelDistance: return "player_travel_distance";
    case EventKind::PlayerInteractionResult: return "player_interaction_result";
    case EventKind::PlayerDamageTaken: return "player_damage_taken";
    case EventKind::PlayerDamageDealt: return "player_damage_dealt";
    case EventKind::PlayerSleepResult: return "player_sleep_result";
    case EventKind::PlayerSneakTime: return "player_sneak_time";
    case EventKind::ItemBrokenResult: return "item_broken_result";
    case EventKind::PlayerKilledByEntity: return "player_killed_by_entity";
    }
    return "unknown";
}

std::string_view to_string(ActorKind kind) noexcept {
    switch (kind) {
    case ActorKind::Player: return "player";
    case ActorKind::Console: return "console";
    case ActorKind::System: return "system";
    case ActorKind::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace bedrock_insight
