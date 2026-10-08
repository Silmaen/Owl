# Roadmap {#page-roadmap}

[TOC]

**Owl is the engine that mixes rendering styles** — 2D, raycast, voxel, isometric and 3D in one game, through the
renderer stack. Each version below has a short goal and one line per item; the detail lives in the design pages under
`doc/pages/design/`. A one-glance summary is in the root `ROADMAP.md`; shipped changes are in the
[Changelog](changelog.md).

Development is heavily AI-assisted (the AI writes the code, the maintainer tests and steers), hence the cadence below.
**Exit criteria take precedence over dates**: a version ships when its criteria hold.

**Ongoing across all releases** ([details](design/ongoing-quality.md)):

- ![Ongoing][ongoing] Render-style mixing — every new mode is a renderer-stack layer that composes with the others
- ![Ongoing][ongoing] Code quality — clang-tidy / clang-format clean, conventions of `.claude/rules/cpp-style.md`
- ![Ongoing][ongoing] Test coverage — no public API without tests, every bug fix with its regression test
- ![Ongoing][ongoing] Performance — measure first (Tracy, frame bench, `bench/`), no per-frame allocations
- ![Ongoing][ongoing] Performance budgets enforced in CI — benchmark regression threshold fails the build
- ![Ongoing][ongoing] Documentation quality — public API documented, `doc/pages` updated in the same PR
- ![Ongoing][ongoing] Editor coverage for every authored object — selection, inspection, editing, bulk ops, discovery

## v1.0.0 -- Expected 2028-04-01

**Goal:** A stable engine a third party can rely on. No new feature: only the criteria below
([details](design/stable-release.md)).

- ![Planned][planned] Public API frozen, semantic versioning and deprecation policy
- ![Planned][planned] Versioned scene and save formats with tested migrations
- ![Planned][planned] Consumable SDK: OwlEngine Conan package, `find_package(OwlEngine)` with 1–2 public dependencies, consumer test in CI
- ![Planned][planned] Two showcase games (2D and 3D) authored only in the editor, each mixing rendering styles
- ![Planned][planned] Linux and Windows flawless, the Web as third platform
- ![Planned][planned] Zero known crash; fuzzers on every file loader
- ![Planned][planned] Performance budgets verified in CI on every target platform
- ![Planned][planned] Complete user documentation: tutorials, Lua and visual-scripting references

## v0.10.0 -- Expected 2028-02-15

**Goal:** Open Owl to modders and to the browser, then freeze the API: this release is the 1.0 release candidate
([details](design/modding-platforms.md)).

- ![Planned][planned] Public API freeze, ABI check in CI — release candidate
- ![Planned][planned] Mod loading system (`.owlmod` packs, manifest, load order, asset overrides)
- ![Planned][planned] Lua mod API (custom components, game event hooks, sandboxed)
- ![Planned][planned] In-game mod manager (enable / disable / reorder, conflict detection)
- ![Planned][planned] Level streaming (chunks loaded around the player in the background)
- ![Planned][planned] Web export through a WebGPU Owl RHI backend ([Owl RHI](design/owl-rhi.md))
- ![Planned][planned] Cross-compile packaging and target platform selector ([Game export](design/game-export.md))
- ![Planned][planned] Gamepad improvements: remapping UI, haptics, dead zones ([Input](design/windowing-input.md))
- ![To evaluate][evaluate] macOS (Metal) and Android — after 1.0

## v0.9.0 -- Expected 2027-12-15

**Goal:** A deterministic simulation with input replay and rewind, and networked multiplayer built on it
([details](design/simulation-networking.md)).

- ![Planned][planned] Deterministic simulation (fixed step, seeded streams, cross-platform world-hash test)
- ![Planned][planned] Input recording and frame-exact replay (replays double as regression tests)
- ![Planned][planned] Rewind in Play (snapshot ring buffer + input log)
- ![Planned][planned] Network transport layer (reliable UDP, authoritative server, connection management)
- ![Planned][planned] Entity replication (replicated components, interpolation, prediction, authority)
- ![Planned][planned] RPC system (Lua `rpc_server` / `rpc_client` / `rpc_all`, reliable and unreliable)
- ![Planned][planned] Lobby and session management
- ![Planned][planned] Network debugging tools (latency / loss simulation, stats overlay)

## v0.8.0 -- Expected 2027-10-15

**Goal:** The systems that make games — AI, 3D physics, audio, narrative — as optional modules outside `Scene`
([details](design/gameplay-systems.md)).

- ![Planned][planned] Pathfinding (NavMesh generation, A*, dynamic avoidance)
- ![Planned][planned] Behaviour trees with a visual editor (`.owlbt`)
- ![Planned][planned] Steering behaviours and flocking
- ![Planned][planned] 3D physics with Jolt on the shared physics interface ([Physics API](design/physics-api.md))
- ![Planned][planned] Audio mixer (buses, per-bus volume, crossfades)
- ![Planned][planned] Audio effects (reverb zones, low / high-pass filters)
- ![Planned][planned] Dialogue system with a visual editor (`.owldlg`)

