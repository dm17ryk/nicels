#include "config.h"

#include "perf.h"

#include <optional>
#include <utility>

namespace nls {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

void Config::Reset() {
    auto& perf_manager = perf::Manager::Instance();
    const bool perf_enabled = perf_manager.enabled();
    std::optional<perf::Timer> timer;
    if (perf_enabled) {
        timer.emplace("config::reset");
        perf_manager.IncrementCounter("config::reset_calls");
    }

    format_ = Format::ColumnsVertical;
    indicator_ = IndicatorStyle::Slash;
    color_theme_ = ColorTheme::Default;
    sort_ = Sort::Name;
    report_ = Report::None;
    quoting_style_ = QuotingStyle::Literal;

    all_ = false;
    almost_all_ = false;
    git_status_ = false;
    group_dirs_first_ = false;
    sort_files_first_ = false;
    dots_first_ = false;
    no_icons_ = false;
    no_color_ = false;
    reverse_ = false;
    bytes_ = false;
    dirs_only_ = false;
    files_only_ = false;
    show_inode_ = false;
    show_owner_ = true;
    show_group_ = true;
    hyperlink_ = false;
    header_ = false;
    tree_ = false;
    numeric_uid_gid_ = false;
    dereference_ = false;
    ignore_backups_ = false;
    hide_control_chars_ = false;
    zero_terminate_ = false;
    show_block_size_ = false;
    perf_logging_ = false;
    db_action_ = DbAction::None;
    db_icon_entry_ = {};
    db_alias_entry_ = {};

    theme_name_.reset();

    tree_depth_.reset();
    output_width_.reset();

    time_style_ = "local";
    hide_patterns_.clear();
    ignore_patterns_.clear();
    paths_.clear();

    tab_size_ = 8;
    block_size_ = 0;
    block_size_specified_ = false;
    block_size_show_suffix_ = false;
    block_size_suffix_.clear();
}

Config::Format Config::format() const { return format_; }
void Config::set_format(Format value) { format_ = value; }

Config::IndicatorStyle Config::indicator() const { return indicator_; }
void Config::set_indicator(IndicatorStyle value) { indicator_ = value; }

Config::ColorTheme Config::color_theme() const { return color_theme_; }
void Config::set_color_theme(ColorTheme value) { color_theme_ = value; }

Config::Sort Config::sort() const { return sort_; }
void Config::set_sort(Sort value) { sort_ = value; }

Config::Report Config::report() const { return report_; }
void Config::set_report(Report value) { report_ = value; }

Config::QuotingStyle Config::quoting_style() const { return quoting_style_; }
void Config::set_quoting_style(QuotingStyle value) { quoting_style_ = value; }

bool Config::all() const { return all_; }
void Config::set_all(bool value) { all_ = value; }

bool Config::almost_all() const { return almost_all_; }
void Config::set_almost_all(bool value) { almost_all_ = value; }

bool Config::git_status() const { return git_status_; }
void Config::set_git_status(bool value) { git_status_ = value; }

bool Config::group_dirs_first() const { return group_dirs_first_; }
void Config::set_group_dirs_first(bool value) { group_dirs_first_ = value; }

bool Config::sort_files_first() const { return sort_files_first_; }
void Config::set_sort_files_first(bool value) { sort_files_first_ = value; }

bool Config::dots_first() const { return dots_first_; }
void Config::set_dots_first(bool value) { dots_first_ = value; }

bool Config::no_icons() const { return no_icons_; }
void Config::set_no_icons(bool value) { no_icons_ = value; }

bool Config::no_color() const { return no_color_; }
void Config::set_no_color(bool value) { no_color_ = value; }

bool Config::reverse() const { return reverse_; }
void Config::set_reverse(bool value) { reverse_ = value; }

bool Config::bytes() const { return bytes_; }
void Config::set_bytes(bool value) { bytes_ = value; }

bool Config::dirs_only() const { return dirs_only_; }
void Config::set_dirs_only(bool value) { dirs_only_ = value; }

bool Config::files_only() const { return files_only_; }
void Config::set_files_only(bool value) { files_only_ = value; }

bool Config::show_inode() const { return show_inode_; }
void Config::set_show_inode(bool value) { show_inode_ = value; }

bool Config::show_owner() const { return show_owner_; }
void Config::set_show_owner(bool value) { show_owner_ = value; }

bool Config::show_group() const { return show_group_; }
void Config::set_show_group(bool value) { show_group_ = value; }

bool Config::hyperlink() const { return hyperlink_; }
void Config::set_hyperlink(bool value) { hyperlink_ = value; }

bool Config::header() const { return header_; }
void Config::set_header(bool value) { header_ = value; }

bool Config::tree() const { return tree_; }
void Config::set_tree(bool value) { tree_ = value; }

bool Config::numeric_uid_gid() const { return numeric_uid_gid_; }
void Config::set_numeric_uid_gid(bool value) { numeric_uid_gid_ = value; }

bool Config::dereference() const { return dereference_; }
void Config::set_dereference(bool value) { dereference_ = value; }

bool Config::ignore_backups() const { return ignore_backups_; }
void Config::set_ignore_backups(bool value) { ignore_backups_ = value; }

bool Config::hide_control_chars() const { return hide_control_chars_; }
void Config::set_hide_control_chars(bool value) { hide_control_chars_ = value; }

bool Config::zero_terminate() const { return zero_terminate_; }
void Config::set_zero_terminate(bool value) { zero_terminate_ = value; }

bool Config::show_block_size() const { return show_block_size_; }
void Config::set_show_block_size(bool value) { show_block_size_ = value; }

bool Config::perf_logging() const { return perf_logging_; }
void Config::set_perf_logging(bool value) { perf_logging_ = value; }

Config::DbAction Config::db_action() const { return db_action_; }
void Config::set_db_action(DbAction value) { db_action_ = value; }

const Config::DbIconEntry& Config::db_icon_entry() const { return db_icon_entry_; }
void Config::set_db_icon_entry(DbIconEntry value) { db_icon_entry_ = std::move(value); }

const Config::DbAliasEntry& Config::db_alias_entry() const { return db_alias_entry_; }
void Config::set_db_alias_entry(DbAliasEntry value) { db_alias_entry_ = std::move(value); }

const std::optional<std::string>& Config::theme_name() const { return theme_name_; }
void Config::set_theme_name(std::optional<std::string> value) { theme_name_ = std::move(value); }

const std::optional<std::size_t>& Config::tree_depth() const { return tree_depth_; }
void Config::set_tree_depth(std::optional<std::size_t> value) { tree_depth_ = std::move(value); }
void Config::clear_tree_depth() { tree_depth_.reset(); }

const std::optional<int>& Config::output_width() const { return output_width_; }
void Config::set_output_width(std::optional<int> value) { output_width_ = std::move(value); }
void Config::clear_output_width() { output_width_.reset(); }

const std::string& Config::time_style() const { return time_style_; }
void Config::set_time_style(std::string value) { time_style_ = std::move(value); }

const std::vector<std::string>& Config::hide_patterns() const { return hide_patterns_; }
std::vector<std::string>& Config::mutable_hide_patterns() { return hide_patterns_; }
void Config::set_hide_patterns(std::vector<std::string> value) { hide_patterns_ = std::move(value); }

const std::vector<std::string>& Config::ignore_patterns() const { return ignore_patterns_; }
std::vector<std::string>& Config::mutable_ignore_patterns() { return ignore_patterns_; }
void Config::set_ignore_patterns(std::vector<std::string> value) { ignore_patterns_ = std::move(value); }

const std::vector<std::string>& Config::paths() const { return paths_; }
std::vector<std::string>& Config::mutable_paths() { return paths_; }
void Config::set_paths(std::vector<std::string> value) { paths_ = std::move(value); }

int Config::tab_size() const { return tab_size_; }
void Config::set_tab_size(int value) { tab_size_ = value; }

uintmax_t Config::block_size() const { return block_size_; }
void Config::set_block_size(uintmax_t value) { block_size_ = value; }

bool Config::block_size_specified() const { return block_size_specified_; }
void Config::set_block_size_specified(bool value) { block_size_specified_ = value; }

bool Config::block_size_show_suffix() const { return block_size_show_suffix_; }
void Config::set_block_size_show_suffix(bool value) { block_size_show_suffix_ = value; }

const std::string& Config::block_size_suffix() const { return block_size_suffix_; }
void Config::set_block_size_suffix(std::string value) { block_size_suffix_ = std::move(value); }

} // namespace nls
