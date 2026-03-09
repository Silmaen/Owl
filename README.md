# Owl

![Version](https://img.shields.io/badge/version-0.0.2-blue)
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

- \subpage architecture -- Engine modules, backends, and shader pipeline
- \subpage building -- Prerequisites, presets, testing, and CMake options
- \subpage roadmap -- Planned and completed features by version
- \subpage contributing -- Code style, conventions, and workflow

## Features

- ![OpenGL](https://img.shields.io/badge/OpenGL-4.5-5586A4?logo=opengl) ![Vulkan](https://img.shields.io/badge/Vulkan-1.4%2B-AC162C?logo=vulkan)
  **Rendering** with Slang shaders compiled to SPIR-V
- ![EnTT](https://img.shields.io/badge/ECS-EnTT-green) **Entity-Component-System** architecture
- ![Box2D](https://img.shields.io/badge/Physics-Box2D-orange) 2D physics simulation
- ![OpenAL](https://img.shields.io/badge/Audio-OpenAL-8B0000) Sound playback backend
- ![ImGui](https://img.shields.io/badge/Editor-ImGui-blue) Owl Nest scene editor with project management
- ![Mesh](https://img.shields.io/badge/Mesh-OBJ%20%7C%20glTF%20%7C%20FBX-purple) 3D model loading
- ![Linux](https://img.shields.io/badge/Linux-x64%20%7C%20arm64-FCC624?logo=linux&logoColor=black) ![Windows](https://img.shields.io/badge/Windows-x64-0078D4?logo=windows&logoColor=white)
  Cross-platform

## Supported Platforms

![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
![Windows](https://img.shields.io/badge/Windows-0078D4?logo=windows&logoColor=white)

| OS      | Architecture | Compilers                      |
|---------|--------------|--------------------------------|
| Linux   | x64, arm64   | GCC 13+, Clang 18+             |
| Windows | x64          | MinGW GCC 14+, MinGW Clang 19+ |

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

```bash
# Install Python dependencies (for DepManager and CI tools)
poetry sync --no-root

# Configure and build
cmake --preset linux-gcc-release -S .
cmake --build output/build/linux-gcc-release

# Run tests
ctest --test-dir output/build/linux-gcc-release --output-on-failure
```

See @ref building for the full build guide and all available presets.

## Dependencies

Dependencies are managed by [DepManager](https://github.com/Silmaen/DepManager)
![GitHub Tag](https://img.shields.io/github/v/tag/Silmaen/DepManager)
and declared in [depmanager.yml](depmanager.yml). They are automatically downloaded
during CMake configure.

Dependency recipes are maintained in a separate repository:
[OwlDependencies](https://github.com/Silmaen/OwlDependencies).