## v0.7.0 -- Expected 2027-08-15

**Goal:** Produce content at scale and script games without C++: offline asset pipeline, animation and effects,
procedural graphs, and Blueprints-style visual scripting compiled to Lua.

- ![Planned][planned] Offline asset pipeline: cook, cache, asset GUIDs ([Content](design/content-pipeline.md))
- ![Planned][planned] Binary runtime scene format produced by the cook
- ![Planned][planned] Parallel pack `readEntry`
- ![Planned][planned] Skeletal animation (skinning, blending, state machine) and mesh LODs
- ![Planned][planned] GPU particle system with editor preview
- ![Planned][planned] Advanced post-processing (chromatic aberration, film grain, motion blur, depth of field)
- ![Planned][planned] Visual scripting MVP, event graphs compiled to Lua ([details](design/visual-scripting.md))
- ![Planned][planned] Node-graph link waypoints
- ![Planned][planned] Script debugging aids (breakpoint markers, live watch)
- ![Planned][planned] Procedural generation graphs on `NodeCanvas` ([PCG graphs](design/pcg-graphs.md))
- ![To evaluate][evaluate] Hot reload of a C++ game module

## v0.6.0 -- Expected 2027-06-15

**Goal:** A focused 3D core on the repaired Owl RHI — static meshes, PBR, shadows, render graph, essential
post-processing — with full 3D editing ([details](design/3d-core.md)).

- ![Planned][planned] 3D render pipeline (forward, depth, perspective camera)
- ![Planned][planned] Render graph hosting the renderer-stack layers
- ![Planned][planned] Lighting: directional, point and spot lights, directional shadow maps
- ![Planned][planned] PBR materials with editor and material library
- ![Planned][planned] Static mesh component with instancing
- ![Planned][planned] Essential post-processing stack (tone mapping, bloom, vignette, LUT)
- ![Planned][planned] Compute-driven frustum / occlusion culling with indirect draws
- ![Planned][planned] GPU raycast sprite stripes and `BitonicSortPass` adoption
- ![Planned][planned] Spatial partitioning and multithreaded render preparation
- ![Planned][planned] 3D scene editing in Owl Nest (3D gizmos, snapping, mesh preview)

## v0.5.0 -- Expected 2027-04-15

**Goal:** Complete the 2D experience — lighting, HUD, text, full 2D physics ([details](design/2d-complete.md)).

- ![Planned][planned] 2D lighting (point / spot lights, normal-mapped sprites, 2D shadows)
- ![Planned][planned] Full 2D physics: callbacks, queries, joints, 3D-ready API ([Physics](design/physics-api.md))
- ![Planned][planned] Aspect-correct text with alignment and fit modes
- ![Planned][planned] Dedicated HUD layer and HUD editor mode
- ![Planned][planned] Tileset editor: compose the atlas from source images
- ![Planned][planned] First procedural brick for tilemaps ([PCG graphs](design/pcg-graphs.md))
- ![Planned][planned] "Look through scene camera" mode
- ![Planned][planned] Custom ImGui-based file picker
- ![Planned][planned] Help panel: mermaid diagrams and rendering polish
- ![Planned][planned] Sample project: inventory and enemies demos (moved out of the engine)

## v0.4.0 -- Expected 2027-02-28

**Goal:** A third non-2D mode — a Transport Tycoon-style isometric renderer — plus a local MCP server in Owl Nest and
basic gamepad support.

- ![Planned][planned] `RendererIsometric` layer, heightmap tiles, editor support ([details](design/isometric.md))
- ![Planned][planned] MCP server built into Owl Nest, on the editor command API ([MCP server](design/mcp-server.md))
- ![Planned][planned] Input actions and basic gamepad support ([Input](design/windowing-input.md))

## v0.3.0 -- Expected 2027-01-15

**Goal:** Foundations — phase 0 first and closed before the rest starts (Conan as the only provider, breaking
dependency upgrades), then every known bug fixed, safety nets, Owl RHI repaired, architecture opened, iteration sped
up, Owl Nest made ergonomic. No new gameplay feature ([details](design/foundations.md)). Order, one PR each: the
rest of A and B, then D, then C, then E.

