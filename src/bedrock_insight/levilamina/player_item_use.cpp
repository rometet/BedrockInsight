#include "bedrock_insight/levilamina/player_item_use.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include "bedrock_insight/core/item_observation.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/ItemUseMethod.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace bedrock_insight::levilamina {
class ItemUseCapture final {
public:
    explicit ItemUseCapture(ItemUseSink fn):sink(std::move(fn)){}
    ItemUseSink sink;
    std::atomic<bool> active{true};
};
namespace {
std::mutex current_mutex,install_mutex;
ItemUseToken current;
bool installed=false,install_failed=false;
thread_local bool inside=false,nested=false;
ItemUseToken current_capture() noexcept {
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

AnalyticsEvent base_event(Player const& player,char const* cause) {
    AnalyticsEvent value;
    value.actor.kind=ActorKind::Player;
    auto const& uuid=player.getUuid();
    value.actor.id="uuid64:"+std::to_string(uuid.a)+":"+std::to_string(uuid.b);
    value.actor.display_name=player.getRealName();
    if(value.actor.display_name.size()>256)value.actor.display_name.resize(256);
    value.position={0,0,0,static_cast<std::int8_t>(static_cast<int>(player.getDimensionId()))};
    value.amount=1;
    value.occurred_at_ms=now_ms();
    value.cause=cause;
    return value;
}

// PlayerUseItem.cpp: Player::$useItem(instance, itemUseMethod, consumeItem).
// Upstream evaluates the arguments before calling origin; this port calls
// origin first and then applies the same switch, so a use that never completed
// cannot be counted.
LL_TYPE_INSTANCE_HOOK(PlayerUseItemHook,ll::memory::HookPriority::Lowest,Player,
    &Player::$useItem,void,::ItemStackBase& instance,::ItemUseMethod itemUseMethod,
    bool consumeItem) {
    if(inside){nested=true;origin(instance,itemUseMethod,consumeItem);return;}
    auto state=current_capture();
    if(!state || !state->active.load(std::memory_order_acquire)){
        origin(instance,itemUseMethod,consumeItem);return;
    }
    HookScope scope;
    // Snapshot the type and the music-disc flag before origin: a consuming use
    // may shrink or clear the stack.
    std::string type;
    bool music_disc=false,type_ok=false;
    try {
        if(consumeItem) {
            type=instance.getTypeName();
            type_ok=valid_item_type(type);
            auto* item=instance.mItem.get();
            if(item)music_disc=item->isMusicDisk();
        }
    } catch (...) {}
    origin(instance,itemUseMethod,consumeItem);
    if(nested || !type_ok || !state->active.load(std::memory_order_acquire))return;
    try {
        auto& player=*this;
        auto const& uuid=player.getUuid();
        if(uuid.a==0 && uuid.b==0)return;
        auto const method=static_cast<int>(itemUseMethod);
        if(use_counts_as_used(method,type)) {
            auto value=base_event(player,"ll26.60.player_use_item_consumed.v1");
            value.kind=EventKind::ItemUseResult;
            value.primary_type=type;
            value.secondary_type=std::string(item_use_method_name(method));
            value.details_json="{\"hook\":\"Player::$useItem\",\"phase\":\"post_origin_consumed\","
                "\"item_type\":\""+type+"\",\"use_method\":"+std::to_string(method)
                +",\"use_method_name\":\""+std::string(item_use_method_name(method))+"\","
                "\"consume_item\":true,\"nested\":false}";
            if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
        }
        // A placed music disc also plays a record in the jukebox.
        if(music_disc && use_place_plays_record(method)) {
            auto value=base_event(player,"ll26.60.play_record_place.v1");
            value.kind=EventKind::PlayerInteractionResult;
            value.primary_type="minecraft:play_record";
            value.secondary_type=type;
            value.details_json="{\"hook\":\"Player::$useItem\",\"phase\":\"post_origin_place\","
                "\"custom_key\":\"minecraft:play_record\",\"item_type\":\""+type+"\","
                "\"use_method\":"+std::to_string(method)+",\"music_disc\":true,\"nested\":false}";
            if(state->active.load(std::memory_order_acquire))state->sink(std::move(value));
        }
    } catch (...) {}
}
}
ItemUseToken start_item_use_capture(ItemUseSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if(install_failed || current_capture())return {};
        if(!installed){if(PlayerUseItemHook::hook()!=0){install_failed=true;return {};}installed=true;}
        auto token=std::make_shared<ItemUseCapture>(std::move(sink));
        {std::lock_guard lock(current_mutex);current=token;}return token;
    }catch(...){return {};}
}
void stop_item_use_capture(ItemUseToken const& token) noexcept {
    if(!token)return;
    token->active.store(false,std::memory_order_release);
    try{std::lock_guard lock(current_mutex);if(current==token)current.reset();}catch(...){}
}
} // namespace bedrock_insight::levilamina
