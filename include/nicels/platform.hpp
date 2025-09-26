#pragma once

#include <cstdint>
#include <string>

namespace nicels::platform {

bool stdout_is_tty();
std::uint32_t terminal_width();
void enable_virtual_terminal_processing();

std::string system_locale();

} // namespace nicels::platform
