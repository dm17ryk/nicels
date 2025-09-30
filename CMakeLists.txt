cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()

# Preserve an explicit record of the minimum CMake version the project expects.
# Versions >= 3.28 provide excellent cross-platform and cross-compilation
# support, while remaining widely available.  Should CMake 4.1.1 (requested by
# the migration plan) become generally accessible, raising the requirement is a
# one-line change.

find_program(_nls_clang clang)
find_program(_nls_clangxx clang++)

if(_nls_clang)
  if(NOT CMAKE_C_COMPILER OR CMAKE_C_COMPILER STREQUAL "clang")
    set(CMAKE_C_COMPILER "${_nls_clang}")
  endif()
else()
  if(CMAKE_C_COMPILER STREQUAL "clang")
    unset(CMAKE_C_COMPILER CACHE)
  endif()
  message(WARNING "clang not found on PATH; falling back to C compiler auto-detection")
endif()

if(_nls_clangxx)
  if(NOT CMAKE_CXX_COMPILER OR CMAKE_CXX_COMPILER STREQUAL "clang++")
    set(CMAKE_CXX_COMPILER "${_nls_clangxx}")
  endif()
else()
  if(CMAKE_CXX_COMPILER STREQUAL "clang++")
    unset(CMAKE_CXX_COMPILER CACHE)
  endif()
  message(WARNING "clang++ not found on PATH; falling back to C++ compiler auto-detection")
endif()

unset(_nls_clang CACHE)
unset(_nls_clangxx CACHE)

