#include "bedrock_insight/levilamina/player_block_interaction.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/interaction_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/events/MinecraftEventing.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/CakeBlock.h"
#include "mc/world/level/block/CauldronBlock.h"
#include "mc/world/level/block/FlowerPotBlock.h"
#include "mc/world/level/block/NoteBlock.h"
#include "mc/world/level/block/actor/FlowerPotBlockActor.h"
#include "mc/world/level/block/block_events/BlockPlayerInteractEvent.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace bedrock_insight::levilamina {
class BlockInteractionCapture final {
public:
    explicit BlockInteractionCapture(BlockInteractionSink fn):sink(std::move(fn)){}
    BlockInteractionSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
BlockInteractionToken current;
bool installed=false,install_failed=false;
thread_local bool inside=false,nested=false;
BlockInteractionToken current_capture() noexcept {
    try{std::lock_guard lock(current_mutex);return current;}catch(...){return {};}
}
struct HookScope final {
    HookScope(){inside=true;nested=false;}
    ~HookScope(){inside=false;nested=false;}
};
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
bool plausible_pos(BlockPos const& pos) noexcept {
    return pos.x>-30000000 && pos.x<30000000 && pos.y>=-512 && pos.y<=512
        && pos.z>-30000000 && pos.z<30000000;
}

// One queued interaction. `key` is the bare CUSTOM key (reader matches
// primary_type), `block_type` is the real namespaced block/item this action
// belongs to. Nothing is emitted unless the identity is usable.
void emit(BlockInteractionCapture& state,Player const& player,BlockPos const& pos,
    std::string_view key,std::string_view block_type,char const* cause,char const* hook,
    char const* phase,std::string extra_fields) {
    if(!state.active.load(std::memory_order_acquire))return;
    auto const& uuid=player.getUuid();
    if(uuid.a==0 && uuid.b==0)return;
    if(!interaction_key_valid(key) || !valid_item_type(block_type))return;
    AnalyticsEvent value;
    value.kind=EventKind::PlayerInteractionResult;
    value.actor.kind=ActorKind::Player;
    value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
    value.actor.display_name=player.getRealName();
    if(value.actor.display_name.size()>256)value.actor.display_name.resize(256);
    value.primary_type=std::string(key);
    value.secondary_type=std::string(block_type);
    value.position={pos.x,pos.y,pos.z,static_cast<std::int8_t>(static_cast<int>(player.getDimensionId()))};
    value.amount=1;
    value.occurred_at_ms=now_ms();
    value.cause=cause;
    value.details_json="{\"hook\":\""+std::string(hook)+"\",\"phase\":\""+phase
        +"\",\"block_type\":\""+std::string(block_type)+"\",\"custom_key\":\"minecraft:"
        +std::string(key)+"\",\"success\":true,\"nested\":false,"
        +"\"simulated_player\":false,\"interaction_success_verified\":true"+extra_fields+"}";
    if(state.active.load(std::memory_order_acquire))state.sink(std::move(value));
}

// Block.cpp: Block::use(player, pos, face, handSlot, hit) -> true, then the
// block that was used is looked up in the upstream block map.
LL_TYPE_INSTANCE_HOOK(BlockUseHook,ll::memory::HookPriority::Lowest,Block,
    &Block::use,bool,::Player& player,::BlockPos const& pos,uchar face,
    ::HandSlot handSlot,::std::optional<::Vec3> hit) {
    if(inside){nested=true;return origin(player,pos,face,handSlot,hit);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))
        return origin(player,pos,face,handSlot,hit);
    HookScope scope;
    bool const res=origin(player,pos,face,handSlot,hit);
    if(!res || nested || !state->active.load(std::memory_order_acquire))return res;
    try {
        if(player.isSimulatedPlayer() || !plausible_pos(pos))return res;
        std::string type;
        try {type=player.getDimensionBlockSource().getBlock(pos).getTypeName();}
        catch (...) {return res;}
        auto const key=block_use_key(type);
        if(key.empty())return res;
        emit(*state,player,pos,key,type,"ll26.60.block_use_success.v1",
            "Block::use","post_origin_true",std::string{});
    } catch (...) {}
    return res;
}

// CakeBlock.cpp: every call removes one slice, so the call itself is the
// success path; counted after it returns.
LL_TYPE_STATIC_HOOK(CakeRemoveSliceHook,ll::memory::HookPriority::Lowest,CakeBlock,
    &CakeBlock::removeCakeSlice,void,::Player& player,::BlockSource& region,
    ::BlockPos const& pos,::Block const* cakeBlock) {
    if(inside){nested=true;origin(player,region,pos,cakeBlock);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin(player,region,pos,cakeBlock);return;}
    HookScope scope;
    origin(player,region,pos,cakeBlock);
    if(nested || !state->active.load(std::memory_order_acquire))return;
    try {
        if(player.isSimulatedPlayer() || !plausible_pos(pos))return;
        emit(*state,player,pos,"eat_cake_slice","minecraft:cake",
            "ll26.60.cake_remove_slice.v1","CakeBlock::removeCakeSlice","post_origin_return",std::string{});
    } catch (...) {}
}

