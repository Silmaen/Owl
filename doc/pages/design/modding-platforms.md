# Modding and platforms {#page-design-modding-platforms}

[TOC]

Design page for the modding and platform part of the v0.8.0 release (opening, release candidate; its networking is
in [Simulation and networking](simulation-networking.md)), summarised in the
[Roadmap](../roadmap.md).

## API freeze — release candidate for 1.0

- Audit every public header, remove or deprecate what will not be supported in 1.x
- ABI check in CI against the release candidate

## Modding

- Mod loading system
    - Discover and load mod packs (`.owlmod`) at runtime
    - Mod manifest: name, version, dependencies, load order
    - Override or extend game assets (textures, scripts, scenes)
    - Builds on the hardened `.owlpack` reader (PR-07) and the Lua quotas (PR-14)
- Lua mod API
    - Register custom components from Lua (on top of the open component registry, PR-37)
    - Hook into game events (on_scene_load, on_entity_spawn, etc.)
    - Sandboxed execution (no filesystem access by default)
- In-game mod manager
    - UI to enable/disable/reorder mods
    - Mod conflict detection

## Large worlds

- World partition ([Large worlds](large-worlds.md))
    - Streaming cells on world coordinates (double), loaded and unloaded around the player
    - Additive scenes: a cell is a scene merged into the running one
    - Background loading via task system
    - Seamless transitions (no loading screen)
- Camera tracks in the sequencer, for cutscenes

## Platforms

Android and Web are both in 1.0 (v0.8.0).

### Android {#platforms-android}

- Vulkan renderer, OpenAL Soft sound
- Conan NDK cross build
- A new Android window and input backend next to GLFW and Null (GLFW does not support Android; it stays on desktop)
- Touch input as prerequisite
- APK packaging and signing, see [Game export](game-export.md)

### Web {#platforms-web}

- WebGPU backend for the [Owl RHI](owl-rhi.md), shaders in WGSL
- Emscripten build target, HTML5 template with loading screen
- Async asset loading adapted for the web
- A local HTTP server for "run in browser"
- No networking on the Web in 1.0 ([networking](simulation-networking.md))

### Other

- Cross-platform export and target selector, see [Game export](game-export.md)
- Gamepad improvements, see [Windowing and input](windowing-input.md)

## Windows SDK compatible with MSVC {#platforms-msvc}

v0.9.0: besides MinGW (kept), a Windows build with the MSVC ABI so the SDK links from Visual Studio.

- First path to evaluate: clang-cl with the Windows SDK and CRT through xwin inside the Docker image; compiled on the
  Linux agents, `hecate` runs the tests
- Real `cl.exe` only if a user needs it

## After 1.0 (To evaluate)

- ![To evaluate][evaluate] macOS (Metal backend) — the maintainer has no Mac
- ![To evaluate][evaluate] WASM game modules and compiled mods
- ![To evaluate][evaluate] AAB for Google Play
- ![To evaluate][evaluate] AppImage and Flatpak

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
