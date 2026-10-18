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
- ![Ongoing][ongoing] Performance — measure first (Tracy, frame bench, `test/bench/`), no per-frame allocations
- ![Ongoing][ongoing] Performance budgets enforced in CI — benchmark regression threshold fails the build
- ![Ongoing][ongoing] Documentation quality — public API documented, `doc/pages` updated in the same PR
- ![Ongoing][ongoing] Editor coverage for every authored object — selection, inspection, editing, bulk ops, discovery
- ![Ongoing][ongoing] Every edit gesture is a named editor command — recorded, replayable, reachable through MCP

## v1.0.0 -- Expected 2027-10-31

**Goal:** A stable engine a third party can rely on. No new feature: only the criteria below
([details](design/stable-release.md)).

- ![Planned][planned] Public API frozen, semantic versioning and deprecation policy
- ![Planned][planned] Versioned scene and save formats with tested migrations
- ![Planned][planned] Consumable SDK: OwlEngine Conan package, `find_package(OwlEngine)` with 1–2 public dependencies, consumer test in CI
- ![Planned][planned] Two showcase games (2D and 3D) authored only in the editor, each mixing rendering styles, picked from the sample games
- ![Planned][planned] Linux and Windows flawless, the Web as third platform
- ![Planned][planned] Zero known crash; fuzzers on every file loader
- ![Planned][planned] Performance budgets verified in CI on every target platform
- ![Planned][planned] Complete user documentation: tutorials, Lua and visual-scripting references

## v0.8.0 -- Expected 2027-09-15

**Goal:** Open Owl to other players, modders and the browser, then freeze the API: networked multiplayer on the
v0.7.0 deterministic simulation, mods, the Web; this release is the 1.0 release candidate
([networking](design/simulation-networking.md), [modding and platforms](design/modding-platforms.md)).

- ![Planned][planned] Public API freeze, ABI check in CI — release candidate
- ![Planned][planned] Network transport layer (reliable UDP, authoritative server, connection management)
- ![Planned][planned] Entity replication (replicated components, interpolation, prediction, authority)
- ![Planned][planned] RPC system (Lua `rpc_server` / `rpc_client` / `rpc_all`, reliable and unreliable)
- ![Planned][planned] Lobby and session management
- ![Planned][planned] Network debugging tools (latency / loss simulation, stats overlay)
- ![Planned][planned] Mod loading system (`.owlmod` packs, manifest, load order, asset overrides)
- ![Planned][planned] Lua mod API (custom components, game event hooks, sandboxed)
- ![Planned][planned] In-game mod manager (enable / disable / reorder, conflict detection)
- ![Planned][planned] Level streaming (chunks loaded around the player in the background)
- ![Planned][planned] Web export through a WebGPU Owl RHI backend ([Owl RHI](design/owl-rhi.md))
- ![Planned][planned] Cross-compile packaging and target platform selector ([Game export](design/game-export.md))
- ![Planned][planned] Gamepad improvements: remapping UI, haptics ([Input](design/windowing-input.md))
- ![Planned][planned] Sample game: a networked Bomberman ([Sample games](design/sample-games.md))
- ![To evaluate][evaluate] macOS (Metal) and Android — after 1.0

## v0.7.0 -- Expected 2027-06-30

**Goal:** The systems that make games — AI, 2D and 3D physics, audio, narrative — as optional modules outside
`Scene`, on a deterministic simulation with input replay and rewind
([details](design/gameplay-systems.md), [simulation](design/simulation-networking.md)).

- ![Planned][planned] Deterministic simulation (seeded streams, cross-platform world-hash test) on the v0.4.0 fixed tick
- ![Planned][planned] Input recording and frame-exact replay (replays double as regression tests)
- ![Planned][planned] Rewind in Play (snapshot ring buffer + input log)
- ![Planned][planned] Full 2D physics: callbacks, queries, joints, on the shared physics interface ([Physics](design/physics-api.md))
- ![Planned][planned] 3D physics with Jolt on the shared physics interface ([Physics API](design/physics-api.md))
- ![Planned][planned] Navigation (NavMesh generation, A*, dynamic avoidance); tile-graph pathfinding comes in v0.4.0
- ![Planned][planned] Behaviour trees with a visual editor (`.owlbt`)
- ![Planned][planned] Steering behaviours and flocking
- ![Planned][planned] Audio mixer (buses, per-bus volume, crossfades)
- ![Planned][planned] Audio effects (reverb zones, low / high-pass filters)
- ![Planned][planned] Dialogue system with a visual editor (`.owldlg`)
- ![Planned][planned] Engine Demo: enemies demo on the AI modules
- ![Planned][planned] Sample games: a one-room Zelda-like and a 3D marble or kart game ([Sample games](design/sample-games.md))

