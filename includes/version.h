#pragma once

#include <string>
#include <string_view>

#ifndef NLS_VERSION_MAJOR
#define NLS_VERSION_MAJOR 0
#endif

#ifndef NLS_VERSION_MINOR
#define NLS_VERSION_MINOR 0
#endif

#ifndef NLS_VERSION_MAINTENANCE
#define NLS_VERSION_MAINTENANCE 0
#endif

#ifndef NLS_VERSION_BUILD
#define NLS_VERSION_BUILD 0
#endif

#ifndef NLS_VERSION_HAS_MAINTENANCE
#define NLS_VERSION_HAS_MAINTENANCE 0
#endif

#ifndef NLS_VERSION_HAS_BUILD
#define NLS_VERSION_HAS_BUILD 0
#endif

#ifndef NLS_VERSION_STRING
#define NLS_VERSION_STRING "0.0"
#endif

#ifndef NLS_VERSION_CORE_STRING
#define NLS_VERSION_CORE_STRING "0.0"
#endif

namespace nls {

class Version {
public:
    static constexpr int Major() noexcept { return NLS_VERSION_MAJOR; }
    static constexpr int Minor() noexcept { return NLS_VERSION_MINOR; }
    static constexpr int Maintenance() noexcept { return NLS_VERSION_MAINTENANCE; }
    static constexpr int Build() noexcept { return NLS_VERSION_BUILD; }

    static constexpr bool HasMaintenance() noexcept { return NLS_VERSION_HAS_MAINTENANCE != 0; }
    static constexpr bool HasBuild() noexcept { return NLS_VERSION_HAS_BUILD != 0; }

    static constexpr std::string_view CoreString() noexcept { return std::string_view{NLS_VERSION_CORE_STRING}; }
    static constexpr std::string_view String() noexcept { return std::string_view{NLS_VERSION_STRING}; }

    static std::string FullString()
    {
        return std::string{String()};
    }
};

}  // namespace nls

