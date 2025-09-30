#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"
#include "file_info.h"

namespace nls {

struct Entry {
    FileInfo info;
};

struct TreeItem {
    Entry entry;
    std::vector<TreeItem> children;
};

enum class VisitResult {
    Ok = 0,
    Minor = 1,
    Serious = 2,
};

VisitResult combine_visit_result(VisitResult a, VisitResult b);

class FileOwnershipResolver;
class SymlinkResolver;

class FileScanner {
public:
    FileScanner(const Config& config,
                FileOwnershipResolver& ownership_resolver,
                SymlinkResolver& symlink_resolver);

    VisitResult collect_entries(const std::filesystem::path& dir,
                                std::vector<Entry>& out,
                                bool is_top_level) const;

private:
    bool matches_any_pattern(const std::string& name,
                             const std::vector<std::string>& patterns) const;
    bool should_include(const std::string& name, bool is_explicit) const;
    void populate_entry(const std::filesystem::directory_entry& de, Entry& entry) const;
    void apply_symlink_metadata(Entry& entry) const;
    void apply_icon_and_color(Entry& entry) const;
    void report_path_error(const std::filesystem::path& path,
                           const std::error_code& ec,
                           const char* fallback) const;

    bool add_entry(const std::filesystem::directory_entry& de,
                   std::vector<Entry>& out,
                   std::string override_name,
                   bool is_explicit) const;

    const Config& config_;
    FileOwnershipResolver& ownership_resolver_;
    SymlinkResolver& symlink_resolver_;
};

}  // namespace nls

