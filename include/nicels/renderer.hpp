#pragma once

#include "nicels/filesystem_scanner.hpp"
#include "nicels/git_status.hpp"
#include "nicels/options.hpp"
#include "nicels/theme.hpp"

#include <iosfwd>
#include <vector>

namespace nicels {

class Renderer {
public:
    Renderer(const Options& options, GitStatusCache& git_cache);

    void render(const std::vector<DirectoryResult>& results, std::ostream& os) const;

private:
    const Options& options_;
    GitStatusCache& git_cache_;
    Theme theme_;

    void render_directory(const DirectoryResult& dir, std::ostream& os, bool multiple) const;
    void render_entries(const std::vector<FileEntry>& entries, std::ostream& os) const;
    void render_entry(const FileEntry& entry, std::ostream& os) const;
    void render_long(const FileEntry& entry, std::ostream& os) const;
    void render_columns(const std::vector<FileEntry>& entries, std::ostream& os) const;
    void render_single(const std::vector<FileEntry>& entries, std::ostream& os) const;
    void render_tree_entry(const FileEntry& entry, const DirectoryResult* child_dir, std::ostream& os, const std::string& indent, bool is_last) const;
    [[nodiscard]] std::string entry_label(const FileEntry& entry) const;
};

} // namespace nicels
