# Modding and platforms {#page-design-modding-platforms}

[TOC]

Design of the planned modding support, the new platforms and the plugins. The networking is in
[Simulation and networking](simulation-networking.md), the export of games in [Game export](game-export.md).

## API freeze

- Every public header audited; what will not be supported in 1.x removed or deprecated
- ABI check in CI against the release candidate

## Modding

- Mod loading and the extension pack format (manifest, dependencies, load order, asset overrides): done, see
  [Mods and extension packs](../mods.md)
- Lua mod API: custom components registered from Lua on the component registry, hooks into game events
  (`on_scene_load`, `on_entity_spawn`...), sandboxed execution on the Lua quotas (no filesystem access); done, see
  [Entry script and game events](../mods.md#mods-script)
- In-game mod manager: enable, disable and reorder mods, conflict detection; done, see
  [Mod manager](../mods.md#mods-manager)

## Platforms

### Android {#platforms-android}

Built by the `android-arm64` preset, see [Building](../building.md#android-build): Vulkan on the system loader,
OpenAL Soft, the native activity window and input next to GLFW and Null, a debug-signed APK of the runner and an
exported game. Still to come:

- Window lifecycle: the game pauses when the system takes its window (background, screen off) and resumes on the new
  one; today it closes
- Touch input beyond the first finger ([Input](windowing-input.md)), gamepads
- The APK from Owl Nest itself on every host, release signing, see [Game export](game-export.md)
- The network transport and the smoke test on the tablet

### Web {#platforms-web}

Built by the `web-release` preset, see [Building](../building.md#web-build): the WebGPU backend of the
[Owl RHI](owl-rhi.md) with WGSL baked by the host, a page that loads the game files behind a loading screen, the export
and its local server in Owl Nest ([Projects](../editor_projects.md)), WebSocket to a desktop server
([Networking](../networking.md#networking-transports)). Still to come: the ImGui backend of the GUI module.

### Other

- Cross-platform export and target selector, see [Game export](game-export.md)
- Gamepad improvements, see [Windowing and input](windowing-input.md)
- macOS (Metal backend), to be evaluated after 1.0

## Windows SDK compatible with MSVC {#platforms-msvc}

Besides MinGW (kept), a Windows build with the MSVC ABI so the SDK links from Visual Studio.

- Built and tested natively on the Windows agent (`hecate`), no Docker image: clang-cl first
- Real `cl.exe` only if a user needs it

## Plugins {#modding-plugins}

Plugins extend the engine and Owl Nest; teams of developers and designers share them. Mods and plugins are one
mechanism: an extension pack (manifest, version, dependencies, load order) that a game loads as a mod, a project or
Owl Nest as a plugin ([Mods and extension packs](../mods.md)). Data and script plugins are done
([Plugins](../mods.md#mods-plugins)), Owl Nest plugins too ([Owl Nest plugins](../mods.md#mods-nest-plugins)); native
plugins are to be evaluated after 1.0.

| Kind     | What it adds                                                                     | Targets                          |
|----------|----------------------------------------------------------------------------------|----------------------------------|
| Data     | Material nodes (done), script nodes (done), templates (done), assets             | Every target                     |
| Script   | Game rules, domain functions, components, in the scripting language (done)       | Every target                     |
| Native   | Engine functions, render layers, renderers, backends, in C++                     | The binaries its authors provide |
| Owl Nest | Panels, commands, inspector drawers, document types, importers, in Lua (done)    | The editor hosts                 |

- A native plugin ships built for each platform by its authors, never built from its sources by Owl; a target without
  its binary runs without it
- MinGW, MSVC and Clang builds share no C++ ABI: native plugins talk to the engine over a versioned C interface, a thin
  C++ header on top of it
- One node registry: every plugin adds its nodes, each graph kind (visual scripts, materials, PCG, behaviour trees)
  reads its own
- Until then, new extension points are registries filled at runtime, not lists fixed at compile time: the Owl Nest
  plugins fill the `CommandRegistry` and the contributions of `session::PluginHost` (panels, drawers, document types,
  importers), which the editor asks before its built-in lists
- An Owl Nest plugin draws through widgets the editor styles (`session::PluginUi`), never through ImGui, and edits
  through registry commands only: the same script would drive another editor front end

WASM game modules and compiled mods (C++, Rust or Zig built once, run by the runner on every target) are to be
evaluated after 1.0 too.
