#include "bedrock_insight/core/rollback.hpp"

#include <algorithm>
#include <utility>

namespace bedrock_insight {
namespace {

struct PositionKey final {
    std::string world_id;
    Position position;

    friend bool operator<(PositionKey const& left, PositionKey const& right) noexcept {
        if (left.world_id != right.world_id) {
            return left.world_id < right.world_id;
        }
        if (left.position.dimension != right.position.dimension) {
            return left.position.dimension < right.position.dimension;
        }
        if (left.position.x != right.position.x) {
            return left.position.x < right.position.x;
        }
        if (left.position.y != right.position.y) {
            return left.position.y < right.position.y;
        }
        return left.position.z < right.position.z;
    }
};

bool reversible(AuditRecord const& record) {
    return record.event_kind == "audit_block_change" && !record.before_type.empty() && !record.after_type.empty();
}

} // namespace

std::string_view to_string(RollbackOperation operation) noexcept {
    switch (operation) {
    case RollbackOperation::Undo: return "undo";
    case RollbackOperation::Restore: return "restore";
    }
    return "unknown";
}

std::string_view to_string(RollbackEntryStatus status) noexcept {
    switch (status) {
    case RollbackEntryStatus::Prepared: return "prepared";
    case RollbackEntryStatus::Applied: return "applied";
    case RollbackEntryStatus::Conflict: return "conflict";
    case RollbackEntryStatus::Failed: return "failed";
    }
    return "unknown";
}

std::string_view to_string(RollbackTransactionStatus status) noexcept {
    switch (status) {
    case RollbackTransactionStatus::Prepared: return "prepared";
    case RollbackTransactionStatus::Applying: return "applying";
    case RollbackTransactionStatus::Applied: return "applied";
    case RollbackTransactionStatus::Conflict: return "conflict";
    case RollbackTransactionStatus::Failed: return "failed";
    }
    return "unknown";
}

bool RollbackPreview::can_apply() const noexcept {
    if (changes.empty() || !unsupported_event_ids.empty() || conflict_count != 0) {
        return false;
    }
    return std::all_of(changes.begin(), changes.end(), [](RollbackChange const& change) {
        return change.reversible && !change.conflict && !change.target_type.empty();
    });
}

RollbackPreview RollbackPlanner::preview(
    RollbackRequest const& request,
    RollbackStateReader const& read_state) const {
    RollbackPreview result;
    result.transaction_id = request.transaction_id;
    result.operation = request.operation;

    std::map<PositionKey, std::vector<AuditRecord>> grouped;
    for (auto const& record : request.audit_records) {
        if (!reversible(record)) {
            result.unsupported_event_ids.push_back(record.event_id);
            continue;
        }
        grouped[PositionKey{record.world_id, record.position}].push_back(record);
    }

    for (auto& [key, records] : grouped) {
        std::sort(records.begin(), records.end(), [](AuditRecord const& left, AuditRecord const& right) {
            if (left.occurred_at_ms != right.occurred_at_ms) {
                return left.occurred_at_ms < right.occurred_at_ms;
            }
            return left.event_id < right.event_id;
        });

        RollbackChange change;
        change.event_id = request.operation == RollbackOperation::Undo ? records.back().event_id : records.front().event_id;
        change.world_id = key.world_id;
        change.position = key.position;
        change.expected_type = request.operation == RollbackOperation::Undo ? records.back().after_type : records.front().before_type;
        change.target_type = request.operation == RollbackOperation::Undo ? records.front().before_type : records.back().after_type;
        change.source_event_ids.reserve(records.size());
        for (auto const& record : records) {
            change.source_event_ids.push_back(record.event_id);
        }
        for (std::size_t index = 1; index < records.size(); ++index) {
            if (records[index - 1].after_type != records[index].before_type) {
                change.conflict = true;
                change.reason = "audit chain does not connect";
                ++result.conflict_count;
                break;
            }
        }

        if (read_state) {
            auto current = read_state({change.world_id, change.position});
            if (!current.has_value()) {
                change.conflict = true;
                change.reason = "current block state unavailable";
                ++result.conflict_count;
            } else {
                change.observed_type = current->type;
                if (current->type != change.expected_type) {
                    change.conflict = true;
                    change.reason = "current block state differs from audit expectation";
                    ++result.conflict_count;
                }
            }
        } else {
            change.conflict = true;
            change.reason = "current block state reader unavailable";
            ++result.conflict_count;
        }
        result.changes.push_back(std::move(change));
    }
    return result;
}

bool RollbackJournal::begin(
    RollbackPreview const& preview,
    std::string actor_id,
    std::string reason,
    std::int64_t created_at_ms,
    std::string& transaction_id,
    std::string& error) {
    if (!preview.can_apply()) {
        error = "rollback preview contains conflicts, unsupported events, or no changes";
        return false;
    }

    std::lock_guard lock(mutex_);
    transaction_id = preview.transaction_id;
    if (transaction_id.empty()) {
        transaction_id = "rollback-" + std::to_string(next_transaction_number_++);
    }
    if (transactions_.contains(transaction_id)) {
        error = "rollback transaction already exists";
        return false;
    }

    RollbackTransaction transaction;
    transaction.transaction_id = transaction_id;
    transaction.operation = preview.operation;
    transaction.actor_id = std::move(actor_id);
    transaction.reason = std::move(reason);
    transaction.created_at_ms = created_at_ms;
    transaction.entries.reserve(preview.changes.size());
    for (auto const& change : preview.changes) {
        transaction.entries.push_back({
            change.event_id,
            change.source_event_ids,
            change.world_id,
            change.position,
            change.expected_type,
            change.target_type,
            change.observed_type,
            RollbackEntryStatus::Prepared,
            {},
        });
    }
    transactions_.emplace(transaction_id, std::move(transaction));
    return true;
}

bool RollbackJournal::apply(
    std::string_view transaction_id,
    RollbackStateReader const& read_state,
    RollbackStateWriter const& write_state,
    std::string& error) {
    // Type strings cannot restore block states/NBT or compensate a partially
    // failed write. No durable write-ahead/backup or world-thread contract is
    // implemented, so there is deliberately no runtime escape hatch here.
    // Keep preview/journal inspection usable without invoking world callbacks.
    static_cast<void>(transaction_id);
    static_cast<void>(read_state);
    static_cast<void>(write_state);
    error = "rollback apply disabled: full state, durable recovery, backup and world-thread validation required";
    return false;
}

bool RollbackJournal::inspect(
    std::string_view transaction_id,
    RollbackTransaction& result,
    std::string& error) const {
    std::lock_guard lock(mutex_);
    auto found = transactions_.find(std::string(transaction_id));
    if (found == transactions_.end()) {
        error = "rollback transaction not found";
        return false;
    }
    result = found->second;
    return true;
}

} // namespace bedrock_insight
