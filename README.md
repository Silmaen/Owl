# Owl Engine

<img src="engine_assets/logo/logo_owl.png" alt="Owl Engine logo" width="160">

**Owl Engine is the C++ engine that mixes rendering styles in one game: 2D, raycast, voxel, isometric and 3D,
stacked under one HUD, in the editor and in game.**

![Owl Nest, the Owl Engine editor, with the Engine Demo open](doc/images/nest/main_window.png)

- **Render-style mixing**: 2D batches, tilemaps, a raycaster, voxel terrain, isometric maps and 3D meshes are
  renderer-stack layers that compose with each other and the HUD, in the editor viewport and in Play.
- **Owl Nest editor**: scenes, tilemaps, node graphs, animations, HUDs and prefabs with undo/redo and hot reload,
  driven by an [MCP server](doc/pages/mcp.md) as well as by hand.
- **Lua gameplay, C++ engine**: scripts on entities, Box2D physics, OpenAL sound, OpenGL 4.5 or Vulkan 1.3+ rendering,
  on Linux x64/arm64 and Windows x64.

## Where to start

| You want to                     | Read                                                             |
|---------------------------------|------------------------------------------------------------------|
| Run the editor and open a game  | [Getting started](doc/pages/getting_started.md)                  |
| Make a game in Owl Nest         | [Owl Nest help](doc/pages/nest_help.md)                          |
| Script gameplay                 | [Scripting games in Lua](doc/pages/scripting.md)                 |
| Understand or extend the engine | [Engine for C++ developers](doc/pages/guide_engine_developer.md) |
| Build Owl from source           | [Building from source](doc/pages/building.md)                    |
| Contribute a change             | [Contributing](doc/pages/contributing.md)                        |

Every page, grouped by audience: [game makers](doc/pages/guide_game_maker.md),
[C++ engine developers](doc/pages/guide_engine_developer.md) and [contributors](doc/pages/guide_contributor.md).
What is planned and what shipped: [Roadmap](doc/pages/roadmap.md) and [Changelog](doc/pages/changelog.md)
(one-glance versions: `ROADMAP.md`, `CHANGELOG.md`).

Documentation and downloads: [owl.argawaen.net](https://owl.argawaen.net). Owl Engine is released under the
[MIT License](LICENSE); the name is a nod to the maintainer's surname, Lachouette.
