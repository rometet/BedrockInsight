#pragma once

#include "bedrock_insight/core/dashboard.hpp"
#include "bedrock_insight/core/http_transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bedrock_insight {

struct HttpServerConfig {
    // The adapter-neutral listener deliberately accepts only IPv4 loopback
    // addresses. A future platform adapter may add an explicitly audited
    // IPv6 listener without weakening this default.
    std::string bind_address = "127.0.0.1";
    std::uint16_t port = 0;
    std::uint32_t receive_timeout_ms = 1000;
    HttpLimits http_limits{};
    bool serve_dashboard = true;
    std::function<std::optional<DashboardAsset>(std::string_view)> dashboard_assets;
};

// Owns one bounded, connection-close HTTP listener. The API/provider
// references must outlive the listener. The listener never opens SQLite or
// calls a server tick API; providers remain responsible for dispatching
// read-only work to their worker/query layer.
class HttpServer final {
public:
    HttpServer(AdminApi const& api, ApiDataProvider const& provider, HttpServerConfig config = {});
    ~HttpServer();

    HttpServer(HttpServer const&) = delete;
    HttpServer& operator=(HttpServer const&) = delete;

    bool start(std::string& error);
    void stop() noexcept;
    bool running() const noexcept;
    std::uint16_t port() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bedrock_insight
