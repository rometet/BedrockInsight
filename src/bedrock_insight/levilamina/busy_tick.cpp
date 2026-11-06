#include "bedrock_insight/levilamina/busy_tick.hpp"
#include "ll/api/memory/Hook.h"
#include "mc/world/level/Level.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <mutex>
#include <thread>

namespace bedrock_insight::levilamina {
namespace {
std::int64_t steady_ns() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

class BusyTickState final {
public:
    explicit BusyTickState(BusyTickSink value) : sink(std::move(value)) {}
    std::atomic<bool> active{true};
    void finish(Level& level, std::int64_t start, std::int64_t end,
                std::int64_t ticks, std::int64_t max_inner, bool completed) noexcept {
      try {
        if (!active.load(std::memory_order_acquire)) return;
        std::optional<BusyTickWindow> window;
        bool invalid=false;
        {
            std::lock_guard lock(mutex);
            if (!active.load(std::memory_order_acquire)) return;
            auto const thread=std::this_thread::get_id();
            if (owner==std::thread::id{}) owner=thread;
            window=sampler.observe(&level,start,end,ticks,max_inner,completed && owner==thread);
            invalid=!sampler.valid();
            if (invalid) active.store(false,std::memory_order_release);
        }
        // Never hold sampler mutex across the queue adapter / capture mutex.
        // Callback errors cannot replace an engine exception or bypass origin.
        try {
            if (invalid) sink(level,{},false);
            else if (window) sink(level,*window,true);
        } catch (...) { active.store(false,std::memory_order_release); }
      } catch (...) { active.store(false,std::memory_order_release); }
    }
private:
    std::mutex mutex;
    std::thread::id owner;
    BusyTickSampler sampler;
    BusyTickSink sink;
};

namespace {
// A tiny copy-under-lock avoids introducing MSVCP140_ATOMIC_WAIT.dll via
// atomic<shared_ptr>. The lock is NEVER held across engine origin or a sink.
std::mutex current_mutex;
BusyTickToken current;
BusyTickToken current_state() noexcept {
    try {std::lock_guard lock(current_mutex);return current;} catch (...) {return {};}
}
std::mutex install_mutex;
bool installed=false, install_failed=false;

struct Frame final {
    Level* level;
    std::int64_t ticks=0, max_inner=0;
    bool inner_active=false, valid=true;
};
thread_local Frame* frame=nullptr;

struct OuterScope final {
    BusyTickToken state;
    Frame local;
    std::int64_t start;
    int exceptions=std::uncaught_exceptions();
    explicit OuterScope(BusyTickToken value, Level& level)
        : state(std::move(value)),local{&level},start(steady_ns()) { frame=&local; }
    ~OuterScope() noexcept {
        auto const end=steady_ns(); frame=nullptr;
        state->finish(*local.level,start,end,local.ticks,local.max_inner,
            local.valid && std::uncaught_exceptions()==exceptions);
    }
};

struct InnerScope final {
    Frame* owner;
    std::int64_t start=steady_ns();
    int exceptions=std::uncaught_exceptions();
    explicit InnerScope(Frame* value) : owner(value) { owner->inner_active=true; ++owner->ticks; }
    ~InnerScope() noexcept {
        auto const end=steady_ns(); owner->inner_active=false;
        if (end<start || std::uncaught_exceptions()!=exceptions) owner->valid=false;
        else owner->max_inner=std::max(owner->max_inner,end-start);
    }
};

// Inspired by Pier TickStats.cpp's Highest/Lowest actual-tick counting design
// (Apache-2.0, pinned f841e43...). Implementation here uses owned lifecycle state,
// RAII exception accounting and constant-memory persistence windows.
LL_TYPE_INSTANCE_HOOK(BusyInnerHook, ll::memory::HookPriority::Lowest, Level, &Level::$tick, void) {
    if (!frame) { origin(); return; }
    if (frame->level!=this || frame->inner_active || frame->ticks>=BusyTickSampler::kMaxTicks) {
        frame->valid=false; origin(); return;
    }
    InnerScope scope(frame);
    origin();
}

LL_TYPE_INSTANCE_HOOK(BusyOuterHook, ll::memory::HookPriority::Highest, Level, &Level::$tick, void) {
    if (frame) { frame->valid=false; origin(); return; } // recursion: preserve engine, invalidate data
    auto state=current_state();
    if (!state || !state->active.load(std::memory_order_acquire)) { origin(); return; }
    OuterScope scope(std::move(state),*this);
    origin(); // always exactly once in our detour; lower hooks may warp/freeze
}
}

BusyTickToken start_busy_ticks(BusyTickSink sink) noexcept {
    try {
        std::lock_guard lock(install_mutex);
        if (install_failed || current_state()) return {};
        if (!installed) {
            auto const outer=BusyOuterHook::hook();
            if (outer!=0) { install_failed=true; return {}; }
            auto const inner=BusyInnerHook::hook();
            if (inner!=0) {
                // No active state yet; even failed removal leaves only a pinned
                // pass-through outer detour. No worker/capture owns this failure.
                static_cast<void>(BusyOuterHook::unhook()); install_failed=true; return {};
            }
            installed=true;
        }
        auto state=std::make_shared<BusyTickState>(std::move(sink));
        {std::lock_guard active_lock(current_mutex);current=state;}
        return state;
    } catch (...) { return {}; }
}

void stop_busy_ticks(BusyTickToken const& token) noexcept {
    if (!token) return;
    token->active.store(false,std::memory_order_release);
    try {std::lock_guard lock(current_mutex);if(current==token)current.reset();} catch (...) {}
}
} // namespace bedrock_insight::levilamina
