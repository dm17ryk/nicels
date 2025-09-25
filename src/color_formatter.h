#pragma once

#include <CLI/CLI.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nls {
    class ColorFormatter : public CLI::Formatter {
private:
    bool ShouldColorizeHelp() const;
    std::string ColorText(std::string_view text, std::string_view theme_key, std::string_view fallback_color) const;
public:
    std::string make_usage(const CLI::App* app, std::string name) const override;
    std::string make_group(std::string group, bool is_positional, std::vector<const CLI::Option*> opts) const override;
    std::string make_option_name(const CLI::Option* opt, bool is_positional) const override;
    std::string make_option_opts(const CLI::Option* opt) const override;
    std::string make_option_desc(const CLI::Option* opt) const override;
    std::string make_footer(const CLI::App* app) const override;
    std::string make_description(const CLI::App* app) const override;
    std::string make_subcommand(const CLI::App* sub) const override;
    std::string make_expanded(const CLI::App* sub, CLI::AppFormatMode mode) const override;
};
}