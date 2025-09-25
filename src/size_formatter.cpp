#include "size_formatter.h"

#include <iomanip>
#include <sstream>

namespace nls {

std::string SizeFormatter::FormatHumanReadable(uintmax_t bytes) const {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double value = static_cast<double>(bytes);
    int unit_index = 0;
    while (value >= 1024.0 && unit_index < 5) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream stream;
    if (unit_index == 0) {
        stream << static_cast<uintmax_t>(value) << ' ' << units[unit_index];
    } else {
        stream << std::fixed << std::setprecision(value < 10 ? 1 : 0)
               << value << ' ' << units[unit_index];
    }
    return stream.str();
}

} // namespace nls
