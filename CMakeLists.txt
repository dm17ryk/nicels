cmake_minimum_required(VERSION 3.25)

project(nicels VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wimplicit-fallthrough)
endif()

set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_LIB ON CACHE BOOL "" FORCE)
add_subdirectory(third-party/cli11 EXCLUDE_FROM_ALL)

file(GLOB NICELS_SOURCES CONFIGURE_DEPENDS
    src/main.cpp
    src/nicels/*.cpp)

add_executable(nicels ${NICELS_SOURCES})

target_include_directories(nicels PRIVATE include)
target_link_libraries(nicels PRIVATE CLI11::CLI11)

if(WIN32)
    target_compile_definitions(nicels PRIVATE NOMINMAX=1)
endif()


set_target_properties(nicels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/obj/Debug
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/obj/Release
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/obj/Debug
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/obj/Release)

add_custom_command(TARGET nicels POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/obj/$<CONFIG>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/CMakeFiles/nicels.dir ${CMAKE_BINARY_DIR}/obj/$<CONFIG>
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nicels> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:nicels>)

include(GNUInstallDirs)
install(TARGETS nicels RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CPACK_PACKAGE_NAME "nicels")
set(CPACK_PACKAGE_VENDOR "nicels contributors")
set(CPACK_PACKAGE_CONTACT "maintainers@nicels.local")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern nicels listing utility")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nicels maintainers")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6, libgcc-s1, git")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_REQUIRES "git")

set(CPACK_MSIX_PACKAGE_NAME "nicels")
set(CPACK_MSIX_PACKAGE_PUBLISHER "CN=Nicels Contributors")
set(CPACK_MSIX_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_MSIX_INSTALL_DIRECTORY "Program Files/nicels")

set(CPACK_GENERATOR "DEB;RPM;MSIX")

include(CPack)
