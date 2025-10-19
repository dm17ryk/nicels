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

set(_nls_version_file "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_nls_version_file}")
if(NOT EXISTS "${_nls_version_file}")
  message(FATAL_ERROR "VERSION file not found at ${_nls_version_file}")
endif()

file(READ "${_nls_version_file}" _nls_version_contents)
string(REGEX MATCH "^[^\n\r]*" _nls_version_raw "${_nls_version_contents}")
string(STRIP "${_nls_version_raw}" _nls_version_raw)
string(REPLACE "." ";" _nls_version_parts "${_nls_version_raw}")
list(LENGTH _nls_version_parts _nls_version_count)
if(_nls_version_count LESS 2 OR _nls_version_count GREATER 4)
  message(FATAL_ERROR "VERSION must contain two to four dot-separated numbers")
endif()

list(GET _nls_version_parts 0 NLS_VERSION_MAJOR)
list(GET _nls_version_parts 1 NLS_VERSION_MINOR)
if(_nls_version_count GREATER 2)
  list(GET _nls_version_parts 2 NLS_VERSION_MAINTENANCE)
else()
  set(NLS_VERSION_MAINTENANCE 0)
endif()
if(_nls_version_count GREATER 3)
  list(GET _nls_version_parts 3 NLS_VERSION_BUILD)
else()
  set(NLS_VERSION_BUILD 0)
endif()

foreach(_nls_component IN ITEMS NLS_VERSION_MAJOR NLS_VERSION_MINOR NLS_VERSION_MAINTENANCE NLS_VERSION_BUILD)
  if(NOT ${_nls_component} MATCHES "^[0-9]+$")
    message(FATAL_ERROR "VERSION component '${${_nls_component}}' for ${_nls_component} is not numeric")
  endif()
endforeach()

if(_nls_version_count GREATER 2)
  set(_nls_version_has_maintenance 1)
else()
  set(_nls_version_has_maintenance 0)
endif()
if(_nls_version_count GREATER 3)
  set(_nls_version_has_build 1)
else()
  set(_nls_version_has_build 0)
endif()

set(NLS_VERSION_CORE_STRING "${NLS_VERSION_MAJOR}.${NLS_VERSION_MINOR}")
set(NLS_VERSION_STRING "${NLS_VERSION_CORE_STRING}")
if(_nls_version_has_maintenance)
  string(APPEND NLS_VERSION_STRING ".${NLS_VERSION_MAINTENANCE}")
  if(_nls_version_has_build)
    string(APPEND NLS_VERSION_STRING ".${NLS_VERSION_BUILD}")
  endif()
endif()

message(STATUS "Configuring nicels version ${NLS_VERSION_STRING}")

project(nicels
  VERSION ${NLS_VERSION_MAJOR}.${NLS_VERSION_MINOR}.${NLS_VERSION_MAINTENANCE}
  DESCRIPTION "Next generation ls clone"
  LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(CheckIPOSupported)
include(GNUInstallDirs)

set(_nls_buildtree_install_prefix "${CMAKE_BINARY_DIR}/install")
cmake_path(NORMAL_PATH _nls_buildtree_install_prefix OUTPUT_VARIABLE _nls_buildtree_install_prefix)
set(_nls_current_install_prefix "${CMAKE_INSTALL_PREFIX}")
cmake_path(NORMAL_PATH _nls_current_install_prefix OUTPUT_VARIABLE _nls_current_install_prefix)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR
   _nls_current_install_prefix STREQUAL _nls_buildtree_install_prefix)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()
unset(_nls_buildtree_install_prefix)
unset(_nls_current_install_prefix)

set(NLS_GLOBAL_CONFIG_INSTALL_DIR "/etc/dm17ryk/nicels"
  CACHE PATH "Install location for nicels global configuration files")
if(NOT IS_ABSOLUTE "${NLS_GLOBAL_CONFIG_INSTALL_DIR}")
  message(FATAL_ERROR "NLS_GLOBAL_CONFIG_INSTALL_DIR must be an absolute path")
endif()

option(NLS_ENABLE_LIBGIT2 "Enable libgit2 integration" ON)
option(NLS_ENABLE_IPO "Enable interprocedural optimisations when available" ON)
option(NLS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NLS_ENABLE_COLOR_DIAGNOSTICS "Enable compiler colour diagnostics" ON)

