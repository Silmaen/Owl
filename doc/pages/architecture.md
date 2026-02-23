# Architecture Overview {#architecture}

[TOC]

This page describes the high-level architecture of the Owl engine.

## Engine Modules

The engine library (`source/owl/`) is organized into the following modules:

| Module       | Description                                                  |
|--------------|--------------------------------------------------------------|
| `core`       | Application lifecycle, logging, assertions, smart pointers   |
| `renderer`   | Rendering abstraction, buffers, shaders, framebuffers        |
| `scene`      | Entity-Component-System (EnTT), scene graph, components      |
| `physic`     | 2D physics (Box2D integration)                               |
| `sound`      | Audio playback and device management                         |
| `input`      | Keyboard, mouse, and gamepad input abstraction               |
| `window`     | Window creation and management                               |
| `gui`        | ImGui/ImGuizmo integration for editor UI                     |
| `data`       | Geometry, mesh loading (OBJ, glTF, FBX), data structures     |
| `math`       | Math utilities (zeus library)                                |
| `debug`      | Profiling, memory tracking, stack traces (cpptrace)          |
| `event`      | Event system (application, input, window events)             |
| `io`         | File I/O, serialization (YAML, XML)                          |

Public headers live in `source/owl/public/` and implementation files in `source/owl/private/`,
both mirroring the module structure.

## Backend System

Owl uses a backend abstraction so that different platform APIs can be swapped at runtime.

### Graphics Backends

| Backend    | API          | Notes                                           |
|------------|--------------|-------------------------------------------------|
| `OpenGL`   | OpenGL 4.5   | Widely supported on desktop; limited on ARM64   |
| `Vulkan`   | Vulkan 1.3+  | Modern low-level API; full desktop support      |
| `Null`     | None         | Headless mode for servers or testing            |

### Input Backends

| Backend | Library | Notes                              |
|---------|---------|------------------------------------|
| `GLFW`  | GLFW    | Windowing, keyboard, mouse, gamepad|
| `Null`  | None    | Headless mode                      |

### Sound Backends

| Backend  | Library | Notes                             |
|----------|---------|-----------------------------------|
| `OpenAL` | OpenAL  | Audio playback and spatial sound  |
| `Null`   | None    | Silent mode                       |

## Applications

The project produces several executables built on top of the `OwlEngine` shared library:

| Application | Directory          | Description                                    |
|-------------|--------------------|------------------------------------------------|
| Owl Nest    | `source/owlnest/`  | Scene editor (editor + runner executables)      |
| Owl Drone   | `source/owldrone/` | Drone navigator (MAVLink via MAVSDK)            |
| Owl Cast    | `source/owlcast/`  | Cast application                                |
| Sandbox     | `source/sandbox/`  | Testing and prototyping playground              |

## Shader Pipeline

Shaders are written in **Slang** (`.slang` files), a single-source shading language:

1. **Source**: `engine_assets/shaders/<renderer>/slang/<name>.slang`
2. **Compilation**: `compileSlangToSpirv()` compiles Slang to SPIR-V at runtime using the Slang C++ API, with `BACKEND_VULKAN` or `BACKEND_OPENGL` preprocessor defines
3. **Reflection**: `shaderReflect()` uses spirv-cross to extract uniform buffers and sampled images from the SPIR-V bytecode
4. **Caching**: SPIR-V binaries are cached as `.spv` files with hash-based validation

Key conventions:
- Entry points: `[shader("vertex")] vertexMain` and `[shader("fragment")] fragmentMain`
- Matrices: `column_major float4x4` for C++ interop (Slang defaults to row-major)
- Vulkan bindings: `[[vk::binding(N)]]` for explicit descriptor bindings
- Backend branching: `#ifdef BACKEND_VULKAN` for texture binding differences

## Dependency Management

Dependencies are managed by [DepManager](https://github.com/Silmaen/DepManager) and declared
in `depmanager.yml` at the project root. During CMake configure, the `cmake/Depmanager.cmake`
module automatically downloads missing packages from the configured remote server.

See @ref building for instructions on configuring and building with dependencies.
