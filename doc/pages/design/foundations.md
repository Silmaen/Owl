# Foundations (v0.3.0) {#page-design-foundations}

[TOC]

Design page of the v0.3.0 release, summarised in the [Roadmap](../roadmap.md). The references in parentheses
(`PR-xx`, `C-01`, `K-20`, …) point to the repository audit in `doc/audit/` (`90-synthese.md` for the PR plan,
`10-constats-*.md` for the findings, `40-avenir.md` for the long-term work items, `20-mesures.md` for the numbers).

## Goal

Take the big risks first (the Conan migration and the breaking dependency upgrades), then
stabilize the engine before building on it: fix every known correctness bug, put safety nets where the bugs were
(editor tests, sanitizers, tests without a window), repair the Owl RHI so Vulkan is a real reference backend, open the
architecture so each game can specialise the engine, move to public dependencies (Conan 2, ConanCenter) with fewer
public ones, and make day-to-day iteration fast. **No new gameplay feature** in this release: hot reload, autosave,
the editor command API and the Owl Nest ergonomics revamp are iteration tooling, not game features.

Order, one pull request per step: the rest of phases A and B together, then phase D, then phase C, then phase E (the
Owl Nest ergonomics revamp, on the architecture phase C leaves behind).

## Already landed

- ![Done][done] teamcity-github-bridge 1.10.0 wiring — `CodeStyle` findings become annotations on the pull
  request's diff (GNU-style diagnostics through `_diag()`), doc-only PRs skip the C++ matrix, a draft build's verdict
  is reused on ready, `[skip ci]` escape hatch, shorter Check Run names. Detail in
  [Continuous Integration](../continuous_integration.md).

## Phase 0 — Risk first: dependencies & Conan

The biggest unknowns of the release come first, so that they surface while the plan can still change: everything
after this phase builds on the new dependency chain, so the phase closes before phases A to E start. The OwlEngine
Conan package is the one exception: it moves to v1.0.0 ([Stable release](stable-release.md)).

- ![Done][done] Conan 2 migration from DepManager, ConanCenter first (see [Conan migration](conan-migration.md))
    - Local recipes for Slang, ufbx, imgui_color_text_edit, ImGuizmo 1.10, nfd-extended, the msdf-atlas-gen library
      and libmp3lame; tinyxml2, zeus and debugbreak removed
    - Every preset builds and passes its tests on Conan in CI: Linux x64 GCC / Clang (release, debug, coverage,
      clang-tidy, include check, sanitizers), Linux arm64, Windows MinGW GCC / Clang and the packages, pinned by
      `conan.lock`, shared libraries copied next to the binaries, binaries shared through the `owl-cache` server
    - Versions absent from ConanCenter (EnTT 4, Taskflow 4.1, OpenAL Soft 1.25, msdfgen 1.13, msdf-atlas-gen 1.4,
      tinyobjloader rc13): local recipes until they land there
- ![Done][done] Breaking dependency upgrades done here, not later: EnTT 4, Taskflow 4.1 and the lagging direct
  versions (G-08: OpenAL Soft 1.25.2, msdfgen 1.13, msdf-atlas-gen 1.4, tinyobjloader rc13); yaml-cpp 0.9 was
  already the Conan version
- ![Done][done] Install tree fixed: headers under `include/`, no build flag imposed on consumers, preset prefix
  honoured (PR-08: G-01, G-02, G-06, A-19, G-19)
- ![Planned][planned] OwlEngine Conan package (PR-09: F-06, G-03) — moved to v1.0.0. `conan create .` already
  packages `owlengine` (shared) and `test_package/` builds on `find_package(OwlEngine)`; the CI runs neither
- ![Done][done] DepManager, `depmanager.yml`, `cmake/Depmanager.cmake` and `owl_engine.py` removed; until the
  v1.0.0 package, consumers (OwlDrone) use the packaged archive: the CPack install tree and `find_package(OwlEngine)`

