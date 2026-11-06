#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bedrock_insight {

struct DashboardAsset final {
    std::string content_type;
    std::string body;
};

// Returns embedded, immutable dashboard assets for exact paths only. The
// listener can serve these without filesystem I/O or a web-framework
// dependency; unknown paths are deliberately not guessed or normalized.
std::optional<DashboardAsset> dashboard_asset(std::string_view path);

} // namespace bedrock_insight
