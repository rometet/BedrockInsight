#include "bedrock_insight/core/http_server.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace bedrock_insight {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

bool is_loopback_bind_address(std::string_view address) {
    return address == "127.0.0.1" || address == "localhost";
}

void close_socket(NativeSocket socket) noexcept {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

int socket_error() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool interrupted_socket_error(int error) noexcept {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

bool set_socket_timeout(NativeSocket socket, std::uint32_t timeout_ms) noexcept {
#ifdef _WIN32
    auto timeout = static_cast<DWORD>(std::min<std::uint32_t>(timeout_ms, std::numeric_limits<DWORD>::max()));
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0
        && setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0;
#else
    timeval timeout{};
    timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(timeout_ms / 1000);
    timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((timeout_ms % 1000) * 1000);
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0
        && setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

bool send_all(NativeSocket socket, std::string_view wire) noexcept {
    std::size_t offset = 0;
    while (offset < wire.size()) {
        auto remaining = wire.size() - offset;
        auto chunk = static_cast<int>(std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
#ifdef _WIN32
        auto sent = ::send(socket, wire.data() + offset, chunk, 0);
#else
        auto sent = static_cast<int>(::send(socket, wire.data() + offset, static_cast<std::size_t>(chunk), 0));
#endif
        if (sent <= 0) {
            auto error = socket_error();
            if (interrupted_socket_error(error)) {
                continue;
            }
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

std::string peer_address(sockaddr_in const& peer) {
    std::array<char, INET_ADDRSTRLEN> buffer{};
    auto* result = inet_ntop(AF_INET, &peer.sin_addr, buffer.data(), static_cast<socklen_t>(buffer.size()));
    return result == nullptr ? std::string{} : std::string(result);
}

} // namespace

struct HttpServer::Impl {
    Impl(AdminApi const& api_ref, ApiDataProvider const& provider_ref, HttpServerConfig config_value)
        : api(api_ref), provider(provider_ref), config(std::move(config_value)) {
        if (!config.dashboard_assets) {
            config.dashboard_assets = dashboard_asset;
        }
    }

    AdminApi const& api;
    ApiDataProvider const& provider;
    HttpServerConfig config;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> running{false};
    std::atomic<std::uint16_t> bound_port{0};
    std::thread thread;
#ifdef _WIN32
    bool winsock_initialized = false;
#endif

    void serve_client(NativeSocket client, sockaddr_in const& peer) noexcept try {
        auto timeout_ms = std::max<std::uint32_t>(config.receive_timeout_ms, 1);
        if (!set_socket_timeout(client, timeout_ms)) {
            close_socket(client);
            return;
        }

        std::string wire;
        wire.reserve(std::min<std::size_t>(config.http_limits.max_request_bytes, 4096));
        HttpParseResult parse_result = HttpParseResult::Incomplete;
        std::string error;
        HttpRequest request;
        std::size_t consumed = 0;

        while (!stop_requested.load(std::memory_order_relaxed)) {
            parse_result = parse_http_request(wire, request, consumed, error, config.http_limits);
            if (parse_result != HttpParseResult::Incomplete) {
                break;
            }
            if (wire.size() > config.http_limits.max_request_bytes) {
                break;
            }

            std::array<char, 4096> buffer{};
            auto receive_size = buffer.size();
            auto remaining = config.http_limits.max_request_bytes - wire.size();
            if (remaining < receive_size && remaining < std::numeric_limits<std::size_t>::max()) {
                receive_size = remaining + 1;
            }
            if (receive_size == 0) {
                receive_size = 1;
            }
#ifdef _WIN32
            auto received = ::recv(client, buffer.data(), static_cast<int>(receive_size), 0);
#else
            auto received = static_cast<int>(::recv(client, buffer.data(), receive_size, 0));
#endif
            if (received <= 0) {
                close_socket(client);
                return;
            }
            wire.append(buffer.data(), static_cast<std::size_t>(received));
        }

        if (parse_result == HttpParseResult::Incomplete) {
            parse_result = parse_http_request(wire, request, consumed, error, config.http_limits);
        }
        if (parse_result == HttpParseResult::Incomplete) {
            close_socket(client);
            return;
        }

        std::string response;
        if (config.serve_dashboard && request.method == "GET" && config.dashboard_assets) {
            auto asset = config.dashboard_assets(request.target);
            if (asset.has_value()) {
                response = serialize_http_response(ApiResponse{200, std::move(asset->content_type),
                    std::move(asset->body)});
            }
        }
        if (response.empty()) {
            HttpParseResult bridge_result = parse_result;
            response = handle_http_request(wire, peer_address(peer), api, provider, bridge_result, error,
                config.http_limits);
        }
        if (!response.empty()) {
            send_all(client, response);
        }
        close_socket(client);
    } catch (...) {
        // Parser/provider/allocation failures must not terminate the host
        // through a noexcept worker entry point.
        close_socket(client);
    }

    void run(NativeSocket listener) noexcept {
        while (!stop_requested.load(std::memory_order_relaxed)) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(listener, &read_set);
            timeval wait_time{};
            wait_time.tv_usec = 100'000;
#ifdef _WIN32
            auto ready = ::select(0, &read_set, nullptr, nullptr, &wait_time);
#else
            auto ready = ::select(listener + 1, &read_set, nullptr, nullptr, &wait_time);
#endif
            if (ready == 0) {
                continue;
            }
            if (ready < 0) {
                auto error = socket_error();
                if (interrupted_socket_error(error)) {
                    continue;
                }
                break;
            }

            sockaddr_in peer{};
#ifdef _WIN32
            int peer_length = sizeof(peer);
#else
            socklen_t peer_length = sizeof(peer);
#endif
            auto client = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_length);
            if (client == kInvalidSocket) {
                auto error = socket_error();
                if (!interrupted_socket_error(error)) {
                    break;
                }
                continue;
            }
            serve_client(client, peer);
        }

        close_socket(listener);
#ifdef _WIN32
        if (winsock_initialized) {
            WSACleanup();
            winsock_initialized = false;
        }
#endif
        running.store(false, std::memory_order_release);
        bound_port.store(0, std::memory_order_release);
    }
};

HttpServer::HttpServer(AdminApi const& api, ApiDataProvider const& provider, HttpServerConfig config)
    : impl_(std::make_unique<Impl>(api, provider, std::move(config))) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(std::string& error) {
    error.clear();
    if (impl_->thread.joinable()) {
        error = "HTTP server must be stopped before restarting";
        return false;
    }
    if (impl_->running.load(std::memory_order_acquire)) {
        error = "HTTP server is already running";
        return false;
    }
    if (!is_loopback_bind_address(impl_->config.bind_address)) {
        error = "HTTP server bind address must be IPv4 loopback";
        return false;
    }
    if (impl_->config.receive_timeout_ms == 0 || impl_->config.http_limits.max_request_bytes == 0
        || impl_->config.http_limits.max_header_bytes == 0 || impl_->config.http_limits.max_body_bytes == 0
        || impl_->config.http_limits.max_header_count == 0) {
        error = "invalid HTTP server limits";
        return false;
    }

#ifdef _WIN32
    WSADATA winsock_data{};
    auto winsock_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (winsock_result != 0) {
        error = "WSAStartup failed: " + std::to_string(winsock_result);
        return false;
    }
    impl_->winsock_initialized = true;
#endif

    auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) {
        error = "socket creation failed: " + std::to_string(socket_error());
#ifdef _WIN32
        WSACleanup();
        impl_->winsock_initialized = false;
#endif
        return false;
    }

    int reuse = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const*>(&reuse), sizeof(reuse)) != 0) {
        error = "SO_REUSEADDR failed: " + std::to_string(socket_error());
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
        impl_->winsock_initialized = false;
#endif
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(impl_->config.port);
    auto address_text = impl_->config.bind_address == "localhost" ? "127.0.0.1" : impl_->config.bind_address.c_str();
    if (inet_pton(AF_INET, address_text, &address.sin_addr) != 1
        || ::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || ::listen(listener, 8) != 0) {
        error = "HTTP loopback bind/listen failed: " + std::to_string(socket_error());
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
        impl_->winsock_initialized = false;
#endif
        return false;
    }

    sockaddr_in bound_address{};
#ifdef _WIN32
    int bound_length = sizeof(bound_address);
#else
    socklen_t bound_length = sizeof(bound_address);
#endif
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&bound_address), &bound_length) != 0) {
        error = "getsockname failed: " + std::to_string(socket_error());
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
        impl_->winsock_initialized = false;
#endif
        return false;
    }

    impl_->bound_port.store(ntohs(bound_address.sin_port), std::memory_order_release);
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->running.store(true, std::memory_order_release);
    try {
        impl_->thread = std::thread([this, listener] { impl_->run(listener); });
    } catch (...) {
        impl_->running.store(false, std::memory_order_release);
        impl_->bound_port.store(0, std::memory_order_release);
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
        impl_->winsock_initialized = false;
#endif
        error = "failed to start HTTP server thread";
        return false;
    }
    return true;
}

void HttpServer::stop() noexcept {
    if (!impl_) {
        return;
    }
    impl_->stop_requested.store(true, std::memory_order_release);
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
    impl_->running.store(false, std::memory_order_release);
    impl_->bound_port.store(0, std::memory_order_release);
}

bool HttpServer::running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

std::uint16_t HttpServer::port() const noexcept {
    return impl_->bound_port.load(std::memory_order_acquire);
}

} // namespace bedrock_insight
