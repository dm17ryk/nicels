cmake_minimum_required(VERSION 3.24)
project(nicels VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NICELS_SOURCES
    src/main.cpp
    src/core/app.cpp
    src/core/cli.cpp
    src/core/config.cpp
    src/core/fs.cpp
    src/core/git_status.cpp
    src/core/logger.cpp
    src/platform/platform.cpp
    src/render/formatter.cpp
    src/render/renderer.cpp
    src/render/theme.cpp
)

add_executable(nicels ${NICELS_SOURCES})

target_include_directories(nicels
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third-party/cli11/include
)

target_compile_features(nicels PRIVATE cxx_std_23)

if (MSVC)
    target_compile_options(nicels PRIVATE /W4 /permissive-)
else()
    target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
endif()

if (WIN32)
    target_compile_definitions(nicels PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set_target_properties(nicels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obj"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/obj/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/obj/Release"
)

add_custom_command(TARGET nicels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nicels> ${PROJECT_SOURCE_DIR}/$<TARGET_FILE_NAME:nicels>
    COMMENT "Copying nicels binary to repository root"
)

install(TARGETS nicels RUNTIME DESTINATION bin)

set(CPACK_PACKAGE_NAME "nicels")
set(CPACK_PACKAGE_VENDOR "nicels project")
set(CPACK_PACKAGE_CONTACT "maintainer@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "nicels - a modern colorful ls alternative")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nicels maintainers")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "git")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_REQUIRES "git")
set(CPACK_MSIX_PACKAGE_NAME "nicels")
set(CPACK_MSIX_PACKAGE_DISPLAY_NAME "nicels")
set(CPACK_MSIX_PUBLISHER "CN=Your Name")
set(CPACK_MSIX_INSTALL_ROOT "Program Files/nicels")
set(CPACK_GENERATOR "DEB;RPM;MSIX")

include(CPack)
