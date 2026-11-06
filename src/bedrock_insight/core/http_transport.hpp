#pragma once

#include "bedrock_insight/core/admin_api.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace bedrock_insight {

struct HttpLimits {
    std::size_t max_request_bytes = 64 * 1024;
    std::size_t max_header_bytes = 16 * 1024;
    std::size_t max_body_bytes = 48 * 1024;
    std::size_t max_header_count = 64;
};

enum class HttpParseResult {
    Complete,
    Incomplete,
    BadRequest,
    TooLarge,
};

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpRequest {
    std::string method;
    std::string target;
    std::string body;
    std::vector<HttpHeader> headers;
};

std::string_view http_header_value(HttpRequest const& request, std::string_view name);

// Parses one bounded HTTP/1.1 request. No socket is opened here; callers can
// feed a receive buffer repeatedly until Complete. `consumed` identifies the
// bytes belonging to this request and allows a future listener to handle a
// connection buffer without guessing at body boundaries.
HttpParseResult parse_http_request(std::string_view wire, HttpRequest& request, std::size_t& consumed,
    std::string& error, HttpLimits limits = {});

std::string serialize_http_response(ApiResponse const& response);

// Bridges the wire format to the authenticated API core. Incomplete input
// returns an empty string so a socket adapter can read more bytes. Malformed
// or oversized input gets a bounded JSON error response and never reaches the
// API/token layer.
std::string handle_http_request(std::string_view wire, std::string_view remote_address, AdminApi const& api,
    ApiDataProvider const& provider, HttpParseResult& parse_result, std::string& error, HttpLimits limits = {});

} // namespace bedrock_insight
