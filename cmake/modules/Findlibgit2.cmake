# SPDX-License-Identifier: MIT
#
# CMake find module for the bundled libgit2 dependency.
#
# This module configures and builds libgit2 from the project's git submodule in
# third-party/libgit2, exposing it as the imported target `libgit2::git2`.
# The system installation is intentionally ignored to keep the build fully
# reproducible and to guarantee that a static variant is linked.
#
# The following cache variables can be tweaked by the caller before invoking
# `find_package(libgit2 REQUIRED)` if custom behaviour is desired:
#   LIBGIT2_BUILD_SHARED          (default: OFF)
#   LIBGIT2_BUILD_TESTS           (default: OFF)
#   LIBGIT2_ENABLE_SSH            (default: exec, uses the system OpenSSH client)
#   LIBGIT2_ENABLE_HTTPS          (default: ON)
#   LIBGIT2_ENABLE_SSH_AGENT      (default: ON)
#   LIBGIT2_HTTPS_BACKEND         (default: WinHTTP on Windows, OpenSSL otherwise)
#   LIBGIT2_REGEX_BACKEND         (default: builtin)
#
# Additional knobs are set internally to prefer the vendored third-party
# dependencies that ship with libgit2 when available.

if(TARGET libgit2::git2)
  set(libgit2_FOUND TRUE)
  return()
endif()

set(_LIBGIT2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../third-party/libgit2")
if(NOT EXISTS "${_LIBGIT2_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "libgit2 submodule not found. Run `git submodule update --init --recursive`.")
endif()

# Allow callers to override fundamental configuration choices.
set(LIBGIT2_BUILD_SHARED OFF CACHE BOOL "Build libgit2 as a shared library")
set(LIBGIT2_BUILD_TESTS OFF CACHE BOOL "Build libgit2 test suite")
set(LIBGIT2_ENABLE_SSH "exec" CACHE STRING "Enable SSH support in libgit2 (libssh2 or exec)")
set(LIBGIT2_ENABLE_HTTPS ON CACHE BOOL "Enable HTTPS support in libgit2")
set(LIBGIT2_ENABLE_SSH_AGENT ON CACHE BOOL "Enable SSH agent support in libgit2")
set(LIBGIT2_USE_BUNDLED_DEPENDENCIES ON CACHE BOOL "Force bundled third-party dependencies")
set(LIBGIT2_REGEX_BACKEND builtin CACHE STRING "Regular expression backend for libgit2")
if(WIN32)
  set(_libgit2_default_https WinHTTP)
else()
  set(_libgit2_default_https OpenSSL)
endif()
set(LIBGIT2_HTTPS_BACKEND "${_libgit2_default_https}" CACHE STRING "HTTPS backend for libgit2")

set(_libgit2_binary_dir "${CMAKE_BINARY_DIR}/libgit2")
file(MAKE_DIRECTORY "${_libgit2_binary_dir}")

foreach(_flag LIBGIT2_ENABLE_HTTPS LIBGIT2_ENABLE_SSH_AGENT LIBGIT2_USE_BUNDLED_DEPENDENCIES)
  if(${_flag})
    set(${_flag}_VALUE ON)
  else()
    set(${_flag}_VALUE OFF)
  endif()
endforeach()

set(BUILD_SHARED_LIBS ${LIBGIT2_BUILD_SHARED} CACHE BOOL "" FORCE)
set(BUILD_CLAR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS ${LIBGIT2_BUILD_TESTS} CACHE BOOL "" FORCE)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
set(USE_SSH ${LIBGIT2_ENABLE_SSH} CACHE STRING "" FORCE)
set(USE_SSH_AGENT ${LIBGIT2_ENABLE_SSH_AGENT_VALUE} CACHE STRING "" FORCE)
set(USE_HTTPS ${LIBGIT2_ENABLE_HTTPS_VALUE} CACHE STRING "" FORCE)
set(HTTPS_BACKEND ${LIBGIT2_HTTPS_BACKEND} CACHE STRING "" FORCE)
set(REGEX_BACKEND ${LIBGIT2_REGEX_BACKEND} CACHE STRING "" FORCE)
set(USE_REGEX ${LIBGIT2_REGEX_BACKEND} CACHE STRING "" FORCE)
set(USE_BUNDLED_ZLIB ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_OPENSSL ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_HTTPPARSER ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_LIBSSH2 ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_NTLM ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_CURL ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(USE_BUNDLED_ICONV ${LIBGIT2_USE_BUNDLED_DEPENDENCIES_VALUE} CACHE STRING "" FORCE)
set(LIBGIT2_FILENAME git2 CACHE STRING "" FORCE)

if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()
add_subdirectory("${_LIBGIT2_ROOT}" "${_libgit2_binary_dir}" EXCLUDE_FROM_ALL)

set(_libgit2_primary_target git2)
if(NOT TARGET ${_libgit2_primary_target})
  if(TARGET libgit2package)
    set(_libgit2_primary_target libgit2package)
  else()
    message(FATAL_ERROR "libgit2 target was not created as expected")
  endif()
endif()

get_target_property(_libgit2_type ${_libgit2_primary_target} TYPE)

if(NLS_RELEASE_COMPILE_OPTIONS)
  target_compile_options(${_libgit2_primary_target}
    PRIVATE $<$<CONFIG:Release>:${NLS_RELEASE_COMPILE_OPTIONS}>)
endif()

if(DEFINED _nls_enable_ipo AND _nls_enable_ipo)
  set_property(TARGET ${_libgit2_primary_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(NLS_RELEASE_LINK_OPTIONS)
  if(_libgit2_type STREQUAL "SHARED_LIBRARY" OR _libgit2_type STREQUAL "MODULE_LIBRARY" OR _libgit2_type STREQUAL "EXECUTABLE")
    target_link_options(${_libgit2_primary_target}
      PRIVATE $<$<CONFIG:Release>:${NLS_RELEASE_LINK_OPTIONS}>)
  endif()
endif()

add_library(libgit2::git2 ALIAS ${_libgit2_primary_target})
set(libgit2_FOUND TRUE)

foreach(_flag LIBGIT2_ENABLE_HTTPS LIBGIT2_ENABLE_SSH_AGENT LIBGIT2_USE_BUNDLED_DEPENDENCIES)
  unset(${_flag}_VALUE)
endforeach()

unset(_libgit2_type)
unset(_libgit2_primary_target)
unset(_libgit2_binary_dir)
unset(_LIBGIT2_ROOT)
unset(_libgit2_default_https)
