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
- ![To evaluate][evaluate] Owl Nest in several languages, on the string tables of the games
  ([Localisation](design/localisation.md))
- ![To evaluate][evaluate] Hot reload of a C++ game module ([Content pipeline](design/content-pipeline.md#hot-reload))
- ![To evaluate][evaluate] macOS (Metal) ([Modding and platforms](design/modding-platforms.md))
- ![To evaluate][evaluate] Native plugins shipped built by their authors
  ([Modding and platforms](design/modding-platforms.md#modding-plugins))

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
- ![Planned][planned] Every game of the sample-game pool possible with the engine, built or not
  ([Pool](design/sample-games.md#sample-games-pool))

## v0.9.0 -- Expected 2026-12-15

**Goal:** The 1.0 release candidate: the freeze of the API and of the formats, the SDK and everything a third party
needs, on an engine whose every public module served a full cycle; followed by the 0.9.x hardening phase
([stable release](design/stable-release.md)). Critical path: the freezes, the SDK.

- ![Planned][planned] Public API freeze, semantic versioning and deprecation policy, ABI check in CI — release candidate
- ![Planned][planned] Versioned scene and save formats with tested migrations
- ![Planned][planned] Project upgrade assistant: a project of an earlier version migrated after a backup
  ([Stable release](design/stable-release.md#stable-release-upgrade))
- ![Planned][planned] Consumable SDK: OwlEngine Conan package, `find_package(OwlEngine)` with 1–2 public dependencies,
  consumer test in CI
- ![Planned][planned] Windows SDK compatible with MSVC (clang-cl), next to MinGW
  ([Platforms](design/modding-platforms.md#platforms-msvc))
- ![Planned][planned] Owl Nest and the runners signed for Windows with a Certum open-source code-signing certificate
  ([Game export](design/game-export.md#game-export-signing))
- ![To evaluate][evaluate] Sample games from the pool, picked once the features of the release are done
  ([Pool](design/sample-games.md#sample-games-pool))
- ![Planned][planned] Fuzzers on every file loader
- ![Planned][planned] Performance budgets verified in CI on every target platform
- ![Planned][planned] Complete user documentation: tutorials, Lua and visual-scripting references
- ![Planned][planned] Test protocols and release checklists for the hardening phase
  ([Hardening](design/stable-release.md#stable-release-hardening))

## v0.8.0 -- Expected 2026-12-06

**Goal:** Ship a game everywhere and play it together: every target exported from every Owl Nest host, Android and
the Web as new platforms, networked multiplayer on the v0.7.0 deterministic simulation, mods and plugins on one
extension mechanism, the gaps of the sample-game pool closed
([game export](design/game-export.md), [modding and platforms](design/modding-platforms.md),
[networking](design/simulation-networking.md)). Critical path: the build image, the network transport, the shaders
per target, Android and the Web.

- ![Done][done] Build images, made by the maintainer: an Android image with the NDK, a Web image with emsdk and a
  headless browser ([Game export](design/game-export.md#game-export-templates))
- ![Done][done] Release pipeline: each runner built once and published as an archive, the Android, Web and arm64
  runners at night and at release, `runners.json` and the Web runner in each Owl Nest package ([Game export](design/game-export.md#game-export-templates))
- ![Done][done] Every target exported from every host: the host and Web runners in each Owl Nest, the others
  downloaded from the runner server or imported, checked against the Nest's version
  ([Game export](design/game-export.md#game-export-targets))
- ![Done][done] Shaders baked by the host for each target, a runner without shader compiler: desktop, Android and the
  Web ([Game export](design/game-export.md#game-export-shaders))
- ![Planned][planned] The APK of a game generated by Owl Nest on a Linux or Windows workstation, without the Android
  SDK nor a JDK: the Android runner bundled as a template APK, the game's assets, name and icon added, aligned and
  signed by the Nest ([Game export](design/game-export.md#game-export-targets))
- ![Planned][planned] Signing at export: Authenticode from a `.pfx` or a custom command, APK v2 / v3, debug and release
  keystores ([Game export](design/game-export.md#game-export-signing))
- ![Done][done] Exported Windows games carry their project icon in the executable, not only on the window
  ([Game export](design/game-export.md#game-export-targets))
- ![Planned][planned] Linux AppImage and Flatpak next to the portable archive, Android App Bundle (AAB) for Google Play
  ([Game export](design/game-export.md#game-export-targets))
- ![Planned][planned] Export report: size per target, baked shaders, unused assets, warnings
  ([Game export](design/game-export.md#game-export-report))
- ![Planned][planned] Smoke test of every exported target in CI, Android on a tablet at night
  ([Game export](design/game-export.md#game-export-smoke-tests))
- ![In Progress][progress] Android: window and input backend, Vulkan, signed APK
  ([Platforms](design/modding-platforms.md#platforms-android))
- ![Planned][planned] Touch screen input: taps and drags as pointer events, pinch and two-finger pan, the HUD and Owl
  Nest usable by touch ([Input](design/windowing-input.md))
- ![Done][done] Web export through a WebGPU Owl RHI backend, run in the browser from Owl Nest: the build, the tests,
  the runner, 2D and 3D rendering, the export and its local server ([Platforms](design/modding-platforms.md#platforms-web))
- ![Planned][planned] Gamepad improvements: remapping UI, haptics, joysticks and throttles with many axes
  ([Input](design/windowing-input.md))
- ![Planned][planned] Crash reporter: a local report to attach to a GitHub issue, nothing sent
  ([Crash reporter](design/crash-reporter.md))
- ![In Progress][progress] Network transport layer (reliable UDP, authoritative server, connection management): done on
  desktop, Android left ([Networking](networking.md))
- ![Done][done] Network transport in the browser: WebSocket to a native server, a Web client in every networked
  game, a desktop server taking ENet and WebSocket clients at once ([Networking](design/simulation-networking.md#networking-multiplayer))
- ![Done][done] Entity replication (replicated components, interpolation, prediction, authority)
  ([Replication](networking.md#networking-replication))
- ![Done][done] Lockstep network model on the bit-exact simulation, desync detection by world hash
  ([Lockstep](networking.md#networking-lockstep))
- ![Done][done] Fair matches: input type in the lobby, server-side lag compensation, minimum hardware profile
  ([Fair play](networking.md#networking-fair-play))
- ![Done][done] RPC system (Lua `rpc_server` / `rpc_client` / `rpc_all`, reliable and unreliable)
  ([Remote calls](networking.md#networking-rpc))
- ![Done][done] Lobby and session management ([Game session](networking.md#networking-game-session))
- ![Done][done] Network debugging tools (latency / loss simulation, stats overlay)
  ([Debugging](networking.md#networking-debugging))
- ![Done][done] Interest management: relevance areas and priorities, a client receives only what is near it
  ([Interest management](networking.md#networking-interest))
- ![Done][done] Extension packs: one format and one loader for game mods and for data and script plugins
  ([Mods and extension packs](mods.md))
- ![Done][done] Mod loading system (`.owlmod` packs, manifest, load order, asset overrides)
  ([Mods and extension packs](mods.md))
- ![Done][done] Lua mod API (custom components, game event hooks, sandboxed) ([Mods and extension packs](mods.md))
- ![Done][done] In-game mod manager (enable / disable / reorder, conflict detection) ([Mods and extension packs](mods.md))
- ![Done][done] Data and script plugins: material and script nodes, templates, game rules, shared by a project
  on every target ([Mods and extension packs](mods.md))
- ![Planned][planned] Owl Nest plugins: panels, commands, inspector drawers, document types, importers
  ([Plugins](design/modding-platforms.md#modding-plugins))
- ![Planned][planned] Every open engine gap of the sample games closed (Lua voxel table, saved procedural chunks,
  voxel edits without a captured cursor, tilemap switch, saved raycast walls, isometric zoom), and the pool audited
  against the engine ([Gaps](design/sample-games.md#sample-games-gaps))

## v0.7.0 -- 2026-11-15

**Goal:** Physics, simulation and AI — 2D and 3D physics and AI as optional modules outside `Scene`, on a
deterministic simulation with input replay and rewind; articulated bodies, gravity and time warp for worlds of any
scale; material graphs compiled to Slang and the groundwork of typed scripting
([details](design/gameplay-systems.md), [simulation](design/simulation-networking.md),
[large worlds](design/large-worlds.md), [material graphs](design/material-graphs.md)). Critical path: the
deterministic simulation and replay, Jolt, gravity fields.

- ![Done][done] Deterministic simulation (seeded streams, cross-platform world-hash test) on the fixed tick
  ([Simulation](design/simulation-networking.md#simulation-deterministic))
- ![Done][done] Bit-exact simulation on every target, always on: no FMA contraction, deterministic maths,
  deterministic Jolt physics, fixed Lua hash seed
  ([Simulation](design/simulation-networking.md#simulation-cross-platform))
- ![Done][done] Fair play: fixed tick whatever the frame rate, gameplay visibility never cut by quality settings,
  bounded field of view ([Fair play](design/simulation-networking.md#networking-fairness))
- ![Done][done] Input recording and frame-exact replay (replays double as regression tests)
  ([Replay](design/simulation-networking.md#simulation-replay))
- ![Done][done] Rewind in Play: the session simulated again from its recorded inputs
  ([Simulation](design/simulation-networking.md#simulation-rewind))
- ![Done][done] Full 2D physics on Jolt (plane-locked bodies): callbacks, queries, joints, on the shared physics
  interface
  ([Physics](design/physics-api.md))
- ![Done][done] 3D physics with Jolt on the shared physics interface, built cross-platform deterministic
  ([Physics API](design/physics-api.md))
- ![Done][done] Articulated bodies: compound bodies rebuilt at runtime, breakable joints, mass recomputed,
  sub-stepping ([Physics API](design/physics-api.md))
- ![Done][done] Gravity fields (uniform, point, spherical) in 2D and 3D, with trajectory prediction
  ([Large worlds](design/large-worlds.md))
- ![Done][done] Time warp: scaled clock, bodies handed to analytic propagation and back
  ([Large worlds](design/large-worlds.md))
- ![Done][done] Simulation level of detail: far entities on a coarse model, on a slower tick
  ([Large worlds](design/large-worlds.md))
- ![Done][done] Navigation (NavMesh generation, A*, dynamic avoidance); on top of the tile-graph pathfinding
  ([Pathfinding](pathfinding.md#pathfinding-navmesh))
- ![Done][done] Behaviour trees with a visual editor (`.owlbt`) ([AI](ai.md#ai-trees))
- ![Done][done] Steering behaviours and flocking, in 2D and in open 3D space (obstacle avoidance, formations)
  ([AI](ai.md#ai-steering))
- ![Done][done] Engine Demo: enemies demo on the AI modules ([AI](ai.md#ai-demo))
- ![Done][done] Material graphs: Slang written as a graph on the surface of the PBR materials, pre-coded and
  custom Slang nodes, material functions and parameters ([Material graphs](design/material-graphs.md))
- ![Done][done] Material graph shaders baked by the export ([Material graphs](design/material-graphs.md))
- ![Done][done] Custom Lua node in visual scripts (title, typed pins, Lua body) and annotated Lua functions as
  nodes ([Visual scripting](design/visual-scripting.md#visual-scripting-custom-lua))
- ![Done][done] Entities handed to Lua as handles (registry id) instead of 64-bit UUIDs
  ([Visual scripting](design/visual-scripting.md#visual-scripting-luau))
- ![Done][done] Luau in place of Lua 5.5: one VM, the bindings, the sandbox, the quotas and the debugger ported
- ![Done][done] Luau types checked in the Owl Nest code editor, declarations generated from the binding registry
  ([Visual scripting](design/visual-scripting.md#visual-scripting-luau))
- ![Done][done] Sample games in visual scripts: the rules of the six earlier games moved to `.owlvs`
  ([Visual scripts](design/sample-games.md#sample-games-visual-scripts))
- ![Done][done] Wolfenstein combat: guards on behaviour trees, weapons, CC0 art
  ([Wolfenstein combat](design/sample-games.md#sample-games-wolfenstein-combat))
- ![Done][done] Lunar lander sample game ([Lunar lander](design/sample-games.md#sample-games-lunar-lander))
- ![Done][done] Bridge builder sample game ([Bridge builder](design/sample-games.md#sample-games-bridge-builder))
- ![Done][done] Asteroid field sample game ([Asteroid field](design/sample-games.md#sample-games-asteroid-field))
- ![Done][done] One-room Zelda-like sample game ([Zelda-like](design/sample-games.md#sample-games-zelda-like))
- ![Done][done] Tower defence, rail shooter and point-and-click on steering and NavMesh

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
