#include "bedrock_insight/core/api_payload.hpp"

#include <array>
#include <charconv>
#include <limits>
#include <optional>

namespace bedrock_insight {
namespace {

bool required_nonempty_string(JsonValue const& object, std::string_view key, std::string& result, std::string& error) {
    if (!json_string_field(object, key, result, true, error)) {
        return false;
    }
    if (result.empty()) {
        error = "JSON string field must not be empty: " + std::string(key);
        return false;
    }
    return true;
}

bool optional_integer(JsonValue const& object, std::string_view key, std::int64_t& result, std::string& error) {
    return json_integer_field(object, key, result, false, error);
}

bool convert_int8(std::int64_t value, std::int8_t& result, std::string_view key, std::string& error) {
    if (value < std::numeric_limits<std::int8_t>::min() || value > std::numeric_limits<std::int8_t>::max()) {
        error = "JSON integer out of range: " + std::string(key);
        return false;
    }
    result = static_cast<std::int8_t>(value);
    return true;
}

bool convert_int32(std::int64_t value, std::int32_t& result, std::string_view key, std::string& error) {
    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
        error = "JSON integer out of range: " + std::string(key);
        return false;
    }
    result = static_cast<std::int32_t>(value);
    return true;
}

bool convert_limit(std::int64_t value, std::size_t& result, std::string& error) {
    if (value < 1 || value > 10'000) {
        error = "JSON limit must be between 1 and 10000";
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

bool parse_unsigned(JsonValue const& value, std::uint64_t& result, std::string& error) {
    if (value.type() != JsonValue::Type::Number) {
        error = "JSON event id must be an unsigned integer";
        return false;
    }
    auto raw = value.number();
    auto conversion = std::from_chars(raw.data(), raw.data() + raw.size(), result, 10);
    if (conversion.ec != std::errc{} || conversion.ptr != raw.data() + raw.size()) {
        error = "JSON event id is out of range or not integral";
        return false;
    }
    return true;
}

bool parse_object(std::string_view input, JsonValue& object, std::string& error, JsonParseResult& result,
    JsonLimits limits) {
    result = parse_json(input, object, error, limits);
    if (result != JsonParseResult::Complete) {
        return false;
    }
    if (!json_require_object(object, error)) {
        result = JsonParseResult::Invalid;
        return false;
    }
    return true;
}

} // namespace

JsonParseResult parse_rollback_api_payload(std::string_view input, RollbackApiPayload& result, std::string& error,
    JsonLimits limits) {
    result = {};
    JsonValue object;
    JsonParseResult parse_result = JsonParseResult::Invalid;
    if (!parse_object(input, object, error, parse_result, limits)) {
        return parse_result;
    }

    std::string operation;
    if (!required_nonempty_string(object, "operation", operation, error)
        || !required_nonempty_string(object, "transaction_id", result.transaction_id, error)
        || !required_nonempty_string(object, "reason", result.reason, error)) {
        return JsonParseResult::Invalid;
    }
    if (operation == "undo") {
        result.operation = RollbackOperation::Undo;
    } else if (operation == "restore") {
        result.operation = RollbackOperation::Restore;
    } else {
        error = "JSON operation must be undo or restore";
        return JsonParseResult::Invalid;
    }

    auto event_ids = object.find("event_ids");
    if (event_ids == nullptr || event_ids->type() != JsonValue::Type::Array || event_ids->array().empty()) {
        error = "JSON event_ids must be a non-empty array";
        return JsonParseResult::Invalid;
    }
    result.event_ids.reserve(event_ids->array().size());
    for (auto const& value : event_ids->array()) {
        std::uint64_t event_id = 0;
        if (!parse_unsigned(value, event_id, error)) {
            return JsonParseResult::Invalid;
        }
        result.event_ids.push_back(event_id);
    }
    error.clear();
    return JsonParseResult::Complete;
}

JsonParseResult parse_audit_filter_api_payload(std::string_view input, AuditFilter& result, std::string& error,
    JsonLimits limits) {
    result = {};
    JsonValue object;
    JsonParseResult parse_result = JsonParseResult::Invalid;
    if (!parse_object(input, object, error, parse_result, limits)) {
        return parse_result;
    }

    if (!json_string_field(object, "event_kind", result.event_kind, false, error)
        || !json_string_field(object, "actor_kind", result.actor_kind, false, error)
        || !json_string_field(object, "actor_id", result.actor_id, false, error)
        || !json_string_field(object, "world_id", result.world_id, false, error)
        || !json_string_field(object, "before_type", result.before_type, false, error)
        || !json_string_field(object, "after_type", result.after_type, false, error)
        || !json_string_field(object, "primary_type", result.primary_type, false, error)
        || !json_string_field(object, "secondary_type", result.secondary_type, false, error)
        || !json_string_field(object, "cause", result.cause, false, error)) {
        return JsonParseResult::Invalid;
    }

    std::int64_t integer_value = 0;
    if (!optional_integer(object, "event_id", integer_value, error)) return JsonParseResult::Invalid;
    if (object.find("event_id") != nullptr) {
        if (integer_value < 0) {
            error = "JSON event_id must be non-negative";
            return JsonParseResult::Invalid;
        }
        result.event_id = static_cast<std::uint64_t>(integer_value);
    }
    if (!optional_integer(object, "start_ms", integer_value, error)) return JsonParseResult::Invalid;
    if (object.find("start_ms") != nullptr) result.start_ms = integer_value;
    if (!optional_integer(object, "end_ms", integer_value, error)) return JsonParseResult::Invalid;
    if (object.find("end_ms") != nullptr) result.end_ms = integer_value;
    if (!optional_integer(object, "dimension", integer_value, error)) return JsonParseResult::Invalid;
    if (object.find("dimension") != nullptr && !convert_int8(integer_value, result.dimension.emplace(), "dimension", error)) {
        return JsonParseResult::Invalid;
    }

    struct CoordinateField {
        char const* name;
        std::optional<std::int32_t>& target;
    };
    std::array<CoordinateField, 6> coordinates = {{
        {"min_x", result.min_x}, {"max_x", result.max_x}, {"min_y", result.min_y},
        {"max_y", result.max_y}, {"min_z", result.min_z}, {"max_z", result.max_z},
    }};
    for (auto const& coordinate : coordinates) {
        if (!optional_integer(object, coordinate.name, integer_value, error)) return JsonParseResult::Invalid;
        if (object.find(coordinate.name) != nullptr) {
            auto& value = coordinate.target.emplace();
            if (!convert_int32(integer_value, value, coordinate.name, error)) return JsonParseResult::Invalid;
        }
    }
    if (!optional_integer(object, "limit", integer_value, error)) return JsonParseResult::Invalid;
    if (object.find("limit") != nullptr && !convert_limit(integer_value, result.limit, error)) {
        return JsonParseResult::Invalid;
    }
    if (!json_boolean_field(object, "newest_first", result.newest_first, false, error)) {
        return JsonParseResult::Invalid;
    }
    error.clear();
    return JsonParseResult::Complete;
}

} // namespace bedrock_insight
