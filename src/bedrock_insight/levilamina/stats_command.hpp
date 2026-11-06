#pragma once

#include <filesystem>

namespace bedrock_insight::levilamina {
// Registers /stats gui|rank|player. The database path is used read-only by each
// invocation; registration itself does not open SQLite. Safe to call once per
// activation; a repeated call only updates the path.
bool register_stats_commands(std::filesystem::path database_path) noexcept;
} // namespace bedrock_insight::levilamina
