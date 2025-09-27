# nicels

## Building with CMake

1. Ensure the CLI11 submodule is available:

   ```bash
   git submodule update --init --recursive
   ```

2. Configure and build:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

The executable is produced under `bin/<Config>` (e.g. `bin/Release`) and copied to the repository root.
