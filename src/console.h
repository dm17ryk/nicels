#pragma once
#include <string>

namespace nls {

bool enable_virtual_terminal();     // Enable ANSI on Windows; no-op on POSIX
int  terminal_width();              // Best-effort columns count; fallback 80

} // namespace nls
