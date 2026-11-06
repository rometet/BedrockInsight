#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bedrock_insight {

enum class ApiRole : std::uint8_t {
    Viewer = 0,
    Moderator = 1,
    Admin = 2,
};

enum class ApiPermission : std::uint8_t {
    ViewAnalytics,
    ViewAudit,
    ViewAdminLog,
    PreviewRollback,
    ApplyRollback,
    ManageTokens,
};

std::string_view to_string(ApiRole role);
std::optional<ApiRole> api_role_from_string(std::string_view value);
bool role_allows(ApiRole role, ApiPermission permission);

struct ApiPrincipal {
    std::string token_id;
    std::string actor_id;
    ApiRole role = ApiRole::Viewer;
};

struct ApiTokenMetadata final {
    std::string token_id;
    std::string secret_hash;
    std::string actor_id;
    ApiRole role = ApiRole::Viewer;
    std::int64_t created_at_ms = 0;
    bool enabled = true;
};

// The store deliberately accepts a hashing callback instead of selecting a
// crypto dependency in the adapter-neutral core. Production adapters must
// supply a cryptographic, salted token hash; tests may use a deterministic
// test-only callback. Plaintext secrets are never retained by this class.
class ApiTokenStore {
public:
    using TokenHasher = std::function<std::string(std::string_view)>;

    explicit ApiTokenStore(TokenHasher hasher);

    bool add_token(std::string token_id, std::string secret, ApiRole role, std::string actor_id,
        std::int64_t created_at_ms, std::string& error);
    // Used during controlled startup restore. The input is already a hash;
    // this method never accepts or persists plaintext token material.
    bool add_hashed_token(ApiTokenMetadata metadata, std::string& error);
    bool set_enabled(std::string_view token_id, bool enabled, std::string& error);
    std::optional<ApiPrincipal> authenticate(std::string_view secret) const;
    std::vector<ApiTokenMetadata> snapshot_metadata() const;
    using ChangeSink = std::function<void(ApiTokenMetadata const&)>;
    void set_change_sink(ChangeSink sink);
    std::size_t size() const;

private:
    struct TokenRecord {
        std::string token_id;
        std::string secret_hash;
        std::string actor_id;
        ApiRole role = ApiRole::Viewer;
        std::int64_t created_at_ms = 0;
        bool enabled = true;
    };

    TokenHasher hasher_;
    ChangeSink change_sink_;
    mutable std::mutex mutex_;
    std::vector<TokenRecord> tokens_;
};

struct AdminActionLogEntry {
    std::uint64_t id = 0;
    std::int64_t occurred_at_ms = 0;
    std::string actor_id;
    ApiRole role = ApiRole::Viewer;
    std::string action;
    std::string method;
    std::string path;
    std::string remote_address;
    bool allowed = false;
    int status = 500;
    std::string detail;
};

class AdminActionLog {
public:
    using ActionSink = std::function<void(AdminActionLogEntry const&)>;

    explicit AdminActionLog(std::size_t capacity = 512, ActionSink sink = {});

    std::uint64_t append(AdminActionLogEntry entry);
    std::vector<AdminActionLogEntry> snapshot() const;
    void set_action_sink(ActionSink sink);
    std::size_t size() const;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<AdminActionLogEntry> entries_;
    ActionSink action_sink_;
    std::uint64_t next_id_ = 1;
};

struct ApiPolicy {
    // This is a request-boundary check. The eventual HTTP listener must also
    // bind to this address or another explicitly configured loopback address.
    std::string bind_address = "127.0.0.1";
    bool require_localhost = true;
};

struct ApiRequest {
    std::string method;
    std::string path;
    std::string bearer_token;
    std::string body;
    std::string remote_address;
    std::int64_t received_at_ms = 0;
};

struct ApiResponse {
    int status = 500;
    std::string content_type = "application/json";
    std::string body;
};

// Providers return already-serialized JSON from the query/command layer.
// Keeping serialization and HTTP transport outside this core avoids pulling
// a web framework or generated LeviLamina headers into the testable library.
struct ApiDataProvider {
    std::function<std::string()> health_json;
    std::function<std::string()> metrics_json;
    std::function<std::string()> rankings_json;
    // Receives the complete request target so the provider can validate and
    // decode a bounded query such as /api/v1/player?id=xuid-42.
    std::function<std::string(std::string_view)> player_json;
    std::function<std::string(std::string_view)> audit_json;
    std::function<std::string()> admin_actions_json;
    std::function<std::string(std::string_view)> rollback_preview_json;
    std::function<std::string(std::string_view)> rollback_apply_json;
    std::function<std::string(std::string_view)> token_admin_json;
};

class AdminApi {
public:
    AdminApi(ApiPolicy policy, ApiTokenStore& tokens, AdminActionLog& action_log);

    ApiResponse handle(ApiRequest const& request, ApiDataProvider const& provider) const;

private:
    ApiPolicy policy_;
    ApiTokenStore& tokens_;
    AdminActionLog& action_log_;
};

} // namespace bedrock_insight
