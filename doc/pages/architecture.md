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
| Owl Nest    | `source/owlnest/`  | Scene editor with project management (editor + runner executables) |
| Owl Drone   | `source/owldrone/` | Drone navigator (MAVLink via MAVSDK)            |
| Owl Cast    | `source/owlcast/`  | Cast application                                |
| Sandbox     | `source/sandbox/`  | Testing and prototyping playground              |

## Project System

Owl Nest supports a project-based workflow. A project is a directory containing an
`owl_project.yml` configuration file:

```yaml
OwlProject:
  name: "My Project"
  firstScene: "scenes/Example.owl"
```

When a project is opened, its directory is added as a high-priority asset directory,
making its contents visible in the content browser. The editor window title reflects
the active project name. Scenes can be imported into the project via the **Project >
Import Scene** menu item.

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

## Scene Hierarchy

Entities support parent-child relationships via the **Hierarchy** component (mandatory on every entity).

### Transform Inheritance

Each entity's `Transform` component stores a **local** transform (relative to its parent).
The world-space transform is computed on demand by walking the parent chain:

```
worldTransform = parentWorldTransform * localTransform
```

For root entities (`parentId == 0`), local equals world (zero overhead).

### Visibility Inheritance

If any ancestor is hidden (editor or game mode), the entity is effectively hidden.
`Scene::isEffectivelyVisible()` walks the parent chain to check.

### Hierarchy Operations

| Operation | Behavior |
|-----------|----------|
| **Set parent** | Circular reference check, local transform recomputed to preserve world position |
| **Unparent** | Entity becomes root, world transform stored as new local |
| **Delete entity** | Children reparented to grandparent (or root); world position preserved |
| **Delete with children** | Cascade delete of entire subtree |
| **Duplicate entity** | Duplicate is a root entity with no children |
| **Duplicate subtree** | Recursive duplicate with new UUIDs and correct parent references |

### Physics and Hierarchy

Physics bodies (Box2D) operate in **world space** independently of the scene hierarchy.
The hierarchy does **not** create physical constraints between entities.

| Situation | Behavior |
|-----------|----------|
| Non-physics parent moves → physics child | Child **stays in place** (Box2D controls its world position). Its local transform is recalculated each frame relative to the moving parent. |
| Physics parent falls → physics child | Each body moves **independently** according to Box2D simulation. No physical link. |
| Physics parent moves → non-physics child | Child **follows** the parent via transform inheritance (normal hierarchy behavior). |

To physically attach a child body to a parent body (e.g., an object welded to a platform),
use Box2D joints (weld, revolute, etc.) — this is separate from the hierarchy system.

### Serialization

The `Hierarchy` component serializes the `parentId` (UUID). Children lists are rebuilt
from parent references after deserialization (`Scene::rebuildHierarchyChildren()`).
Old scenes without hierarchy data load correctly (all entities default to root).

### Editor (Owl Nest)

The Scene Hierarchy panel displays entities as a tree. Drag-and-drop reparents entities.
Right-click context menu provides: Create Root/Child Entity, Duplicate/Duplicate Subtree,
Unparent, Delete Entity Only, Delete with Children.

## Icon System

Editor icons are managed as **SVG sources** rasterized to **PNG** at build time.

### Directory Structure

SVG sources and PNG outputs share the same directory layout:

| Directory      | Size    | Content                                           |
|----------------|---------|---------------------------------------------------|
| `toolbar/`     | 64x64   | Playback buttons (play, pause, stop, step), gizmo controls (ctrl_*) |
| `browser/`     | 512x512 | Content browser file type icons (folder, glsl, png, ...) |
| `visibility/`  | 32x32   | Eye/camera visibility toggles                     |
| `triggers/`    | 32x32   | Trigger type overlay icons (victory, death, ...)   |
| `components/`  | 32x32   | Component display icons (transform, camera, ...)   |
| `panels/`      | 32x32   | Panel icons (scene_hierarchy, properties, ...)     |
| `actions/`     | 32x32   | Menu/action icons (save, delete, duplicate, ...)   |
| `templates/`   | —       | SVG base templates (not rasterized)                |

- **SVG sources**: `source/owlnest/assets_sources/icons/<category>/`
- **PNG outputs**: `source/owlnest/assets/icons/<category>/`

### Rasterization

```bash
poetry run python source/owlnest/assets/icons/generate_icons.py
```

Uses `cairosvg` to rasterize SVGs at the appropriate size tier.

### Runtime Atlas

At startup, `IconBank::build()` packs all PNG icons into a single GPU texture atlas
(64px cell size, with mipmaps). Icons are looked up by name via `IconBank::getIcon()`.

### Future: Runtime SVG Rendering

A planned improvement is to load SVGs directly at runtime using a C++ library (e.g., lunasvg),
enabling dynamic color theming where icon colors follow the editor theme.

## Task System

The engine includes a task scheduler backed by [Taskflow](https://github.com/taskflow/taskflow) 4.0:

- **Public API** (`core/task/`): `Task`, `Scheduler`, `Timer`
- **Private implementation**: `SchedulerImpl` owns a `tf::Executor` (thread pool sized to `hardware_concurrency`)
- **Parallel utilities**: `parallelForEach` / `parallelForIndex` templates
- Taskflow is a PRIVATE dependency — not exposed in public headers

## Dependency Management

Dependencies are managed by [DepManager](https://github.com/Silmaen/DepManager) and declared
in `depmanager.yml` at the project root. During CMake configure, the `cmake/Depmanager.cmake`
module automatically downloads missing packages from the configured remote server.

See @ref building for instructions on configuring and building with dependencies.