set(_nls_enable_ipo FALSE)
if(NLS_ENABLE_IPO)
  check_ipo_supported(RESULT _nls_ipo_supported OUTPUT _nls_ipo_output)
  if(_nls_ipo_supported)
    set(_nls_enable_ipo TRUE)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MSVC)
      # ThinLTO on Clang/MinGW relies on the LLVMgold plugin and libLTO shipped
      # with the toolchain.  Older MSYS2 installations can miss these runtime
      # pieces even though the compiler itself is available.  Detect the
      # situation early and provide an actionable hint instead of failing during
      # the final link step.
      get_filename_component(_nls_clang_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
      get_filename_component(_nls_clang_root "${_nls_clang_bindir}/.." ABSOLUTE)
      set(_nls_llvm_search_paths
        "${_nls_clang_bindir}"
        "${_nls_clang_root}"
      )
      find_file(_nls_llvmgold_plugin
        NAMES LLVMgold.dll LLVMgold.so
        PATHS ${_nls_llvm_search_paths}
        PATH_SUFFIXES lib lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib
      )
      find_file(_nls_liblto
        NAMES libLTO.dll libLTO.so
        PATHS ${_nls_llvm_search_paths}
        PATH_SUFFIXES bin lib lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib
      )
      if(NOT (_nls_llvmgold_plugin AND _nls_liblto))
        message(WARNING
          "IPO/LTO disabled: install the 'mingw-w64-ucrt-x86_64-llvm-libs'"
          " package (and its dependencies) so GNU ld can load LLVMgold/libLTO")
        set(_nls_enable_ipo FALSE)
      endif()
      unset(_nls_llvmgold_plugin)
      unset(_nls_liblto)
      unset(_nls_clang_bindir)
      unset(_nls_clang_root)
      unset(_nls_llvm_search_paths)
    endif()
  else()
    message(WARNING "IPO/LTO was requested but is not supported: ${_nls_ipo_output}")
  endif()
  unset(_nls_ipo_supported)
  unset(_nls_ipo_output)
endif()

if(_nls_enable_ipo)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
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
  includes/*.h
)

add_executable(nls)
target_sources(nls
  PRIVATE
    ${_nls_sources}
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS includes
      FILES ${_nls_headers}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${_nls_sources})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/includes" FILES ${_nls_headers})
unset(_nls_sources)
unset(_nls_headers)

add_library(nls_sqlite3 STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/third-party/sqlite/sqlite3.c"
)

target_include_directories(nls_sqlite3
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sqlite
)

target_compile_definitions(nls_sqlite3
  PUBLIC
    SQLITE_THREADSAFE=1
  PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
)

set_target_properties(nls_sqlite3 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
  set(_nls_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/icons/nls.ico")
  if(NOT EXISTS "${_nls_app_icon}")
    message(FATAL_ERROR "Application icon not found at ${_nls_app_icon}")
  endif()

  set(_nls_rc_file "${CMAKE_CURRENT_BINARY_DIR}/nls.rc")
  file(TO_NATIVE_PATH "${_nls_app_icon}" _nls_app_icon_native)
  string(REPLACE "\\" "\\\\" _nls_app_icon_native "${_nls_app_icon_native}")
  set(NLS_ICON_PATH "${_nls_app_icon_native}")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nls.rc.in"
    "${_nls_rc_file}"
    @ONLY
  )
  unset(NLS_ICON_PATH)
  target_sources(nls PRIVATE "${_nls_rc_file}")
endif()

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
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sqlite
)

target_link_libraries(nls
  PRIVATE
    nls_sqlite3
    CLI11::CLI11
)

target_compile_definitions(nls
  PRIVATE
    NLS_VERSION_MAJOR=${NLS_VERSION_MAJOR}
    NLS_VERSION_MINOR=${NLS_VERSION_MINOR}
    NLS_VERSION_MAINTENANCE=${NLS_VERSION_MAINTENANCE}
    NLS_VERSION_BUILD=${NLS_VERSION_BUILD}
    NLS_VERSION_HAS_MAINTENANCE=${_nls_version_has_maintenance}
    NLS_VERSION_HAS_BUILD=${_nls_version_has_build}
    NLS_VERSION_STRING="${NLS_VERSION_STRING}"
    NLS_VERSION_CORE_STRING="${NLS_VERSION_CORE_STRING}"
)

set(NLS_RELEASE_COMPILE_OPTIONS)
set(NLS_RELEASE_LINK_OPTIONS)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  list(APPEND NLS_RELEASE_COMPILE_OPTIONS
    -O3
    -fstrict-aliasing
    -fomit-frame-pointer
    -fdata-sections
    -ffunction-sections
  )
  list(APPEND NLS_RELEASE_LINK_OPTIONS -Wl,-O3)
  if(APPLE)
    list(APPEND NLS_RELEASE_LINK_OPTIONS -Wl,-dead_strip)
  else()
    list(APPEND NLS_RELEASE_LINK_OPTIONS -Wl,--gc-sections)
  endif()
elseif(MSVC)
  list(APPEND NLS_RELEASE_COMPILE_OPTIONS /O2 /Oi /Ot /GL /Gy /fp:fast)
  list(APPEND NLS_RELEASE_LINK_OPTIONS /LTCG /OPT:REF /OPT:ICF)
endif()

if(NLS_RELEASE_COMPILE_OPTIONS)
  target_compile_options(nls PRIVATE $<$<CONFIG:Release>:${NLS_RELEASE_COMPILE_OPTIONS}>)
  target_compile_options(nls_sqlite3 PRIVATE $<$<CONFIG:Release>:${NLS_RELEASE_COMPILE_OPTIONS}>)
endif()

if(NLS_RELEASE_LINK_OPTIONS)
  target_link_options(nls PRIVATE $<$<CONFIG:Release>:${NLS_RELEASE_LINK_OPTIONS}>)
endif()

if(NLS_ENABLE_LIBGIT2)
  target_link_libraries(nls PRIVATE libgit2::git2)
  target_compile_definitions(nls PRIVATE USE_LIBGIT2)
  target_include_directories(nls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/libgit2/include
    ${CMAKE_BINARY_DIR}/libgit2/include)
endif()

if(_nls_enable_ipo)
  set_property(TARGET nls PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

unset(_nls_enable_ipo)

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
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nls
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(_nls_db_source "${CMAKE_CURRENT_SOURCE_DIR}/DB/NLS.sqlite3")
set(_nls_db_install_path)
if(EXISTS "${_nls_db_source}")
  if(WIN32)
    set(_nls_db_install_destination "DB")
  elseif(UNIX AND NOT APPLE)
    set(_nls_db_install_destination "${NLS_GLOBAL_CONFIG_INSTALL_DIR}")
  else()
    set(_nls_db_install_destination "${CMAKE_INSTALL_DATADIR}/nicels/DB")
  endif()

  install(FILES "${_nls_db_source}"
    DESTINATION "${_nls_db_install_destination}")

  if(IS_ABSOLUTE "${_nls_db_install_destination}")
    set(_nls_db_install_path "${_nls_db_install_destination}/NLS.sqlite3")
  endif()
endif()

unset(_nls_db_install_destination)
unset(_nls_db_source)

include(CTest)
if(BUILD_TESTING)
  message(STATUS "No tests have been defined yet. Add them under the test/ directory.")
endif()

set(CPACK_PACKAGE_NAME "nicels")
set(CPACK_PACKAGE_VENDOR "nicels Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Next generation ls clone")
set(CPACK_PACKAGE_CONTACT "https://github.com/nicels")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "nicels")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "nicels")
set(CPACK_PACKAGE_VERSION ${NLS_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${NLS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${NLS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${NLS_VERSION_MAINTENANCE})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/nicels/nicels")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")

set(_nls_cpack_arch_candidate "${CMAKE_SYSTEM_PROCESSOR}")
if(NOT _nls_cpack_arch_candidate AND CMAKE_HOST_SYSTEM_PROCESSOR)
  set(_nls_cpack_arch_candidate "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

if(WIN32 AND CMAKE_GENERATOR_PLATFORM)
  string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _nls_cpack_vs_platform)
  if(_nls_cpack_vs_platform STREQUAL "win32")
    set(_nls_cpack_arch_candidate "x86")
  elseif(_nls_cpack_vs_platform STREQUAL "x64")
    set(_nls_cpack_arch_candidate "x86_64")
  elseif(_nls_cpack_vs_platform STREQUAL "arm")
    set(_nls_cpack_arch_candidate "arm")
  elseif(_nls_cpack_vs_platform STREQUAL "arm64")
    set(_nls_cpack_arch_candidate "aarch64")
  endif()
endif()

string(TOLOWER "${_nls_cpack_arch_candidate}" _nls_cpack_arch_lower)
if(_nls_cpack_arch_lower STREQUAL "amd64" OR _nls_cpack_arch_lower STREQUAL "x64" OR
   _nls_cpack_arch_lower STREQUAL "x86_64")
  set(_nls_cpack_arch "x86_64")
elseif(_nls_cpack_arch_lower STREQUAL "arm64" OR _nls_cpack_arch_lower STREQUAL "aarch64")
  set(_nls_cpack_arch "aarch64")
elseif(_nls_cpack_arch_lower MATCHES "^i[3-6]86$" OR _nls_cpack_arch_lower STREQUAL "x86" OR
       _nls_cpack_arch_lower STREQUAL "ia32")
  set(_nls_cpack_arch "x86")
elseif(_nls_cpack_arch_lower MATCHES "^armv[0-9]+" OR _nls_cpack_arch_lower STREQUAL "arm" OR
       _nls_cpack_arch_lower STREQUAL "armhf" OR _nls_cpack_arch_lower STREQUAL "armel")
  set(_nls_cpack_arch "arm")
elseif(_nls_cpack_arch_candidate)
  set(_nls_cpack_arch "${_nls_cpack_arch_candidate}")
else()
  set(_nls_cpack_arch "unknown")
endif()

set(CPACK_SYSTEM_NAME "${_nls_cpack_arch}")
set(CPACK_PACKAGE_FILE_NAME "nicels-${NLS_VERSION_STRING}-${_nls_cpack_arch}")

if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_PACKAGE_NAME "nicels")
  set(CPACK_NSIS_DISPLAY_NAME "nicels")
  set(CPACK_NSIS_HELP_LINK "https://github.com/nicels/nicels")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/nicels/nicels")
  set(CPACK_NSIS_CONTACT "https://github.com/nicels/nicels/issues")
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
  # Escape the executable name to avoid NSIS interpreting the "\n" in
  # "nls.exe" as a literal newline when generating the script.  Without the
  # doubled backslash the resulting installer aborts while writing the
  # DisplayIcon registry entry.
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\nls.exe")
  set(CPACK_NSIS_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NSIS/NicelsNSISTemplate.in")
  set(CPACK_NSIS_INSTALL_OPTIONS_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NSIS/NicelsInstallOptions.ini.in")
  set(CPACK_NSIS_MUI_ICON "${_nls_app_icon}")
  set(CPACK_NSIS_MUI_UNIICON "${_nls_app_icon}")
  set(CPACK_NSIS_INSTALLER_ICON "${_nls_app_icon}")
  set(CPACK_NSIS_UNINSTALLER_ICON "${_nls_app_icon}")
elseif(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB;RPM")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_nls_cpack_arch}")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "${_nls_cpack_arch}")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nicels maintainers")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/nicels/nicels")
  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "nicels is a feature-rich replacement for ls that adds icons, git status, and modern defaults.")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
  set(CPACK_RPM_PACKAGE_URL "https://github.com/nicels/nicels")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
  set(CPACK_RPM_PACKAGE_DESCRIPTION
    "nicels is a feature-rich replacement for ls that adds icons, git status, and modern defaults.")

  if(_nls_db_install_path)
    set(_nls_cpack_aux_dir "${CMAKE_CURRENT_BINARY_DIR}/cpack")
    file(MAKE_DIRECTORY "${_nls_cpack_aux_dir}")

    set(_nls_debian_conffiles "${_nls_cpack_aux_dir}/conffiles")
    file(WRITE "${_nls_debian_conffiles}" "${_nls_db_install_path}\n")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${_nls_debian_conffiles}")

    set(CPACK_RPM_USER_FILELIST "%config(noreplace) ${_nls_db_install_path}")
    unset(_nls_debian_conffiles)
  endif()
endif()
unset(_nls_cpack_aux_dir)

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  "/[.]git/"
  "/[.]github/"
  "/[.]gitignore"
  "/[.]gitmodules"
  "/build/"
  "_CPack_Packages/"
  "CPackConfig.cmake"
  "CPackSourceConfig.cmake"
  "/cmake-build-"
  "/third-party/libgit2/[.]git"
)

include(CPack)
unset(_nls_db_install_path)
unset(_nls_cpack_arch)
unset(_nls_cpack_arch_candidate)
unset(_nls_cpack_arch_lower)
unset(_nls_cpack_vs_platform)