Exit of the phase: every preset (Linux GCC / Clang, arm64, MinGW) builds and passes the tests on Conan alone, with
DepManager removed.

## Phase A — Correctness

Every fix lands with its regression test.

- ![Done][done] Deferred entity destruction; `destroy_entity(self)` no longer frees the running Lua VM
  (PR-01: C-01, D-01)
- ![Done][done] Undo restores entities in place; dirty flag driven by a generation counter, so closing never
  loses edits silently (PR-03: E-01, E-02, C-05, E-05)
- ![Done][done] Unbounded hierarchy depth, CPU world transforms sent to the GPU; `setParent` keeps the world
  position at any depth (PR-04: P-01, C-18, P-03, P-04, B-12)
- ![Done][done] Runtime scene robustness — Play isolated from the editor voxel world, no segfault on a dangling
  `EntityLink`, SceneFlow double `addComponent`, hierarchy cycles and duplicate UUIDs rejected at load, failed
  teleport recovers (PR-05: C-03, C-04, E-08, C-06, C-17, E-12)
- ![Done][done] Voxel meshed in the scene render pipeline, so voxel scenes show in the exported game
  (PR-06: D-03)
- ![Done][done] `.owlpack` hardening — validated paths and sizes, exceptions caught, libFuzzer target behind
  `OWL_FUZZING`, run nightly by the `Fuzzing` CI job (PR-07: D-02, D-28)
- ![Done][done] Prefab update / revert in place, with override detection (PR-10: C-02, E-11)
- ![Done][done] CI secrets kept out of argv and logs, `api.py` replaced by an in-repository upload client, secret
  scan in `CodeStyle` (PR-12: H-01, H-02, G-09, H-12)
- ![Done][done] Physics, sound and script lifecycle on EnTT hooks — no ghost collider, `on_destroy` always
  called (PR-13: C-08, D-04)
- ![Done][done] Lua hardening — text-only chunks, time / memory quotas, exception trampoline
  (PR-14: D-06, D-16)
- ![Done][done] Format version and atomic writes — every engine file carries a `FormatVersion` with a migration
  chain, newer files are refused, and writes go through a temporary file renamed in place (PR-25: C-10, C-13)
- ![Done][done] `on_collision` implemented — the callback documented since v0.1 is fed by Box2D contact events,
  with the entity it collided with (D-07, I-02); the rest of the 2D physics API is v0.5.0, see
  [Physics API](physics-api.md)
- ![Done][done] Game export works end to end — packaging, pack, runner, assets, voxel in the runner (D-03),
  window icon; an automated test exports the sample project and runs it headless. See [Game export](game-export.md)
- ![Done][done] Full Wayland support — Owl icon, editor multi-window (detached ImGui windows), X11 kept as an
  option. See [Windowing and input](windowing-input.md)
- ![Done][done] OpenGL backend fixed and tested as the compatibility backend: honest version check, asynchronous
  picking (B-16), mipmaps generated and Nearest filtering honoured on both backends (B-18)

## Phase B — Safety nets

- ![Done][done] Engine benchmark harness `bench/` behind `OWL_BENCHMARK` (scene, frame, Renderer2D, YAML, voxel,
  Lua, physics, Slang, startup); the real runner's cold start in `OwlRunner --frame-bench` (`startup_ms`)
- ![Done][done] `owlnest_tests` category: undo, commands, snapshots; the empty round-trip assertions fixed
  (PR-02: E-03, F-02, C-07, P-14)
- ![Done][done] Sanitizers that fail the build (ASan, UBSan, TSan), `--gtest_shuffle`, LSan job folded into
  ASan (PR-11: F-03, F-05, F-12, H-05)
