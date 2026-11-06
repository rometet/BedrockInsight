#pragma once

#include "bedrock_insight/core/event.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <chrono>
#include <memory>
#include <mutex>

namespace bedrock_insight {

class EventQueue final {
public:
    using EventPtr = std::shared_ptr<const AnalyticsEvent>;

    struct PushResult final {
        bool accepted = false;
        std::uint64_t sequence = 0;
    };

    explicit EventQueue(std::size_t capacity) : capacity_(capacity) {}

    EventQueue(EventQueue const&) = delete;
    EventQueue& operator=(EventQueue const&) = delete;

    // Startup only, before producers are enabled. Resume durable event IDs.
    bool resume_after(std::uint64_t sequence) {
        std::lock_guard lock(mutex_);
        if (closed_ || !queue_.empty() || next_sequence_ != 1
            || sequence >= static_cast<std::uint64_t>(INT64_MAX)) {
            return false;
        }
        next_sequence_ = sequence + 1;
        return true;
    }

    [[nodiscard]] PushResult try_push(AnalyticsEvent event) {
        std::lock_guard lock(mutex_);
        if (closed_ || queue_.size() >= capacity_ || next_sequence_ > static_cast<std::uint64_t>(INT64_MAX)) {
            return {};
        }

        event.sequence = next_sequence_;
        auto immutable = std::make_shared<const AnalyticsEvent>(std::move(event));
        auto sequence = immutable->sequence;
        queue_.push_back(std::move(immutable));
        ++next_sequence_;
        available_.notify_one();
        return {true, sequence};
    }

    // Returns false only after close() and once all queued values have been
    // drained. The caller owns no mutable event state.
    bool wait_pop(EventPtr& result) {
        std::unique_lock lock(mutex_);
        available_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        result = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    [[nodiscard]] bool try_pop(EventPtr& result) {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        result = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    // Returns false on timeout or after close() and drain. This bounded wait
    // lets StorageWriter multiplex event values with control-plane work.
    bool wait_pop_for(EventPtr& result, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!available_.wait_for(lock, timeout, [this] { return closed_ || !queue_.empty(); })) {
            return false;
        }
        if (queue_.empty()) {
            return false;
        }
        result = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        available_.notify_all();
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::deque<EventPtr> queue_;
    std::uint64_t next_sequence_ = 1;
    bool closed_ = false;
};

} // namespace bedrock_insight
