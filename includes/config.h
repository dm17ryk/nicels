#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nls {

class Config {
public:
    enum class Format {
        ColumnsVertical,
        ColumnsHorizontal,
        Long,
        SingleColumn,
        CommaSeparated
    };

    enum class IndicatorStyle { None, Slash };
    enum class ColorTheme { Default, Light, Dark };
    enum class Sort { Name, Time, Size, Extension, None };
    enum class Report { None, Short, Long };
    enum class QuotingStyle {
        Literal,
        Locale,
        Shell,
        ShellAlways,
        ShellEscape,
        ShellEscapeAlways,
        C,
        Escape
    };

    enum class DbAction {
        None,
        ShowFiles,
        ShowFolders,
        ShowFileAliases,
        ShowFolderAliases
    };

    static Config& Instance();

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void Reset();

    Format format() const;
    void set_format(Format value);

    IndicatorStyle indicator() const;
    void set_indicator(IndicatorStyle value);

    ColorTheme color_theme() const;
    void set_color_theme(ColorTheme value);

    Sort sort() const;
    void set_sort(Sort value);

    Report report() const;
    void set_report(Report value);

    QuotingStyle quoting_style() const;
    void set_quoting_style(QuotingStyle value);

    bool all() const;
    void set_all(bool value);

    bool almost_all() const;
    void set_almost_all(bool value);

    bool git_status() const;
    void set_git_status(bool value);

    bool group_dirs_first() const;
    void set_group_dirs_first(bool value);

    bool sort_files_first() const;
    void set_sort_files_first(bool value);

    bool dots_first() const;
    void set_dots_first(bool value);

    bool no_icons() const;
    void set_no_icons(bool value);

    bool no_color() const;
    void set_no_color(bool value);

    bool reverse() const;
    void set_reverse(bool value);

    bool bytes() const;
    void set_bytes(bool value);

    bool dirs_only() const;
    void set_dirs_only(bool value);

    bool files_only() const;
    void set_files_only(bool value);

    bool show_inode() const;
    void set_show_inode(bool value);

    bool show_owner() const;
    void set_show_owner(bool value);

    bool show_group() const;
    void set_show_group(bool value);

    bool hyperlink() const;
    void set_hyperlink(bool value);

    bool header() const;
    void set_header(bool value);

    bool tree() const;
    void set_tree(bool value);

    bool numeric_uid_gid() const;
    void set_numeric_uid_gid(bool value);

    bool dereference() const;
    void set_dereference(bool value);

    bool ignore_backups() const;
    void set_ignore_backups(bool value);

    bool hide_control_chars() const;
    void set_hide_control_chars(bool value);

    bool zero_terminate() const;
    void set_zero_terminate(bool value);

    bool show_block_size() const;
    void set_show_block_size(bool value);

    bool perf_logging() const;
    void set_perf_logging(bool value);

    bool copy_config_only() const;
    void set_copy_config_only(bool value);

    DbAction db_action() const;
    void set_db_action(DbAction value);

    const std::optional<std::string>& theme_name() const;
    void set_theme_name(std::optional<std::string> value);

    const std::optional<std::size_t>& tree_depth() const;
    void set_tree_depth(std::optional<std::size_t> value);
    void clear_tree_depth();

    const std::optional<int>& output_width() const;
    void set_output_width(std::optional<int> value);
    void clear_output_width();

    const std::string& time_style() const;
    void set_time_style(std::string value);

    const std::vector<std::string>& hide_patterns() const;
    std::vector<std::string>& mutable_hide_patterns();
    void set_hide_patterns(std::vector<std::string> value);

    const std::vector<std::string>& ignore_patterns() const;
    std::vector<std::string>& mutable_ignore_patterns();
    void set_ignore_patterns(std::vector<std::string> value);

    const std::vector<std::string>& paths() const;
    std::vector<std::string>& mutable_paths();
    void set_paths(std::vector<std::string> value);

    int tab_size() const;
    void set_tab_size(int value);

    uintmax_t block_size() const;
    void set_block_size(uintmax_t value);

    bool block_size_specified() const;
    void set_block_size_specified(bool value);

    bool block_size_show_suffix() const;
    void set_block_size_show_suffix(bool value);

    const std::string& block_size_suffix() const;
    void set_block_size_suffix(std::string value);

private:
    Config() = default;

    Format format_ = Format::ColumnsVertical;
    IndicatorStyle indicator_ = IndicatorStyle::Slash;
    ColorTheme color_theme_ = ColorTheme::Default;
    Sort sort_ = Sort::Name;
    Report report_ = Report::None;
    QuotingStyle quoting_style_ = QuotingStyle::Literal;

    bool all_ = false;
    bool almost_all_ = false;
    bool git_status_ = false;
    bool group_dirs_first_ = false;
    bool sort_files_first_ = false;
    bool dots_first_ = false;
    bool no_icons_ = false;
    bool no_color_ = false;
    bool reverse_ = false;
    bool bytes_ = false;
    bool dirs_only_ = false;
    bool files_only_ = false;
    bool show_inode_ = false;
    bool show_owner_ = true;
    bool show_group_ = true;
    bool hyperlink_ = false;
    bool header_ = false;
    bool tree_ = false;
    bool numeric_uid_gid_ = false;
    bool dereference_ = false;
    bool ignore_backups_ = false;
    bool hide_control_chars_ = false;
    bool zero_terminate_ = false;
    bool show_block_size_ = false;
    bool perf_logging_ = false;
    bool copy_config_only_ = false;
    DbAction db_action_ = DbAction::None;

    std::optional<std::string> theme_name_{};

    std::optional<std::size_t> tree_depth_;
    std::optional<int> output_width_;

    std::string time_style_;
    std::vector<std::string> hide_patterns_;
    std::vector<std::string> ignore_patterns_;
    std::vector<std::string> paths_;

    int tab_size_ = 8;
    uintmax_t block_size_ = 0;
    bool block_size_specified_ = false;
    bool block_size_show_suffix_ = false;
    std::string block_size_suffix_;
};

} // namespace nls
