#pragma once

#include "bedrock_insight/core/admin_api.hpp"
#include "bedrock_insight/core/event_queue.hpp"
#include "bedrock_insight/core/rollback.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

namespace bedrock_insight {

struct StorageConfig final {
    std::filesystem::path database_path;
    std::size_t queue_capacity = 8192;
};

struct StorageMetrics final {
    std::uint64_t accepted_events = 0;
    std::uint64_t committed_events = 0;
    std::uint64_t dropped_events = 0;
    std::uint64_t accepted_rollback_transactions = 0;
    std::uint64_t committed_rollback_transactions = 0;
    std::uint64_t accepted_api_tokens = 0;
    std::uint64_t committed_api_tokens = 0;
    std::uint64_t accepted_admin_actions = 0;
    std::uint64_t committed_admin_actions = 0;
    bool failed = false;
};

class StorageWriter final {
public:
    explicit StorageWriter(StorageConfig config);
    ~StorageWriter();

    StorageWriter(StorageWriter const&) = delete;
    StorageWriter& operator=(StorageWriter const&) = delete;

    // Starts the database-owning thread and waits only for open/migration.
    // It must be called during mod startup, not from a tick callback.
    [[nodiscard]] bool start();
    void stop();

    // This method only copies immutable value data into the bounded queue.
    // It never opens SQLite or performs database I/O.
    [[nodiscard]] bool submit(AnalyticsEvent event);

    // Queues a complete rollback snapshot for the same SQLite-owning worker
    // that persists analytics events. This method performs no database I/O.
    [[nodiscard]] bool submit_rollback_transaction(RollbackTransaction transaction);
    [[nodiscard]] bool submit_api_token(ApiTokenMetadata token);
    [[nodiscard]] bool submit_admin_action(AdminActionLogEntry entry);

    // Used during controlled shutdown/tests. It waits for all accepted values
    // to commit and reports the worker's terminal health.
    [[nodiscard]] bool flush();

    [[nodiscard]] StorageMetrics metrics() const;
    [[nodiscard]] std::string last_error() const;

private:
    class Database;
    using ControlOperation = std::variant<RollbackTransaction, ApiTokenMetadata, AdminActionLogEntry>;

    void run();
    void run_worker();
    void set_error(std::string message);

    StorageConfig config_;
    EventQueue queue_;
    std::thread worker_;
    // Serialize lifecycle calls, including joining a worker that already exited.
    std::mutex lifecycle_mutex_;
    mutable std::mutex state_mutex_;
    std::condition_variable state_changed_;
    mutable std::mutex control_mutex_;
    std::deque<ControlOperation> control_queue_;
    bool control_closed_ = false;
    bool started_ = false;
    bool ready_ = false;
    bool stopped_ = false;
    std::string last_error_;
    std::atomic<bool> failed_{false};
    std::atomic<std::uint64_t> accepted_events_{0};
    std::atomic<std::uint64_t> committed_events_{0};
    std::atomic<std::uint64_t> dropped_events_{0};
    std::atomic<std::uint64_t> accepted_rollback_transactions_{0};
    std::atomic<std::uint64_t> committed_rollback_transactions_{0};
    std::atomic<std::uint64_t> accepted_api_tokens_{0};
    std::atomic<std::uint64_t> committed_api_tokens_{0};
    std::atomic<std::uint64_t> accepted_admin_actions_{0};
    std::atomic<std::uint64_t> committed_admin_actions_{0};
    std::atomic<std::uint64_t> pending_control_operations_{0};
    std::atomic<std::uint64_t> last_submitted_sequence_{0};
    std::atomic<std::uint64_t> last_committed_sequence_{0};
};

} // namespace bedrock_insight
