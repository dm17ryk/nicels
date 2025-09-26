#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "nicels/config.h"
#include "nicels/fs.h"
#include "nicels/git_status.h"
#include "nicels/theme.h"

namespace nicels {

class Renderer {
public:
    Renderer(const Config::Options& options, std::ostream& output);

    void render(std::vector<FileEntry> entries);

private:
    void render_long(const std::vector<FileEntry>& entries);
    void render_single_column(const std::vector<FileEntry>& entries);
    void render_columns(const std::vector<FileEntry>& entries, bool horizontal);
    void render_comma_separated(const std::vector<FileEntry>& entries);
    std::string tree_prefix(const FileEntry& entry) const;

    const Config::Options& options_;
    std::ostream& out_;
    Theme theme_;
};

} // namespace nicels
