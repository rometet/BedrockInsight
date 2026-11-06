#include "bedrock_insight/core/http_transport.hpp"
#include "bedrock_insight/core/json.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <utility>

namespace bedrock_insight {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool has_control(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](char character) {
        auto byte = static_cast<unsigned char>(character);
        return byte < 0x20 || byte == 0x7f;
    });
}

bool valid_token(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        auto byte = static_cast<unsigned char>(character);
        if (byte <= 0x20 || byte >= 0x7f) {
            return false;
        }
        switch (character) {
        case '(': case ')': case '<': case '>': case '@': case ',': case ';': case ':':
        case '\\': case '"': case '/': case '[': case ']': case '?': case '=':
        case '{': case '}': case ' ':
            return false;
        default:
            return true;
        }
    });
}

bool equals_ignore_case(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    return std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
}

bool is_json_content_type(std::string_view value) {
    value = trim(value);
    auto separator = value.find(';');
    auto media_type = trim(value.substr(0, separator));
    return equals_ignore_case(media_type, "application/json");
}

std::string_view header_value(HttpRequest const& request, std::string_view name) {
    auto header = std::find_if(request.headers.begin(), request.headers.end(), [&](HttpHeader const& candidate) {
        return equals_ignore_case(candidate.name, name);
    });
    return header == request.headers.end() ? std::string_view{} : header->value;
}

bool has_header(HttpRequest const& request, std::string_view name) {
    return std::any_of(request.headers.begin(), request.headers.end(), [&](HttpHeader const& candidate) {
        return equals_ignore_case(candidate.name, name);
    });
}

bool parse_size(std::string_view value, std::size_t& result) {
    value = trim(value);
    if (value.empty()) {
        return false;
    }
    std::size_t parsed = 0;
    auto conversion = std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (conversion.ec != std::errc{} || conversion.ptr != value.data() + value.size()) {
        return false;
    }
    result = parsed;
    return true;
}

ApiResponse http_error(int status, std::string_view body) {
    return ApiResponse{status, "application/json", std::string("{\"error\":\"") + std::string(body) + "\"}"};
}

std::string_view status_text(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default: return "Unknown";
    }
}

std::string bearer_token(std::string_view authorization) {
    authorization = trim(authorization);
    constexpr std::string_view scheme = "Bearer";
    if (authorization.size() <= scheme.size()
        || !equals_ignore_case(authorization.substr(0, scheme.size()), scheme)
        || authorization[scheme.size()] != ' ') {
        return {};
    }
    auto token = trim(authorization.substr(scheme.size() + 1));
    if (token.empty() || token.find_first_of(" \t\r\n") != std::string_view::npos) {
        return {};
    }
    return std::string(token);
}

} // namespace

std::string_view http_header_value(HttpRequest const& request, std::string_view name) {
    return header_value(request, name);
}

