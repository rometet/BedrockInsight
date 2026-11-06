#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace bedrock_insight {

// Vanilla-style movement modes. Names are the LK-Stats CUSTOM *_one_cm keys
// (without the minecraft: prefix), so a record maps 1:1 to an upstream key.
enum class TravelMode : std::uint8_t {
    Walk,
    Sprint,
    Sneak,
    Swim,
    Fly,
    Glide,
    Climb,
    Fall,
    Boat,
    Minecart,
    RidePig,
    RideStrider,
    RideHorse,
    RideNautilus,
    RideHappyGhast,
    RideOther,
    WalkOnWater,
    WalkUnderWater,
};

struct MovementFlags final {
    bool sprinting = false;
    bool sneaking = false;
    bool swimming = false;
    bool gliding = false;
    bool flying = false;
    bool in_water = false;
    bool on_ground = true;
    bool riding = false;
    // True only when the player is ascending while standing in a vanilla
    // climbable block (ladder/vine/scaffolding/...). Derived from a block query,
    // never from a guessed movement-flag bit.
    bool climbing = false;
    std::string_view vehicle_type{};
};

[[nodiscard]] inline std::string_view travel_mode_name(TravelMode mode) noexcept {
    switch (mode) {
    case TravelMode::Walk: return "walk";
    case TravelMode::Sprint: return "sprint";
    case TravelMode::Sneak: return "crouch";
    case TravelMode::Swim: return "swim";
    case TravelMode::Fly: return "fly";
    case TravelMode::Glide: return "aviate";
    case TravelMode::Climb: return "climb";
    case TravelMode::Fall: return "fall";
    case TravelMode::Boat: return "boat";
    case TravelMode::Minecart: return "minecart";
    case TravelMode::RidePig: return "pig";
    case TravelMode::RideStrider: return "strider";
    case TravelMode::RideHorse: return "horse";
    case TravelMode::RideNautilus: return "nautilus";
    case TravelMode::RideHappyGhast: return "happy_ghast";
    case TravelMode::RideOther: return "ride";
    case TravelMode::WalkOnWater: return "walk_on_water";
    case TravelMode::WalkUnderWater: return "walk_under_water";
    }
    return "walk";
}

// Only namespaced content types are accepted as vehicle identifiers.
[[nodiscard]] inline bool valid_vehicle_type(std::string_view name) noexcept {
    if (name.empty() || name.size() > 128) return false;
    auto colon = name.find(':');
    if (colon == 0 || colon == std::string_view::npos || colon + 1 == name.size()) return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (i == colon) continue;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'
              || (i > colon && c == '/'))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool contains(std::string_view haystack, std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

// Exact vanilla climbable block identifiers (1.26.60). Modded climbables are
// intentionally NOT guessed; only namespaced vanilla ladders/vines/scaffolding
// qualify so an unrelated block never turns into climb distance.
[[nodiscard]] inline bool is_climbable_block_name(std::string_view name) noexcept {
    static constexpr std::string_view kClimbable[] = {
        "minecraft:ladder",
        "minecraft:vine",
        "minecraft:scaffolding",
        "minecraft:twisting_vines",
        "minecraft:twisting_vines_plant",
        "minecraft:weeping_vines",
        "minecraft:weeping_vines_plant",
        "minecraft:cave_vines",
        "minecraft:cave_vines_plant",
        "minecraft:cave_vines_body_with_berries",
        "minecraft:cave_vines_head_with_berries",
    };
    for (auto candidate : kClimbable) {
        if (name == candidate) return true;
    }
    return false;
}

// Priority order matters: a gliding player is also flagged as flying on some
// builds, and a riding player still reports sprint/sneak input. Vehicle and
// gliding states therefore win over ground movement flags.
[[nodiscard]] inline TravelMode classify_travel(MovementFlags const& f) noexcept {
    if (f.riding) {
        auto const& v = f.vehicle_type;
        if (contains(v, "_boat") || contains(v, "_raft")) return TravelMode::Boat;
        if (contains(v, "minecart")) return TravelMode::Minecart;
        if (contains(v, "strider")) return TravelMode::RideStrider;
        if (contains(v, "happy_ghast")) return TravelMode::RideHappyGhast;
        if (contains(v, "nautilus")) return TravelMode::RideNautilus;
        if (contains(v, "pig")) return TravelMode::RidePig;
        if (contains(v, "horse") || contains(v, "donkey") || contains(v, "mule") || contains(v, "llama")
            || contains(v, "camel") || contains(v, "skeleton_horse") || contains(v, "zombie_horse")) {
            return TravelMode::RideHorse;
        }
        return TravelMode::RideOther;
    }
    if (f.gliding) return TravelMode::Glide;
    if (f.flying) return TravelMode::Fly;
    // Climbing is an explicit ascending-on-climbable state and wins over fall,
    // swim and ground modes. It never overrides a vehicle or elytra flight.
    if (f.climbing) return TravelMode::Climb;
    if (f.swimming || (f.in_water && !f.on_ground)) return TravelMode::Swim;
    if (!f.on_ground) return TravelMode::Fall;
    if (f.sneaking) return TravelMode::Sneak;
    if (f.sprinting) return TravelMode::Sprint;
    return TravelMode::Walk;
}

// Converts a per-tick movement delta into integer centimeters for the mode.
// Horizontal modes use the XZ plane. Fall/climb measure the vertical component.
// Non-finite or absurd deltas are rejected (0) instead of fabricating distance.
[[nodiscard]] inline std::int64_t travel_distance_cm(TravelMode mode, double dx, double dy, double dz) noexcept {
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return 0;
    if (std::fabs(dx) > 1024.0 || std::fabs(dy) > 1024.0 || std::fabs(dz) > 1024.0) return 0;
    double blocks = 0.0;
    switch (mode) {
    case TravelMode::Fall:
        blocks = -dy; // descending only
        if (blocks <= 0.0) return 0;
        break;
    case TravelMode::Climb:
        blocks = dy;
        if (blocks <= 0.0) return 0;
        break;
    default:
        blocks = std::sqrt(dx * dx + dz * dz);
        break;
    }
    auto cm = static_cast<std::int64_t>(std::llround(blocks * 100.0));
    if (cm < 0 || cm > 1000000) return 0;
    return cm;
}

// One position sample between two server ticks. `false` means the previous
// position is no longer a valid baseline (dimension change, respawn or a
// teleport/pearl-scale jump) and the caller must reset without counting.
[[nodiscard]] inline bool travel_step_is_continuous(double dx, double dy, double dz) noexcept {
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return false;
    constexpr double kTeleportBlocks = 64.0;
    double const dxz = std::sqrt(dx * dx + dz * dz);
    double const dxy = std::sqrt(dx * dx + dy * dy);
    double const dyz = std::sqrt(dy * dy + dz * dz);
    double const dxyz = std::sqrt(dx * dx + dy * dy + dz * dz);
    return dxz <= kTeleportBlocks && dxy <= kTeleportBlocks
        && dyz <= kTeleportBlocks && dxyz <= kTeleportBlocks;
}

} // namespace bedrock_insight
