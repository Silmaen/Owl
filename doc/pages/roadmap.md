# Roadmap {#page-roadmap}

[TOC]

**Owl Engine is the engine that mixes rendering styles** — 2D, raycast, voxel, isometric and 3D in one game, through the
renderer stack. Each version below has a short goal and one line per item; the detail lives in the design pages under
`doc/pages/design/`. A one-glance summary is in the root `ROADMAP.md`; shipped changes are in the
[Changelog](changelog.md).

Development uses Claude Code as a supervised tool: every change is reviewed and owned by the maintainer, behind a
strict CI, hence the cadence below.
**Exit criteria take precedence over dates**: a version ships when its criteria hold.

**Ongoing across all releases** ([details](design/ongoing-quality.md)):

- ![Ongoing][ongoing] Render-style mixing — every new mode is a renderer-stack layer that composes with the others
- ![Ongoing][ongoing] Code quality — clang-tidy / clang-format clean, conventions of `.claude/rules/cpp-style.md`
- ![Ongoing][ongoing] Test coverage — no public API without tests, every bug fix with its regression test
- ![Ongoing][ongoing] Performance — measure first (Tracy, frame bench, `test/bench/`), no per-frame allocations
- ![Ongoing][ongoing] Performance budgets enforced in CI — benchmark regression threshold fails the build
- ![Ongoing][ongoing] Documentation quality — public API documented, `doc/pages` updated in the same PR
- ![Ongoing][ongoing] Editor coverage for every authored object — selection, inspection, editing, bulk ops, discovery; every script and Lua function visible where it is bound ([Editor reach](design/editor-reach.md))
- ![Ongoing][ongoing] Every edit gesture is a named editor command — recorded, replayable, reachable through MCP

## After 1.0 -- To evaluate

