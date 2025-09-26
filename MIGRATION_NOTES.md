# Migration Notes

## Overview

The original `nls` bootstrap has been restructured into a modular C++23 application named **nicels**. The project now uses CMake for cross-platform builds, CLI11 for option parsing, and an object-oriented architecture to remove global state.

## Key Refactors

- Introduced `App`, `Config`, `Cli`, `FileSystemScanner`, `Renderer`, `Theme`, `GitStatusCache`, and `Logger` components with clear responsibilities and dependency injection.
- All global mutable state has been eliminated. `Config` and `Logger` are the only singletons and follow thread-safe lazy initialization.
- Replaced the prior Makefile with a modern CMake build (Debug/Release presets) and CPack packaging targets for `.deb`, `.rpm`, and `.msix` artifacts.
- Added helpers for platform concerns (TTY detection, ANSI enabling), formatting utilities, and scoped performance timers.
- Established README generation hooks (`--help-markdown`) to keep CLI documentation synchronized with code-defined options.
- Reorganized sources into `include/nicels` and `src/nicels` for clear modular compilation units.

## Follow-Up Ideas

- Integrate real Git status retrieval (libgit2 or `git status --porcelain` parser) and wire into `GitStatusCache`.
- Expand theme/icon coverage and expose configuration via files/environment.
- Add richer tests and golden outputs for deterministic validation across platforms.
- Consider benchmarking harnesses for filesystem and Git-heavy workloads.
