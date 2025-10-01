#include "fs_scanner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <optional>
#include <system_error>
#include <utility>

#ifdef _WIN32
#   ifndef NOMINMAX
#       define NOMINMAX 1
#   endif
#include <windows.h>
#include <winioctl.h>
#ifndef REPARSE_DATA_BUFFER_HEADER_SIZE
typedef struct _REPARSE_DATA_BUFFER {
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
    union {
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            BYTE DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#endif
#endif

#include "file_ownership_resolver.h"
#include "perf.h"
#include "string_utils.h"
#include "symlink_resolver.h"
#include "theme.h"

namespace nls {

namespace fs = std::filesystem;

namespace {

class WildcardMatcher final {
public:
    [[nodiscard]] static bool Matches(const std::string& pattern, const std::string& text) {
        size_t p = 0;
        size_t t = 0;
        size_t star = std::string::npos;
        size_t match = 0;
        while (t < text.size()) {
            if (p < pattern.size()) {
                char pc = pattern[p];
                if (pc == '?') {
                    ++p;
                    ++t;
                    continue;
                }
                if (pc == '*') {
                    star = ++p;
                    match = t;
                    continue;
                }
                if (pc == '[') {
                    size_t idx = p + 1;
                    if (MatchCharClass(pattern, idx, text[t])) {
                        p = idx;
                        ++t;
                        continue;
                    }
                } else {
                    if (pc == '\\' && p + 1 < pattern.size()) {
                        ++p;
                        pc = pattern[p];
                    }
                    if (pc == text[t]) {
                        ++p;
                        ++t;
                        continue;
                    }
                }
            }
            if (star != std::string::npos) {
                p = star;
                ++match;
                t = match;
                continue;
            }
            return false;
        }
        while (p < pattern.size() && pattern[p] == '*') ++p;
        return p == pattern.size();
    }

private:
    [[nodiscard]] static bool MatchCharClass(const std::string& pattern, size_t& idx, char ch) {
        size_t start = idx;
        if (idx >= pattern.size()) return false;
        bool negated = false;
        if (pattern[idx] == '!' || pattern[idx] == '^') {
            negated = true;
            ++idx;
        }
        bool matched = false;
        while (idx < pattern.size() && pattern[idx] != ']') {
            char start_char = pattern[idx];
            if (start_char == '\\' && idx + 1 < pattern.size()) {
                ++idx;
                start_char = pattern[idx];
            }
            ++idx;
            if (idx < pattern.size() && pattern[idx] == '-' && idx + 1 < pattern.size() && pattern[idx + 1] != ']') {
                ++idx;
                char end_char = pattern[idx];
                if (end_char == '\\' && idx + 1 < pattern.size()) {
                    ++idx;
                    end_char = pattern[idx];
                }
                if (start_char <= ch && ch <= end_char) {
                    matched = true;
                }
                ++idx;
            } else {
                if (ch == start_char) matched = true;
            }
        }
        if (idx < pattern.size() && pattern[idx] == ']') {
            ++idx;
            return negated ? !matched : matched;
        }
        idx = start;
        return false;
    }
};

#ifdef _WIN32
struct WindowsLinkInfo {
    bool is_link = false;
    bool has_target = false;
    fs::path target;
};

class WindowsLinkInspector final {
public:
    [[nodiscard]] static WindowsLinkInfo Inspect(const fs::path& path) {
        WindowsLinkInfo info;
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return info;
        }
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            return info;
        }

        HANDLE handle = CreateFileW(path.c_str(), 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            info.is_link = true;
            return info;
        }

        std::array<unsigned char, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> buffer{};
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0,
                                  buffer.data(), static_cast<DWORD>(buffer.size()),
                                  &bytes_returned, nullptr);
        CloseHandle(handle);
        if (!ok) {
            info.is_link = true;
            return info;
        }

        auto* header = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer.data());
        switch (header->ReparseTag) {
            case IO_REPARSE_TAG_SYMLINK: {
                info.is_link = true;
                const auto& data = header->SymbolicLinkReparseBuffer;
                std::wstring target = ReadReparseString(data.PathBuffer, data.PrintNameOffset, data.PrintNameLength);
                if (target.empty()) {
                    target = ReadReparseString(data.PathBuffer, data.SubstituteNameOffset, data.SubstituteNameLength);
                }
                target = CleanTarget(std::move(target));
                if (!target.empty()) {
                    info.target = fs::path(target);
                    info.has_target = true;
                }
                break;
            }
            case IO_REPARSE_TAG_MOUNT_POINT: {
                info.is_link = true;
                const auto& data = header->MountPointReparseBuffer;
                std::wstring target = ReadReparseString(data.PathBuffer, data.PrintNameOffset, data.PrintNameLength);
                if (target.empty()) {
                    target = ReadReparseString(data.PathBuffer, data.SubstituteNameOffset, data.SubstituteNameLength);
                }
                target = CleanTarget(std::move(target));
                if (!target.empty()) {
                    info.target = fs::path(target);
                    info.has_target = true;
                }
                break;
            }
            default:
                break;
        }
        return info;
    }

private:
    [[nodiscard]] static std::wstring ReadReparseString(const WCHAR* path_buffer, USHORT offset, USHORT length) {
        if (length == 0) return {};
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(path_buffer);
        const WCHAR* start = reinterpret_cast<const WCHAR*>(bytes + offset);
        size_t count = length / sizeof(WCHAR);
        return std::wstring(start, start + count);
    }

    [[nodiscard]] static std::wstring CleanTarget(std::wstring target) {
        const std::wstring prefix = L"\\??\\";
        if (target.rfind(prefix, 0) == 0) {
            target.erase(0, prefix.size());
            const std::wstring unc_prefix = L"UNC\\";
            if (target.rfind(unc_prefix, 0) == 0) {
                target.erase(0, unc_prefix.size());
                target.insert(0, L"\\\\");
            }
        }
        return target;
    }
};
#endif  // _WIN32

