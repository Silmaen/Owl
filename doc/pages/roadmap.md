# Roadmap {#page-roadmap}

[TOC]

**Owl Engine is the engine that mixes rendering styles** — 2D, raycast, voxel, isometric and 3D in one game, through the
renderer stack. Each upcoming version below, and the last released one, has a short goal and one line per item; the
detail lives in the design pages under `doc/pages/design/`. A one-glance summary is in the root `ROADMAP.md`; shipped
changes are in the [Changelog](changelog.md).

Development uses Claude Code as a supervised tool: every change is reviewed and owned by the maintainer, behind a
strict CI, hence the cadence below. **Exit criteria take precedence over dates**: a version ships when its criteria
hold.

**Ongoing across all releases** ([details](design/ongoing-quality.md)):

- ![Ongoing][ongoing] Render-style mixing — every new mode is a renderer-stack layer that composes with the others
- ![Ongoing][ongoing] Code quality — clang-tidy / clang-format clean, conventions of `.claude/rules/cpp-style.md`
- ![Ongoing][ongoing] Test coverage — no public API without tests, every bug fix with its regression test
- ![Ongoing][ongoing] Performance — measure first (Tracy, frame bench, `test/bench/`), no per-frame allocations
- ![Ongoing][ongoing] Performance budgets enforced in CI — benchmark regression threshold fails the build
- ![Ongoing][ongoing] Documentation quality — public API documented, `doc/pages` updated in the same PR
- ![Ongoing][ongoing] Editor coverage for every authored object — selection, inspection, editing, bulk ops, discovery;
  every script and Lua function visible where it is bound ([Editor reach](design/editor-reach.md))
- ![Ongoing][ongoing] Every edit gesture is a named editor command — recorded, replayable, reachable through MCP

## After 1.0 -- To evaluate

- ![To evaluate][evaluate] WASM game modules and compiled mods: C++, Rust or Zig built once, run by the runner on every
  target ([Modding and platforms](design/modding-platforms.md))