- ![Done][done] Tracy behind `OWL_PROFILE_*`, memory tracker off in Debug timings, client logs on the client
  logger (PR-16: D-12, D-11, D-23, A-17); see [Profiling](../profiling.md)
    - `OWL_PROFILER=none|tracy|chrome` (default `none`), CPU zones, frame marks, thread names (Taskflow workers
      included), allocations with the memory tracker, one GPU zone per OpenGL frame and per Vulkan batch
    - Logs: disabled levels evaluate nothing, `OWL_LOG_LEVEL` compiled level, flush on warnings only
    - Left for later: per-pass GPU zones (`OWL_PROFILE_GPU_SCOPE`, with the Owl RHI), Lua zones, memory usage by
      asset type, entity / component counts, `tracy-capture` in the build image
- ![Done][done] Runner `--frame-bench` mode with GPU timestamps — first OpenGL vs Vulkan numbers
  (PR-17: 20-mesures §7, B-01; baseline in `doc/audit/20-mesures.md` §8)
- ![Done][done] Image-comparison render tests on lavapipe (Vulkan) and llvmpipe (OpenGL): six reference scenes per
  backend, Vulkan validation clean on the sample, OpenGL GLSL fallback for drivers without `GL_ARB_gl_spirv`
  (PR-18: F-01, B-06, B-20)
- ![Done][done] Tests without a window: headless runner driven by scripted inputs (load scene, play N frames,
  assert on the world)
- ![Done][done] Diff-scoped clang-tidy: a `ClangTidy` action driven by `compile_commands.json`, analysing on a
  pull request only the `.cpp` files the diff can affect (include closure from `ninja -t deps`), everything elsewhere
  or when in doubt, one job per available core by default (H-03)
- ![Done][done] ClangTidy multi-process with the static analyzer on the tests too, coverage gate, dead `NOLINT`
  check (PR-19: H-03, F-07, F-04, F-08, F-09, H-07); cognitive-complexity threshold kept at 75
- ![Done][done] Module dependency direction checked in CodeStyle; the 10-module cycle broken
  (PR-20: A-01, A-13)
- ![Done][done] Tests for the CI tooling itself: pytest, ruff and mypy in CodeStyle (PR-34: H-04, H-08)
- ![Done][done] Benchmarks in CI with a regression threshold against a stored baseline: compiled on every pull
  request, run nightly on `main` (`Bench` action, +15 % on the median, suspects measured twice)

### Packages without a rebuild

![Done][done] Each platform used to build its two packages (Engine SDK, Owl Nest) in their own trees, apart from the
tested builds, with `OWL_DEVELOPMENT` / `OWL_PACKAGE_ENGINE` telling a package from a development build. Now:

- the asset lookup is a run-time decision: `assets/` next to the working directory, then `engine_assets/` and the
  application's assets found above it (a development tree); no build switch is left;
- every tree runs `cpack` and writes one archive per CPack component, `OwlEngine-…` (`lib/`, `include/`, `assets/`,
  CMake config) and `OwlNest-…` (`bin/<platform>_<arch>/`: Nest, the runner, shared libraries, assets), copying
  only what ships from a tree that also holds tests and their outputs;
- on `main`, the Clang builds package the release tree they have just tested (`Package Release`); the nightly x64
  package configurations publish those archives and the documentation without building; arm64, which has no tested
  release tree, builds `package-linux` (the release build without tests).

The SDK is now Release only (it was Debug + Release): a Debug consumer links the Release library through CMake's
imported-configuration fallback.

## Phase D — Usability & dependency reduction

- ![Done][done] Fewer public dependencies — only EnTT, plus imgui through the optional `Owl::Gui` target
  (see [Conan migration](conan-migration.md))
- ![Done][done] Configure without network or Doxygen, CMake clean-up (PR-26: G-05, G-09, G-13, G-14, G-15,
  I-09, G-08)
- ![Planned][planned] Hot reload for iteration (editor and development runner)
    - Assets: textures, scenes, tilesets reloaded when the file changes on disk
    - Slang shaders recompiled and swapped live
    - Lua scripts reloaded, with properties preserved
    - Hot reload of a C++ game module is a separate v0.7.0 evaluation, see [Content pipeline](content-pipeline.md)
