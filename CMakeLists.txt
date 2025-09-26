cmake_minimum_required(VERSION 3.26)

project(nicels VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

file(GLOB_RECURSE NICELS_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

add_executable(nicels ${NICELS_SOURCES})

target_include_directories(nicels
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third-party/cli11/include
)

if(MSVC)
    target_compile_options(nicels PRIVATE /W4 /permissive-)
else()
    target_compile_options(nicels PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
endif()

target_compile_definitions(nicels PRIVATE $<$<BOOL:${WIN32}>:NOMINMAX=1>)

if(WIN32)
    target_link_libraries(nicels PRIVATE advapi32 shell32)
endif()

# Output directories
set_target_properties(nicels PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
)

foreach(config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(nicels PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_SOURCE_DIR}/bin/${config}"
        LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_SOURCE_DIR}/bin/${config}"
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_SOURCE_DIR}/bin/${config}"
    )
endforeach()

add_custom_command(TARGET nicels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nicels> ${PROJECT_SOURCE_DIR}/$<TARGET_FILE_NAME:nicels>
    COMMENT "Copying nicels executable to repository root"
)

install(TARGETS nicels RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CPACK_GENERATOR "DEB;RPM;MSIX")
set(CPACK_PACKAGE_NAME "nicels")
set(CPACK_PACKAGE_VENDOR "nicels maintainers")
set(CPACK_PACKAGE_CONTACT "nicels maintainers <maintainers@example.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern colorful ls replacement")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nicels maintainers")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++, libgcc, git")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_REQUIRES "git")
set(CPACK_MSIX_PACKAGE_NAME "nicels")
set(CPACK_MSIX_PACKAGE_DISPLAY_NAME "nicels")
set(CPACK_MSIX_PACKAGE_PUBLISHER "CN=Your Name")
set(CPACK_MSIX_PACKAGE_DESCRIPTION "Modern colorful ls replacement")
set(CPACK_MSIX_INSTALL_SCOPE "perMachine")
set(CPACK_MSIX_PACKAGE_IDENTITY_NAME "nicels")
set(CPACK_MSIX_PACKAGE_INSTALL_DIRECTORY "Program Files/nicels")

include(CPack)