- ![Done][done] teamcity-github-bridge 1.10.0 wiring (diff annotations, doc-only PRs skip the C++ matrix)
- ![Done][done] Phase 0 — Conan 2 on ConanCenter, missing recipes first, every preset green in CI ([Conan](design/conan-migration.md))
- ![Done][done] Phase 0 — breaking upgrades first: EnTT 4, Taskflow 4.1, lagging dependencies
- ![Done][done] Phase 0 — DepManager and `owl_engine.py` removed: Conan the only provider, consumers on the packaged archive
- ![Done][done] Phase A — every confirmed correctness bug fixed with its regression test (PR-01 to PR-25; B-19 in phase C)
- ![Done][done] Phase A — `on_collision` implemented from Box2D contact events (D-07)
- ![Done][done] Phase A — game export tested end to end, sample run headless ([Export](design/game-export.md))
- ![Done][done] Phase A — full Wayland: icon, editor multi-window, X11 kept ([details](design/windowing-input.md))
- ![Done][done] Phase A — OpenGL compatibility backend fixed and tested, mipmaps and Nearest filtering on both backends
- ![Done][done] Phase B — engine benchmark harness `bench/` (`OWL_BENCHMARK`), runner cold start measured
- ![Done][done] Phase B — diff-scoped clang-tidy CI action, parallel by default (H-03)
- ![Done][done] Phase B — blocking sanitizers, shuffled test order, LSan job removed (PR-11)
- ![Done][done] Phase B — CI in two parallel levels, fast subset for `Experiment/*` PRs, `PR Ready` merge gate
- ![Planned][planned] Phase B — package once per platform from the tested release tree, no rebuild ([details](design/foundations.md))
- ![Done][done] Phase B — Tracy behind `OWL_PROFILE_*`, memory tracker opt-in, cheaper logs (PR-16, [Profiling](profiling.md))
- ![Done][done] Phase B — runner frame bench: `OwlRunner --frame-bench`, GPU timestamps, Vulkan/OpenGL baseline (PR-17)
- ![Done][done] Phase B — editor tests: `owlnest_tests` (undo, every command family, snapshots)
- ![Done][done] Phase B — image tests on lavapipe / llvmpipe, validation clean, OpenGL GLSL fallback (PR-18)
- ![Done][done] Phase B — tests without a window: scripted headless runner (`OwlRunner --scenario`)
- ![Done][done] Phase B — benchmarks in CI: compiled on every PR, nightly run against a baseline (+15 % fails)
- ![Planned][planned] Phase B — module dependency check, CI tooling tests
- ![Planned][planned] Phase D — public dependencies reduced to EnTT (+ `Owl::Gui`)
- ![Planned][planned] Phase D — hot reload of assets, Slang shaders and Lua scripts
- ![Planned][planned] Phase D — autosave, crash recovery, session restore, error messages, project templates
- ![Planned][planned] Phase D — documentation faithful to the code and checked in CI
- ![Planned][planned] Phase C — Vulkan foundation first, as soon as the image tests land ([RHI](design/owl-rhi.md))
- ![Planned][planned] Phase C — Owl RHI named, OpenGL frozen as fallback, backend interface ready for more
- ![Planned][planned] Phase C — ABI cleanup, world per scene, phased systems, open component registry
- ![Planned][planned] Phase C — optional CMake modules so each game can specialise the engine
- ![Planned][planned] Phase C — typed Lua binding registry and editor command API
- ![To evaluate][evaluate] Phase C — SDL3 for windowing, input, dialogues, audio (Wayland is the argument)
- ![Planned][planned] Phase E — Owl Nest ergonomics revamp, designed together, no new feature ([Nest UI](design/nest-ui.md))
- ![Planned][planned] Phase E — interaction basics: tooltips, context menus, drag & drop, text scale / DPI
- ![Planned][planned] Phase E — visual overhaul: theme, icon set, thumbnails, style guide
- ![In Progress][progress] Phase E — editor camera controls overhaul (presets, view-cube, sensitivity)
- ![Planned][planned] Performance targets met: frame, Renderer2D, Vulkan drains, scene load, Box2D, voxel, start-up

## v0.2.1 -- 2026-06-27

**Goal:** Add the second non-2D rendering mode — a voxel engine for block-based worlds (Minecraft-style).

- ![Done][done] Voxel world core — `BlockRegistry`, RLE `Chunk`, sparse `VoxelWorld`, async chunk streaming
- ![Done][done] Greedy chunk meshing with face culling, per-vertex AO and per-chunk CPU frustum culling
- ![Done][done] Seed-reproducible terrain generator (height layers, shoreline, caves, biomes)
- ![Done][done] `VoxelPlayer` first-person controller with voxel collision, mouse-look and fly mode
- ![Done][done] Block break / place via `raycastVoxel`, per-block orientation and state metadata
- ![Done][done] `Renderer3D` + `RendererVoxel` on Vulkan and OpenGL, lighting and water pass, editor and Play
- ![Done][done] Editor Voxel Palette brush and `.owlvoxstruct` structure capture / stamp
- ![Done][done] Vulkan / OpenGL hardening — zero Vulkan validation messages in Owl Nest

