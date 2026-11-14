# Modding and platforms {#page-design-modding-platforms}

[TOC]

Design of the planned modding support, the new platforms and the plugins. The networking is in
[Simulation and networking](simulation-networking.md), the export of games in [Game export](game-export.md).

## API freeze

- Every public header audited; what will not be supported in 1.x removed or deprecated
- ABI check in CI against the release candidate

## Modding

- Mod loading: mod packs (`.owlmod`) discovered and loaded at runtime on the `.owlpack` reader; a manifest with name,
  version, dependencies and load order; game assets (textures, scripts, scenes) overridden or extended
- Lua mod API on Luau ([Visual scripting](visual-scripting.md#visual-scripting-luau)): custom components registered
  from Lua on the open component registry, hooks into game events (`on_scene_load`, `on_entity_spawn`...), sandboxed
  execution on the Lua quotas (no filesystem access by default)
- In-game mod manager: enable, disable and reorder mods, conflict detection

## Platforms

### Android {#platforms-android}

- Vulkan renderer, OpenAL Soft sound
- Conan NDK cross build
- An Android window and input backend next to GLFW and Null (GLFW does not support Android; it stays on desktop)
- Touch input as prerequisite
- APK packaging and signing, see [Game export](game-export.md)

### Web {#platforms-web}

- WebGPU backend for the [Owl RHI](owl-rhi.md), shaders in WGSL
- Emscripten build target, HTML5 template with loading screen
- Async asset loading adapted for the web
- A local HTTP server for "run in browser"
- Networking over WebSocket to a native server ([networking](simulation-networking.md#networking-multiplayer))

### Other

- Cross-platform export and target selector, see [Game export](game-export.md)
- Gamepad improvements, see [Windowing and input](windowing-input.md)
- macOS (Metal backend), to be evaluated after 1.0

## Windows SDK compatible with MSVC {#platforms-msvc}

Besides MinGW (kept), a Windows build with the MSVC ABI so the SDK links from Visual Studio.

- First path to evaluate: clang-cl with the Windows SDK and CRT through xwin inside the Docker image, compiled on the
  Linux runners, tested on Windows
- Real `cl.exe` only if a user needs it

## Plugins {#modding-plugins}

Plugins extend the engine and Owl Nest; teams of developers and designers share them. Mods and plugins are one
mechanism: an extension pack (manifest, version, dependencies, load order) that a game loads as a mod, a project or
Owl Nest as a plugin. Data, script and Owl Nest plugins are planned; native plugins are to be evaluated after 1.0.

| Kind     | What it adds                                                                     | Targets                          |
|----------|----------------------------------------------------------------------------------|----------------------------------|
| Data     | Material nodes (Slang functions and their pins), script nodes, templates, assets | Every target                     |
| Script   | Game rules, domain functions, components, in the scripting language              | Every target                     |
| Native   | Engine functions, render layers, renderers, backends, in C++                     | The binaries its authors provide |
| Owl Nest | Panels, `CommandRegistry` commands, inspector drawers, document types, importers | The editor hosts                 |

- A native plugin ships built for each platform by its authors, never built from its sources by Owl; a target without
  its binary runs without it
- MinGW, MSVC and Clang builds share no C++ ABI: native plugins talk to the engine over a versioned C interface, a thin
  C++ header on top of it
- One node registry: every plugin adds its nodes, each graph kind (visual scripts, materials, PCG, behaviour trees)
  reads its own
- Until then, new extension points are registries filled at runtime, not lists fixed at compile time

WASM game modules and compiled mods (C++, Rust or Zig built once, run by the runner on every target) are to be
evaluated after 1.0 too.
