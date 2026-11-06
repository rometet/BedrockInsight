#pragma once

#include "bedrock_insight/core/analytics_query.hpp"
#include "bedrock_insight/core/json.hpp"
#include "bedrock_insight/core/rollback.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

// The authenticated principal supplies actor_id; clients cannot impersonate
// an operator through this payload. Event IDs identify records already loaded
// by the control-plane provider before RollbackPlanner is invoked.
struct RollbackApiPayload final {
    RollbackOperation operation = RollbackOperation::Undo;
    std::string transaction_id;
    std::string reason;
    std::vector<std::uint64_t> event_ids;
};

JsonParseResult parse_rollback_api_payload(std::string_view input, RollbackApiPayload& result, std::string& error,
    JsonLimits limits = {});
JsonParseResult parse_audit_filter_api_payload(std::string_view input, AuditFilter& result, std::string& error,
    JsonLimits limits = {});

} // namespace bedrock_insight