- ![To evaluate][evaluate] Android App Bundle (AAB) for Google Play
  ([Game export](design/game-export.md#game-export-signing))
- ![To evaluate][evaluate] Linux AppImage and Flatpak next to the portable archive
  ([Game export](design/game-export.md#game-export-targets))
- ![To evaluate][evaluate] Networking on the Web: a WebSocket or WebRTC transport
  ([Networking](design/simulation-networking.md))
- ![To evaluate][evaluate] Owl Nest in several languages, on the string tables of the games
  ([Localisation](design/localisation.md))
- ![To evaluate][evaluate] Hot reload of a C++ game module ([Content pipeline](design/content-pipeline.md#hot-reload))
- ![To evaluate][evaluate] macOS (Metal) ([Modding and platforms](design/modding-platforms.md))

## v1.0.0 -- Expected 2026-12-31

**Goal:** A stable engine a third party can rely on. No new feature: 1.0.0 is a 0.9.x that held every criterion
below, validated and proven through the hardening phase ([details](design/stable-release.md)).

- ![Planned][planned] CI green everywhere: every configuration and the smoke test of every exported target
- ![Planned][planned] Test protocols and release checklists passed
  ([Hardening](design/stable-release.md#stable-release-hardening))
- ![Planned][planned] Public API frozen under semantic versioning, with its deprecation policy
- ![Planned][planned] Two showcase games (2D and 3D) authored only in the editor, each mixing rendering styles, picked
  from the sample games
- ![Planned][planned] Linux, Windows, Android and the Web flawless, each exported from every Owl Nest host
- ![Planned][planned] Zero known crash
- ![Planned][planned] Performance budgets held on every target platform

## v0.9.0 -- Expected 2026-12-15

**Goal:** The 1.0 release candidate: networked multiplayer on the v0.7.0 deterministic simulation, then the freeze of
the API and of the formats, the SDK and everything a third party needs; followed by the 0.9.x hardening phase
([networking](design/simulation-networking.md), [stable release](design/stable-release.md)). Critical path: transport
and
replication, the freezes, the SDK.

- ![Planned][planned] Network transport layer (reliable UDP, authoritative server, connection management), on desktop
  and Android
- ![Planned][planned] Entity replication (replicated components, interpolation, prediction, authority)
- ![Planned][planned] Lockstep network model on the bit-exact simulation, desync detection by world hash
  ([Networking](design/simulation-networking.md))
- ![Planned][planned] Fair matches: input type in the lobby, server-side lag compensation, minimum hardware profile
  ([Fair play](design/simulation-networking.md#networking-fairness))
- ![Planned][planned] RPC system (Lua `rpc_server` / `rpc_client` / `rpc_all`, reliable and unreliable)
- ![Planned][planned] Lobby and session management
- ![Planned][planned] Network debugging tools (latency / loss simulation, stats overlay)
- ![Planned][planned] Interest management: relevance areas and priorities, a client receives only what is near it
  ([Large worlds](design/large-worlds.md))
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a
  networked Bomberman ([Pool](design/sample-games.md#sample-games-pool))
- ![Planned][planned] Public API freeze, semantic versioning and deprecation policy, ABI check in CI — release candidate
- ![Planned][planned] Versioned scene and save formats with tested migrations
- ![Planned][planned] Project upgrade assistant: a project of an earlier version migrated after a backup
  ([Stable release](design/stable-release.md#stable-release-upgrade))
- ![Planned][planned] Consumable SDK: OwlEngine Conan package, `find_package(OwlEngine)` with 1–2 public dependencies,
  consumer test in CI
- ![Planned][planned] Windows SDK compatible with MSVC (clang-cl), next to MinGW
  ([Platforms](design/modding-platforms.md#platforms-msvc))
- ![Planned][planned] Owl Nest and the runners signed for Windows: a certificate provider for the signing of the
  `Package` action ([Game export](design/game-export.md#game-export-signing))
- ![Planned][planned] Fuzzers on every file loader
- ![Planned][planned] Performance budgets verified in CI on every target platform
- ![Planned][planned] Complete user documentation: tutorials, Lua and visual-scripting references
- ![Planned][planned] Test protocols and release checklists for the hardening phase
  ([Hardening](design/stable-release.md#stable-release-hardening))

## v0.8.0 -- Expected 2026-12-06

**Goal:** Ship a game everywhere: every target exported from every Owl Nest host, Android and the Web as new
platforms, mods and a local crash reporter
([game export](design/game-export.md), [modding and platforms](design/modding-platforms.md)). Critical path: the build
image,
the templates, the shaders per target, Android and the Web.

- ![Planned][planned] Build image with the Android NDK, emsdk, xwin and a headless browser, extended by the maintainer
  ([Game export](design/game-export.md#game-export-templates))
- ![Planned][planned] Release pipeline: each runner built once, the Android, Web and arm64 runners at night and at
  release ([Game export](design/game-export.md#game-export-templates))
- ![Planned][planned] Every target exported from every host: the five release runners bundled in each Owl Nest
  ([Game export](design/game-export.md#game-export-targets))
- ![Planned][planned] Shaders baked by the host for each target, a runner without shader compiler
  ([Game export](design/game-export.md#game-export-shaders))
- ![Planned][planned] Signing at export: Authenticode from a `.pfx` or a custom command, APK v2 / v3, debug and release
  keystores ([Game export](design/game-export.md#game-export-signing))
- ![Planned][planned] Exported Windows games carry their project icon in the executable, not only on the window
- ![Planned][planned] Export report: size per target, baked shaders, unused assets, warnings
  ([Game export](design/game-export.md#game-export-report))
- ![Planned][planned] Smoke test of every exported target in CI, Android on a tablet at night
  ([Game export](design/game-export.md#game-export-smoke-tests))
- ![Planned][planned] Android: window and input backend, Vulkan, signed APK
  ([Platforms](design/modding-platforms.md#platforms-android))
- ![Planned][planned] Touch screen input: taps and drags as pointer events, pinch and two-finger pan, the HUD and Owl
  Nest usable by touch ([Input](design/windowing-input.md))
- ![Planned][planned] Web export through a WebGPU Owl RHI backend, run in the browser from Owl Nest
  ([Platforms](design/modding-platforms.md#platforms-web))
- ![Planned][planned] Gamepad improvements: remapping UI, haptics, joysticks and throttles with many axes
  ([Input](design/windowing-input.md))
- ![Planned][planned] Crash reporter: a local report to attach to a GitHub issue, nothing sent
  ([Crash reporter](design/crash-reporter.md))
- ![Planned][planned] Mod loading system (`.owlmod` packs, manifest, load order, asset overrides)
- ![Planned][planned] Lua mod API (custom components, game event hooks, sandboxed)
- ![Planned][planned] In-game mod manager (enable / disable / reorder, conflict detection)

## v0.7.0 -- Expected 2026-11-22

**Goal:** Physics, simulation and AI — 2D and 3D physics and AI as optional modules outside `Scene`, on a
deterministic simulation with input replay and rewind; articulated bodies, gravity and time warp for worlds of any
scale ([details](design/gameplay-systems.md), [simulation](design/simulation-networking.md),
[large worlds](design/large-worlds.md)). Critical path: the deterministic simulation and replay, Jolt, gravity fields.

- ![Planned][planned] Deterministic simulation (seeded streams, cross-platform world-hash test) on the v0.4.0 fixed tick
- ![Planned][planned] Bit-exact simulation on every target, always on: no FMA contraction, deterministic maths,
  deterministic Box2D and Jolt, fixed Lua hash seed
  ([Simulation](design/simulation-networking.md#simulation-cross-platform))
- ![Planned][planned] Fair play: fixed tick whatever the frame rate, gameplay visibility never cut by quality settings,
  bounded field of view ([Fair play](design/simulation-networking.md#networking-fairness))
- ![Planned][planned] Input recording and frame-exact replay (replays double as regression tests)
- ![Planned][planned] Rewind in Play (snapshot ring buffer + input log)
- ![Planned][planned] Full 2D physics: callbacks, queries, joints, on the shared physics interface
  ([Physics](design/physics-api.md))
- ![Planned][planned] 3D physics with Jolt on the shared physics interface ([Physics API](design/physics-api.md))
- ![Planned][planned] Articulated bodies: compound bodies rebuilt at runtime, breakable joints, mass recomputed,
  sub-stepping ([Physics API](design/physics-api.md))
- ![Planned][planned] Gravity fields (uniform, point, spherical) with trajectory prediction
  ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Time warp: scaled clock, bodies handed to analytic propagation and back
  ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Simulation level of detail: far entities on a coarse model, on a slower tick
  ([Large worlds](design/large-worlds.md))
- ![Planned][planned] Navigation (NavMesh generation, A*, dynamic avoidance); tile-graph pathfinding comes in v0.4.0
- ![Planned][planned] Behaviour trees with a visual editor (`.owlbt`)
- ![Planned][planned] Steering behaviours and flocking, in 2D and in open 3D space (obstacle avoidance, formations)
- ![Planned][planned] Engine Demo: enemies demo on the AI modules
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a
  Zelda-like, a marble or kart game, a lunar lander, an asteroid dogfight, an orbital golf, a bridge builder
  ([Pool](design/sample-games.md#sample-games-pool))

## v0.6.0 -- Expected 2026-11-08

**Goal:** Produce content at scale and script games without C++: offline asset pipeline, animation and effects,
procedural graphs, tile tools, Lua editor scripts, Blueprints-style visual scripting compiled to Lua; then the content
side of large worlds, audio, dialogue and cutscenes, accessibility ([large worlds](design/large-worlds.md),
[gameplay systems](design/gameplay-systems.md)).

- ![Done][done] CI: one GCC build (Debug, tests, coverage), a fast first stage, a quick check for agents
  ([CI streamlining](design/ci-streamlining.md#ci-streamlining-cost-review))
- ![Done][done] Offline asset pipeline: cook, content-addressed cache, asset GUIDs, KTX2 BC7/ASTC texture compression
  ([Content](design/content-pipeline.md))
- ![Done][done] Binary runtime scene format produced by the cook
- ![Done][done] A game packs only what it uses: shaders of its renderer stack, its fonts, no Owl Nest asset
  ([Game export](design/game-export.md#game-export-content))
- ![Done][done] Games read their assets from the pack in memory, with a parallel `readEntry`; nothing extracted to disk
  ([Game export](design/game-export.md#game-export-in-memory))
- ![Done][done] Project shaders baked by the cook, no shader compiled at the first run of a game
  ([Game export](design/game-export.md#game-export-project-shaders))
- ![Done][done] Localisation of games: string tables edited in Owl Nest, `tr()` in Lua, fonts per language
  ([Localisation](design/localisation.md))
- ![Done][done] World positions in double with a floating origin, through scene, physics, audio and Owl Nest
  ([Large worlds](design/large-worlds.md))
- ![Done][done] Spline component and its editor: paths, lanes, rails ([Large worlds](design/large-worlds.md#large-worlds-splines))
- ![Done][done] Data tables: typed records edited in a grid in Owl Nest, read from Lua
  ([Large worlds](design/large-worlds.md#large-worlds-data-tables))
- ![Done][done] Mesh sockets: named attachment points, entities assembled at runtime
  ([Skeletal animation](design/skeletal-animation.md))
- ![Done][done] World partition: streaming cells on world coordinates, additive scenes loaded around the player
  ([World partition](design/large-worlds.md#world-partition))
- ![Done][done] Planetary terrain layer: cube-sphere quadtree LOD from PCG graphs, atmospheric scattering, scaled
  space ([Large worlds](design/large-worlds.md#planetary-terrain))
- ![Done][done] Skeletal animation (skinning, blending, state machine) and mesh LODs
  ([Skeletal animation](design/skeletal-animation.md))
- ![Done][done] GPU particle system with editor preview and trails, emissive materials and lens flares
  ([Effects](design/gpu-effects.md))
- ![Done][done] Audio mixer (buses, per-bus volume, crossfades) ([Gameplay systems](design/gameplay-systems.md))
- ![Done][done] Audio effects (reverb zones, low / high-pass filters, Doppler) and adaptive music driven by game
  states ([Gameplay systems](design/gameplay-systems.md))
- ![Planned][planned] Dialogue system with a visual editor (`.owldlg`) ([Gameplay systems](design/gameplay-systems.md))
- ![Planned][planned] Camera tracks in the sequencer, for cutscenes ([Large worlds](design/large-worlds.md))
- ![Planned][planned] HUD markers anchored to world entities, with off-screen indicators
  ([HUD system](design/hud-system.md))
- ![Planned][planned] Accessibility: UI scale, colour-blind modes, subtitles, reduced motion
  ([Accessibility](design/accessibility.md))
- ![Done][done] Settings, logs and caches stay in the application folder, with an import of the settings of a
  previous version ([User directory](architecture.md#architecture-user-directory))
- ![Done][done] Advanced post-processing (chromatic aberration, film grain, motion blur, depth of field)
- ![Done][done] Angular radius of a directional light: soft shadows (PCSS), wider highlight, sun disc in the sky
  ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Done][done] Area lights (rectangle, disc, sphere) lighting through LTC, with soft shadows sharing the PCSS of the
  sun ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Done][done] Point and spot light shadows (cube and perspective shadow maps)
  ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Done][done] Local reflection probes: baked cube maps blended by volume, parallax-corrected, so metals reflect the
  room ([3D core](design/3d-core.md#reflection-probes))
- ![Done][done] Visual scripting MVP, event graphs compiled to Lua, node editing of assets
  ([details](design/visual-scripting.md))
- ![Done][done] Node-graph link waypoints, in visual scripts, PCG graphs and every node graph
- ![Done][done] Scene Flow view removed: it showed few of the scene links of a real project
- ![Done][done] Script debugging aids (breakpoint markers, live watch)
- ![Done][done] Lua editor scripts and interactive Lua console ([Editor scripting](design/editor-scripting.md))
- ![To evaluate][evaluate] Lua to visual script: turn a Lua script into a graph
  ([Visual scripting](design/visual-scripting.md#visual-scripting-lua-to-graph))
- ![Done][done] Procedural generation graphs on `NodeCanvas`, first brick on tilemaps
  ([PCG graphs](design/pcg-graphs.md))
- ![Done][done] Tileset editor: compose the atlas from source images ([2D](design/2d-complete.md))
- ![Done][done] Mipmapped pixel-art atlases: sharp texels up close, filtered mips far away, no bleeding between tiles,
  for the voxel, raycast and tilemap atlases ([2D](design/2d-complete.md#pixel-art-atlases))
- ![Done][done] Sound editor in Owl Nest: preview, trim, loop points, volume and pitch, random variants of one sound
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done: e.g. a Mini
  Minecraft, combat in the Wolfenstein mini, a station trader, a rail shooter
  ([Pool](design/sample-games.md#sample-games-pool))

## v0.5.0 -- 2026-10-25

**Goal:** A focused 3D core on the repaired Owl RHI — static meshes, PBR, lights and shadows for 2D, isometric and 3D,
render graph, essential post-processing — with full 3D editing ([details](design/3d-core.md)).

- ![Done][done] CI streamlining, before anything else of the cycle: a pull request answered in under 10 min, builds
  halved, `-j` everywhere, full suites at night, artefacts under 50 MB ([CI streamlining](design/ci-streamlining.md))
- ![Done][done] 3D render pipeline (forward, depth, perspective camera)
- ![Done][done] Render graph hosting the renderer-stack layers
- ![Done][done] Lighting: directional, point and spot lights, directional shadow maps
- ![Done][done] 2D lighting (point / spot lights, normal-mapped sprites, 2D shadows), isometric sprites included
  ([2D](design/2d-complete.md))
- ![Done][done] PBR materials with editor and material library, emissive colour and strength included
  ([3D core](design/3d-core.md#emissive-and-area-lights))
- ![Done][done] Static mesh component with instancing
- ![Done][done] Essential post-processing stack (tone mapping, bloom, vignette, LUT)
- ![Done][done] Camera-relative rendering and reversed-Z depth, precise at any distance
  ([Large worlds](design/large-worlds.md))
- ![Done][done] Compute-driven frustum / occlusion culling with indirect draws
- ![Done][done] GPU raycast sprite stripes and `BitonicSortPass` adoption
- ![Done][done] Spatial partitioning (loose octree shared by the 2D and 3D layers)
- ![Done][done] Multithreaded render preparation: world transforms and spatial boxes of scenes above 4096 entities on
  the task scheduler ([Renderer](renderer.md))
- ![Done][done] Cost of the 2D lighting cut back: 80-byte quad instances, unlit tilemaps, near-plane-only frustum
  tolerance ([2D](design/2d-complete.md#lighting-cost))
- ![Done][done] Laid-out text kept between frames while its font, text and layout stay
  ([Renderer](renderer.md#renderer-text-cache))
- ![Done][done] Frame pacing: FIFO vsync, a frame rate cap, an idle editor waiting for input, the frame cost on the
  viewport chip ([Windowing](design/windowing-input.md))
- ![Done][done] HUD minimap on a render target: `RenderTarget`, `FollowTarget`, `UiImage.renderTarget`
  ([HUD system](design/hud-system.md#minimap))
- ![Done][done] 3D scene editing in Owl Nest (3D gizmos, snapping, mesh preview), "look through scene camera" mode
- ![Done][done] Editor reach, before the release: every script and Lua function of a scene visible in Owl Nest with its
  hooks and an **Open** at the line, a scene script, richer script properties, layer overrides for every key
  ([Editor reach](design/editor-reach.md))
- ![Done][done] Behaviour components replacing demo scripts: `LightFlicker` (fire, candles), `Oscillator`, `Patrol`,
  `Drift` ([Editor reach](design/editor-reach.md#behaviour-components))
- ![Done][done] Tuned constants of the Engine Demo and the sample games moved to script properties; a test failing on
  any component field or layer key without an editor widget ([Editor reach](design/editor-reach.md))
- ![Done][done] `ProceduralSky` and `DayNightCycle`: a sky and a sun and moon moving over the voxel level
  ([3D core](design/3d-core.md#procedural-sky-and-day-night-cycle))
- ![Done][done] Voxel and isometric art revamp: relief and roughness maps on voxel tilesets, 128 px demo blocks, more
  surface detail on the isometric tiles
- ![Done][done] `FirstPersonController` walking on `MeshRenderer` boxes, and the Engine Demo tavern as a fifth hub level
- ![Done][done] Owl Nest fixes: material texture **Open** buttons, text files (`gamecontrollerdb.txt`) in the code
  editor; material thumbnails and a prefab document ([Editor reach](design/editor-reach.md#editor-fixes-and-gaps))
- ![Done][done] Help panel: mermaid diagrams drawn as SVG at build time, list hanging indent, flat inline code, task
  lists ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Owl Nest user help: an editor start page, editor pages per part, a sidebar grouped by theme, no planning
  page in the help ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Help reading: gallery tables, coloured status tags, anchored links, browser-like history, Lua API per
  theme ([Nest UI](design/nest-ui.md#in-editor-documentation))
- ![Done][done] Lua gaps of the sample games: spawn a prefab, tilemap cells kept by saves, `ui.set_image`, pause and
  time scale, entity under the mouse ([Gaps](design/sample-games.md#sample-games-gaps))
- ![Done][done] Sample game: a tower defence, first map in basic art
  ([Sample games](design/sample-games.md#sample-games-tower-defence))
- ![Done][done] Tower defence campaign: 10 levels in 3D, day and night, modifiers, sprite enemies, world map, star
  upgrades ([Sample games](design/sample-games.md#sample-games-tower-defence))

## Earlier versions

What the earlier versions shipped is in the [Changelog](changelog.md).

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
