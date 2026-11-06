#include "bedrock_insight/core/json.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <utility>

namespace bedrock_insight {
namespace {

class Parser final {
public:
    Parser(std::string_view input, JsonLimits limits, std::string& error)
        : input_(input), limits_(limits), error_(error) {}

    bool parse(JsonValue& result) {
        skip_whitespace();
        if (!parse_value(result, 0)) {
            return false;
        }
        skip_whitespace();
        if (position_ != input_.size()) {
            return fail("trailing JSON data");
        }
        return true;
    }

    [[nodiscard]] bool too_large() const noexcept {
        return too_large_;
    }

private:
    bool fail(char const* message) {
        if (error_.empty()) {
            error_ = message;
        }
        return false;
    }

    bool fail_limit(char const* message) {
        too_large_ = true;
        return fail(message);
    }

    void skip_whitespace() {
        while (position_ < input_.size()) {
            auto character = input_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
                break;
            }
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    bool parse_value(JsonValue& result, std::size_t depth) {
        if (++value_count_ > limits_.max_values) {
            return fail_limit("JSON value count exceeds limit");
        }
        if (depth > limits_.max_depth) {
            return fail_limit("JSON nesting exceeds limit");
        }
        skip_whitespace();
        if (position_ >= input_.size()) {
            return fail("missing JSON value");
        }
        switch (input_[position_]) {
        case 'n': return parse_literal("null", JsonValue(nullptr), result);
        case 't': return parse_literal("true", JsonValue(true), result);
        case 'f': return parse_literal("false", JsonValue(false), result);
        case '"': {
            std::string parsed;
            if (!parse_string(parsed)) {
                return false;
            }
            result = JsonValue(std::move(parsed));
            return true;
        }
        case '{': return parse_object(result, depth);
        case '[': return parse_array(result, depth);
        default:
            if (input_[position_] == '-' || std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return parse_number(result);
            }
            return fail("invalid JSON value");
        }
    }

    bool parse_literal(std::string_view literal, JsonValue value, JsonValue& result) {
        if (input_.substr(position_, literal.size()) != literal) {
            return fail("invalid JSON literal");
        }
        position_ += literal.size();
        result = std::move(value);
        return true;
    }

    static int hex_digit(char character) {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    }

    bool append_codepoint(std::string& result, std::uint32_t codepoint) {
        if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return fail("invalid JSON Unicode codepoint");
        }
        if (codepoint <= 0x7f) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        return true;
    }

    bool parse_unicode_escape(std::string& result) {
        if (position_ + 4 > input_.size()) {
            return fail("incomplete JSON Unicode escape");
        }
        std::uint32_t codepoint = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            auto digit = hex_digit(input_[position_ + index]);
            if (digit < 0) {
                return fail("invalid JSON Unicode escape");
            }
            codepoint = (codepoint << 4) | static_cast<std::uint32_t>(digit);
        }
        position_ += 4;
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (position_ + 6 > input_.size() || input_[position_] != '\\' || input_[position_ + 1] != 'u') {
                return fail("unpaired JSON high surrogate");
            }
            position_ += 2;
            std::uint32_t low = 0;
            for (std::size_t index = 0; index < 4; ++index) {
                auto digit = hex_digit(input_[position_ + index]);
                if (digit < 0) {
                    return fail("invalid JSON low surrogate");
                }
                low = (low << 4) | static_cast<std::uint32_t>(digit);
            }
            if (low < 0xdc00 || low > 0xdfff) {
                return fail("invalid JSON low surrogate");
            }
            position_ += 4;
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
        }
        return append_codepoint(result, codepoint);
    }

