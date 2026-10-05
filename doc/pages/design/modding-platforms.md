# Modding and platforms {#page-design-modding-platforms}

[TOC]

Design page for the v0.10.0 release (opening, platforms, release candidate), summarised in the
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

- Level streaming
    - Load/unload scene chunks on demand based on player position
    - Background loading via task system
    - Seamless transitions (no loading screen)

## Platforms

- Web export — the **third platform** required for 1.0
    - WebGPU backend for the [Owl RHI](owl-rhi.md) (Dawn or wgpu through `webgpu.h`)
    - Emscripten build target
    - HTML5 template with loading screen
    - Async asset loading adapted for web
- Cross-platform packaging and target selector, see [Game export](game-export.md)
- Gamepad improvements, see [Windowing and input](windowing-input.md)

## After 1.0 (To evaluate)

- ![To evaluate][evaluate] macOS (Metal backend) — the maintainer has no Mac
- ![To evaluate][evaluate] Android support
    - Touch input backend
    - Vulkan mobile renderer (OpenGL ES not planned: OpenGL is frozen)
    - APK packaging from Owl Nest

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
