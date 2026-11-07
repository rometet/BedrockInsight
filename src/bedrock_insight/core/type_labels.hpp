#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace bedrock_insight {
// Read-only cosmetic catalog. A failed load leaves the published catalog intact.
// CUSTOM observation labels always take precedence; IDs/values are not renamed.
bool load_type_labels(std::filesystem::path const&,std::string& error,bool merge=false) noexcept;
std::string stat_display_label(std::string_view key);
}