project(nicels
  VERSION 0.1.0
  DESCRIPTION "Next generation ls clone"
  LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(CheckIPOSupported)

option(NLS_ENABLE_LIBGIT2 "Enable libgit2 integration" ON)
option(NLS_ENABLE_IPO "Enable interprocedural optimisations when available" ON)
option(NLS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NLS_ENABLE_COLOR_DIAGNOSTICS "Enable compiler colour diagnostics" ON)

if(NLS_ENABLE_IPO)
  check_ipo_supported(RESULT _nls_ipo_supported OUTPUT _nls_ipo_output)
  if(_nls_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(WARNING "IPO/LTO was requested but is not supported: ${_nls_ipo_output}")
  endif()
  unset(_nls_ipo_supported)
  unset(_nls_ipo_output)
endif()

find_package(CLI11 REQUIRED)

if(NLS_ENABLE_LIBGIT2)
  find_package(libgit2 REQUIRED)
endif()

file(GLOB _nls_sources
  CONFIGURE_DEPENDS
  src/*.cpp
)

file(GLOB _nls_headers
  CONFIGURE_DEPENDS
  src/*.h
)

add_executable(nls)
target_sources(nls
  PRIVATE
    ${_nls_sources}
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS src
      FILES ${_nls_headers}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${_nls_sources} ${_nls_headers})
unset(_nls_sources)
unset(_nls_headers)

set_target_properties(nls PROPERTIES
  OUTPUT_NAME nls
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

target_compile_features(nls PRIVATE cxx_std_23)

target_include_directories(nls
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nls
  PRIVATE
    CLI11::CLI11
)

if(NLS_ENABLE_LIBGIT2)
  target_link_libraries(nls PRIVATE libgit2::git2)
  target_compile_definitions(nls PRIVATE USE_LIBGIT2)
  target_include_directories(nls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/libgit2/include
    ${CMAKE_BINARY_DIR}/libgit2/include)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
  set(_nls_detected_system "${CMAKE_SYSTEM_NAME}")
else()
  set(_nls_detected_system "${CMAKE_SYSTEM_NAME} (host: ${CMAKE_HOST_SYSTEM_NAME})")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _nls_arch_lower)
set(_nls_known_arch "generic")
if(_nls_arch_lower MATCHES "(x86_64|amd64)")
  set(_nls_known_arch "x86_64")
elseif(_nls_arch_lower MATCHES "(i[3-6]86|x86)")
  set(_nls_known_arch "x86")
elseif(_nls_arch_lower MATCHES "(arm64|aarch64)")
  set(_nls_known_arch "arm64")
elseif(_nls_arch_lower MATCHES "arm")
  set(_nls_known_arch "arm")
endif()
unset(_nls_arch_lower)

string(TOUPPER "${_nls_known_arch}" _nls_arch_define)
string(REPLACE "+" "P" _nls_arch_define "${_nls_arch_define}")
string(REPLACE "-" "_" _nls_arch_define "${_nls_arch_define}")
message(STATUS "Configuring nicels for ${_nls_detected_system} on architecture ${_nls_known_arch}")

if(WIN32)
  target_compile_definitions(nls PRIVATE NLS_PLATFORM_WINDOWS)
else()
  target_compile_definitions(nls PRIVATE NLS_PLATFORM_POSIX)
endif()

target_compile_definitions(nls
  PRIVATE
    NLS_TARGET_SYSTEM="${CMAKE_SYSTEM_NAME}"
    NLS_TARGET_ARCHITECTURE="${_nls_known_arch}"
    NLS_TARGET_ARCH_${_nls_arch_define}
)
unset(_nls_arch_define)

if(NLS_WARNINGS_AS_ERRORS)
  set(_nls_warn_error -Werror)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(nls PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<BOOL:${NLS_ENABLE_COLOR_DIAGNOSTICS}>:-fcolor-diagnostics>
    ${_nls_warn_error}
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(nls PRIVATE -Wall -Wextra -Wpedantic ${_nls_warn_error})
endif()
unset(_nls_warn_error)

if(WIN32 AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  # Ensure the Windows builds produced under MSYS2/MinGW are self-contained and do not
  # require distributing libgcc_s_seh-1.dll, libwinpthread-1.dll or libstdc++-6.dll
  # alongside the executable.  These options request the static variants of the GCC/
  # libstdc++ runtimes and explicitly locate the static winpthread archive that ships
  # with the MinGW toolchain so only the desired runtime pieces are linked statically.
  target_link_options(nls PRIVATE -static-libgcc -static-libstdc++ -static)

  # MinGW installs both import libraries (libwinpthread.dll.a) and the fully static
  # archive (libwinpthread.a).  We temporarily adjust the lookup suffixes so the
  # archive is preferred, search relative to the detected compiler prefix, then restore
  # the caller's suffix list to avoid impacting any other find_library() calls.
  set(_nls_prev_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)

  get_filename_component(_nls_compiler_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_nls_toolchain_prefix "${_nls_compiler_bindir}" DIRECTORY)

  message(STATUS "Compiler dir: ${_nls_compiler_bindir}")
  message(STATUS "Toolchain dir: ${_nls_toolchain_prefix}")

  find_library(_nls_winpthread_static
    NAMES libwinpthread.a winpthread
    HINTS
      "${_nls_toolchain_prefix}/lib"
      "${_nls_toolchain_prefix}/lib64"
  )

  if(NOT _nls_winpthread_static)
    message(FATAL_ERROR "Unable to locate the static libwinpthread archive. Install the MinGW static runtime packages or adjust the search hints.")
  else()
    message(STATUS "Found static winpthread: ${_nls_winpthread_static}")
  endif()

  set(CMAKE_FIND_LIBRARY_SUFFIXES "${_nls_prev_library_suffixes}")
  unset(_nls_prev_library_suffixes)
  unset(_nls_compiler_bindir)
  unset(_nls_toolchain_prefix)

  target_link_libraries(nls PRIVATE "${_nls_winpthread_static}")
  unset(_nls_winpthread_static CACHE)
endif()


install(TARGETS nls
  FILE_SET HEADERS DESTINATION include/nls
  RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
  message(STATUS "No tests have been defined yet. Add them under the test/ directory.")
endif()