- ![Planned][planned] Autosave and crash recovery — periodic autosave of dirty documents, recovery offered at the next
  launch
- ![Planned][planned] Session restore (persisted open tabs)
    - Remember the list of open documents between launches (per project)
    - Restore active tab, selection, and viewport layout
    - Stored in `EditorSettings` or `owl_project.yml`
- ![Planned][planned] Actionable error messages — load, script and pack errors name the file, the entity and the fix,
  in the editor log and the runner
- ![Planned][planned] Project templates (empty 2D, raycast, voxel, mixed-style) in the new-project dialogue
- ![Planned][planned] Documentation faithful to the code — Lua, renderer, README, guides (PR-15: I-01, I-02, I-03,
  I-04, I-05, D-07, B-16, B-18)
- ![Planned][planned] Identifiers cited in `doc/pages` checked in CI (PR-39: I-01, I-09)
- ![Planned][planned] Proportionate Doxygen — public API documented, no boilerplate `@brief` on trivial members
  (PR-38: I-06, I-07)

## Phase C — Owl RHI & architecture

The Vulkan foundation is the second big risk of the release: it starts first in this phase, as soon as the frame
bench (PR-17) and the image tests (PR-18) of phase B are in place, and runs alongside the rest of phase C.

- ![Done][done] Vulkan foundation — real frames in flight, no `vkQueueWaitIdle` on the hot path, transitions
  inside the frame, correct `loadOp`, swapchain image used only after acquisition (PR-28: B-01, B-02, B-04, B-19)
- ![Done][done] Per-frame uniform ring and VMA sub-allocation (PR-29: B-03, B-11, B-23)
- ![Done][done] Owl RHI named and documented; Vulkan reference, OpenGL frozen fallback, Null for tests;
  pipeline objects and explicit bindings instead of global state and call-order conventions (B-07). See
  [Owl RHI](owl-rhi.md)
- ![Done][done] ABI — YAML out of the public API, hidden visibility by default, third-party symbols not
  exported (PR-27: A-03, G-07, A-10); the header weight (A-12) stays to watch, after a build-time measurement
