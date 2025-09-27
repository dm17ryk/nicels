#pragma once

#include "nicels/config.hpp"
#include "nicels/entry.hpp"

#include <filesystem>
#include <ostream>
#include <span>

namespace nicels {

class Renderer {
public:
    Renderer(const Config::Data& config, std::ostream& stream);

    void render(const std::filesystem::path& root, std::span<const FileEntry> entries) const;

private:
    const Config::Data& config_;
    std::ostream& out_;

    [[nodiscard]] std::string format_permissions(const FileEntry& entry) const;
    [[nodiscard]] std::string format_size(const FileEntry& entry) const;
    [[nodiscard]] std::string format_time(const FileEntry& entry) const;
    [[nodiscard]] std::string format_git(const FileEntry& entry) const;
    [[nodiscard]] std::string format_name(const FileEntry& entry) const;
    [[nodiscard]] std::string apply_hyperlink(const FileEntry& entry, std::string_view text) const;
    [[nodiscard]] std::string colorize(const FileEntry& entry, std::string_view text) const;
};

} // namespace nicels
