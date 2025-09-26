#include "nicels/cli_formatter.h"
#include "nicels/theme.h"

namespace nicels {

bool ColorFormatter::ShouldColorizeHelp() const {
    static const bool colorize = [] {
        if (std::getenv("NO_COLOR") != nullptr) {
            return false;
        }
#ifdef _WIN32
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        if (GetFileType(handle) != FILE_TYPE_CHAR) {
            return false;
        }
        DWORD mode = 0;
        return GetConsoleMode(handle, &mode) != 0;
#else
        return ::isatty(STDOUT_FILENO) != 0;
#endif
    }();
    return colorize;
}

std::string ColorFormatter::ColorText(std::string_view text, std::string_view theme_key, std::string_view fallback_color) const{
    if (!ShouldColorizeHelp()) {
        return std::string(text);
    }

    return std::string(text);

    /* const Theme& theme = active_theme();
    const std::string color = theme.color_or(theme_key, fallback_color);
    return apply_color(color, text, theme, false); */
}

std::string ColorFormatter::make_usage(const CLI::App* app, std::string name) const {
    std::ostringstream out;
    out << '\n';

    out << ColorText(get_label("Usage"), "help_usage_label", "\x1b[33m") << ':';
    if (!name.empty()) {
        out << ' ' << ColorText(name, "help_usage_command", "\x1b[33m");
    }

    std::vector<const CLI::Option*> non_positional =
        app->get_options([](const CLI::Option* opt) { return opt->nonpositional(); });
    if (!non_positional.empty()) {
        out << " [" << get_label("OPTIONS") << "]";
    }

    std::vector<const CLI::Option*> positionals =
        app->get_options([](const CLI::Option* opt) { return opt->get_positional(); });
    if (!positionals.empty()) {
        std::vector<std::string> positional_names(positionals.size());
        std::transform(positionals.begin(), positionals.end(), positional_names.begin(),
            [this](const CLI::Option* opt) { return make_option_usage(opt); });
        out << " " << CLI::detail::join(positional_names, " ");
    }

    if (!app->get_subcommands([](const CLI::App* subc) {
            return (!subc->get_disabled()) && (!subc->get_name().empty());
        }).empty()) {
        out << ' ' << (app->get_require_subcommand_min() == 0 ? "[" : "")
            << get_label(app->get_require_subcommand_max() == 1 ? "SUBCOMMAND" : "SUBCOMMANDS")
            << (app->get_require_subcommand_min() == 0 ? "]" : "");
    }

    out << "\n\n";
    return out.str();
}

std::string ColorFormatter::make_group(std::string group, bool is_positional, std::vector<const CLI::Option*> opts) const {
    if (opts.empty()) {
        return {};
    }

    std::ostringstream out;
    out << "\n";
    if (!group.empty()) {
        out << ColorText(group, "help_option_group", "\x1b[36m");
    }
    out << ":\n";
    for (const CLI::Option* opt : opts) {
        out << make_option(opt, is_positional);
    }
    return out.str();
}

std::string ColorFormatter::make_option_name(const CLI::Option* opt, bool is_positional) const {
    return ColorText(Formatter::make_option_name(opt, is_positional), "help_option_name", "\x1b[33m");
}

std::string ColorFormatter::make_option_opts(const CLI::Option* opt) const {
    return ColorText(Formatter::make_option_opts(opt), "help_option_opts", "\x1b[34m");
}

std::string ColorFormatter::make_option_desc(const CLI::Option* opt) const {
    return ColorText(Formatter::make_option_desc(opt), "help_option_desc", "\x1b[32m");
}

std::string ColorFormatter::make_footer(const CLI::App* app) const {
    std::string footer = Formatter::make_footer(app);
    if (footer.empty()) {
        return footer;
    }
    return ColorText(footer, "help_footer", "\x1b[35m");
}

std::string ColorFormatter::make_description(const CLI::App* app) const {
    std::string desc = Formatter::make_description(app);
    if (desc.empty()) {
        return desc;
    }
    return ColorText(desc, "help_description", "\x1b[35m");
}

std::string ColorFormatter::make_subcommand(const CLI::App* sub) const {
    std::string cmd = Formatter::make_subcommand(sub);
    if (cmd.empty()) {
        return cmd;
    }
    return ColorText(cmd, "help_option_group", "\x1b[33m");
}

std::string ColorFormatter::make_expanded(const CLI::App* sub, CLI::AppFormatMode mode) const {
    std::string cmd = Formatter::make_expanded(sub, mode);
    if (cmd.empty()) {
        return cmd;
    }
    return ColorText(cmd, "help_option_group", "\x1b[33m");
}

}