// Lifecycle seam ONLY, never linked into a real DLL. Not an engine/state test.
#include "bedrock_insight/levilamina/player_block.hpp"
#include <atomic>
namespace bedrock_insight::levilamina {
class BlockCapture final {
public:
    explicit BlockCapture(BlockSink fn):sink(std::move(fn)){}
    BlockSink sink;std::atomic<bool> active{true};
};
namespace{std::weak_ptr<BlockCapture> current;}
bool test_block_install_failure=false;
BlockToken start_block_capture(BlockSink sink) noexcept {
    if(test_block_install_failure || !current.expired())return {};
    try{auto token=std::make_shared<BlockCapture>(std::move(sink));current=token;return token;}catch(...){return {};}
}
void stop_block_capture(BlockToken const& token) noexcept {
    if(!token)return;
    token->active=false;if(current.lock()==token)current.reset();
}
BlockSink test_block_callback(){if(auto token=current.lock())return token->sink;return {};}
}
