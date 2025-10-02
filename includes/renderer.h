#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "config.h"
#include "fs_scanner.h"
#include "permission_formatter.h"
#include "size_formatter.h"
#include "time_formatter.h"

namespace nls {

class Renderer {
public:
    explicit Renderer(const Config& config);

    void PrintPathHeader(const std::filesystem::path& path) const;
    void PrintDirectoryHeader(const std::filesystem::path& path, bool is_directory) const;

    void RenderTree(const std::vector<TreeItem>& nodes,
                    const std::vector<Entry>& flat_entries) const;

    void RenderEntries(const std::vector<Entry>& entries) const;

    void RenderReport(const std::vector<Entry>& entries) const;

    void TerminateLine() const;

private:
    struct LongFormatColumns {
        size_t inode_width = 0;
        size_t block_width = 0;
        size_t perm_width = 10;
        size_t nlink_width = 0;
        size_t owner_width = 0;
        size_t group_width = 0;
        size_t size_width = 0;
        size_t time_width = 0;
        size_t git_width = 0;
    };

    struct ReportStats {
        size_t total = 0;
        size_t folders = 0;
        size_t recognized_files = 0;
        size_t unrecognized_files = 0;
        size_t links = 0;
        size_t dead_links = 0;
        uintmax_t total_size = 0;

        size_t files() const { return recognized_files + unrecognized_files; }
    };

    const Config& opt_;
    SizeFormatter size_formatter_;
    TimeFormatter time_formatter_;
    PermissionFormatter permission_formatter_;

    std::string ApplyControlCharHandling(const std::string& name) const;
    std::string ApplyQuoting(const std::string& name) const;
    std::string StyledName(const Entry& entry) const;
    std::string FormatEntryCell(const Entry& entry,
                                size_t inode_width,
                                size_t block_width,
                                bool include_git_prefix) const;

    LongFormatColumns ComputeLongColumns(const std::vector<Entry>& entries,
                                         size_t inode_width,
                                         size_t block_width) const;
    void PrintLongHeader(const LongFormatColumns& columns) const;
    void PrintLongEntry(const Entry& entry, const LongFormatColumns& columns) const;

    std::string OwnerDisplay(const Entry& entry) const;
    std::string GroupDisplay(const Entry& entry) const;

    size_t ComputeInodeWidth(const std::vector<Entry>& entries) const;
    size_t ComputeBlockWidth(const std::vector<Entry>& entries) const;

    std::string BlockDisplay(const Entry& entry) const;
    std::string FormatSizeValue(uintmax_t size) const;
    size_t PrintableWidth(const std::string& text) const;
    int EffectiveTerminalWidth() const;

    void PrintTreeNodes(const std::vector<TreeItem>& nodes,
                        size_t inode_width,
                        size_t block_width,
                        const LongFormatColumns* long_columns,
                        std::vector<bool>& branch_stack) const;

    void PrintLong(const std::vector<Entry>& entries,
                   size_t inode_width,
                   size_t block_width) const;
    void PrintColumns(const std::vector<Entry>& entries,
                      size_t inode_width,
                      size_t block_width) const;
    void PrintCommaSeparated(const std::vector<Entry>& entries,
                             size_t inode_width,
                             size_t block_width) const;

    ReportStats ComputeReportStats(const std::vector<Entry>& entries) const;
    void PrintReportShort(const ReportStats& stats) const;
    void PrintReportLong(const ReportStats& stats) const;

    std::string BaseDisplayName(const Entry& entry) const;
    std::string FileUri(const std::filesystem::path& path) const;
    std::string TreePrefix(const std::vector<bool>& branches, bool is_last) const;
};

}  // namespace nls

