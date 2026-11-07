#include "bedrock_insight/levilamina/player_item_transfer.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/item_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/world/SimpleSparseContainer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/containers/ContainerEnumName.h"
#include "mc/world/containers/models/LevelContainerModel.h"
#include "mc/world/inventory/network/ContainerScreenContext.h"
#include "mc/world/inventory/network/ItemStackNetResult.h"
#include "mc/world/inventory/network/ItemStackRequestActionHandler.h"
#include "mc/world/inventory/network/ItemStackRequestActionTransferBase.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace bedrock_insight::levilamina {
class ItemTransferCapture final {
public:
    explicit ItemTransferCapture(ItemTransferSink fn):sink(std::move(fn)){}
    ItemTransferSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
ItemTransferToken current;
bool installed=false,install_failed=false;
thread_local bool inside=false,nested=false;
ItemTransferToken current_capture() noexcept {
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
AnalyticsEvent player_event(Player const& player,EventKind kind,char const* cause) {
    AnalyticsEvent value;
    value.kind=kind;
    value.actor.kind=ActorKind::Player;
    auto const& uuid=player.getUuid();
    value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
    value.actor.display_name=player.getRealName();
    if(value.actor.display_name.size()>256)value.actor.display_name.resize(256);
    value.position={0,0,0,static_cast<std::int8_t>(static_cast<int>(player.getDimensionId()))};
    value.occurred_at_ms=now_ms();
    value.cause=cause;
    return value;
}

// ItemStackRequestActionHandler.cpp: a successful transfer out of the game's
// own CreatedOutputContainer is the committed craft/trade/enchant result.
// Handler::onPlayerCraftedItem maps the open screen type to the statistic.
LL_TYPE_INSTANCE_HOOK(CreatedOutputTransferHook,ll::memory::HookPriority::Lowest,
    ItemStackRequestActionHandler,&ItemStackRequestActionHandler::_handleTransfer,
    ::ItemStackNetResult,::ItemStackRequestActionTransferBase const& requestAction,
    bool const isSrcHintSlot,bool const isDstHintSlot,bool const isSwap) {
    if(inside){
        nested=true;
        return origin(requestAction,isSrcHintSlot,isDstHintSlot,isSwap);
    }
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire))
        return origin(requestAction,isSrcHintSlot,isDstHintSlot,isSwap);
    HookScope scope;
    bool candidate=false;
    std::string item_type;
    int amount=0;
    int screen_type=0;
    std::string_view branch{};
    try {
        auto const* src=requestAction.mSrc.operator->();
        if(src && static_cast<int>(static_cast<::ContainerEnumName>(src->mFullContainerName.mName))
                == static_cast<int>(::ContainerEnumName::CreatedOutputContainer)) {
            amount=static_cast<int>(requestAction.mAmount);
            screen_type=static_cast<int>(static_cast<::SharedTypes::Legacy::ContainerType>(
                getScreenContext().mScreenContainerType));
            branch=created_output_branch(screen_type);
            if(!branch.empty()) {
                auto container=_getOrInitSparseContainer(src->mFullContainerName);
                if(container) {
                    item_type=container->getItem(static_cast<int>(src->mSlot)).getTypeName();
                    candidate=created_output_amount_valid(amount,valid_item_type(item_type));
                }
            }
        }
    } catch (...) {candidate=false;}
    auto const result=origin(requestAction,isSrcHintSlot,isDstHintSlot,isSwap);
    if(!candidate || nested || result!=::ItemStackNetResult::Success
        || !state->active.load(std::memory_order_acquire)) return result;
    try {
        auto& player=mPlayer;
        auto const& uuid=player.getUuid();
        if(uuid.a==0 && uuid.b==0)return result;
        // Trade results and enchantments are reported separately from crafted
        // items, exactly like the upstream handler switch.
        bool const counts_as_crafted=branch=="workbench"||branch=="inventory"||branch=="trade";
        if(counts_as_crafted) {
            auto value=player_event(player,EventKind::ItemCraftResult,
                "ll26.60.created_output_transfer.v1");
            value.primary_type=item_type;
            value.secondary_type=std::string(branch);
            value.amount=amount;
            value.details_json="{\"hook\":\"ItemStackRequestActionHandler::_handleTransfer\","
                "\"phase\":\"post_origin_transfer_success\",\"item_type\":\""+item_type+"\","
                "\"transfer_amount\":"+std::to_string(amount)+",\"branch\":\""+std::string(branch)
                +"\",\"screen_container_type\":"+std::to_string(screen_type)
                +",\"net_result\":0,\"nested\":false,\"inventory_commit_verified\":true}";
            if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
        }
        if(branch=="trade") {
            auto value=player_event(player,EventKind::PlayerInteractionResult,
                "ll26.60.trade_created_output.v1");
            value.primary_type="minecraft:traded_with_villager";
            value.secondary_type="trade_output";
            value.amount=1;
            value.details_json="{\"hook\":\"ItemStackRequestActionHandler::_handleTransfer\","
                "\"phase\":\"post_origin_transfer_success\",\"custom_key\":\"minecraft:traded_with_villager\","
                "\"item_type\":\""+item_type+"\",\"transfer_amount\":"+std::to_string(amount)
                +",\"screen_container_type\":"+std::to_string(screen_type)
                +",\"villager_entity_identified\":false,\"nested\":false}";
            if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
        }
        if(branch=="enchantment") {
            auto value=player_event(player,EventKind::PlayerInteractionResult,
                "ll26.60.enchant_output_transfer.v1");
            value.primary_type="minecraft:enchant_item";
            value.secondary_type="enchantment";
            value.amount=1;
            value.details_json="{\"hook\":\"ItemStackRequestActionHandler::_handleTransfer\","
                "\"phase\":\"post_origin_transfer_success\",\"custom_key\":\"minecraft:enchant_item\","
                "\"item_type\":\""+item_type+"\",\"transfer_amount\":"+std::to_string(amount)
                +",\"screen_container_type\":"+std::to_string(screen_type)+",\"nested\":false}";
            if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
        }
    } catch (...) {}
    return result;
}

