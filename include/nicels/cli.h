#pragma once

#include <CLI/CLI.hpp>

#include <memory>
#include <string>
#include <vector>

#include "nicels/config.h"

namespace nicels {

class Cli {
public:
    Cli();
    ~Cli();

    Config::Options parse(int argc, char** argv);
    std::string usage_markdown() const;

private:
    struct OptionDoc {
        std::string name;
        std::string description;
        std::string default_value;
    };

    template <typename OptionPtr>
    void document_option(const OptionPtr& option, std::string name, std::string description);

    void add_layout_options();
    void add_filter_options();
    void add_sort_options();
    void add_appearance_options();
    void add_information_options();

    std::unique_ptr<CLI::App> app_;
    Config::Options options_{};
    std::vector<OptionDoc> docs_;
};

} // namespace nicels

#include "nicels/cli.tpp"
