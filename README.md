# Owl Engine

![Version](https://img.shields.io/badge/version-0.4.0-blue)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue?logo=cplusplus)
![CMake 3.24+](https://img.shields.io/badge/CMake-3.24%2B-blue?logo=cmake)
![GitHub License](https://img.shields.io/github/license/Silmaen/Owl)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/Silmaen/Owl)
![GitHub top language](https://img.shields.io/github/languages/top/Silmaen/Owl)
![GitHub Repo stars](https://img.shields.io/github/stars/Silmaen/Owl)
[![Website](https://img.shields.io/website?url=https%3A%2F%2Fowl.argawaen.net&label=owl.argawaen.net)](https://owl.argawaen.net)

![Owl Engine logo](engine_assets/logo/logo_owl.png)

**Owl Engine is the C++ engine that mixes rendering styles in one game: 2D, raycast, voxel, isometric and 3D,
stacked under one HUD, in the editor and in game.**

| ![2D RPG hub](doc/images/showcase/hub.png)            | ![Raycast level](doc/images/showcase/raycast.png)     |
|-------------------------------------------------------|-------------------------------------------------------|
| ![Voxel terrain](doc/images/showcase/voxel.png)       | ![Isometric town](doc/images/showcase/iso_town.png)   |
| ![2D platformer](doc/images/showcase/platformer.png)  | ![Main menu](doc/images/showcase/main_menu.png)       |

*Scenes of the Engine Demo (`engine_demo/`), rendered headless by the game runner on Vulkan.*

Documentation and downloads: [owl.argawaen.net](https://owl.argawaen.net). The name is a nod to the maintainer's
surname, Lachouette.

## Why Owl Engine

- **Render-style mixing**: 2D batches, tilemaps, a raycaster, voxel terrain, isometric maps and 3D meshes are
  renderer-stack layers that compose with each other and the HUD, in the editor viewport and in Play.
- **Owl Nest editor**: scenes, tilemaps, node graphs, animations, HUDs and prefabs with undo/redo and hot reload,
  plus an [MCP server](doc/pages/mcp.md) an AI agent can drive through the same editor commands as the user.
- **Engineering rigour**: unit and scenario tests, CI on Linux x64/arm64 and Windows, sanitizers, Vulkan validation
  clean and performance budgets checked against a baseline.

## How it is built

Owl Engine is developed by one maintainer with Claude Code as a development tool. Every change is reviewed and owned
by the maintainer, and goes through a strict CI: unit and scenario tests on Linux x64/arm64 and Windows, sanitizers,
clang-tidy, include checks and performance budgets.

## Engine Demo and samples

The **Engine Demo** (`engine_demo/`, see its `README.md`) is the all-in-one showcase: an RPG hub
leading to a platformer, a raycast level, an isometric town and a voxel terrain. Small real games live as sample
games (`samples/`, from v0.4.0).

## Features

- ![OpenGL](https://img.shields.io/badge/OpenGL-4.5-5586A4?logo=opengl) ![Vulkan](https://img.shields.io/badge/Vulkan-1.4%2B-AC162C?logo=vulkan)
  **Rendering** with Slang shaders compiled to SPIR-V, precompiled at build time
- ![EnTT](https://img.shields.io/badge/ECS-EnTT-green) **Entity-Component-System** with parent-child hierarchy,
  prefabs and Lua 5.5 scripting
- ![Box2D](https://img.shields.io/badge/Physics-Box2D-orange) 2D physics,
  ![OpenAL](https://img.shields.io/badge/Audio-OpenAL-8B0000) spatial sound
- ![Mesh](https://img.shields.io/badge/Mesh-OBJ%20%7C%20glTF%20%7C%20FBX-purple) 3D model loading, asset packs and a
  game runner

## Documentation

The full generated documentation is on [owl.argawaen.net](https://owl.argawaen.net); the pages also read on GitHub
([doc/pages](doc/pages)):

- [Getting Started](doc/pages/getting_started.md) -- First run of Owl Nest and where to go next
- [Architecture](doc/pages/architecture.md) -- Engine modules, backends, and shader pipeline
- [Editor](doc/pages/editor.md) -- Owl Nest editor: panels, prefabs, icons, undo/redo
- [MCP server](doc/pages/mcp.md) -- Drive Owl Nest from an AI agent
- [Scene System](doc/pages/scene.md) -- Hierarchy, components, serialization, triggers
- [Renderer](doc/pages/renderer.md) -- Renderer stack, shaders, textures, backgrounds
- [Physics](doc/pages/physics.md) -- Box2D integration, collisions, triggers
- [Sound System](doc/pages/sound.md) -- Components, spatial audio, and gameplay triggers
- [Events & Input](doc/pages/event_input.md) -- Event system, keyboard, mouse and gamepad
- [Lua Scripting](doc/pages/scripting.md) -- API, lifecycle, sandboxing
- [Lua API reference](doc/pages/lua-api.md) -- Every Lua binding, generated from the binding registry
- [Building](doc/pages/building.md) -- Prerequisites, presets, testing, and CMake options
- [Roadmap](doc/pages/roadmap.md) -- Planned and completed features by version (one-glance summary: `ROADMAP.md`)
- [Changelog](doc/pages/changelog.md) -- Detailed version history (one line per release: `CHANGELOG.md`)
- [Contributing](doc/pages/contributing.md) -- How to contribute (code style, checks, PRs, issues)
- [Sample games](samples/README.md) -- Small complete games made with Owl, beside the Engine Demo (Breakout)

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

Every command runs inside the build image through `./docker_run.sh` (drop the prefix to build with a local
toolchain, see [Building](doc/pages/building.md#prerequisites)). The first configure installs the Python tools and
builds the Conan dependencies; no server needs to be set up.

```bash
# Configure and build
./docker_run.sh cmake --preset linux-clang-release -S .
./docker_run.sh cmake --build output/build/linux-clang-release

# Run tests
./docker_run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure

# Run the editor
./docker_run.sh --gui output/build/linux-clang-release/bin/OwlNest
```

See [Building](doc/pages/building.md) for the full build guide and all available presets.

## Dependencies

Dependencies come from [Conan 2](https://conan.io) and [ConanCenter](https://conan.io/center), declared in
[conanfile.py](conanfile.py) and pinned by `conan.lock`; the few recipes missing from ConanCenter live in
`conan/recipes/`. They are fetched (or built) during CMake configure, from public infrastructure only.