## v0.2.0 -- 2026-06-02

**Goal:** A composable renderer stack mixing rendering modes, the raycasting renderer, tilemaps, scene transitions,
and a GPU-driven renderer foundation.

- ![Done][done] Renderer stack — `RenderLayer` / `RenderStack` / `RendererTag`, YAML round-trip, per-scene overrides
- ![Done][done] Renderer stack editor UI — project stack composition, per-scene `Scene Settings` panel
- ![Done][done] Tilemap system — `.owltileset` / `.owltilemap`, `TilemapDocument` editor, static Box2D body
- ![Done][done] Raycasting renderer — per-column DDA, textured floors / ceilings, billboard sprites, fog
- ![Done][done] Raycast map features — variable wall heights, transparent walls, doors and pushwalls
- ![Done][done] Raycast map editor — top-down edit view, first-person Play, camera viewport marker
- ![Done][done] Scene transition effects — fades and wipes, custom tint, Lua `ui.transition_play`
- ![Done][done] Scene loading ~550× faster, plus async load and in-game hot-path caches
- ![Done][done] GPU-driven Renderer2D — compute foundation, instanced SSBO batches, `WorldTransformPass`
- ![Done][done] GPU raycast DDA pass, `BitonicSortPass` utility, single-drawcall tilemap rendering
- ![Done][done] Bug fixes — editor modifier shortcuts, top-down player drift, hidden triggers no longer fire

## v0.1.1 -- 2026-04-30

**Goal:** A multi-document editor workspace with dedicated asset editors; long-running operations become
asynchronous with progress feedback.

- ![Done][done] Async operations — packaging, scene save / load, shader compilation, textures, transitions, browser
- ![Done][done] Multi-document tabs with per-document undo, viewports and Play state; detachable panels
- ![Done][done] Code editor document with syntax highlighting and live Markdown / SVG preview
- ![Done][done] Node graph framework (`NodeCanvas`) and the Scene Flow view
- ![Done][done] Animation editor (`.owlanim`) with sequencer timeline
- ![Done][done] Enhanced inspector — sound preview, texture / font thumbnails, drag-drop, curve editor
- ![Done][done] Packaging wizard with validation and post-pack report
- ![Done][done] Ribbon main menu, recent projects, Save Project As
- ![Done][done] In-editor help pages, tooltips everywhere, unique ImGui IDs, icon clarity pass
- ![Done][done] Linux ARM64 CI restored, Windows Debug builds fixed

## v0.1.0 -- 2026-04-16

**Goal:** Design a complete game in Owl Nest and package it as a standalone application (Linux / Windows).

- ![Done][done] Lua 5.5 scripting — `LuaScript` component, lifecycle callbacks, engine API, runner support
- ![Done][done] In-game UI — `Canvas`, base widgets, input handling, UI editor, standard game screens
- ![Done][done] Game state and save system — `GameState`, multi-slot `SaveManager`, full scene state save
- ![Done][done] Runner and distribution — project config, improved runner, persistent settings, packaging
- ![Done][done] Prefab system and undo / redo in Owl Nest
- ![Done][done] Timer, Interaction and LuaCallback triggers with enter / exit edges; shared game settings
- ![Done][done] Complete sample game — six scenes exercising every Lua API, widget and trigger

## v0.0.3 -- 2026-04-09

- ![Done][done] Sound — one-shot and looping sources, categories, full playback control, 3D spatialization
- ![Done][done] Animated sprites with spritesheet grid
- ![Done][done] Mesh loading — OBJ, glTF, GLB, FBX
- ![Done][done] Configurable keymap
- ![Done][done] Scene hierarchy, `.owlpack` asset packing, Taskflow task system
- ![Done][done] Owl Nest project system, settings panel, hierarchy panel, runtime SVG icons, level links

## v0.0.2 -- 2026-03-03

- ![Done][done] Third-party-free public headers, more static linking
- ![Done][done] Backgrounds / sky boxes and shaders migrated to Slang
- ![Done][done] Pause, frame stepping, general settings, scene jumps
- ![Done][done] Owl Map: global game settings, log frame, visibility toggles

## v0.0.1 -- 2025-02-06

First basic release: minimal viable engine with the ability to run simple games defined in scenes.

## Badge Legend

- ![Done][done] Completed features
- ![In Progress][progress] Features currently being implemented
- ![Planned][planned] Features that are planned but not yet being worked on
- ![To evaluate][evaluate] Options to study before committing to them; they may be dropped
- ![Ongoing][ongoing] Permanent commitments maintained across every release

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square

[progress]: https://img.shields.io/badge/-In_Progress-d29922?style=flat-square

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square

[ongoing]: https://img.shields.io/badge/-Ongoing-6e7781?style=flat-square
