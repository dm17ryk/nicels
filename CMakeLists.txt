cmake_minimum_required(VERSION 3.26)

project(nicels VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NICELS_ENABLE_WARNINGS "Enable compiler warnings" ON)

set(SOURCES
    src/app.cpp
    src/cli.cpp
    src/color_formatter.cpp
    src/fs_scanner.cpp
    src/git_status.cpp
    src/logger.cpp
    src/main.cpp
    src/renderer.cpp
)

add_executable(nicels ${SOURCES})
set_target_properties(nicels PROPERTIES OUTPUT_NAME nls)

target_include_directories(nicels
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

set(cli11_header "${PROJECT_SOURCE_DIR}/third-party/cli11/include/CLI/CLI.hpp")
if(EXISTS "${cli11_header}")
    target_include_directories(nicels PRIVATE ${PROJECT_SOURCE_DIR}/third-party/cli11/include)
else()
    include(FetchContent)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.2
    )
    FetchContent_MakeAvailable(CLI11)
    target_link_libraries(nicels PRIVATE CLI11::CLI11)
endif()

target_compile_definitions(nicels PRIVATE NOMINMAX)

if(NICELS_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
    endif()
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

set_target_properties(nicels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin/Release"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/obj/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/obj/Release"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/obj/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/obj/Release"
)

add_custom_command(TARGET nicels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nicels> ${PROJECT_SOURCE_DIR}/$<TARGET_FILE_NAME:nicels>
    COMMENT "Copying executable to repository root"
)