// CauldronBlock.cpp: bucketing in/out of the cauldron. The item type is copied
// before origin because _useInventory may replace the stack.
LL_TYPE_STATIC_HOOK(CauldronUseInventoryHook,ll::memory::HookPriority::Lowest,CauldronBlock,
    &CauldronBlock::_useInventory,void,::Player& player,::ItemStack& current,
    ::ItemStack& replaceWith,int useCount) {
    if(inside){nested=true;origin(player,current,replaceWith,useCount);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){
        origin(player,current,replaceWith,useCount);return;
    }
    HookScope scope;
    std::string item_type;
    try {if(current.mCount>0)item_type=current.getTypeName();} catch (...) {}
    origin(player,current,replaceWith,useCount);
    if(nested || !state->active.load(std::memory_order_acquire))return;
    try {
        if(player.isSimulatedPlayer() || !valid_item_type(item_type))return;
        auto const key=cauldron_inventory_key(item_type);
        if(key.empty())return;
        emit(*state,player,BlockPos{0,0,0},key,"minecraft:cauldron",
            "ll26.60.cauldron_use_inventory.v1","CauldronBlock::_useInventory","post_origin_return",
            ",\"item_type\":\""+item_type+"\",\"use_count\":"+std::to_string(useCount));
    } catch (...) {}
}

// CauldronBlock.cpp: washing an item in a cauldron. Upstream counts only the
// ClearItem interaction and classifies the legacy numeric item id.
LL_TYPE_STATIC_HOOK(CauldronClearItemHook,ll::memory::HookPriority::Lowest,CauldronBlock,
    &CauldronBlock::_sendCauldronUsedEventToClient,void,::Player const& player,short itemId,
    ::MinecraftEventing::POIBlockInteractionType interactionType) {
    if(inside){nested=true;origin(player,itemId,interactionType);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin(player,itemId,interactionType);return;}
    HookScope scope;
    origin(player,itemId,interactionType);
    if(nested || !state->active.load(std::memory_order_acquire))return;
    try {
        if(player.isSimulatedPlayer())return;
        if(interactionType!=::MinecraftEventing::POIBlockInteractionType::ClearItem)return;
        auto const key=cauldron_clear_key(static_cast<std::int32_t>(itemId));
        if(key.empty())return;
        emit(*state,player,BlockPos{0,0,0},key,"minecraft:cauldron",
            "ll26.60.cauldron_clear_item.v1","CauldronBlock::_sendCauldronUsedEventToClient",
            "post_origin_clear_item","");
    } catch (...) {}
}

// FlowerPotBlock.cpp: a plant was placed only when the pot was empty before the
// call, holds a plant after it, and the interaction reported success.
LL_TYPE_INSTANCE_HOOK(FlowerPotUseHook,ll::memory::HookPriority::Lowest,FlowerPotBlock,
    &FlowerPotBlock::use,void,::BlockEvents::BlockPlayerInteractEvent& eventData) {
    if(inside){nested=true;origin(eventData);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin(eventData);return;}
    HookScope scope;
    bool before=false,after=false;
    try {
        auto const* entity=eventData.getBlockSource().getBlockEntity(eventData.mPos);
        if(entity)before=static_cast<::FlowerPotBlockActor const*>(entity)->mPlant!=nullptr;
    } catch (...) {}
    origin(eventData);
    if(nested || !state->active.load(std::memory_order_acquire))return;
    try {
        auto& player=eventData.mPlayer;
        if(player.isSimulatedPlayer())return;
        auto const* entity=eventData.getBlockSource().getBlockEntity(eventData.mPos);
        if(entity)after=static_cast<::FlowerPotBlockActor const*>(entity)->mPlant!=nullptr;
        bool const successful=eventData.mSuccessful->value_or(false);
        if(!flower_pot_planted(successful,before,after))return;
        emit(*state,player,eventData.mPos,"pot_flower","minecraft:flower_pot",
            "ll26.60.flower_pot_place.v1","FlowerPotBlock::use","post_origin_success",std::string{});
    } catch (...) {}
}

// NoteBlock.cpp: NoteBlock::$attack returns true only when a note was played.
LL_TYPE_INSTANCE_HOOK(NoteBlockAttackHook,ll::memory::HookPriority::Lowest,NoteBlock,
    &NoteBlock::$attack,bool,::Player* player,::BlockPos const& pos) {
    if(inside){nested=true;return origin(player,pos);}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))return origin(player,pos);
    HookScope scope;
    bool const res=origin(player,pos);
    if(!res || nested || player==nullptr || !state->active.load(std::memory_order_acquire))return res;
    try {
        if(player->isSimulatedPlayer() || !plausible_pos(pos))return res;
        emit(*state,*player,pos,"play_noteblock","minecraft:noteblock",
            "ll26.60.noteblock_attack.v1","NoteBlock::$attack","post_origin_true",std::string{});
    } catch (...) {}
    return res;
}
}
BlockInteractionToken start_block_interaction_capture(BlockInteractionSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){
            if(BlockUseHook::hook()!=0){install_failed=true;return {};}
            if(CakeRemoveSliceHook::hook()!=0){install_failed=true;return {};}
            if(CauldronUseInventoryHook::hook()!=0){install_failed=true;return {};}
            if(CauldronClearItemHook::hook()!=0){install_failed=true;return {};}
            if(FlowerPotUseHook::hook()!=0){install_failed=true;return {};}
            if(NoteBlockAttackHook::hook()!=0){install_failed=true;return {};}
            installed=true;
        }
        auto token=std::make_shared<BlockInteractionCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    } catch (...) {return {};}
}
void stop_block_interaction_capture(BlockInteractionToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