## v0.6.0 -- Expected 2027-04-15

**Goal:** Produce content at scale and script games without C++: offline asset pipeline, animation and effects,
procedural graphs, tile tools, Lua editor scripts, and Blueprints-style visual scripting compiled to Lua.

- ![Planned][planned] Offline asset pipeline: cook, cache, asset GUIDs ([Content](design/content-pipeline.md))
- ![Planned][planned] Binary runtime scene format produced by the cook
- ![Planned][planned] Parallel pack `readEntry`
- ![Planned][planned] Skeletal animation (skinning, blending, state machine) and mesh LODs
- ![Planned][planned] GPU particle system with editor preview
- ![Planned][planned] Advanced post-processing (chromatic aberration, film grain, motion blur, depth of field)
- ![Planned][planned] Visual scripting MVP, event graphs compiled to Lua, node editing of assets ([details](design/visual-scripting.md))
- ![Planned][planned] Node-graph link waypoints
- ![Planned][planned] Script debugging aids (breakpoint markers, live watch)
- ![Planned][planned] Lua editor scripts and interactive Lua console ([Editor scripting](design/editor-scripting.md))
- ![Planned][planned] Procedural generation graphs on `NodeCanvas`, first brick on tilemaps ([PCG graphs](design/pcg-graphs.md))
- ![Planned][planned] Tileset editor: compose the atlas from source images ([2D](design/2d-complete.md))
- ![Planned][planned] Sound editor in Owl Nest: preview, trim, loop points, volume and pitch, random variants of one sound
- ![To evaluate][evaluate] Hot reload of a C++ game module
- ![Planned][planned] Sample games: a Mini Minecraft, and combat (guards, weapons) in the Wolfenstein mini ([Sample games](design/sample-games.md))

## v0.5.0 -- Expected 2027-01-31

**Goal:** A focused 3D core on the repaired Owl RHI — static meshes, PBR, lights and shadows for 2D, isometric and 3D,
render graph, essential post-processing — with full 3D editing ([details](design/3d-core.md)).

- ![Planned][planned] 3D render pipeline (forward, depth, perspective camera)
- ![Planned][planned] Render graph hosting the renderer-stack layers
- ![Planned][planned] Lighting: directional, point and spot lights, directional shadow maps
- ![Planned][planned] 2D lighting (point / spot lights, normal-mapped sprites, 2D shadows), isometric sprites included ([2D](design/2d-complete.md))
- ![Planned][planned] PBR materials with editor and material library
- ![Planned][planned] Static mesh component with instancing
- ![Planned][planned] Essential post-processing stack (tone mapping, bloom, vignette, LUT)
- ![Planned][planned] Compute-driven frustum / occlusion culling with indirect draws
- ![Planned][planned] GPU raycast sprite stripes and `BitonicSortPass` adoption
- ![Planned][planned] Spatial partitioning and multithreaded render preparation
- ![Planned][planned] HUD minimap on a render target ([HUD system](design/hud-system.md))
- ![Planned][planned] 3D scene editing in Owl Nest (3D gizmos, snapping, mesh preview), "look through scene camera" mode
- ![Planned][planned] Help panel: mermaid diagrams and rendering polish
- ![Planned][planned] Sample game: a Mini Transport Tycoon ([Sample games](design/sample-games.md))

## v0.4.0 -- Expected 2026-11-01

**Goal:** An engine able to carry a Transport Tycoon-like game: an isometric renderer on large chunked tile maps with
slopes, vehicles finding their way on a tile graph, a HUD for management games and its editor, input actions with a gamepad, a fixed simulation tick, and a local
MCP server in Owl Nest ([plan](design/isometric-worlds.md)).

