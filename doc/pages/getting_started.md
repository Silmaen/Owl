# Getting Started {#page-getting_started}

[TOC]

Welcome to **Owl**, a C++23 game engine with a scene editor (Owl Nest), Lua scripting,
2D rendering, physics, audio, packaging, and more.

This page is your entry point. It points to the in-depth guides bundled with the editor
and accessible from the **Help** ribbon button or by pressing `F1`.

## First Run

1. Launch **Owl Nest**.
2. From the welcome screen, click **New Project** to create an empty project, or
   **Open Project** to point at an existing folder containing an `owl_project.yml`.
3. The editor opens with the scene viewport, content browser, hierarchy and inspector
   docked around a central area. All panels are detachable — drag the title bar to
   tear them out into a floating window.

## Where to Go Next

| Topic                             | Page                               |
|-----------------------------------|------------------------------------|
| Editor panels, ribbon, shortcuts  | [editor.md](editor.md)             |
| Scene system and components       | [scene.md](scene.md)               |
| Lua scripting (gameplay)          | [scripting.md](scripting.md)       |
| 2D renderer, sprites, fonts       | [renderer.md](renderer.md)         |
| Voxel engine (blocks, chunks)     | [voxel.md](voxel.md)               |
| Physics (Box2D bodies, triggers)  | [physics.md](physics.md)           |
| Sound (effects, music, listeners) | [sound.md](sound.md)               |
| Node graph editor                 | [node_graph.md](node_graph.md)     |
| Building from source              | [building.md](building.md)         |
| Engine architecture               | [architecture.md](architecture.md) |
| How to contribute                 | [contributing.md](contributing.md) |

## Useful Shortcuts

| Shortcut            | Action                     |
|---------------------|----------------------------|
| `F1`                | Open contextual help       |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo                |
| `Ctrl+S`            | Save active document       |
| `Ctrl+W`            | Close active document      |
| `Ctrl+Tab`          | Cycle to the next document |
| `F5` / `F6` / `F7`  | Play / Pause / Stop        |

## Sample Project

A complete demonstrator covering all major engine features lives in
`sample_project/`. Open it from **File → Open Project** and play any scene to see
how Lua scripts, UI, sound and triggers fit together. Treat the sample as both a
showcase and a reference for your own projects.
