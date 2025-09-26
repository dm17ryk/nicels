#pragma once

#include "nicels/options.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace nicels {

[[nodiscard]] bool stdout_is_terminal();
[[nodiscard]] int detect_terminal_width();
[[nodiscard]] std::string owner_name(const std::filesystem::path& path, bool numeric, bool follow_symlink);
[[nodiscard]] std::string group_name(const std::filesystem::path& path, bool numeric, bool follow_symlink);
[[nodiscard]] std::optional<std::uintmax_t> inode_number(const std::filesystem::directory_entry& entry);
[[nodiscard]] bool has_executable_bit(const std::filesystem::directory_entry& entry);
[[nodiscard]] bool is_hidden(const std::filesystem::directory_entry& entry);
[[nodiscard]] bool supports_color(ColorPolicy policy);
[[nodiscard]] std::string resolve_symlink(const std::filesystem::directory_entry& entry, bool follow);

} // namespace nicels