    bool parse_string(std::string& result) {
        if (!consume('"')) {
            return fail("JSON string must start with quote");
        }
        result.clear();
        while (position_ < input_.size()) {
            auto character = input_[position_++];
            if (character == '"') {
                if (result.size() > limits_.max_string_bytes) {
                    return fail_limit("JSON string exceeds limit");
                }
                return true;
            }
            if (static_cast<unsigned char>(character) < 0x20) {
                return fail("control character in JSON string");
            }
            if (character != '\\') {
                result.push_back(character);
            } else {
                if (position_ >= input_.size()) {
                    return fail("incomplete JSON escape");
                }
                switch (input_[position_++]) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u':
                    if (!parse_unicode_escape(result)) {
                        return false;
                    }
                    break;
                default:
                    return fail("invalid JSON escape");
                }
            }
            if (result.size() > limits_.max_string_bytes) {
                return fail_limit("JSON string exceeds limit");
            }
        }
        return fail("unterminated JSON string");
    }

    bool parse_number(JsonValue& result) {
        auto start = position_;
        if (input_[position_] == '-') {
            ++position_;
        }
        if (position_ >= input_.size()) {
            return fail("incomplete JSON number");
        }
        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return fail("leading zero in JSON number");
            }
        } else {
            if (input_[position_] < '1' || input_[position_] > '9') {
                return fail("invalid JSON number");
            }
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            auto fraction_start = position_;
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
            if (position_ == fraction_start) {
                return fail("missing JSON fraction digits");
            }
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            auto exponent_start = position_;
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
            if (position_ == exponent_start) {
                return fail("missing JSON exponent digits");
            }
        }
        result = JsonValue(JsonNumber{std::string(input_.substr(start, position_ - start))});
        return true;
    }

    bool parse_object(JsonValue& result, std::size_t depth) {
        consume('{');
        JsonValue::Object object;
        skip_whitespace();
        if (consume('}')) {
            result = JsonValue(std::move(object));
            return true;
        }
        while (true) {
            skip_whitespace();
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            if (std::any_of(object.begin(), object.end(), [&](auto const& member) { return member.first == key; })) {
                return fail("duplicate JSON object key");
            }
            skip_whitespace();
            if (!consume(':')) {
                return fail("missing JSON object colon");
            }
            JsonValue child;
            if (!parse_value(child, depth + 1)) {
                return false;
            }
            object.emplace_back(std::move(key), std::move(child));
            skip_whitespace();
            if (consume('}')) {
                result = JsonValue(std::move(object));
                return true;
            }
            if (!consume(',')) {
                return fail("missing JSON object comma");
            }
        }
    }

    bool parse_array(JsonValue& result, std::size_t depth) {
        consume('[');
        JsonValue::Array array;
        skip_whitespace();
        if (consume(']')) {
            result = JsonValue(std::move(array));
            return true;
        }
        while (true) {
            JsonValue child;
            if (!parse_value(child, depth + 1)) {
                return false;
            }
            array.push_back(std::move(child));
            skip_whitespace();
            if (consume(']')) {
                result = JsonValue(std::move(array));
                return true;
            }
            if (!consume(',')) {
                return fail("missing JSON array comma");
            }
        }
    }

    std::string_view input_;
    JsonLimits limits_;
    std::string& error_;
    std::size_t position_ = 0;
    std::size_t value_count_ = 0;
    bool too_large_ = false;
};

} // namespace

JsonValue::JsonValue()
    : value_(nullptr) {}

JsonValue::JsonValue(std::nullptr_t)
    : value_(nullptr) {}

JsonValue::JsonValue(bool value)
    : value_(value) {}

JsonValue::JsonValue(JsonNumber value)
    : value_(std::move(value)) {}

JsonValue::JsonValue(std::string value)
    : value_(std::move(value)) {}

JsonValue::JsonValue(Object value)
    : value_(std::move(value)) {}

JsonValue::JsonValue(Array value)
    : value_(std::move(value)) {}

JsonValue::Type JsonValue::type() const noexcept {
    switch (value_.index()) {
    case 0: return Type::Null;
    case 1: return Type::Boolean;
    case 2: return Type::Number;
    case 3: return Type::String;
    case 4: return Type::Object;
    case 5: return Type::Array;
    default: return Type::Null;
    }
}