- ![To evaluate][evaluate] WASM game modules and compiled mods: C++, Rust or Zig built once, run by the runner on every target ([Modding and platforms](design/modding-platforms.md))
- ![To evaluate][evaluate] Android App Bundle (AAB) for Google Play ([Game export](design/game-export.md#game-export-signing))
- ![To evaluate][evaluate] Linux AppImage and Flatpak next to the portable archive ([Game export](design/game-export.md#game-export-targets))
- ![To evaluate][evaluate] Networking on the Web: a WebSocket or WebRTC transport ([Networking](design/simulation-networking.md))
- ![To evaluate][evaluate] Owl Nest in several languages, on the string tables of the games ([Localisation](design/localisation.md))
- ![To evaluate][evaluate] Hot reload of a C++ game module ([Content pipeline](design/content-pipeline.md#hot-reload))
- ![To evaluate][evaluate] macOS (Metal) ([Modding and platforms](design/modding-platforms.md))

## v1.0.0 -- Expected 2026-12-31

**Goal:** A stable engine a third party can rely on. No new feature: 1.0.0 is a 0.9.x that held every criterion
below, validated and proven through the hardening phase ([details](design/stable-release.md)).

- ![Planned][planned] CI green everywhere: every configuration and the smoke test of every exported target
- ![Planned][planned] Test protocols and release checklists passed ([Hardening](design/stable-release.md#stable-release-hardening))
- ![Planned][planned] Public API frozen under semantic versioning, with its deprecation policy
- ![Planned][planned] Two showcase games (2D and 3D) authored only in the editor, each mixing rendering styles, picked from the sample games
- ![Planned][planned] Linux, Windows, Android and the Web flawless, each exported from every Owl Nest host
- ![Planned][planned] Zero known crash
- ![Planned][planned] Performance budgets held on every target platform

## v0.9.0 -- Expected 2026-12-15

**Goal:** The 1.0 release candidate: networked multiplayer on the v0.7.0 deterministic simulation, then the freeze of
the API and of the formats, the SDK and everything a third party needs; followed by the 0.9.x hardening phase
([networking](design/simulation-networking.md), [stable release](design/stable-release.md)). Critical path: transport and
replication, the freezes, the SDK.

- ![Planned][planned] Network transport layer (reliable UDP, authoritative server, connection management), on desktop and Android
- ![Planned][planned] Entity replication (replicated components, interpolation, prediction, authority)
- ![Planned][planned] Lockstep network model on the bit-exact simulation, desync detection by world hash ([Networking](design/simulation-networking.md))
- ![Planned][planned] Fair matches: input type in the lobby, server-side lag compensation, minimum hardware profile ([Fair play](design/simulation-networking.md#networking-fairness))
- ![Planned][planned] RPC system (Lua `rpc_server` / `rpc_client` / `rpc_all`, reliable and unreliable)
- ![Planned][planned] Lobby and session management
- ![Planned][planned] Network debugging tools (latency / loss simulation, stats overlay)
- ![Planned][planned] Interest management: relevance areas and priorities, a client receives only what is near it ([Large worlds](design/large-worlds.md))
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a networked Bomberman ([Pool](design/sample-games.md#sample-games-pool))
- ![Planned][planned] Public API freeze, semantic versioning and deprecation policy, ABI check in CI — release candidate
- ![Planned][planned] Versioned scene and save formats with tested migrations
- ![Planned][planned] Project upgrade assistant: a project of an earlier version migrated after a backup ([Stable release](design/stable-release.md#stable-release-upgrade))
- ![Planned][planned] Consumable SDK: OwlEngine Conan package, `find_package(OwlEngine)` with 1–2 public dependencies, consumer test in CI
- ![Planned][planned] Windows SDK compatible with MSVC (clang-cl), next to MinGW ([Platforms](design/modding-platforms.md#platforms-msvc))
- ![Planned][planned] Owl Nest and the runners signed for Windows: a certificate provider for the signing of the `Package` action ([Game export](design/game-export.md#game-export-signing))
- ![Planned][planned] Settings, logs and caches stay in the application folder, with an import of the settings of a previous version
- ![Planned][planned] Fuzzers on every file loader
- ![Planned][planned] Performance budgets verified in CI on every target platform
- ![Planned][planned] Complete user documentation: tutorials, Lua and visual-scripting references
- ![Planned][planned] Test protocols and release checklists for the hardening phase ([Hardening](design/stable-release.md#stable-release-hardening))

## v0.8.0 -- Expected 2026-12-06

**Goal:** Ship a game everywhere: every target exported from every Owl Nest host, Android and the Web as new
platforms, mods, accessibility and a local crash reporter
([game export](design/game-export.md), [modding and platforms](design/modding-platforms.md)). Critical path: the build image,
the templates, the shaders per target, Android and the Web.

- ![Planned][planned] Build image with the Android NDK, emsdk, xwin and a headless browser, extended by the maintainer ([Game export](design/game-export.md#game-export-templates))
- ![Planned][planned] Release pipeline: each runner built once, the Android, Web and arm64 runners at night and at release ([Game export](design/game-export.md#game-export-templates))
- ![Planned][planned] Every target exported from every host: the five release runners bundled in each Owl Nest ([Game export](design/game-export.md#game-export-targets))
- ![Planned][planned] Shaders baked by the host for each target, a runner without shader compiler ([Game export](design/game-export.md#game-export-shaders))
- ![Planned][planned] Signing at export: Authenticode from a `.pfx` or a custom command, APK v2 / v3, debug and release keystores ([Game export](design/game-export.md#game-export-signing))
- ![Planned][planned] Exported Windows games carry their project icon in the executable, not only on the window
- ![Planned][planned] Export report: size per target, baked shaders, unused assets, warnings ([Game export](design/game-export.md#game-export-report))
- ![Planned][planned] Smoke test of every exported target in CI, Android on a tablet at night ([Game export](design/game-export.md#game-export-smoke-tests))
- ![Planned][planned] Android: window and input backend, Vulkan, signed APK ([Platforms](design/modding-platforms.md#platforms-android))
- ![Planned][planned] Touch screen input: taps and drags as pointer events, pinch and two-finger pan, the HUD and Owl Nest usable by touch ([Input](design/windowing-input.md))
- ![Planned][planned] Web export through a WebGPU Owl RHI backend, run in the browser from Owl Nest ([Platforms](design/modding-platforms.md#platforms-web))
- ![Planned][planned] Gamepad improvements: remapping UI, haptics, joysticks and throttles with many axes ([Input](design/windowing-input.md))
- ![Planned][planned] Accessibility: UI scale, colour-blind modes, subtitles, reduced motion ([Accessibility](design/accessibility.md))
- ![Planned][planned] Crash reporter: a local report to attach to a GitHub issue, nothing sent ([Crash reporter](design/crash-reporter.md))
- ![Planned][planned] Mod loading system (`.owlmod` packs, manifest, load order, asset overrides)
- ![Planned][planned] Lua mod API (custom components, game event hooks, sandboxed)
- ![Planned][planned] In-game mod manager (enable / disable / reorder, conflict detection)
- ![Planned][planned] World partition: streaming cells on world coordinates, additive scenes loaded around the player ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Camera tracks in the sequencer, for cutscenes ([Large worlds](design/large-worlds.md))

## v0.7.0 -- Expected 2026-11-22

**Goal:** The systems that make games — AI, 2D and 3D physics, audio, narrative — as optional modules outside
`Scene`, on a deterministic simulation with input replay and rewind; articulated bodies, gravity, time warp and
planets for worlds of any scale ([details](design/gameplay-systems.md), [simulation](design/simulation-networking.md),
[large worlds](design/large-worlds.md)). Critical path: the deterministic simulation and replay, Jolt, gravity fields.

- ![Planned][planned] Deterministic simulation (seeded streams, cross-platform world-hash test) on the v0.4.0 fixed tick
- ![Planned][planned] Bit-exact simulation on every target, always on: no FMA contraction, deterministic maths, deterministic Box2D and Jolt, fixed Lua hash seed ([Simulation](design/simulation-networking.md#simulation-cross-platform))
- ![Planned][planned] Fair play: fixed tick whatever the frame rate, gameplay visibility never cut by quality settings, bounded field of view ([Fair play](design/simulation-networking.md#networking-fairness))
- ![Planned][planned] Input recording and frame-exact replay (replays double as regression tests)
- ![Planned][planned] Rewind in Play (snapshot ring buffer + input log)
- ![Planned][planned] Full 2D physics: callbacks, queries, joints, on the shared physics interface ([Physics](design/physics-api.md))
- ![Planned][planned] 3D physics with Jolt on the shared physics interface ([Physics API](design/physics-api.md))
- ![Planned][planned] Articulated bodies: compound bodies rebuilt at runtime, breakable joints, mass recomputed, sub-stepping ([Physics API](design/physics-api.md))
- ![Planned][planned] Gravity fields (uniform, point, spherical) with trajectory prediction ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Time warp: scaled clock, bodies handed to analytic propagation and back ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Simulation level of detail: far entities on a coarse model, on a slower tick ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Planetary terrain layer: cube-sphere quadtree LOD from PCG graphs, atmospheric scattering, scaled space ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Navigation (NavMesh generation, A*, dynamic avoidance); tile-graph pathfinding comes in v0.4.0
- ![Planned][planned] Behaviour trees with a visual editor (`.owlbt`)
- ![To evaluate][evaluate] Lua to visual script: turn a Lua script into a graph ([Visual scripting](design/visual-scripting.md#visual-scripting-lua-to-graph))
- ![Planned][planned] Steering behaviours and flocking, in 2D and in open 3D space (obstacle avoidance, formations)
- ![Planned][planned] Audio mixer (buses, per-bus volume, crossfades)
- ![Planned][planned] Audio effects (reverb zones, low / high-pass filters, Doppler) and adaptive music driven by game states
- ![Planned][planned] Dialogue system with a visual editor (`.owldlg`)
- ![Planned][planned] HUD markers anchored to world entities, with off-screen indicators ([HUD system](design/hud-system.md))
- ![Planned][planned] Engine Demo: enemies demo on the AI modules
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a Zelda-like, a marble or kart game, a lunar lander, an asteroid dogfight, a rail shooter, an orbital golf, a bridge builder ([Pool](design/sample-games.md#sample-games-pool))

## v0.6.0 -- Expected 2026-11-08

**Goal:** Produce content at scale and script games without C++: offline asset pipeline, animation and effects,
procedural graphs, tile tools, Lua editor scripts, and Blueprints-style visual scripting compiled to Lua. Critical path:
the cook, double positions with a floating origin, visual scripting; the rest may slip to v0.7.0.

- ![Done][done] CI: one GCC build (Debug, tests, coverage), a fast first stage, a quick check for agents ([CI streamlining](design/ci-streamlining.md#ci-streamlining-cost-review))
- ![Done][done] Offline asset pipeline: cook, content-addressed cache, asset GUIDs, KTX2 BC7/ASTC texture compression ([Content](design/content-pipeline.md))
- ![Done][done] Binary runtime scene format produced by the cook
- ![Done][done] A game packs only what it uses: shaders of its renderer stack, its fonts, no Owl Nest asset ([Game export](design/game-export.md#game-export-content))
- ![Done][done] Games read their assets from the pack in memory, with a parallel `readEntry`; nothing extracted to disk ([Game export](design/game-export.md#game-export-in-memory))
- ![Done][done] Project shaders baked by the cook, no shader compiled at the first run of a game ([Game export](design/game-export.md#game-export-project-shaders))
- ![Planned][planned] Localisation of games: string tables edited in Owl Nest, `tr()` in Lua, fonts per language ([Localisation](design/localisation.md))
- ![Done][done] World positions in double with a floating origin, through scene, physics, audio and Owl Nest ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Spline component and its editor: paths, lanes, rails ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Data tables: typed records edited in a grid in Owl Nest, read from Lua ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Mesh sockets: named attachment points, entities assembled at runtime ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Skeletal animation (skinning, blending, state machine) and mesh LODs
- ![Planned][planned] GPU particle system with editor preview and trails, emissive materials and lens flares
- ![Planned][planned] Advanced post-processing (chromatic aberration, film grain, motion blur, depth of field)
- ![Planned][planned] Angular radius of a directional light: soft shadows (PCSS), wider highlight, sun disc in the sky ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Planned][planned] Area lights (rectangle, disc, sphere) lighting through LTC, with soft shadows sharing the PCSS of the sun ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Planned][planned] Point and spot light shadows (cube and perspective shadow maps) ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Planned][planned] Local reflection probes: baked cube maps blended by volume, parallax-corrected, so metals reflect the room ([3D core](design/3d-core.md#reflection-probes))
- ![Done][done] Visual scripting MVP, event graphs compiled to Lua, node editing of assets ([details](design/visual-scripting.md))
- ![Planned][planned] Node-graph link waypoints
- ![Done][done] Scene Flow view removed: it showed few of the scene links of a real project
- ![Done][done] Script debugging aids (breakpoint markers, live watch)
- ![Done][done] Lua editor scripts and interactive Lua console ([Editor scripting](design/editor-scripting.md))
- ![Done][done] Procedural generation graphs on `NodeCanvas`, first brick on tilemaps ([PCG graphs](design/pcg-graphs.md))
- ![Done][done] Tileset editor: compose the atlas from source images ([2D](design/2d-complete.md))
- ![Done][done] Mipmapped pixel-art atlases: sharp texels up close, filtered mips far away, no bleeding between tiles, for the voxel, raycast and tilemap atlases ([2D](design/2d-complete.md#mipmapped-pixel-art-atlases-v060))
- ![Planned][planned] Sound editor in Owl Nest: preview, trim, loop points, volume and pitch, random variants of one sound
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a Mini Minecraft, combat in the Wolfenstein mini, a station trader ([Pool](design/sample-games.md#sample-games-pool))

## v0.5.0 -- 2026-10-25

**Goal:** A focused 3D core on the repaired Owl RHI — static meshes, PBR, lights and shadows for 2D, isometric and 3D,
render graph, essential post-processing — with full 3D editing ([details](design/3d-core.md)).

- ![Done][done] CI streamlining, before anything else of the cycle: a pull request answered in under 10 min, builds halved, `-j` everywhere, full suites at night, artefacts under 50 MB ([CI streamlining](design/ci-streamlining.md))
- ![Done][done] 3D render pipeline (forward, depth, perspective camera)
- ![Done][done] Render graph hosting the renderer-stack layers
- ![Done][done] Lighting: directional, point and spot lights, directional shadow maps
- ![Done][done] 2D lighting (point / spot lights, normal-mapped sprites, 2D shadows), isometric sprites included ([2D](design/2d-complete.md))
- ![Done][done] PBR materials with editor and material library, emissive colour and strength included ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Done][done] Static mesh component with instancing
- ![Done][done] Essential post-processing stack (tone mapping, bloom, vignette, LUT)
- ![Done][done] Camera-relative rendering and reversed-Z depth, precise at any distance ([Large worlds](design/large-worlds.md))
- ![Done][done] Compute-driven frustum / occlusion culling with indirect draws
- ![Done][done] GPU raycast sprite stripes and `BitonicSortPass` adoption
- ![Done][done] Spatial partitioning (loose octree shared by the 2D and 3D layers)
- ![Done][done] Multithreaded render preparation: world transforms and spatial boxes of scenes above 4096 entities on the task scheduler ([Renderer](renderer.md))
- ![Done][done] Cost of the 2D lighting cut back: 80-byte quad instances, unlit tilemaps, near-plane-only frustum tolerance ([2D](design/2d-complete.md#lighting-cost))
- ![Done][done] Laid-out text kept between frames while its font, text and layout stay ([Renderer](renderer.md#renderer-text-cache))
- ![Done][done] Frame pacing: FIFO vsync, a frame rate cap, an idle editor waiting for input, the frame cost on the viewport chip ([Windowing](design/windowing-input.md))
- ![Done][done] HUD minimap on a render target: `RenderTarget`, `FollowTarget`, `UiImage.renderTarget` ([HUD system](design/hud-system.md#minimap))
- ![Done][done] 3D scene editing in Owl Nest (3D gizmos, snapping, mesh preview), "look through scene camera" mode
- ![Done][done] Editor reach, before the release: every script and Lua function of a scene visible in Owl Nest with its hooks and an **Open** at the line, a scene script, richer script properties, layer overrides for every key ([Editor reach](design/editor-reach.md))
- ![Done][done] Behaviour components replacing demo scripts: `LightFlicker` (fire, candles), `Oscillator`, `Patrol`, `Drift` ([Editor reach](design/editor-reach.md#behaviour-components))
- ![Done][done] Tuned constants of the Engine Demo and the sample games moved to script properties; a test failing on any component field or layer key without an editor widget ([Editor reach](design/editor-reach.md))
- ![Done][done] `ProceduralSky` and `DayNightCycle`: a sky and a sun and moon moving over the voxel level ([3D core](design/3d-core.md#procedural-sky-and-day-night-cycle))
- ![Done][done] Voxel and isometric art revamp: relief and roughness maps on voxel tilesets, 128 px demo blocks, more surface detail on the isometric tiles
- ![Done][done] `FirstPersonController` walking on `MeshRenderer` boxes, and the Engine Demo tavern as a fifth hub level
- ![Done][done] Owl Nest fixes: material texture **Open** buttons, text files (`gamecontrollerdb.txt`) in the code editor; material thumbnails and a prefab document ([Editor reach](design/editor-reach.md#editor-fixes-and-gaps))
- ![Done][done] Help panel: mermaid diagrams drawn as SVG at build time, list hanging indent, flat inline code, task lists ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Owl Nest user help: an editor start page, editor pages per part, a sidebar grouped by theme, no planning page in the help ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Help reading: gallery tables, coloured status tags, anchored links, browser-like history, Lua API per theme ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Lua gaps of the sample games: spawn a prefab, tilemap cells kept by saves, `ui.set_image`, pause and time scale, entity under the mouse ([Gaps](design/sample-games.md#sample-games-gaps))
- ![Done][done] Sample game: a tower defence, first map in basic art ([Sample games](design/sample-games.md#sample-games-tower-defence))
- ![Done][done] Tower defence campaign: 10 levels in 3D, day and night, modifiers, sprite enemies, world map, star upgrades ([Sample games](design/sample-games.md#sample-games-tower-defence))

## v0.4.0 -- 2026-10-20

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
- ![Done][done] Sample game Sokoban: level editing in Owl Nest, undo and saves (moved from v0.5.0) ([Sample games](design/sample-games.md))
- ![Done][done] Sample game Wolfenstein mini: exploration with score, items, secrets and map explored, combat later (moved from v0.6.0) ([Sample games](design/sample-games.md))

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
  optional engine modules, editor command API, typed Lua bindings ([Lua API](lua-api.md)), Owl Engine stays on GLFW
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
