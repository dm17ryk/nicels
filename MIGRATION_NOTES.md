# Migration Notes

## Overview

The project has been refactored to a modular C++23 architecture. Major subsystems now live under the `nicels` namespace and are split into dedicated components for configuration, CLI parsing, filesystem scanning, Git integration, rendering, and logging. CLI11 powers argument parsing and a modern CMake build replaces the previous Makefile.

## Key Modules

- **`nicels::App`** – orchestrates CLI parsing, scanning, and rendering.
- **`nicels::Cli`** – wraps CLI11 configuration to populate `nicels::Config`.
- **`nicels::FileSystemScanner`** – enumerates directory contents with filtering, sorting, and Git status integration.
- **`nicels::Renderer`** – handles output formatting (long listings, column variants, colors, hyperlinks).
- **`nicels::GitRepositoryStatus`** – libgit2-backed status retrieval.
- **`nicels::Logger`** – minimal singleton for leveled diagnostics.

## Build System

- New `CMakeLists.txt` targeting C++23 with optional warning flags and automatic libgit2 detection via pkg-config or fallback search.
- Multi-config layout writes binaries to `bin/{Config}` and objects to `obj/{Config}` while copying the final executable to the repository root after each build.

## CLI & Behaviour

- CLI11 defines all GNU `ls` compatible options from the original tool; defaults remain unchanged.
- Git status relies on libgit2; ensure the development package is available when configuring CMake.

## Follow-up

- Additional renderer polish (column layout, advanced quoting styles, theme/icon loading) can be layered on top of the new module structure.
- Extend `Renderer` and `FileSystemScanner` to cover any edge cases from the legacy implementation as needed.