HttpParseResult parse_http_request(std::string_view wire, HttpRequest& request, std::size_t& consumed,
    std::string& error, HttpLimits limits) {
    request = {};
    consumed = 0;
    error.clear();
    if (limits.max_request_bytes == 0 || limits.max_header_bytes == 0 || limits.max_body_bytes == 0
        || limits.max_header_count == 0) {
        error = "invalid HTTP limits";
        return HttpParseResult::BadRequest;
    }
    if (wire.size() > limits.max_request_bytes) {
        error = "request exceeds byte limit";
        return HttpParseResult::TooLarge;
    }

    auto header_end = wire.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        if (wire.size() > limits.max_header_bytes) {
            error = "headers exceed byte limit";
            return HttpParseResult::TooLarge;
        }
        return HttpParseResult::Incomplete;
    }
    auto header_bytes = header_end + 4;
    if (header_bytes > limits.max_header_bytes) {
        error = "headers exceed byte limit";
        return HttpParseResult::TooLarge;
    }

    auto first_line_end = wire.find("\r\n");
    if (first_line_end == std::string_view::npos || first_line_end >= header_end) {
        error = "missing request line";
        return HttpParseResult::BadRequest;
    }
    auto request_line = wire.substr(0, first_line_end);
    auto method_end = request_line.find(' ');
    auto target_end = method_end == std::string_view::npos ? std::string_view::npos
                                                            : request_line.find(' ', method_end + 1);
    if (method_end == std::string_view::npos || target_end == std::string_view::npos
        || !valid_token(request_line.substr(0, method_end))
        || request_line.substr(method_end + 1, target_end - method_end - 1).empty()
        || request_line.substr(target_end + 1) != "HTTP/1.1") {
        error = "invalid HTTP/1.1 request line";
        return HttpParseResult::BadRequest;
    }
    auto target = request_line.substr(method_end + 1, target_end - method_end - 1);
    if (target.front() != '/' || has_control(target)) {
        error = "invalid request target";
        return HttpParseResult::BadRequest;
    }
    request.method = std::string(request_line.substr(0, method_end));
    request.target = std::string(target);

    auto header_cursor = first_line_end + 2;
    while (header_cursor < header_end) {
        auto line_end = wire.find("\r\n", header_cursor);
        if (line_end == std::string_view::npos || line_end > header_end) {
            error = "malformed header line";
            return HttpParseResult::BadRequest;
        }
        auto line = wire.substr(header_cursor, line_end - header_cursor);
        auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            error = "malformed header line";
            return HttpParseResult::BadRequest;
        }
        auto name = line.substr(0, colon);
        auto value = trim(line.substr(colon + 1));
        if (!valid_token(name) || has_control(value) || has_header(request, name)) {
            error = "invalid or duplicate header";
            return HttpParseResult::BadRequest;
        }
        request.headers.push_back(HttpHeader{std::string(name), std::string(value)});
        if (request.headers.size() > limits.max_header_count) {
            error = "header count exceeds limit";
            return HttpParseResult::TooLarge;
        }
        header_cursor = line_end + 2;
    }

    if (!has_header(request, "Host")) {
        error = "Host header is required";
        return HttpParseResult::BadRequest;
    }
    if (has_header(request, "Transfer-Encoding")) {
        error = "transfer encoding is unsupported";
        return HttpParseResult::BadRequest;
    }

    std::size_t body_bytes = 0;
    auto content_length = header_value(request, "Content-Length");
    if (!content_length.empty() && !parse_size(content_length, body_bytes)) {
        error = "invalid content length";
        return HttpParseResult::BadRequest;
    }
    if (header_bytes > limits.max_request_bytes
        || body_bytes > limits.max_body_bytes
        || body_bytes > limits.max_request_bytes - header_bytes) {
        error = "body exceeds byte limit";
        return HttpParseResult::TooLarge;
    }
    auto total_bytes = header_bytes + body_bytes;
    if (wire.size() < total_bytes) {
        return HttpParseResult::Incomplete;
    }
    request.body = std::string(wire.substr(header_bytes, body_bytes));
    consumed = total_bytes;
    return HttpParseResult::Complete;
}

std::string serialize_http_response(ApiResponse const& response) {
    auto body = response.body;
    auto content_type = response.content_type.empty() ? "application/json" : response.content_type;
    std::string wire;
    wire.reserve(body.size() + content_type.size() + 128);
    wire += "HTTP/1.1 ";
    wire += std::to_string(response.status);
    wire += " ";
    wire += status_text(response.status);
    wire += "\r\nContent-Type: ";
    wire += content_type;
    wire += "\r\nContent-Length: ";
    wire += std::to_string(body.size());
    wire += "\r\nConnection: close\r\n\r\n";
    wire += body;
    return wire;
}

std::string handle_http_request(std::string_view wire, std::string_view remote_address, AdminApi const& api,
    ApiDataProvider const& provider, HttpParseResult& parse_result, std::string& error, HttpLimits limits) {
    HttpRequest request;
    std::size_t consumed = 0;
    parse_result = parse_http_request(wire, request, consumed, error, limits);
    if (parse_result == HttpParseResult::Incomplete) {
        return {};
    }
    if (parse_result == HttpParseResult::BadRequest) {
        return serialize_http_response(http_error(400, "bad request"));
    }
    if (parse_result == HttpParseResult::TooLarge) {
        return serialize_http_response(http_error(413, "request too large"));
    }

    if (request.method == "POST" || !request.body.empty()) {
        if (request.body.empty()) {
            error = "JSON request body is required";
            return serialize_http_response(http_error(400, "JSON body required"));
        }
        if (!is_json_content_type(header_value(request, "Content-Type"))) {
            error = "application/json content type is required";
            return serialize_http_response(http_error(400, "JSON content type required"));
        }
        JsonValue document;
        JsonLimits json_limits;
        json_limits.max_bytes = limits.max_body_bytes;
        auto json_result = parse_json(request.body, document, error, json_limits);
        if (json_result == JsonParseResult::TooLarge) {
            return serialize_http_response(http_error(413, "JSON body too large"));
        }
        if (json_result != JsonParseResult::Complete || document.type() != JsonValue::Type::Object) {
            return serialize_http_response(http_error(400, "valid JSON object required"));
        }
    }

    ApiRequest api_request;
    api_request.method = std::move(request.method);
    api_request.path = std::move(request.target);
    api_request.bearer_token = bearer_token(http_header_value(request, "Authorization"));
    api_request.body = std::move(request.body);
    api_request.remote_address = std::string(remote_address);
    auto response = api.handle(api_request, provider);
    return serialize_http_response(response);
}

} // namespace bedrock_insight