// LevelContainerModel.cpp: taking the finished product out of a furnace or
// smoker output slot. The model change already happened, so the delta between
// the two stacks IS the committed extraction; origin only syncs the UI.
LL_TYPE_INSTANCE_HOOK(FurnaceOutputHook,ll::memory::HookPriority::Lowest,LevelContainerModel,
    &LevelContainerModel::$_onItemChanged,void,int slotNumber,::ItemStack const& oldItem,
    ::ItemStack const& newItem) {
    if(inside){nested=true;origin(slotNumber,oldItem,newItem);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){origin(slotNumber,oldItem,newItem);return;}
    HookScope scope;
    try {
        // Only the base container model is classified, never a derived screen.
        if(*reinterpret_cast<void***>(this)==LevelContainerModel::$vftable()) {
            auto& player=mPlayer;
            auto const& uuid=player.getUuid();
            ::BlockPos const& pos=mBlockPos;
            if((uuid.a!=0 || uuid.b!=0) && pos.x>-30000000 && pos.x<30000000
                && pos.y>=-512 && pos.y<=512 && pos.z>-30000000 && pos.z<30000000) {
                auto block_type=player.getDimensionBlockSource().getBlock(pos).getTypeName();
                auto const slot=slotNumber+_getContainerOffset();
                auto const old_count=static_cast<int>(oldItem.mCount);
                auto const new_count=static_cast<int>(newItem.mCount);
                if(furnace_output_extract(block_type,slot,old_count,new_count)) {
                    auto const extracted=old_count-new_count;
                    auto type=oldItem.getTypeName();
                    if(extracted>0 && extracted<=255 && valid_item_type(type)) {
                        auto value=player_event(player,EventKind::ItemCraftResult,
                            "ll26.60.furnace_output_extract.v1");
                        value.primary_type=type;
                        value.secondary_type=block_type;
                        value.amount=extracted;
                        value.details_json="{\"hook\":\"LevelContainerModel::$_onItemChanged\","
                            "\"phase\":\"pre_origin_model_delta\",\"block_type\":\""+block_type+"\","
                            "\"slot\":"+std::to_string(slot)+",\"old_count\":"+std::to_string(old_count)
                            +",\"new_count\":"+std::to_string(new_count)+",\"extracted\":"
                            +std::to_string(extracted)+",\"nested\":false}";
                        if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
                    }
                }
            }
        }
    } catch (...) {}
    origin(slotNumber,oldItem,newItem);
}
}
ItemTransferToken start_item_transfer_capture(ItemTransferSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){
            if(CreatedOutputTransferHook::hook()!=0){install_failed=true;return {};}
            if(FurnaceOutputHook::hook()!=0){install_failed=true;return {};}
            installed=true;
        }
        auto token=std::make_shared<ItemTransferCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_item_transfer_capture(ItemTransferToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