- ![Planned][planned] Engine context and one world per scene (PR-33: A-04, D-15, A-08, A-07, F-05)
- ![Planned][planned] Phased systems; gameplay moved out of `Scene` (PR-36: A-02, C-14)
- ![Planned][planned] Open component registry, after the EnTT 4 upgrade (PR-37: A-05, A-18)
- ![Planned][planned] Entity references by UUID, remapped on duplication (PR-35: C-12, C-04)
- ![Planned][planned] `EditorLayer` split (packager, ribbon, project opening) (PR-32: E-07, E-12)
- ![Done][done] Optional CMake modules so each game can specialise the engine: `OWL_MODULE_RENDER`, `_PHYSICS`,
  `_AUDIO`, `_SCRIPT`, `_GUI` (and Conan options) around an always-built core; a module off drops its third parties
  and keeps its public API (Null backend or no-op); `linux-clang-minimal` (every module off) runs in CI. See
  [Building](../building.md#engine-modules)
    - Modules: core, render, physics, audio, script, Gui (`Owl::Gui`, the only one pulling imgui)
    - Extension points documented ([Architecture](../architecture.md#extension-points)): renderer-stack layers and
      application layers today; the open component registry (PR-37) and replaceable phased systems (PR-36) come with
      their PRs
- ![Planned][planned] Typed Lua binding registry — one declaration per binding gives the Lua function, its
  documentation and (later) its visual-scripting node; documented-but-missing bindings either land or leave the docs
  (D-07, D-26). See [Visual scripting](visual-scripting.md)
- ![Planned][planned] Editor command API — every editor mutation goes through a command executed by `UndoManager`;
  the same API drives the headless runner and the tests. See [MCP server](mcp-server.md)
- ![Done][done] Dead code removed or wired: unused `parallelForEach`, shared `LuaEngine`, single-use
  `IFactory` (D-24, D-26, A-18)
- ![To evaluate][evaluate] SDL3 for windowing, input, dialogues and audio (possibly SDL GPU as an Owl RHI backend);
  the GLFW limits under Wayland are the concrete argument. See [Windowing and input](windowing-input.md)

## Phase E — Owl Nest ergonomics revamp

A much more ergonomic editor, designed together with the maintainer, with no new feature: the existing tools
reorganised, made discoverable and consistent. See [Owl Nest UI](nest-ui.md).

- ![Planned][planned] Ergonomics review — main workflows walked through, pain points listed, target layout agreed
- ![Planned][planned] Interaction basics — tooltips, context menus, consistent drag & drop, text scale and DPI
- ![Planned][planned] Visual overhaul — theme, icon set, asset thumbnails, style guide, layout presets, cleaner ribbon
- ![In Progress][progress] Editor camera controls overhaul — presets, interactive view-cube, sensitivity

## Performance work

- ![Planned][planned] Dense per-frame transforms, direct TRS composition (PR-21: P-02, P-06, C-09)
- ![Done][done] Fixed-step physics, multi-threaded Box2D solver on Taskflow (PR-22: D-05, P-12, D-14)
- ![Done][done] Inspector serializes only the edited component, only on edit (PR-23: E-04)
- ![Done][done] Voxel meshing on workers with a per-frame budget, neighbours invalidated; streaming frame peak
  14.3 → 0.35 ms (PR-24: D-08, P-09, B-15)
- ![Planned][planned] Renderer2D per-frame transients, 2D sort order, UTF-8 text (PR-30: B-09, B-10, D-18)
- ![Planned][planned] Persistent, chunked and culled tilemap (PR-31: B-13)
- ![Planned][planned] Faster scene loading (YAML path optimised or replaced, prefab instantiation without a YAML
  round-trip per entity)
- ![Planned][planned] Shaders precompiled at pack time, so the runner never compiles Slang at startup

## Performance targets

Measured on the `bench/` harness, Null backend unless stated (source: `doc/audit/20-mesures.md`).

| Indicator                               | Measured today                                   | v0.3.0 target                         |
|-----------------------------------------|--------------------------------------------------|---------------------------------------|
| CPU frame, 10 000 sprites (`flat10000`) | 2.52 ms                                          | < 0.5 ms                              |
| Renderer2D cost per quad                | 10.5 ns (`worldIndex`), 86 ns (transient)        | < 10 ns on every path                 |
| GPU queue drains per frame (Vulkan)     | 2 to 4 in the runner, ≥ 10 in the editor         | 0                                     |
| Runner frame, Vulkan vs OpenGL (NVIDIA) | 0.6–2.0 ms vs 0.35–0.76 ms CPU (frame bench)     | Vulkan ≤ OpenGL                       |
| Scene load per entity                   | 134 µs (10 000 entities: 1.34 s)                 | < 10 µs                               |
| Box2D step, 5 000 bodies in contact     | 4.88 ms (single thread)                          | < 1.5 ms (multi-thread, fixed step)   |
| Voxel meshing                           | on workers, streaming frame peak 0.35 ms (PR-24) | off the main thread, per-frame budget |
| Cold start (real runner, GPU backend)   | ~400 ms to first frame on lavapipe / llvmpipe    | measured, then shaders precompiled    |

## Exit criteria

The release ships when these hold, whatever the date:

- Every preset builds on Conan 2, DepManager removed
- Zero known correctness bug
- Vulkan validation clean on NVIDIA, Intel and lavapipe
- At least one image-comparison render test per backend (Vulkan, OpenGL)
- UBSan and ASan blocking in CI
- Editor coverage above 50 %
- Exported sample project runs headless in CI
- Owl Nest ergonomics revamp shipped, with no new feature
- Performance targets above reached and protected by the CI regression threshold

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square

[progress]: https://img.shields.io/badge/-In_Progress-d29922?style=flat-square

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
