# Contributing to Owl {#page-guide-contributor}

[TOC]

The pages for people who build Owl, change it or follow where it goes. The rules are in
[Contributing](contributing.md); the plans live in the design pages.

## Working on the code

| Page                                                | What you learn                                                          |
|-----------------------------------------------------|-------------------------------------------------------------------------|
| [Building from Source](building.md)                 | Build image, presets, options, tests, packaging, Wayland and X11        |
| [Contributing](contributing.md)                     | Conventions, tests, dependencies, logging, checks before a pull request |
| [Continuous Integration](continuous_integration.md) | The TeamCity pipeline, the build matrix, the Python CI actions          |
| [Profiling](profiling.md)                           | Profiler scopes, Tracy, benchmarks and budgets                          |
| [Architecture Overview](architecture.md)            | Modules, backends and the project system                                |

## Where the project goes

| Page                      | What you learn                                   |
|---------------------------|--------------------------------------------------|
| [Roadmap](roadmap.md)     | Planned and released versions, one line per item |
| [Changelog](changelog.md) | What changed in each version                     |

Each roadmap item with a detailed plan links to a design page; the root `ROADMAP.md` and `CHANGELOG.md` are the
one-glance versions.

## Design pages

| Page                                                         | Subject                                                    |
|--------------------------------------------------------------|------------------------------------------------------------|
| [Foundations](design/foundations.md)                         | The v0.3.0 foundations: the audit fixes, tests and tooling |
| [Conan migration](design/conan-migration.md)                 | Dependencies through Conan 2                               |
| [CI streamlining](design/ci-streamlining.md)                 | Faster CI                                                  |
| [Ongoing quality](design/ongoing-quality.md)                 | Cross-cutting quality commitments                          |
| [Stable release](design/stable-release.md)                   | Criteria of v1.0.0                                         |
| [Owl RHI](design/owl-rhi.md)                                 | The render hardware interface                              |
| [2D complete](design/2d-complete.md)                         | The 2D renderer work                                       |
| [3D core](design/3d-core.md)                                 | The 3D core                                                |
| [Isometric renderer](design/isometric.md)                    | The isometric renderer                                     |
| [Isometric worlds](design/isometric-worlds.md)               | The v0.4.0 isometric release plan                          |
| [Physics API](design/physics-api.md)                         | The physics work                                           |
| [Windowing and input](design/windowing-input.md)             | Wayland, SDL3 evaluation, input actions                    |
| [Large worlds](design/large-worlds.md)                       | Games spanning any scale                                   |
| [Gameplay systems](design/gameplay-systems.md)               | The gameplay systems                                       |
| [Simulation and networking](design/simulation-networking.md) | Deterministic simulation and networking                    |
| [Modding and platforms](design/modding-platforms.md)         | Modding and new platforms                                  |
| [Content pipeline](design/content-pipeline.md)               | Assets, animation, effects                                 |
| [PCG graphs](design/pcg-graphs.md)                           | Procedural generation graphs                               |
| [HUD system](design/hud-system.md)                           | The HUD and its text                                       |
| [Game export](design/game-export.md)                         | Exporting a game from Owl Nest                             |
| [Sample games](design/sample-games.md)                       | The sample games                                           |
| [Owl Nest UI](design/nest-ui.md)                             | The editor user interface                                  |
| [Owl Nest style guide](design/nest-style-guide.md)           | Rules every editor panel follows                           |
| [Editor reach](design/editor-reach.md)                       | Everything a game uses is authorable in the editor         |
| [Editor console and scripting](design/editor-scripting.md)   | Scripting Owl Nest itself                                  |
| [MCP server](design/mcp-server.md)                           | The editor command API and MCP                             |
| [Visual scripting](design/visual-scripting.md)               | Blueprints-style visual scripting                          |

The guides for the other audiences: [game makers](guide_game_maker.md), [C++ engine developers](guide_engine_developer.md).

<!-- subpage: contributing.md -->
<!-- subpage: building.md -->
<!-- subpage: continuous_integration.md -->
<!-- subpage: roadmap.md -->
<!-- subpage: changelog.md -->
<!-- subpage: design/2d-complete.md -->
<!-- subpage: design/3d-core.md -->
<!-- subpage: design/ci-streamlining.md -->
<!-- subpage: design/conan-migration.md -->
<!-- subpage: design/content-pipeline.md -->
<!-- subpage: design/editor-reach.md -->
<!-- subpage: design/editor-scripting.md -->
<!-- subpage: design/foundations.md -->
<!-- subpage: design/game-export.md -->
<!-- subpage: design/gameplay-systems.md -->
<!-- subpage: design/hud-system.md -->
<!-- subpage: design/isometric-worlds.md -->
<!-- subpage: design/isometric.md -->
<!-- subpage: design/large-worlds.md -->
<!-- subpage: design/mcp-server.md -->
<!-- subpage: design/modding-platforms.md -->
<!-- subpage: design/nest-style-guide.md -->
<!-- subpage: design/nest-ui.md -->
<!-- subpage: design/ongoing-quality.md -->
<!-- subpage: design/owl-rhi.md -->
<!-- subpage: design/pcg-graphs.md -->
<!-- subpage: design/physics-api.md -->
<!-- subpage: design/sample-games.md -->
<!-- subpage: design/simulation-networking.md -->
<!-- subpage: design/stable-release.md -->
<!-- subpage: design/visual-scripting.md -->
<!-- subpage: design/windowing-input.md -->