class ExecutableClassifier final {
public:
    [[nodiscard]] static bool IsExecutable(const fs::directory_entry& de) {
#ifdef _WIN32
        std::string n = de.path().filename().string();
        auto pos = n.rfind('.');
        if (pos != std::string::npos) {
            std::string ext = n.substr(pos + 1);
            for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            return (ext == "exe" || ext == "bat" || ext == "cmd" || ext == "ps1");
        }
        return false;
#else
        std::error_code ec;
        auto perm = de.status(ec).permissions();
        (void)ec;
        return ((perm & fs::perms::owner_exec) != fs::perms::none ||
                (perm & fs::perms::group_exec) != fs::perms::none ||
                (perm & fs::perms::others_exec) != fs::perms::none);
#endif
    }
};

}  // namespace

FileScanner::FileScanner(const Config& config,
                         FileOwnershipResolver& ownership_resolver,
                         SymlinkResolver& symlink_resolver)
    : config_(config),
      ownership_resolver_(ownership_resolver),
      symlink_resolver_(symlink_resolver) {}

bool FileScanner::matches_any_pattern(const std::string& name,
                                      const std::vector<std::string>& patterns) const {
    for (const auto& pat : patterns) {
        if (WildcardMatcher::Matches(pat, name)) return true;
    }
    return false;
}

bool FileScanner::should_include(const std::string& name, bool is_explicit) const {
    if (name == "." || name == "..") {
        if (!config_.all()) return false;
        if (config_.almost_all()) return false;
        return true;
    }

    if (!config_.all() && !config_.almost_all() && StringUtils::IsHidden(name)) {
        return false;
    }

    if (!is_explicit) {
        if (config_.ignore_backups() && !name.empty() && name.back() == '~') {
            return false;
        }
        if (!config_.ignore_patterns().empty() && matches_any_pattern(name, config_.ignore_patterns())) {
            return false;
        }
        if (!config_.hide_patterns().empty() && !config_.all() && !config_.almost_all() &&
            matches_any_pattern(name, config_.hide_patterns())) {
            return false;
        }
    }

    return true;
}