- ![Done][done] Renderer2D transients sized per frame (PR-30: B-09)
- ![Done][done] Persistent, chunked and culled tilemaps, binary storage up to 1024² (PR-31: B-13)
- ![Done][done] `RendererIsometric` layer: dimetric camera, depth with alpha cutout, billboards ([details](design/isometric.md))
- ![Done][done] Isometric terrain: corner heights, 19 slopes, foundations, tracks and roads, slope-aware picking
- ![Done][done] Isometric atlas generator (`tools/iso_atlas`): terrain × 19 slopes, foundations, overlays, sliced buildings, albedo + normal + depth
- ![Done][done] Isometric objects sliced per footprint tile and thousands of vehicles as matrix-free sprites, pixel snapping at integer zoom
- ![Done][done] Persistent isometric chunk buffers and the `iso_town` demo
- ![Done][done] Tile-graph pathfinding on chunked tilemaps, vehicles following roads (moved from v0.5.0) ([Pathfinding](pathfinding.md))
- ![Done][done] Guaranteed 2D draw order: a sort key instead of the primitive type and storage order (PR-30: B-10, moved from v0.5.0)
- ![Done][done] UTF-8 text, Latin charset with extra ranges per font, atlas cached on disk (PR-30: D-18)
- ![Done][done] Aspect-correct text with alignment and fit
- ![Done][done] HUD overhaul: HUD pass in pixels, `.owlhud` asset, windows, layouts, tooltips and tool cursor, HUD editor, keyboard and gamepad focus, migration ([details](design/hud-system.md))
- ![Done][done] HUD scroll views and virtualised lists (moved from v0.5.0) ([HUD system](design/hud-system.md))
- ![Done][done] Fixed simulation tick (`on_tick`) for systems and scripts ([Scene](scene.md#fixed-tick))
- ![Done][done] Input frames with edges, gamepad with dead zones and the action map and its editor ([Input](design/windowing-input.md))
- ![Done][done] MCP server built into Owl Nest, on the editor command API, with `play.run` and a client guide ([MCP server](mcp.md))
- ![Done][done] Native file dialogues through the desktop portal, no GTK ([Nest UI](design/nest-ui.md))
- ![Done][done] Editor console and command journal, macros replayable as `.owlmacro` ([Editor scripting](design/editor-scripting.md))
- ![Done][done] Engine Demo: inventory demo on the new HUD widgets, playable with a gamepad
- ![Done][done] Engine Demo overhaul: the isometric town, the new HUD everywhere, hub and platformer redrawn with CC0 art
- ![Done][done] Sample game Breakout, the first project of `samples/` (moved from v0.5.0) ([Sample games](design/sample-games.md))
- ![Planned][planned] Sample game Sokoban: level editing in Owl Nest, undo and saves (moved from v0.5.0) ([Sample games](design/sample-games.md))
- ![Planned][planned] Sample game Wolfenstein mini: exploration with score, items, secrets and map explored, combat later (moved from v0.6.0) ([Sample games](design/sample-games.md))

## v0.3.0 -- 2026-10-11

**Goal:** Foundations — every known bug fixed, safety nets, Owl RHI repaired, architecture opened, iteration sped up,
Owl Nest made ergonomic. No new gameplay feature ([details](design/foundations.md)).

- ![Done][done] Phase 0 — Conan 2 the only dependency provider (ConanCenter + local recipes), breaking upgrades first
  (EnTT 4, Taskflow 4.1) ([Conan](design/conan-migration.md))
- ![Done][done] Phase A — every confirmed correctness bug fixed with its regression test, `on_collision`, game export
  tested end to end, full Wayland, OpenGL compatibility backend fixed
- ![Done][done] Phase B — benchmark harness and runner frame bench in CI, blocking sanitizers, image tests on
  lavapipe / llvmpipe, headless `OwlRunner --scenario`, editor tests, two-level CI with a `PR Ready` gate, Tracy
  ([Profiling](profiling.md))
- ![Done][done] Phase C — Vulkan first behind Owl RHI, OpenGL frozen as fallback, ABI clean-up, world per scene,
  optional engine modules, editor command API, typed Lua bindings ([Lua API](lua-api.md)), Owl stays on GLFW
- ![Done][done] Phase D — public dependencies reduced to EnTT, hot reload of assets, shaders and scripts, autosave,
  crash recovery, session restore, project templates, documentation checked in CI
- ![Done][done] Phase E — Owl Nest ergonomics revamp: default layout, ribbon and File screen, command palette, context
  menus, drag & drop, text scale, Owl theme and duotone icons, asset previews, camera presets and view-cube
  ([Nest UI](design/nest-ui.md))
- ![Done][done] Performance targets met or accepted ([targets](design/foundations.md#performance-targets))

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
