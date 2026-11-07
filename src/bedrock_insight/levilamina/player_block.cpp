#include "bedrock_insight/levilamina/player_block.hpp"
#include "bedrock_insight/core/block_record.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include <atomic>
#include <chrono>
#include <mutex>

namespace bedrock_insight::levilamina {
class BlockCapture final {
public:
    explicit BlockCapture(BlockSink fn):sink(std::move(fn)){}
    BlockSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
BlockToken current;
bool installed_mined=false,installed_placed=false,install_failed=false;
thread_local bool inside_mined=false,nested_mined=false;
thread_local bool inside_placed=false,nested_placed=false;
BlockToken current_capture() noexcept {
    try {std::lock_guard lock(current_mutex);return current;} catch (...) {return {};}
}
struct MinedScope final {
    MinedScope(){inside_mined=true;nested_mined=false;}
    ~MinedScope(){inside_mined=false;nested_mined=false;}
};
struct PlacedScope final {
    PlacedScope(){inside_placed=true;nested_placed=false;}
    ~PlacedScope(){inside_placed=false;nested_placed=false;}
};
bool plausible_pos(BlockPos const& pos) noexcept {
    // Reject corrupt/nonsensical coordinates before a block-source read.
    return pos.x>-30000000 && pos.x<30000000 && pos.y>=-512 && pos.y<=512
        && pos.z>-30000000 && pos.z<30000000;
}
int dimension_of(Player const& player) noexcept {
    return static_cast<int>(player.getDimensionId());
}
std::string block_type_at(Player const& player,BlockPos const& pos) {
    auto const& region=player.getDimensionBlockSource();
    return region.getBlock(pos).getTypeName(); // copied immediately from the palette
}
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
struct MinedEntry {
    WeakEntityRef weak;
    Player const* identity;
    Level const* level;
    BlockPos pos;
    int dimension;
    std::string before_type;
    AnalyticsEvent value;
};
struct PlacedEntry {
    WeakEntityRef weak;
    Player const* identity;
    Level const* level;
    BlockPos pos;
    int dimension;
    std::string before_type;
    int hand;
    bool sim_tick;
    AnalyticsEvent value;
};
LL_TYPE_INSTANCE_HOOK(BlockMinedHook,ll::memory::HookPriority::Lowest,GameMode,
    &GameMode::$destroyBlock,bool,::BlockPos const& pos,uchar face) {
    if(inside_mined){nested_mined=true;return origin(pos,face);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))return origin(pos,face);
    MinedScope scope;
    return observe_block_outcome([&]() -> std::optional<MinedEntry> {
        if(!state->active.load(std::memory_order_acquire) || !plausible_pos(pos))return {};
        auto& player=mPlayer; // exact pinned typed reference
        auto* checked=Player::tryGetFromEntity(player.getEntityContext(),false);
        if(checked!=&player || player.mRemoved)return {};
        auto dimension=dimension_of(player);if(dimension< -1 || dimension>127)return {};
        auto const& uuid=player.getUuid();if(uuid.a==0 && uuid.b==0)return {};
        auto before=block_type_at(player,pos);
        if(!valid_item_type(before) || before=="minecraft:air")return {};
        MinedEntry entry{player.getEntityContext().getWeakRef(),&player,&player.getLevel(),pos,dimension,
            std::move(before),{}};
        entry.value.kind=EventKind::BlockMinedResult;
        entry.value.actor.kind=ActorKind::Player;
        entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.value.actor.display_name=player.getRealName();
        if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
        entry.value.primary_type=entry.before_type;
        return entry;
    },[&]{return origin(pos,face);},[&](MinedEntry& entry) {
        if(nested_mined || !state->active.load(std::memory_order_acquire))return false;
        // Never read the old GameMode/Player after origin.
        auto player=entry.weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=entry.identity)return false;
        if(player->mRemoved || !block_frame_confirmed(false,true,&player->getLevel()==entry.level,entry.dimension,dimension_of(*player)))return false;
        std::string after;
        try {after=block_type_at(*player,entry.pos);} catch (...) {return false;}
        auto value=block_transition_record(entry.value.actor,false,entry.before_type,after,{entry.pos.x,entry.pos.y,entry.pos.z,entry.dimension},-1,false,now_ms());
        if(!value)return false;entry.value=std::move(*value);
        return true;
    },[&](MinedEntry entry) {
        if(!state->active.load(std::memory_order_acquire))return;
        state->sink(std::move(entry.value));
    });
}
LL_TYPE_INSTANCE_HOOK(BlockPlacedHook,ll::memory::HookPriority::Lowest,GameMode,
    &GameMode::$buildBlock,bool,::BlockPos const& pos,uchar face,::HandSlot handSlot,bool const isSimTick) {
    if(inside_placed){nested_placed=true;return origin(pos,face,handSlot,isSimTick);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))return origin(pos,face,handSlot,isSimTick);
    PlacedScope scope;
    return observe_block_outcome([&]() -> std::optional<PlacedEntry> {
        if(!state->active.load(std::memory_order_acquire) || !plausible_pos(pos))return {};
        if(handSlot!=HandSlot::Mainhand && handSlot!=HandSlot::Offhand)return {};
        auto& player=mPlayer;
        auto* checked=Player::tryGetFromEntity(player.getEntityContext(),false);
        if(checked!=&player || player.mRemoved)return {};
        auto dimension=dimension_of(player);if(dimension< -1 || dimension>127)return {};
        auto const& uuid=player.getUuid();if(uuid.a==0 && uuid.b==0)return {};
        auto before=block_type_at(player,pos);
        if(!valid_item_type(before))return {};
        PlacedEntry entry{player.getEntityContext().getWeakRef(),&player,&player.getLevel(),pos,dimension,
            std::move(before),static_cast<int>(handSlot),isSimTick,{}};
        entry.value.kind=EventKind::BlockPlacedResult;
        entry.value.actor.kind=ActorKind::Player;
        entry.value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
        entry.value.actor.display_name=player.getRealName();
        if(entry.value.actor.display_name.size()>256)entry.value.actor.display_name.resize(256);
        entry.value.secondary_type=handSlot==HandSlot::Mainhand?"main_hand":"off_hand";
        return entry;
    },[&]{return origin(pos,face,handSlot,isSimTick);},[&](PlacedEntry& entry) {
        if(nested_placed || !state->active.load(std::memory_order_acquire))return false;
        auto player=entry.weak.tryUnwrap<Player>();
        if(!player || player.as_ptr()!=entry.identity)return false;
        if(player->mRemoved || !block_frame_confirmed(false,true,&player->getLevel()==entry.level,entry.dimension,dimension_of(*player)))return false;
        std::string after;
        try {after=block_type_at(*player,entry.pos);} catch (...) {return false;}
        auto value=block_transition_record(entry.value.actor,true,entry.before_type,after,{entry.pos.x,entry.pos.y,entry.pos.z,entry.dimension},entry.hand,entry.sim_tick,now_ms());
        if(!value)return false;entry.value=std::move(*value);
        return true;
    },[&](PlacedEntry entry) {
        if(!state->active.load(std::memory_order_acquire))return;
        state->sink(std::move(entry.value));
    });
}
}
BlockToken start_block_capture(BlockSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed_mined){
            if(BlockMinedHook::hook()!=0){install_failed=true;return {};}
            installed_mined=true;
        }
        if(!installed_placed){
            if(BlockPlacedHook::hook()!=0){install_failed=true;return {};}
            installed_placed=true;
        }
        auto token=std::make_shared<BlockCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    } catch (...) {return {};}
}
void stop_block_capture(BlockToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();} catch (...) {}
}
}
