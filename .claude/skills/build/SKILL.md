---
name: build
description: Configure and build the Owl project with a CMake preset inside the Docker build image
---

# Build the project

All commands go through `docker/run.sh` (never native).

1. Preset: the argument if given, else `linux-clang-release`. Use `linux-clang-debug` for coverage work.
2. Configure only if `output/build/<preset>/CMakeCache.txt` is missing, a file was added/removed (GLOB),
   or the user asks:
   ```bash
   docker/run.sh cmake --preset <preset> -S .
   ```
3. Build (optionally `--target <name>`):
   ```bash
   docker/run.sh cmake --build output/build/<preset>
   ```
4. On failure, quote the first real error (not the cascade), fix or propose a fix.
5. Report success/failure and the number of steps built.
