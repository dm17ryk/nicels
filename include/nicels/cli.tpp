#pragma once

namespace nicels {

template <typename OptionPtr>
void Cli::document_option(const OptionPtr& option) {
    OptionDoc doc;
    doc.name = option->get_name(true,true);
    doc.description = option->get_description();
    doc.default_value = option->get_default_str();
    docs_.push_back(std::move(doc));
}

} // namespace nicels
