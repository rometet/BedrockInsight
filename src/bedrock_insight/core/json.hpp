#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bedrock_insight {

struct JsonNumber final {
    std::string raw;
};

class JsonValue final {
public:
    enum class Type : std::uint8_t {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array,
    };

    using Object = std::vector<std::pair<std::string, JsonValue>>;
    using Array = std::vector<JsonValue>;

    JsonValue();
    explicit JsonValue(std::nullptr_t);
    explicit JsonValue(bool value);
    explicit JsonValue(JsonNumber value);
    explicit JsonValue(std::string value);
    explicit JsonValue(Object value);
    explicit JsonValue(Array value);

    [[nodiscard]] Type type() const noexcept;
    [[nodiscard]] bool boolean() const;
    [[nodiscard]] std::string_view number() const;
    [[nodiscard]] std::string_view string() const;
    [[nodiscard]] Object const& object() const;
    [[nodiscard]] Array const& array() const;
    [[nodiscard]] JsonValue const* find(std::string_view key) const;

private:
    std::variant<std::nullptr_t, bool, JsonNumber, std::string, Object, Array> value_;
};

struct JsonLimits final {
    std::size_t max_bytes = 48 * 1024;
    std::size_t max_depth = 16;
    std::size_t max_values = 1024;
    std::size_t max_string_bytes = 16 * 1024;
};

enum class JsonParseResult {
    Complete,
    Invalid,
    TooLarge,
};

JsonParseResult parse_json(std::string_view input, JsonValue& value, std::string& error, JsonLimits limits = {});

bool json_require_object(JsonValue const& value, std::string& error);
bool json_string_field(JsonValue const& object, std::string_view key, std::string& result, bool required,
    std::string& error);
bool json_integer_field(JsonValue const& object, std::string_view key, std::int64_t& result, bool required,
    std::string& error);
bool json_boolean_field(JsonValue const& object, std::string_view key, bool& result, bool required,
    std::string& error);

} // namespace bedrock_insight
