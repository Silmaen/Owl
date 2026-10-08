# Owl

![Version](https://img.shields.io/badge/version-0.3.0--dev-blue)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue?logo=cplusplus)
![CMake 3.24+](https://img.shields.io/badge/CMake-3.24%2B-blue?logo=cmake)
![GitHub License](https://img.shields.io/github/license/Silmaen/Owl)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/Silmaen/Owl)
![GitHub top language](https://img.shields.io/github/languages/top/Silmaen/Owl)
![GitHub Repo stars](https://img.shields.io/github/stars/Silmaen/Owl)

![](engine_assets/logo/logo_owl.png)

Owl is a C++23 game engine built for learning game engine development. It features multiple
graphics, input, and sound backends, an Entity-Component-System architecture, and a scene
editor.

The full generated documentation is available online:
[![Website](https://img.shields.io/website?url=https%3A%2F%2Fowl.argawaen.net&label=owl%20site&link=https%3A%2F%2Fowl.argawaen.net)](https://owl.argawaen.net)

**Documentation pages** ([browse on GitHub](doc/pages)):

- [Architecture](doc/pages/architecture.md) -- Engine modules, backends, and shader pipeline
- [Editor](doc/pages/editor.md) -- Owl Nest editor: panels, prefabs, icons, undo/redo
- [Scene System](doc/pages/scene.md) -- Hierarchy, components, serialization, triggers
- [Renderer](doc/pages/renderer.md) -- 2D renderer, shaders, textures, backgrounds
- [Physics](doc/pages/physics.md) -- Box2D integration, collisions, triggers
- [Sound System](doc/pages/sound.md) -- Sound system: components, spatial audio, and gameplay triggers
- [Events & Input](doc/pages/event_input.md) -- Event system, keyboard and mouse
- [Lua Scripting](doc/pages/scripting.md) -- Lua scripting: API, lifecycle, sandboxing
- [Lua API reference](doc/pages/lua-api.md) -- Every Lua binding, generated from the binding registry
- [Building](doc/pages/building.md) -- Prerequisites, presets, testing, and CMake options
- [Roadmap](doc/pages/roadmap.md) -- Planned and completed features by version (one-glance summary: `ROADMAP.md`)
- [Changelog](doc/pages/changelog.md) -- Detailed version history (one line per release: `CHANGELOG.md`)
- [Contributing](doc/pages/contributing.md) -- How to contribute (code style, checks, PRs, issues)

## Features

- ![OpenGL](https://img.shields.io/badge/OpenGL-4.5-5586A4?logo=opengl) ![Vulkan](https://img.shields.io/badge/Vulkan-1.4%2B-AC162C?logo=vulkan)
  **Rendering** with Slang shaders compiled to SPIR-V
- **Mixed render styles** in one game: 2D batches, tilemaps, raycaster, voxel terrain and 3D meshes stacked as
  layers under the HUD
- ![EnTT](https://img.shields.io/badge/ECS-EnTT-green) **Entity-Component-System** architecture with
  parent-child hierarchy, prefabs and Lua scripting
- ![Box2D](https://img.shields.io/badge/Physics-Box2D-orange) 2D physics simulation
- ![OpenAL](https://img.shields.io/badge/Audio-OpenAL-8B0000) Sound playback backend
- ![ImGui](https://img.shields.io/badge/Editor-ImGui-blue) Owl Nest editor (scenes, tilemaps, node graphs,
  animations) with hot reload and a game runner
- ![Mesh](https://img.shields.io/badge/Mesh-OBJ%20%7C%20glTF%20%7C%20FBX-purple) 3D model loading
- ![Linux](https://img.shields.io/badge/Linux-x64%20%7C%20arm64-FCC624?logo=linux&logoColor=black) ![Windows](https://img.shields.io/badge/Windows-x64-0078D4?logo=windows&logoColor=white)
  Cross-platform

## Supported Platforms

![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
![Windows](https://img.shields.io/badge/Windows-0078D4?logo=windows&logoColor=white)

| OS      | Architecture | Compilers                      |
|---------|--------------|--------------------------------|
| Linux   | x64, arm64   | Clang 22+, GCC 14+             |
| Windows | x64          | MinGW Clang 22+, MinGW GCC 15+ |

## Backends

![OpenGL](https://img.shields.io/badge/OpenGL-4.5-5586A4?logo=opengl)
![Vulkan](https://img.shields.io/badge/Vulkan-1.4%2B-AC162C?logo=vulkan)
![GLFW](https://img.shields.io/badge/GLFW-Input-yellow)
![OpenAL](https://img.shields.io/badge/OpenAL-Audio-8B0000)

| Category | Backends                      |
|----------|-------------------------------|
| Graphics | OpenGL 4.5, Vulkan 1.4+, Null |
| Input    | GLFW, Null                    |
| Sound    | OpenAL, Null                  |

## Quick Start

Every command runs inside the build image through `docker/run.sh` (drop the prefix to build with a local
toolchain, see [Building](doc/pages/building.md#prerequisites)). The first configure installs the Python tools and
builds the Conan dependencies; no server needs to be set up.

```bash
# Configure and build
docker/run.sh cmake --preset linux-clang-release -S .
docker/run.sh cmake --build output/build/linux-clang-release

# Run tests
docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure

# Run the editor
docker/run.sh --gui output/build/linux-clang-release/bin/OwlNest
```

See [Building](doc/pages/building.md) for the full build guide and all available presets.

## Dependencies

Dependencies come from [Conan 2](https://conan.io) and [ConanCenter](https://conan.io/center), declared in
[conanfile.py](conanfile.py) and pinned by `conan.lock`; the few recipes missing from ConanCenter live in
`conan/recipes/`. They are fetched (or built) during CMake configure, from public infrastructure only.
