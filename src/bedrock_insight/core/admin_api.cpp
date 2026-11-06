#include "bedrock_insight/core/admin_api.hpp"

#include <algorithm>
#include <array>

namespace bedrock_insight {
namespace {

struct Route {
    std::string_view method;
    std::string_view path;
    std::string_view action;
    ApiPermission permission;
};

constexpr std::array<Route, 9> kRoutes = {{
    {"GET", "/api/v1/health", "health.read", ApiPermission::ViewAnalytics},
    {"GET", "/api/v1/metrics", "metrics.read", ApiPermission::ViewAnalytics},
    {"GET", "/api/v1/rankings", "rankings.read", ApiPermission::ViewAnalytics},
    {"GET", "/api/v1/player", "player.read", ApiPermission::ViewAnalytics},
    {"GET", "/api/v1/audit", "audit.read", ApiPermission::ViewAudit},
    {"GET", "/api/v1/admin/actions", "admin_actions.read", ApiPermission::ViewAdminLog},
    {"POST", "/api/v1/rollback/preview", "rollback.preview", ApiPermission::PreviewRollback},
    {"POST", "/api/v1/rollback/apply", "rollback.apply", ApiPermission::ApplyRollback},
    {"POST", "/api/v1/tokens", "tokens.manage", ApiPermission::ManageTokens},
}};

Route const* find_route(ApiRequest const& request) {
    auto match = std::find_if(kRoutes.begin(), kRoutes.end(), [&](Route const& route) {
        if (route.method != request.method) {
            return false;
        }
        if (route.path == request.path) {
            return true;
        }
        return route.action == "player.read" && request.path.starts_with(route.path)
            && request.path.size() > route.path.size() && request.path[route.path.size()] == '?';
    });
    return match == kRoutes.end() ? nullptr : &*match;
}

std::string json_quote(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char character : value) {
        switch (character) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20) {
                result += "\\u00";
                result.push_back(hex[(character >> 4) & 0x0f]);
                result.push_back(hex[character & 0x0f]);
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

ApiResponse error_response(int status, std::string_view message) {
    return ApiResponse{status, "application/json", "{\"error\":" + json_quote(message) + "}"};
}

bool is_loopback(std::string_view address) {
    return address == "127.0.0.1" || address == "::1" || address == "localhost";
}

void log_request(AdminActionLog& log, ApiRequest const& request, std::string_view action,
    ApiPrincipal const* principal, bool allowed, ApiResponse const& response, std::string_view detail) {
    AdminActionLogEntry entry;
    entry.occurred_at_ms = request.received_at_ms;
    entry.actor_id = principal == nullptr ? std::string{} : principal->actor_id;
    entry.role = principal == nullptr ? ApiRole::Viewer : principal->role;
    entry.action = std::string(action);
    entry.method = request.method;
    entry.path = request.path;
    entry.remote_address = request.remote_address;
    entry.allowed = allowed;
    entry.status = response.status;
    entry.detail = std::string(detail);
    log.append(std::move(entry));
}

} // namespace

std::string_view to_string(ApiRole role) {
    switch (role) {
    case ApiRole::Viewer: return "viewer";
    case ApiRole::Moderator: return "moderator";
    case ApiRole::Admin: return "admin";
    }
    return "unknown";
}

std::optional<ApiRole> api_role_from_string(std::string_view value) {
    if (value == "viewer") return ApiRole::Viewer;
    if (value == "moderator") return ApiRole::Moderator;
    if (value == "admin") return ApiRole::Admin;
    return std::nullopt;
}

bool role_allows(ApiRole role, ApiPermission permission) {
    if (role != ApiRole::Viewer && role != ApiRole::Moderator && role != ApiRole::Admin) {
        return false;
    }
    switch (permission) {
    case ApiPermission::ViewAnalytics:
        return true;
    case ApiPermission::ViewAudit:
    case ApiPermission::PreviewRollback:
        return role >= ApiRole::Moderator;
    case ApiPermission::ViewAdminLog:
        return role >= ApiRole::Admin;
    case ApiPermission::ApplyRollback:
    case ApiPermission::ManageTokens:
        return role >= ApiRole::Admin;
    }
    return false;
}

ApiTokenStore::ApiTokenStore(TokenHasher hasher)
    : hasher_(std::move(hasher)) {}

bool ApiTokenStore::add_token(std::string token_id, std::string secret, ApiRole role, std::string actor_id,
    std::int64_t created_at_ms, std::string& error) {
    if (token_id.empty() || secret.empty() || actor_id.empty()) {
        error = "token id, secret, and actor id are required";
        return false;
    }
    if (!hasher_) {
        error = "token hasher is not configured";
        return false;
    }
    auto secret_hash = hasher_(secret);
    if (secret_hash.empty()) {
        error = "token hasher returned an empty hash";
        return false;
    }

    ApiTokenMetadata metadata{std::move(token_id), std::move(secret_hash), std::move(actor_id), role, created_at_ms, true};
    if (!add_hashed_token(metadata, error)) {
        return false;
    }
    ChangeSink sink;
    {
        std::lock_guard lock(mutex_);
        sink = change_sink_;
    }
    if (sink) {
        sink(metadata);
    }
    return true;
}

bool ApiTokenStore::add_hashed_token(ApiTokenMetadata metadata, std::string& error) {
    if (metadata.token_id.empty() || metadata.secret_hash.empty() || metadata.actor_id.empty()) {
        error = "token id, secret hash, and actor id are required";
        return false;
    }
    if (!role_allows(metadata.role, ApiPermission::ViewAnalytics)) {
        error = "invalid API role";
        return false;
    }
    std::lock_guard lock(mutex_);
    auto duplicate = std::find_if(tokens_.begin(), tokens_.end(), [&](TokenRecord const& token) {
        return token.token_id == metadata.token_id;
    });
    if (duplicate != tokens_.end()) {
        error = "token id already exists";
        return false;
    }
    tokens_.push_back(TokenRecord{std::move(metadata.token_id), std::move(metadata.secret_hash), std::move(metadata.actor_id),
        metadata.role, metadata.created_at_ms, metadata.enabled});
    error.clear();
    return true;
}

bool ApiTokenStore::set_enabled(std::string_view token_id, bool enabled, std::string& error) {
    ApiTokenMetadata metadata;
    ChangeSink sink;
    {
        std::lock_guard lock(mutex_);
        auto token = std::find_if(tokens_.begin(), tokens_.end(), [&](TokenRecord const& candidate) {
            return candidate.token_id == token_id;
        });
        if (token == tokens_.end()) {
            error = "token not found";
            return false;
        }
        token->enabled = enabled;
        metadata = ApiTokenMetadata{token->token_id, token->secret_hash, token->actor_id, token->role,
            token->created_at_ms, token->enabled};
        sink = change_sink_;
    }
    if (sink) {
        sink(metadata);
    }
    error.clear();
    return true;
}

std::optional<ApiPrincipal> ApiTokenStore::authenticate(std::string_view secret) const {
    if (secret.empty() || !hasher_) {
        return std::nullopt;
    }
    std::string secret_hash;
    try {
        secret_hash = hasher_(secret);
    } catch (...) {
        return std::nullopt;
    }
    if (secret_hash.empty()) return std::nullopt;
    std::lock_guard lock(mutex_);
    auto token = std::find_if(tokens_.begin(), tokens_.end(), [&](TokenRecord const& candidate) {
        return candidate.enabled && candidate.secret_hash == secret_hash;
    });
    if (token == tokens_.end()) {
        return std::nullopt;
    }
    return ApiPrincipal{token->token_id, token->actor_id, token->role};
}

std::size_t ApiTokenStore::size() const {
    std::lock_guard lock(mutex_);
    return tokens_.size();
}

std::vector<ApiTokenMetadata> ApiTokenStore::snapshot_metadata() const {
    std::lock_guard lock(mutex_);
    std::vector<ApiTokenMetadata> result;
    result.reserve(tokens_.size());
    for (auto const& token : tokens_) {
        result.push_back(ApiTokenMetadata{token.token_id, token.secret_hash, token.actor_id, token.role,
            token.created_at_ms, token.enabled});
    }
    return result;
}

void ApiTokenStore::set_change_sink(ChangeSink sink) {
    std::lock_guard lock(mutex_);
    change_sink_ = std::move(sink);
}

AdminActionLog::AdminActionLog(std::size_t capacity, ActionSink sink)
    : capacity_(capacity), action_sink_(std::move(sink)) {}

std::uint64_t AdminActionLog::append(AdminActionLogEntry entry) {
    ActionSink sink;
    {
        std::lock_guard lock(mutex_);
        entry.id = next_id_++;
        if (capacity_ != 0) {
            while (entries_.size() >= capacity_) {
                entries_.pop_front();
            }
            entries_.push_back(entry);
        }
        sink = action_sink_;
    }
    if (sink) {
        sink(entry);
    }
    return entry.id;
}

std::vector<AdminActionLogEntry> AdminActionLog::snapshot() const {
    std::lock_guard lock(mutex_);
    return {entries_.begin(), entries_.end()};
}

std::size_t AdminActionLog::size() const {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

void AdminActionLog::set_action_sink(ActionSink sink) {
    std::lock_guard lock(mutex_);
    action_sink_ = std::move(sink);
}

AdminApi::AdminApi(ApiPolicy policy, ApiTokenStore& tokens, AdminActionLog& action_log)
    : policy_(std::move(policy)), tokens_(tokens), action_log_(action_log) {}

ApiResponse AdminApi::handle(ApiRequest const& request, ApiDataProvider const& provider) const {
    auto const* route = find_route(request);
    if (route == nullptr) {
        auto response = error_response(404, "route not found");
        log_request(action_log_, request, "route.not_found", nullptr, false, response, "unknown route");
        return response;
    }

    if (policy_.require_localhost && !is_loopback(request.remote_address)) {
        auto response = error_response(403, "localhost access required");
        log_request(action_log_, request, route->action, nullptr, false, response, "non-loopback request");
        return response;
    }

    auto principal = tokens_.authenticate(request.bearer_token);
    if (!principal.has_value()) {
        auto response = error_response(401, "authentication required");
        log_request(action_log_, request, route->action, nullptr, false, response, "invalid or disabled token");
        return response;
    }
    if (!role_allows(principal->role, route->permission)) {
        auto response = error_response(403, "insufficient role");
        log_request(action_log_, request, route->action, &*principal, false, response, "role denied");
        return response;
    }

    if (route->permission == ApiPermission::ApplyRollback) {
        auto response = error_response(501, "rollback apply is disabled pending full-state recovery validation");
        log_request(action_log_, request, route->action, &*principal, false, response, "rollback disabled");
        return response;
    }

    std::function<std::string(std::string_view)> body_provider;
    if (route->action == "health.read") {
        if (provider.health_json) {
            auto response = ApiResponse{200, "application/json", provider.health_json()};
            log_request(action_log_, request, route->action, &*principal, true, response, "ok");
            return response;
        }
    } else if (route->action == "metrics.read") {
        if (provider.metrics_json) {
            auto response = ApiResponse{200, "application/json", provider.metrics_json()};
            log_request(action_log_, request, route->action, &*principal, true, response, "ok");
            return response;
        }
    } else if (route->action == "rankings.read") {
        if (provider.rankings_json) {
            auto response = ApiResponse{200, "application/json", provider.rankings_json()};
            log_request(action_log_, request, route->action, &*principal, true, response, "ok");
            return response;
        }
    } else if (route->action == "player.read") {
        if (provider.player_json) {
            auto response = ApiResponse{200, "application/json", provider.player_json(request.path)};
            log_request(action_log_, request, route->action, &*principal, true, response, "ok");
            return response;
        }
    } else if (route->action == "audit.read") {
        body_provider = provider.audit_json;
    } else if (route->action == "admin_actions.read") {
        if (provider.admin_actions_json) {
            auto response = ApiResponse{200, "application/json", provider.admin_actions_json()};
            log_request(action_log_, request, route->action, &*principal, true, response, "ok");
            return response;
        }
    } else if (route->action == "rollback.preview") {
        body_provider = provider.rollback_preview_json;
    } else if (route->action == "rollback.apply") {
        body_provider = provider.rollback_apply_json;
    } else if (route->action == "tokens.manage") {
        body_provider = provider.token_admin_json;
    }

    if (body_provider) {
        auto response = ApiResponse{200, "application/json", body_provider(request.body)};
        log_request(action_log_, request, route->action, &*principal, true, response, "ok");
        return response;
    }

    auto response = error_response(501, "endpoint provider is not configured");
    log_request(action_log_, request, route->action, &*principal, false, response, "provider missing");
    return response;
}

} // namespace bedrock_insight
