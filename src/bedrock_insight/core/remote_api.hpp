#pragma once

#include <filesystem>
#include "bedrock_insight/core/stats_api.h"

namespace bedrock_insight {
// Publishes the database path used by the exported companion statistics API.
// Called once during mod load; it does not open the database.
void set_remote_api_database(std::filesystem::path database_path);
} // namespace bedrock_insight
