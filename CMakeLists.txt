cmake_minimum_required(VERSION 3.25)

project(nicels VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default configuration types when using multi-config generators
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(default_build_configs Debug Release)
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${default_build_configs}" CACHE STRING "" FORCE)
endif()

# Output layout
set(_runtime_root "${CMAKE_SOURCE_DIR}/bin")
set(_object_root "${CMAKE_SOURCE_DIR}/obj")
foreach(config ${default_build_configs})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${_runtime_root}/${config}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${_object_root}/${config}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${_object_root}/${config}")
endforeach()

# Collect sources
file(GLOB nicels_sources
    CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(nicels ${nicels_sources})

set_target_properties(nicels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_runtime_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_object_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_object_root}"
)

foreach(config ${default_build_configs})
    string(TOUPPER "${config}" config_upper)
    set_target_properties(nicels PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${_runtime_root}/${config}"
        LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${_object_root}/${config}"
        ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${_object_root}/${config}"
    )
endforeach()

# Include directories
target_include_directories(nicels
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third-party/cli11/include
)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(nicels PRIVATE /W4 /permissive-)
endif()

# Build-specific optimisations
if(MSVC)
    target_compile_options(nicels PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/DNDEBUG>
    )
else()
    target_compile_options(nicels PRIVATE
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
    )
endif()

# Platform definitions
if(WIN32)
    target_compile_definitions(nicels PRIVATE NOMINMAX)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBGIT2 QUIET libgit2)
endif()

if(LIBGIT2_FOUND)
    target_link_libraries(nicels PRIVATE ${LIBGIT2_LIBRARIES})
    target_include_directories(nicels PRIVATE ${LIBGIT2_INCLUDE_DIRS})
else()
    find_library(LIBGIT2_LIBRARY git2)
    if(LIBGIT2_LIBRARY)
        target_link_libraries(nicels PRIVATE ${LIBGIT2_LIBRARY})
    endif()
endif()

if(MSVC)
    target_compile_definitions(nicels PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Copy final executable to repository root after build
add_custom_command(TARGET nicels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:nicels>" "${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:nicels>"
    COMMENT "Copying nicels to repository root"
)