bool JsonValue::boolean() const {
    return std::get<bool>(value_);
}

std::string_view JsonValue::number() const {
    return std::get<JsonNumber>(value_).raw;
}

std::string_view JsonValue::string() const {
    return std::get<std::string>(value_);
}

JsonValue::Object const& JsonValue::object() const {
    return std::get<Object>(value_);
}

JsonValue::Array const& JsonValue::array() const {
    return std::get<Array>(value_);
}

JsonValue const* JsonValue::find(std::string_view key) const {
    if (type() != Type::Object) {
        return nullptr;
    }
    auto member = std::find_if(object().begin(), object().end(), [&](auto const& candidate) {
        return candidate.first == key;
    });
    return member == object().end() ? nullptr : &member->second;
}

JsonParseResult parse_json(std::string_view input, JsonValue& value, std::string& error, JsonLimits limits) {
    value = JsonValue();
    error.clear();
    if (limits.max_bytes == 0 || limits.max_depth == 0 || limits.max_values == 0 || limits.max_string_bytes == 0) {
        error = "invalid JSON limits";
        return JsonParseResult::Invalid;
    }
    if (input.size() > limits.max_bytes) {
        error = "JSON input exceeds byte limit";
        return JsonParseResult::TooLarge;
    }
    Parser parser(input, limits, error);
    if (!parser.parse(value)) {
        return parser.too_large() ? JsonParseResult::TooLarge : JsonParseResult::Invalid;
    }
    return JsonParseResult::Complete;
}

bool json_require_object(JsonValue const& value, std::string& error) {
    if (value.type() != JsonValue::Type::Object) {
        error = "JSON object required";
        return false;
    }
    error.clear();
    return true;
}

bool json_string_field(JsonValue const& object, std::string_view key, std::string& result, bool required,
    std::string& error) {
    if (!json_require_object(object, error)) {
        return false;
    }
    auto value = object.find(key);
    if (value == nullptr) {
        result.clear();
        if (required) {
            error = "missing JSON string field: " + std::string(key);
            return false;
        }
        error.clear();
        return true;
    }
    if (value->type() != JsonValue::Type::String) {
        error = "JSON field is not a string: " + std::string(key);
        return false;
    }
    result = std::string(value->string());
    error.clear();
    return true;
}

bool json_integer_field(JsonValue const& object, std::string_view key, std::int64_t& result, bool required,
    std::string& error) {
    if (!json_require_object(object, error)) {
        return false;
    }
    auto value = object.find(key);
    if (value == nullptr) {
        result = 0;
        if (required) {
            error = "missing JSON integer field: " + std::string(key);
            return false;
        }
        error.clear();
        return true;
    }
    if (value->type() != JsonValue::Type::Number) {
        error = "JSON field is not an integer: " + std::string(key);
        return false;
    }
    auto raw = value->number();
    auto conversion = std::from_chars(raw.data(), raw.data() + raw.size(), result, 10);
    if (conversion.ec != std::errc{} || conversion.ptr != raw.data() + raw.size()) {
        error = "JSON integer is out of range or not integral: " + std::string(key);
        return false;
    }
    error.clear();
    return true;
}

bool json_boolean_field(JsonValue const& object, std::string_view key, bool& result, bool required,
    std::string& error) {
    if (!json_require_object(object, error)) {
        return false;
    }
    auto value = object.find(key);
    if (value == nullptr) {
        result = false;
        if (required) {
            error = "missing JSON boolean field: " + std::string(key);
            return false;
        }
        error.clear();
        return true;
    }
    if (value->type() != JsonValue::Type::Boolean) {
        error = "JSON field is not a boolean: " + std::string(key);
        return false;
    }
    result = value->boolean();
    error.clear();
    return true;
}

} // namespace bedrock_insight
