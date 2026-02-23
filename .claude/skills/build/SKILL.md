---
name: build
description: Configure and build the Owl project with a CMake preset
---

# Build the project

1. If no preset is specified in the arguments, ask the user which preset to use. Common presets:
   - `linux-gcc-release` (default for Linux dev)
   - `linux-clang-release`
   - `linux-gcc-debug`, `linux-clang-debug`
2. Configure (only if `output/build/<preset>` doesn't exist or user requests):
   ```bash
   cmake --preset <preset> -S .
   ```
3. Build:
   ```bash
   cmake --build output/build/<preset>
   ```
4. If the build fails, analyze the error output and suggest fixes.
5. Report success/failure and number of targets built.
