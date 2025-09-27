#include "nicels/color_formatter.hpp"

#include <sstream>

namespace nicels {

std::string ColorFormatter::make_usage(const CLI::App* app, std::string name) const {
    std::ostringstream out;
    out << '\n';
    out << "Usage:";
    if (!name.empty()) {
        out << ' ' << name;
    }

    auto non_positional = app->get_options([](const CLI::Option* opt) { return opt->nonpositional(); });
    if (!non_positional.empty()) {
        out << " [OPTIONS]";
    }

    auto positional = app->get_options([](const CLI::Option* opt) { return opt->get_positional(); });
    if (!positional.empty()) {
        std::vector<std::string> names;
        names.reserve(positional.size());
        for (const CLI::Option* opt : positional) {
            names.push_back(make_option_usage(opt));
        }
        out << ' ' << CLI::detail::join(names);
    }

    if (!app->get_subcommands([](const CLI::App* sub) { return !sub->get_disabled() && !sub->get_name().empty(); }).empty()) {
        out << " [SUBCOMMAND]";
    }

    out << "\n\n";
    return out.str();
}

} // namespace nicels