void FileScanner::populate_entry(const fs::directory_entry& de, Entry& entry) const {
    entry.info.path = de.path();

    std::error_code info_ec;
    auto status = de.symlink_status(info_ec);
    if (!info_ec) {
        entry.info.is_socket = fs::is_socket(status);
        entry.info.is_block_device = fs::is_block_file(status);
        entry.info.is_char_device = fs::is_character_file(status);
        entry.info.is_symlink = fs::is_symlink(status);
        entry.info.symlink_status = status;
        entry.info.has_symlink_status = true;
    }
    info_ec.clear();
    if (!entry.info.is_symlink) {
        entry.info.is_symlink = de.is_symlink(info_ec);
    }
    info_ec.clear();
    entry.info.is_dir = de.is_directory(info_ec);
    info_ec.clear();

    auto fill_symlink_target = [&]() {
        if (!entry.info.is_symlink || entry.info.has_symlink_target) return;
        std::error_code link_ec;
        auto target = fs::read_symlink(entry.info.path, link_ec);
        if (!link_ec) {
            entry.info.symlink_target = std::move(target);
            entry.info.has_symlink_target = true;
        }
    };

    fill_symlink_target();
#ifdef _WIN32
    if (!entry.info.is_symlink || !entry.info.has_symlink_target) {
        WindowsLinkInfo link_info = WindowsLinkInspector::Inspect(de.path());
        if (link_info.is_link) {
            entry.info.is_symlink = true;
            if (!entry.info.has_symlink_target && link_info.has_target) {
                entry.info.symlink_target = std::move(link_info.target);
                entry.info.has_symlink_target = true;
            }
        }
    }
    fill_symlink_target();
#endif

    bool is_reg = de.is_regular_file(info_ec);
    if (info_ec) {
        is_reg = false;
        info_ec.clear();
    }
    entry.info.size = entry.info.is_dir ? 0 : (is_reg ? de.file_size(info_ec) : 0);
    if (info_ec) {
        entry.info.size = 0;
        info_ec.clear();
    }
    entry.info.mtime = de.last_write_time(info_ec);
    entry.info.is_exec = ExecutableClassifier::IsExecutable(de);
    entry.info.is_hidden = StringUtils::IsHidden(entry.info.name);
    std::error_code exists_ec;
    bool exists = fs::exists(entry.info.path, exists_ec);
    entry.info.is_broken_symlink = entry.info.is_symlink && (!exists || exists_ec);

    ownership_resolver_.Populate(entry.info, config_.dereference());
    apply_symlink_metadata(entry);
    apply_icon_and_color(entry);
}

void FileScanner::apply_symlink_metadata(Entry& entry) const {
    if (config_.dereference() && entry.info.is_symlink && !entry.info.is_broken_symlink) {
        fs::path follow_path = entry.info.path;
        if (auto resolved = symlink_resolver_.ResolveTarget(entry.info)) {
            follow_path = std::move(*resolved);
        }
        std::error_code follow_ec;
        auto follow_status = fs::status(follow_path, follow_ec);
        if (!follow_ec) {
            entry.info.is_dir = fs::is_directory(follow_status);
            entry.info.is_socket = fs::is_socket(follow_status);
            entry.info.is_block_device = fs::is_block_file(follow_status);
            entry.info.is_char_device = fs::is_character_file(follow_status);
            entry.info.target_status = follow_status;
            entry.info.has_target_status = true;

            bool follow_is_reg = fs::is_regular_file(follow_status);
            std::error_code size_ec;
            if (entry.info.is_dir) {
                entry.info.size = 0;
            } else if (follow_is_reg) {
                auto followed_size = fs::file_size(follow_path, size_ec);
                if (!size_ec) {
                    entry.info.size = followed_size;
                }
            }
        }

        std::error_code time_ec;
        auto follow_time = fs::last_write_time(follow_path, time_ec);
        if (!time_ec) {
            entry.info.mtime = follow_time;
        }
    } else if (entry.info.is_symlink && entry.info.has_link_size) {
        entry.info.size = entry.info.link_size;
    }
}

void FileScanner::apply_icon_and_color(Entry& entry) const {
    Theme& theme_manager = Theme::instance();
    const ThemeColors& theme_colors = theme_manager.colors();
    IconResult icon = theme_manager.get_icon(entry.info.name, entry.info.is_dir, entry.info.is_exec);
    if (!config_.no_icons()) {
        entry.info.icon = icon.icon;
    }
    entry.info.has_recognized_icon = icon.recognized && !entry.info.is_dir;

    if (config_.no_color()) {
        entry.info.color_fg.clear();
        entry.info.color_reset.clear();
        return;
    }

    std::string color;
    if (entry.info.is_socket) {
        color = theme_colors.get("socket");
    } else if (entry.info.is_block_device) {
        color = theme_colors.get("blockdev");
    } else if (entry.info.is_char_device) {
        color = theme_colors.get("chardev");
    } else if (entry.info.is_dir) {
        color = entry.info.is_hidden ? theme_colors.get("hidden_dir") : theme_colors.get("dir");
    } else if (entry.info.is_hidden) {
        color = theme_colors.get("hidden");
    } else if (entry.info.is_exec) {
        color = theme_colors.get("executable_file");
    } else if (entry.info.has_recognized_icon) {
        color = theme_colors.get("recognized_file");
    } else {
        color = theme_colors.get("unrecognized_file");
    }
    entry.info.color_fg = std::move(color);
    entry.info.color_reset = theme_colors.reset;
}

