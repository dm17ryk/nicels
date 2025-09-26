#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "nicels/config.hpp"
#include "nicels/fs_scanner.hpp"
#include "nicels/git_status.hpp"
#include "nicels/theme.hpp"

namespace nicels {

class Renderer {
public:
    Renderer(const ListingOptions& options, const Theme& theme, GitStatusCache& git_cache);

    void render_entries(const std::vector<FileEntry>& entries) const;
    void render_single(const FileEntry& entry) const;
    void render_tree(const FileEntry& root, const FileSystemScanner& scanner, std::optional<int> depth_limit) const;
    [[nodiscard]] std::vector<FileEntry> prepare_entries(std::vector<FileEntry> entries) const;

private:
    std::string format_name(const FileEntry& entry) const;
    std::string format_line(const FileEntry& entry) const;
    std::optional<char> git_indicator(const FileEntry& entry) const;

    const ListingOptions& options_;
    const Theme& theme_;
    GitStatusCache& git_cache_;
};

} // namespace nicels
