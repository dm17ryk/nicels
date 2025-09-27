# SPDX-License-Identifier: MIT
#
# Find module for the CLI11 header-only command line parsing library.
#
# This module defines the following imported target on success:
#   CLI11::CLI11 - Interface target exposing the include directory.
#
# The search strategy first checks for a vendored copy in the project tree
# (expected at <project>/third-party/cli11).  This keeps the build
# reproducible and hermetic even when CLI11 is not installed globally.  If the
# vendored copy is absent, the usual CMake package search locations are
# queried, allowing downstream consumers to provide their own installation.
#
# Callers can provide hints via CLI11_ROOT, CLI11_DIR or standard
# CMAKE_PREFIX_PATH entries.

if(TARGET CLI11::CLI11)
  set(CLI11_FOUND TRUE)
  return()
endif()

set(_CLI11_HINTS)
if(DEFINED CLI11_ROOT)
  list(APPEND _CLI11_HINTS "${CLI11_ROOT}")
endif()

# Always prioritise the in-tree submodule when present.
set(_CLI11_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../third-party/cli11")
if(EXISTS "${_CLI11_VENDOR_DIR}/include/CLI/CLI.hpp")
  list(PREPEND _CLI11_HINTS "${_CLI11_VENDOR_DIR}")
endif()

find_path(CLI11_INCLUDE_DIR
  NAMES CLI/CLI.hpp
  PATH_SUFFIXES include
  HINTS ${_CLI11_HINTS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CLI11
  REQUIRED_VARS CLI11_INCLUDE_DIR
)

if(NOT CLI11_FOUND)
  return()
endif()

add_library(CLI11::CLI11 INTERFACE IMPORTED)
set_target_properties(CLI11::CLI11 PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}"
)

mark_as_advanced(CLI11_INCLUDE_DIR)
unset(_CLI11_HINTS)
unset(_CLI11_VENDOR_DIR)
