# Repository Guidelines

## Project Structure & Module Organization
- `src/` C++23 implementation of the `nls` CLI; entry point in `src/main.cpp`.
- `includes/` public headers mirrored to `src/` filenames (e.g., `includes/config.h` + `src/config.cpp`).
- `DB/` default SQLite config (`NLS.sqlite3`) plus schema/migration helpers.
- `test/` CLI regression harness and fixtures (`test/run_nls_cli_tests.py`, `test/assets/`).
- `cmake/` presets, toolchains, and packaging helpers; `CMakePresets.json` defines build variants.
- `tools/`, `man/`, `docs/`, `images/`, `icons/` for scripts, manuals, and assets.
- `third-party/` vendored dependencies (CLI11, libgit2, sqlite).

## Build, Test, and Development Commands
- `git submodule update --init --recursive`: fetch vendored dependencies.
- `cmake --preset linux-clang` or `cmake --preset msys-clang`: configure out-of-source builds.
- `cmake --build --preset linux-clang-release` (or `msys-clang-release`): compile Release binaries into `build/<preset>/Release/`.
- `python test/run_nls_cli_tests.py --binary build/<preset>/Release/nls[.exe]`: run CLI regression suite and regenerate fixtures.
- `ctest -C Release -R nls_cli_tests --test-dir build/<preset>/`: invoke the same tests via CTest.
- `cpack -G NSIS --config build/msys-clang/CPackConfig.cmake`: build the Windows installer (optional).

### configure build and test from codex-cli in windows
- configure `c:\msys64\msys2_shell.cmd -ucrt64 -here -defterm -no-start -shell bash -c "cmake --preset msys-clang"`
- build release `c:\msys64\msys2_shell.cmd -ucrt64 -here -defterm -no-start -shell bash -c "cmake --build --preset msys-clang-release"`
- build debug `c:\msys64\msys2_shell.cmd -ucrt64 -here -defterm -no-start -shell bash -c "cmake --build --preset msys-clang-debug"`
- test `c:\msys64\msys2_shell.cmd -ucrt64 -here -defterm -no-start -shell bash -c "ctest -C Release -R nls_cli_tests --test-dir build/<preset>/"`
- create installer `c:\msys64\msys2_shell.cmd -ucrt64 -here -defterm -no-start -shell bash -c "cpack -G NSIS --config build/msys-clang/CPackConfig.cmake"`

## Coding Style & Naming Conventions
- C++ is ISO C++23; keep code in the `nls` namespace and align with existing Allman-style braces and 4-space indents.
- File names use `snake_case` and paired header/source files live in `includes/` + `src/`.
- No repo-wide formatter is enforced; keep formatting changes scoped and avoid touching `third-party/`.

## Testing Guidelines
- Add new CLI cases to `test/run_nls_cli_tests.py` with a descriptive `name` and explicit arguments.
- Fixture content is generated from `test/assets/` archives; update those only when output expectations change.
- Ensure DB-dependent checks use `DB/NLS.sqlite3` (tests read theme/icon data from it).

## Commit & Pull Request Guidelines
- Git history favors short, sentence-style subjects without prefixes; use a clear, imperative summary (e.g., "Update CLI db filters").
- PRs should include a concise description, testing commands run, and any platform notes; link issues when applicable.
- If CLI options or output change, update `README.md` and relevant `man/` pages.

## Configuration & Data
- Runtime config lives in a single SQLite file (`DB/NLS.sqlite3`); for local overrides use `NLS_DATA_DIR` or `NLS_DEV_MODE` as documented in `README.md`.
