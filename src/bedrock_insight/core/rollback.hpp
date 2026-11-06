#pragma once

#include "bedrock_insight/core/analytics_query.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

enum class RollbackOperation : std::uint8_t {
    Undo,
    Restore,
};

enum class RollbackEntryStatus : std::uint8_t {
    Prepared,
    Applied,
    Conflict,
    Failed,
};

enum class RollbackTransactionStatus : std::uint8_t {
    Prepared,
    Applying,
    Applied,
    Conflict,
    Failed,
};

[[nodiscard]] std::string_view to_string(RollbackOperation operation) noexcept;
[[nodiscard]] std::string_view to_string(RollbackEntryStatus status) noexcept;
[[nodiscard]] std::string_view to_string(RollbackTransactionStatus status) noexcept;

struct RollbackState final {
    std::string type;
};

struct RollbackLocation final {
    std::string world_id;
    Position position;
};

using RollbackStateReader = std::function<std::optional<RollbackState>(RollbackLocation const&)>;
using RollbackStateWriter = std::function<bool(RollbackLocation const&, std::string_view, std::string&)>;

struct RollbackRequest final {
    RollbackOperation operation = RollbackOperation::Undo;
    std::string transaction_id;
    std::string actor_id;
    std::string reason;
    std::vector<AuditRecord> audit_records;
};

struct RollbackChange final {
    std::uint64_t event_id = 0;
    std::vector<std::uint64_t> source_event_ids;
    std::string world_id;
    Position position;
    std::string expected_type;
    std::string target_type;
    std::string observed_type;
    bool reversible = true;
    bool conflict = false;
    std::string reason;
};

struct RollbackPreview final {
    std::string transaction_id;
    RollbackOperation operation = RollbackOperation::Undo;
    std::vector<RollbackChange> changes;
    std::vector<std::uint64_t> unsupported_event_ids;
    std::size_t conflict_count = 0;

    [[nodiscard]] bool can_apply() const noexcept;
};

class RollbackPlanner final {
public:
    [[nodiscard]] RollbackPreview preview(
        RollbackRequest const& request,
        RollbackStateReader const& read_state) const;
};

struct RollbackJournalEntry final {
    std::uint64_t event_id = 0;
    std::vector<std::uint64_t> source_event_ids;
    std::string world_id;
    Position position;
    std::string expected_type;
    std::string target_type;
    std::string observed_type;
    RollbackEntryStatus status = RollbackEntryStatus::Prepared;
    std::string error;
};

struct RollbackTransaction final {
    std::string transaction_id;
    RollbackOperation operation = RollbackOperation::Undo;
    std::string actor_id;
    std::string reason;
    std::int64_t created_at_ms = 0;
    RollbackTransactionStatus status = RollbackTransactionStatus::Prepared;
    std::vector<RollbackJournalEntry> entries;
};

// The journal owns transaction state and enforces preview-before-apply. It is
// deliberately independent of Bedrock objects; the future LeviLamina adapter
// must marshal world access onto the verified server thread. Apply remains
// disabled until complete state/NBT and durable recovery are implemented.
class RollbackJournal final {
public:
    [[nodiscard]] bool begin(
        RollbackPreview const& preview,
        std::string actor_id,
        std::string reason,
        std::int64_t created_at_ms,
        std::string& transaction_id,
        std::string& error);

    [[nodiscard]] bool apply(
        std::string_view transaction_id,
        RollbackStateReader const& read_state,
        RollbackStateWriter const& write_state,
        std::string& error);

    [[nodiscard]] bool inspect(
        std::string_view transaction_id,
        RollbackTransaction& result,
        std::string& error) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, RollbackTransaction> transactions_;
    std::uint64_t next_transaction_number_ = 1;
};

} // namespace bedrock_insight