void FileScanner::report_path_error(const fs::path& path,
                                    const std::error_code& ec,
                                    const char* fallback) const {
    std::cerr << "nls: " << path.string() << ": ";
    if (ec) {
        std::cerr << ec.message();
    } else if (fallback) {
        std::cerr << fallback;
    } else {
        std::cerr << "Unknown error";
    }
    std::cerr << '\n';
}

bool FileScanner::add_entry(const fs::directory_entry& de,
                            std::vector<Entry>& out,
                            std::string override_name,
                            bool is_explicit) const {
    std::string name = override_name.empty() ? de.path().filename().string() : std::move(override_name);

    if (!should_include(name, is_explicit)) {
        return false;
    }

    Entry entry{};
    entry.info.name = std::move(name);
    populate_entry(de, entry);
    if (config_.dirs_only() && !entry.info.is_dir) {
        return false;
    }
    if (config_.files_only() && entry.info.is_dir) {
        return false;
    }

    out.push_back(std::move(entry));

    auto& perf_manager = perf::Manager::Instance();
    if (perf_manager.enabled()) {
        perf_manager.IncrementCounter("entries_included");
        const Entry& stored = out.back();
        if (stored.info.is_dir) {
            perf_manager.IncrementCounter("directories_included");
        } else {
            perf_manager.IncrementCounter("files_included");
        }
    }
    return true;
}

VisitResult FileScanner::collect_entries(const fs::path& dir,
                                         std::vector<Entry>& out,
                                         bool is_top_level) const {
    VisitResult status = VisitResult::Ok;

    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("fs::collect_entries");
        perf_manager.IncrementCounter("paths_scanned");
    }

    std::error_code exists_ec;
    if (!fs::exists(dir, exists_ec)) {
        report_path_error(dir, exists_ec, "No such file or directory");
        return is_top_level ? VisitResult::Serious : VisitResult::Minor;
    }

    std::error_code type_ec;
    bool is_directory = fs::is_directory(dir, type_ec);
    if (type_ec) {
        report_path_error(dir, type_ec, "Unable to access");
        return is_top_level ? VisitResult::Serious : VisitResult::Minor;
    }

    if (is_directory) {
        if (perf_enabled) {
            perf_manager.IncrementCounter("directories_scanned");
        }
        if (config_.all()) {
            std::error_code self_ec;
            fs::directory_entry self(dir, self_ec);
            if (!self_ec) add_entry(self, out, ".", true);

            std::error_code parent_ec;
            fs::directory_entry parent(dir / "..", parent_ec);
            if (!parent_ec) add_entry(parent, out, "..", true);
        }

        std::error_code iter_ec;
        fs::directory_iterator it(dir, iter_ec);
        if (iter_ec) {
            report_path_error(dir, iter_ec, "Unable to open directory");
            return is_top_level ? VisitResult::Serious : VisitResult::Minor;
        }

        fs::directory_iterator end;
        while (it != end) {
            add_entry(*it, out, {}, false);
            it.increment(iter_ec);
            if (iter_ec) break;
        }
        if (iter_ec) {
            report_path_error(dir, iter_ec, "Unable to read directory");
            status = VisitResultAggregator::Combine(status, is_top_level ? VisitResult::Serious : VisitResult::Minor);
        }
    } else {
        std::error_code entry_ec;
        fs::directory_entry de(dir, entry_ec);
        if (entry_ec) {
            report_path_error(dir, entry_ec, "Unable to access");
            return is_top_level ? VisitResult::Serious : VisitResult::Minor;
        }
        add_entry(de, out, {}, true);
    }
    return status;
}

}  // namespace nls

