#pragma once

namespace nicels {

template <typename OptionPtr>
void Cli::document_option(const OptionPtr& option, std::string name, std::string description) {
    OptionDoc doc;
    doc.name = std::move(name);
    doc.description = std::move(description);
    doc.default_value = option->get_default_str();
    docs_.push_back(std::move(doc));
}

} // namespace nicels
