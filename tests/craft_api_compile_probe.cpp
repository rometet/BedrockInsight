// Declaration/import evidence ONLY. Never linked into a mod or run in BDS.
#include "mc/world/inventory/network/ItemStackRequestActionHandler.h"
#include "mc/world/inventory/network/crafting/CraftHandlerBase.h"
#include <type_traits>

using Transfer=ItemStackNetResult (ItemStackRequestActionHandler::*)(
    ItemStackRequestActionTransferBase const&,bool,bool,bool);
using PostRequest=void (CraftHandlerBase::*)(bool);
using ScreenContext=ContainerScreenContext const& (ItemStackRequestActionHandler::*)() const;
static_assert(std::is_same_v<decltype(&ItemStackRequestActionHandler::_handleTransfer),Transfer>);
static_assert(std::is_same_v<decltype(&CraftHandlerBase::postRequest),PostRequest>);
static_assert(std::is_same_v<decltype(&ItemStackRequestActionHandler::getScreenContext),ScreenContext>);
static_assert(static_cast<unsigned>(ItemStackNetResult::Success)==0);
static_assert(static_cast<unsigned>(ItemStackNetResult::ItemRequestActionHandlerCommitFailed)==5);

// Externally visible objects retain real compiler-produced relocations for nm.
Transfer craft_transfer_probe=&ItemStackRequestActionHandler::_handleTransfer;
PostRequest craft_post_request_probe=&CraftHandlerBase::postRequest;
ScreenContext craft_screen_context_probe=&ItemStackRequestActionHandler::getScreenContext;
