# Changelog {#page-changelog}

[TOC]

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] — 0.3.0 (in development)

### Added

- Benchmarks: first baseline `bench/baseline/linux-bench.json` (141 benchmarks, 112 metrics, median of 5 runs), the nightly bench pinned to the agent that measured it.
- CodeStyle `test-assertions` sub-check: every gtest test asserts something; the 26 that only ran code now check its effect (or say `EXPECT_NO_THROW`), e.g. a trigger timer is seen firing and stopping.
- clang-tidy and the static analyzer analyse the tests too (`test/.clang-tidy` relaxes only what a test is right to do); the 224 findings they raised are fixed.
- Coverage gate: the `Coverage` action publishes the line and branch coverage to TeamCity, and the Linux Clang build fails when the line coverage drops more than one point below its last successful build.
- CodeStyle `python` sub-check: `ruff check`, `ruff format --check`, `mypy` and the `ci/tests` pytest suite (never run in CI before) on the CI code (configured in `pyproject.toml`; `black`, never run, removed).
- `OwlRunner --scenario <file.owltest>`: scripted headless runs (frames, held inputs, expectations on entities and the game state), with four sample scenarios run by CTest (label `scenario`).
- `OwlRunner --frame-bench` measures the cold start (`startup_ms`: engine ready, first frame), so start-up time has a number on a real backend.
- Wayland smoke test (`owl_wayland_smoke`, label `wayland`): the runner presents frames on a headless weston with Vulkan and OpenGL; skipped where weston is missing.
- Nightly fuzzing: `linux-fuzz` preset (libFuzzer + AddressSanitizer) and `Fuzz` CI action running every `owl_*_fuzzer` for five minutes, failing inputs published by the `Fuzzing` TeamCity configuration.
- Benchmarks in CI: `owl_bench` compiled on every pull request (`linux-clang-debug`), run nightly on `main` by the `Bench` action against `bench/baseline/linux-bench.json`; a median slower by more than 15 % twice fails the build.
- Optional Conan binary cache (`OWL_CONAN_CACHE_URL`, remote `owl-cache`): read before ConanCenter, filled by every CI build with the binaries of Owl's graph it built, even when the install fails midway, skipped with a warning when unreachable.
- `PR Ready` composite TeamCity configuration: red when any ready-PR configuration is red, the single check to require on `main`.
- Conan profiles for Windows MinGW (`windows-clang`, `windows-gcc`), DLLs deployed next to the binaries, provider selectable through `OWL_DEPENDENCY_PROVIDER` (environment and TeamCity parameter).
- `Clang Static Analyzer` CI configuration (`ClangTidy -- --tool=analyzer`); both analyses close the TeamCity chain and are the checks to require on `main`.
- `docker/run.sh` runs any build, test or CI command in the Docker build image (`--gui`, `--perf`).
- Versioned file formats: scenes, prefabs, saves, tilesets, tilemaps, animation clips, voxel structures, node graphs, settings and `owl_project.yml` carry a `FormatVersion`; files without it load as version 1, newer files are refused with a clear error, older ones go through a registered migration chain (`core::DocumentFormat`).
- `platform::writeFileAtomic` (temporary file, `fsync`, atomic rename) now writes all those files: a crash or a full disk keeps the previous file instead of truncating it.
- `bench/` engine benchmark harness (scene, serialization, Renderer2D, voxel, Lua, physics, Slang), built with `-DOWL_BENCHMARK=ON`.
- `ClangTidy` CI action: on a pull request, analyses only the touched `.cpp` files plus every `.cpp` whose include closure (`ninja -t deps`) holds a touched header; elsewhere, everything.
- `-DOWL_DEPENDENCY_PROVIDER=conan` builds `linux-clang-release` with Conan 2 and ConanCenter (`conanfile.py`, six local recipes in `conan/recipes/`), without the DepManager server.
- Every Linux preset builds on Conan, pinned by `conan.lock`, and `conan create .` packages OwlEngine, checked by `test_package/` (`find_package(OwlEngine)`).
- `-DOWL_PROFILER=tracy` puts Tracy (ConanCenter, on demand) behind the `OWL_PROFILE_*` macros: CPU zones, frame marks, named Taskflow workers, OpenGL and Vulkan GPU zones, tracked allocations ([Profiling](profiling.md)).
- `-DOWL_LOG_LEVEL=<level>` compiles out the log macros below a level.
- `OwlRunner --frame-bench`: deterministic frame benchmark with CPU phase timings, GPU timestamps (Vulkan, OpenGL) and Vulkan queue-drain counters, JSON report; RHI gains `GpuFrameTiming`, `RenderCounters` and a vsync request.
- Image tests (`owl_render_tests`, CTest label `render`): six reference scenes rendered offscreen on lavapipe and llvmpipe, compared to versioned PNGs with a per-pixel tolerance (PR-18).
- `OwlRunner --frame-bench --capture <png>` renders into an offscreen framebuffer and writes the last frame; `Framebuffer::readColorAttachment` and `renderer::writeImagePng` back it.
- OpenGL GLSL fallback: without `GL_ARB_gl_spirv` (llvmpipe, GL 4.5 drivers) the Slang SPIR-V is translated to GLSL 4.50 by spirv-cross, `OWL_OPENGL_SHADERS=glsl|spirv` forces the choice.
- `owlnest_tests` category: undo manager, every scene command family, node-graph commands and entity snapshots, linked through the new `OwlNestCore` editor library.
- `GameExporter` (`data::assets::pack`): one export path shared by *Pack Game*, `OwlNest --export <project> <output>` and the tests.
- Runner `--headless` (Null window, renderer and sound) and `--smoke-test [frames]` (plays every packed scene, non-zero exit code on any error log).
- `export_tests` (CTest label `export`): exports `sample_project`, moves the game out of its export folder and smoke-tests every scene headless.
- `Application::setExitCode()`: `main` returns it, and 1 when the application ended in the `Error` state.

### Changed

- `find_package(OwlEngine)` exposes EnTT as its only public dependency: yaml-cpp left the public headers (render-stack `defaultConfig`, `overrides` and `RenderLayer::applyConfig` carry YAML text), and imgui comes with the optional `Owl::Gui` target (`COMPONENTS Gui`, `<owlgui.h>`), `<owl.h>` no longer including `gui/utils.h`; `test_package` checks both targets.
- A configure needs neither the network nor Doxygen: `poetry sync` runs only when `poetry.lock` changed, help badges come from their cache (`OWL_HELP_FETCH_BADGES=ON` downloads), the `documentation` target exists when Doxygen is found (`OWL_ENABLE_DOCUMENTATION=ON` requires it) and its `Doxyfile` is generated in the build tree with absolute paths and the venv's Python.
- Packages without a rebuild: `cpack` writes the `OwlEngine` and `OwlNest` archives (CPack components) from the release tree the Clang builds test on `main`, the nightly x64 package jobs only publish them, and assets are located at run time (`OWL_DEVELOPMENT`, `OWL_PACKAGE_ENGINE`, `OWL_PACKAGING` and the `package-engine-*` / `package-app-nest-*` presets removed, `package-linux` for arm64).
- Engine modules are layered (`core` up to `gui`) and the public headers follow it, checked by the CodeStyle `module-deps` sub-check: the 10-module include cycle is gone. `PhysicsSettings` and `PhysicsSnapshot` moved to `scene` (aliases kept in `physics`), `MeshLoader` to `data::geometry` (alias kept in `data`), and `GameExporterSettings::rendererStack` became `rendererStackYaml`; `input::Input::init()` without argument picks GLFW.
- Tests: the per-binary ctest timeout drops from 1 h to 10 min (`OWL_TEST_TIMEOUT`, 1 h on the emulated arm64), so a hung binary no longer stalls a build for hours.
- CI: the emulated arm64 nightly builds Clang only, on a `linux-emulated` preset without coverage, benchmarks or image tests, so it fits its time limit.
- EnTT 4.0.0 (C++20), Taskflow 4.1.0, OpenAL Soft 1.25.2, msdfgen 1.13, msdf-atlas-gen 1.4 and tinyobjloader rc13, through local Conan recipes until ConanCenter publishes them.
- CI on teamcity-github-bridge 1.11.0: PR Ready keeps a fixed check name (`checkName`), pull requests get labels by changed paths and are assigned to their author.
- TeamCity: Include Check and PR Ready move to the root beside Code Style (GitHub checks `Include Check` and `PR Ready`).
- CI: Windows builds compute the coverage on `main` only, no longer on pull requests.
- CI on Conan everywhere (the TeamCity `OWL_DEPENDENCY_PROVIDER` default) and on a single Ubuntu 26.04 image (`builder-ubuntu2604`: GCC 15, Clang 22) for every Linux preset and `docker/run.sh`.
- CI: release build and tests on `main` only; arm64 builds and every package nightly (they held the agents for hours and packages publish to the site).
- CI in two levels: Code Style and Include Check in parallel, then every build, sanitizer and analysis after Code Style only.
- CI: `Experiment/*` pull requests run the fast subset only; secrets reach `ci_action.py` through `env.*` parameters; the CI flow is documented case by case (`main`, draft, ready, experiment, doc-only).
- TeamCity DSL laid out like EvenementLoto's (`common/`, `build/`, `quality/`, `packaging/`), chain Code Style → builds → sanitizers → analyses → packages; PR builds named by their real branch; configs version 2026.2.
- TeamCity follows the EvenementLoto model: findings annotated on the diff by default, PRs built from their `Feature/*` / `Experiment/*` head branch, packages never on a PR, no `/ci full` comment.
- Explicit windowing platform (`OWL_WINDOW_PLATFORM` / `windowPlatform` in `config.yml`: `auto`, `wayland`, `x11`), with fallback to `auto` when the requested one fails.
- Wayland `app_id` / X11 `WM_CLASS` per application and a hidden user desktop entry (`platform::installDesktopEntry`) so Wayland compositors show the Owl icon.
- `SIGINT` / `SIGTERM` close the application cleanly; `Window::getPresentedFrames()` and an exit log report the presented frame count.
- `docker/run.sh --gpu=intel|nvidia` (PRIME offload) and `--platform=wayland|x11`, with a warning when the desktop session is locked.
- GLFW `FEATURE_UNAVAILABLE` errors (window position / icon under Wayland) logged at trace level instead of error.
- The memory tracker is no longer on by default in Debug (it doubled the live allocations and skewed Debug timings): opt in with `OWL_ENABLE_MEMORY_TRACKER`.
- A log message below the verbosity no longer formats nor evaluates its arguments, and the log file is flushed on warnings only.
- Voxel chunks are meshed on the task workers from an immutable `ChunkNeighborhood` copy and uploaded under a per-frame budget (`VoxelMeshingConfig`): the streaming frame peak drops from 14.3 ms to 0.35 ms on the main thread (PR-24).
- Roadmap rethought toward 1.0.0: v0.3.0 Foundations, renumbered releases, three reading levels (`ROADMAP.md`, `doc/pages/roadmap.md`, `doc/pages/design/`).
- Changelog split the same way: one line per release in `CHANGELOG.md`, details in `doc/pages/changelog.md`.
- clang-tidy decoupled from the compiler: `CMAKE_CXX_CLANG_TIDY` unset, the analysis is a `ClangTidy` step on `Build/Quality/Clang-Tidy` driven by `compile_commands.json`.
- teamcity-github-bridge 1.10.0 wiring: findings pinned to the PR diff as Check Run annotations, doc-only PRs skip the C++ matrix, `[skip ci]` phrase, `main` left to the VCS trigger.
- `Scene::getWorldsBuffer()` replaced by `Scene::getWorldMatrices()`, `Renderer2D::setSceneWorldsBuffer()` by `Renderer2D::setSceneWorlds()`; `renderer::utils::WorldTransformPass` removed.
- Physics runs at a fixed step (60 Hz by default, per-scene `Physics:` settings edited in *Scene Settings*) with an accumulator, a bound on steps per frame and interpolated transforms; the result no longer depends on the frame rate and `on_collision` still fires once per pair and frame.
- Performance: the Box2D solver runs multi-threaded on a dedicated Taskflow executor (`workerCount`, automatic above 2 000 dynamic bodies), 5 000 stacked boxes going from 5.1 to 3.2 ms per step with 4 workers.
- Sanitizers now fail the build on their first report (`-fno-sanitize-recover=all`, `halt_on_error=1` set by ctest), sanitizer presets run the tests with `--gtest_shuffle`, and the UB job no longer captures a stack trace per allocation (its tests went from about 30 min to under 10 s).
- `SceneSerializer::deserialize`, `deserializeFromBuffer` and `applyParsed` return a `SceneLoadResult` (`owl::expected<void, SceneLoadError>`) instead of `bool`.

### Deprecated

- `OWL_ENABLE_PROFILING`, replaced by `OWL_PROFILER=chrome`.
- `OWL_FORCE_X11=1`, replaced by `OWL_WINDOW_PLATFORM=x11` (still honoured, with a warning).

### Removed

- Dead CMake: the no-op `FORCE_RELEASE` flag (`THIRD_PARTY_RELEASE`), the unused `print_target_properties` and `dump_cmake_variables`, the orphan `cmake/Python.cmake`.
- 97 dead `NOLINT` check names (checks `.clang-tidy` does not enable), and the CodeStyle `nolint` sub-check that keeps them out.
- DepManager: `depmanager.yml`, `cmake/Depmanager.cmake`, `owl_engine.py`, the `ConfigureRemote` CI action and the *Define Remote* TeamCity step; Conan 2 is the only provider (`OWL_DEPENDENCY_PROVIDER` is gone) and other projects take OwlEngine from the packaged archive.
- Unused `tinyxml2`, `zeus` and `debugbreak` dependencies (`OWL_DEBUG_BREAK()` in `core/Assert.h` replaces `debug_break()`).
- LeakSanitizer preset, option and TeamCity job: on Linux ASan already reports leaks.

### Fixed

- `Matrix::norm()` is the Frobenius norm: it summed `a_ij * a_ji` (wrong for any non-symmetric matrix) and read out of range on a non-square one.
- The Lua sandbox bytecode test loads its whole fake chunk: the literal was cut at its embedded NUL.
- CodeStyle now checks `test/` and `source/owlnest/runner` (it pointed at a missing `source/owlrunner`, skipped in silence; a missing root now fails): 48 test file headers, typos and formatting fixed.
- CI build logs: only compiler, linker and Ninja errors show as errors in TeamCity, warnings as warnings (every compiler line was red).
- CI: the native `DefineTeamCityVariables` step runs on a host Python older than 3.12 again, and the TeamCity step ids say what they build (`Build_Preset`, `Build_Release_Main`).
- MinGW Release: the physics tests link again, passing the exported `PhysicsSettings` limits by value (an odr-use of a `static constexpr` member of an `OWL_API` class needs an import MinGW never emits).
- CI: build artifacts leave out the test executables, keeping the Windows `BuildArtefact.zip` under the server's 300 MB limit.
- Windows: test binaries and `OwlRunner` exit again: the Box2D solver pool is released with the physics world and the Lua watchdog is never destroyed, so no static destructor waits for threads Windows already killed.
- Windows: the frame bench runner test quotes its whole command line, which `cmd /c` otherwise mangles.
- OpenGL picking no longer stalls the pipeline each hovered frame: `readPixel` reads through a fenced pixel pack buffer and returns the latest completed value; the viewport ignores an entity destroyed since.
- Textures: `generateMips` is honoured on both backends (OpenGL `glGenerateTextureMipmap`, Vulkan blit chain) and now defaults to `false`; `Nearest` filtering applies on Vulkan too (the raycast walls rendered blurred there), and `Linear` magnifies linearly on OpenGL as on Vulkan.
- OpenGL textures: `R8` allocates a valid `GL_R8` storage (was `GL_RED_INTEGER`) and `Rgba32F` uploads floats.
- GLFW no longer crashes at init on a Wayland compositor without input devices (no `wl_seat`, e.g. headless weston).
- Wayland works again on Linux: GLFW comes from a local recipe building both backends against the system Wayland (ConanCenter's built X11 only, and with Wayland on shipped a libwayland that hid the system one from the GPU drivers).
- `importSharedLibs.py` copies a library next to the binaries again when its package changed, instead of keeping the first copy forever (a stale X11-only `libglfw.so.3`, an old `libopenal.so.1`).
- Linux binaries link with `--as-needed`: they no longer require every `libxcb-*` that Conan's `xorg/system` lists, so Owl Nest and exported games start on a desktop missing an unused one (`libxcb-ewmh2`, `libxcb-dri2-0`); the OpenGL backend keeps the GL library GLFW loads resident, so its Mesa driver is not unloaded before LeakSanitizer runs.
- TSan builds: the Lua time quota fires again, the watchdog setting the hook from its own thread because TSan holds back the interrupt signal.
- TSan: the Vulkan image tests no longer fail on races inside lavapipe and the validation layer, suppressed by library in `test/tsan.supp`.
- Windows: packed assets keep `/` in their pack paths (`AssetScanner` wrote `scenes\level.owl`), so an exported game finds its scenes, fonts and textures.
- Image tests cap llvmpipe at SSE4.1 with Mesa's shader cache off, so a CI agent without AVX no longer fails the tilemap capture.
- `OwlRunner --frame-bench` links on MinGW again: its report no longer goes through `std::println`, whose console path needs `libstdc++exp`.
- Runtime lifecycle on EnTT hooks: removing a `PhysicBody`, `Tilemap`, raycast door or pushwall in Play destroys its Box2D body (no ghost collider), a `PhysicBody` added or duplicated in Play gets a body of its own, removing a `LuaScript` calls `on_destroy` and removing a `SoundSource` stops it; `destroyEntity` and `destroyEntityWithChildren` release scripts, bodies and sounds too (PR-13).
- Vulkan: Owl Nest text and images render again with ImGui 1.92.9, whose backend expects sampled-image texture sets and more descriptor sets than the ImGui pool allowed.
- CI: the engine package builds its documentation again (the Documentation step required a release preset, which a package preset has not, so Publish Documentation found nothing).
- Hierarchies deeper than 64 levels: world transforms, inherited visibility and `setParent` cycle checks walk the whole chain, so `setParent` no longer corrupts a deep entity's position; world matrices are composed once on the CPU and uploaded by `Renderer2D` (the `world_transform` compute pass, which recomposed them on the GPU with the same 64-level cap, is removed).
- Pack extraction and the texture / font pack caches now detect and log failed writes instead of failing silently.
- `docker/run.sh` works from a git worktree: it mounts the main git directory and finds the shared `fake_home`.
- TeamCity: `triggerOnPrDraft` is written explicitly on every configuration (the plugin defaults to `true`, so draft PRs ran the whole matrix).
- Conan on Windows: host settings and options go through a generated profile (cmd.exe split `&:shared=…` at `&`), and configure stops when `conan install` produced no toolchain.
- `ClangTidy` analyses only the repository's own translation units: the third-party sources a Conan build compiles (imgui backends, `imgui_stdlib.cpp`) no longer fail the gate.
- `run_command` reads stdout and stderr concurrently: on Windows it read them one after the other, so the Conan install showed nothing and hung once its stderr pipe filled.
- Conan package builds run their Python generators with the interpreter that runs Conan (glad needs jinja2, missing from MSYS2's Python), and `owl-local` is registered again only when missing, no longer wiping its recipe cache at every configure.
- `libmp3lame` local recipe: MinGW Clang builds it with autotools (ConanCenter's recipe ran `nmake`, taking it for clang-cl).
- Conan profiles force one version per build tool (`cmake/4.4.3`, `pkgconf/2.5.1` through `[replace_tool_requires]`, `CMAKE_POLICY_VERSION_MINIMUM=3.5` for old upstream projects), instead of one per recipe range.
- Windows test binaries copy the gtest DLLs only when gtest is shared (DepManager); Conan links it statically.
- `Log` names its file sink with `SPDLOG_FILENAME_T`, so Windows builds against spdlog with or without wide filenames (Conan's has none).
- `windows-gcc` Conan profile pins MinGW's `gcc.exe` by full path: in the MSYS2 bash of autotools builds a bare `gcc` was MSYS's POSIX compiler (mpg123 failed); ImGuizmo and plutovg are shared, and the imgui backends and imgui_stdlib compiled into the engine drop their API macro, as a shared imgui declares them dllimport on Windows.
- CI: Doxygen moves to Linux x64 Clang, which builds the release on every run for it (release tests stay on `main`); Windows x64 Clang drops its artifact-size gate, wrong whenever a run skips the release.
- `conan.lock` pins the current `slang` recipe revision (a fresh Conan cache, as on a new agent, could not resolve the old one).
- Windows CI: dependencies from Conan, built by the agent's GCC 16 toolchain (the prebuilt DepManager packages broke the GCC test runtime and the Clang link).
- TeamCity: every `ci_action.py` step runs in the build image again (lost in the DSL relayout: `poetry: not found`).
- Windows build with a recent libstdc++ (MSYS2): every file now includes the standard headers it uses, checked by the `std-includes` Code Style audit and the `linux-include-check` strict-libc++ build (`OWL_INCLUDE_CHECK`).
- `ClangTidy` runs one job per available core by default instead of a single process (`--jobs=N` still overrides).
- Doxygen on Windows: `doc/fix_md_links.py` writes its output as UTF-8, the locale codepage could not encode the doc pages' `✅` / `❌`.
- Client log macros with arguments (`OWL_INFO("… {}", x)`) went to the engine logger instead of `APP`.
- `OWL_ENABLE_PROFILING` never enabled the Chrome profiler (the header tested another macro).
- Installed OwlEngine package: headers under `include/` again, no `-Werror -Weverything` imposed on consumers, preset install prefix honoured.
- `PhysicCommand` no longer keeps a dangling `Scene*` once its scene is destroyed (`~Scene` releases the world through `PhysicCommand::releaseScene`), and the core, physics and renderer tests no longer depend on their order.
- Memory tracker with `OWL_ENABLE_STACKTRACE`: an `AllocationInfo` built outside the tracker no longer deadlocks on cpptrace's mutex.
- OpenGL under Wayland no longer freezes after the first frame when the window is not shown: vsync is paced by the engine instead of blocking in `eglSwapBuffers`.
- Wayland framebuffer kept at the window size (`GLFW_SCALE_FRAMEBUFFER` off), so HiDPI outputs no longer get a swapchain / viewport mismatch.
- Owl Nest disables ImGui multi-viewports under Wayland instead of enabling windows GLFW cannot place.
- Vulkan descriptor sets are written at draw time with every declared binding (default white texture and empty buffers for unbound ones): no more `VUID-08114`, lavapipe no longer crashes.
- Vulkan colour samplers no longer enable depth comparison, which made lavapipe sample textures as plain white.
- Vulkan background quad flipped to Vulkan clip space: gradients, textures and skyboxes now match OpenGL.
- OpenGL: the main context is made current again after ImGui platform windows, so the window keeps rendering.
- OpenGL: Renderer2D rebinds its texture units after the tilemap pass, which reuses units 0..n.
- OpenGL SPIR-V: `InstanceIndex` / `VertexIndex` mapped to the OpenGL built-ins, so instanced 2D draws show on NVIDIA.
- Shader cache key covers backend, module, Slang version, macros and profile (B-05), not only the source.
- Raycast scenes no longer leak their textures past the device: the per-frame sprite, wall and door lists are cleared after drawing.
- `raycast_demo.owl` uses asset-relative texture paths instead of absolute paths from one machine.
- Voxel worlds now show in the exported game: `Scene::renderWithStack` meshes them for the runner and the editor alike (D-03).
- `SceneSerializer::serialize`, `PrefabSerializer::serialize`, `SettingsManager::saveUserSettings` and the editor's `Project::loadFromFile` / `saveToFile` return `bool`; Lua `settings.save()` returns whether it succeeded.
- Inspector edits serialize only the edited component, only while it is edited (`InspectorEditTracker`, `SceneSerializer::serializeComponentToString`): an idle inspector on a 13-component entity drops from 3.1 ms to 0.03 ms per frame, and an edit records one undo step when it ends.
- Editor undo restores entities in place: undoing an edit of a parent no longer detaches and moves its children, and the entity handle, UUID and sibling slot survive.
- Deleting an entity keeps its children's world position and sibling order, and undoing the deletion puts them back exactly.
- Editor dirty flag driven by an undo generation counter: "save, undo, edit" and an edit merged into the saved step now mark the document as modified.
- `Scene::getEntityCount()` counts the entities instead of always returning 0; the round-trip tests that compared 0 to 0 now check real counts.
- Prefab "Update from Prefab" / "Revert to Prefab" work in place: the instance keeps its hierarchy and placement, new and removed prefab entities follow, editor edits mark overrides (shown in the inspector, revertable per component, undoable), and Unlink is undoable.
- Textures picked inside an asset directory are saved as `nam:` relative names, no longer as absolute `pat:` paths that break on another machine and in the pack.
- Export rewrites the remaining absolute `pat:` references of scenes, tilesets and tilemaps to the packed `nam:` entry; `raycast_demo.owl` no longer points at a developer checkout.
- `AssetScanner` packs the `VoxelWorld` tileset and its atlas, and the `UiText` font.
- Voxel edits and streamed-in chunks re-mesh every neighbour chunk whose faces or ambient occlusion they change, edge and corner chunks included; a chunk generated with old parameters or already streamed out is dropped (D-08).
- The project icon is found in the asset directories too, so the exported game gets its icon.
- `runner.yml` and `game_info.yml` are written with a YAML emitter: a game name or description with `:` or `#` no longer corrupts them.
- Re-exporting over a previous export replaces the bundled shared libraries instead of keeping stale ones.
- `launch.sh` no longer adds the current directory to `LD_LIBRARY_PATH` when it is empty.
- The runner exits with code 1 when `runner.yml` or the first scene is missing.
- A missing tilemap or tileset at scene start is logged as a warning instead of being ignored silently.
- Help bundle: page names differing only by case no longer overwrite each other, stale pages are removed, and `HelpPanel` matches page ids case-insensitively.
- `scene.destroy_entity` is deferred to the end of the frame (`Scene::destroyEntityDeferred`): a script destroying its own entity no longer frees its running Lua state, and the destroyed entity gets `on_destroy` once, loses its Box2D body and takes its children with it.
- Lua `on_collision(other_id)` is now called: Box2D begin-touch contact events reach both entities' scripts once per touching pair, skipping entities hidden or pending destruction; `on_trigger_enter` / `on_trigger_exit` / `on_triggered` now receive the documented `other_id`.
- An `EntityLink` whose target is missing (misspelt, destroyed or renamed) is ignored with a single warning instead of crashing `onUpdateRuntime`.
- Play no longer shares voxel chunks with the editor scene: `VoxelWorld` copies are deep, so blocks broken or placed in Play no longer stay in the editor scene after Stop.
- A script hiding an entity, or a trigger teleporting the player, takes effect in the same frame: the per-pass caches are armed after scripts, physics, links and triggers.
- Scene loading validates its input: a malformed scene fails with a typed `SceneLoadError` and is rolled back, duplicated UUIDs are renamed, dangling parents and hierarchy cycles are moved to the root, and the editor names the reason a scene cannot be opened.
- SceneFlow: creating a teleport link no longer adds a second `Transform` to the new trigger entity (assertion in Debug, storage corruption in Release).
- A teleport to a missing or corrupted level keeps the current level playing, in the editor and in the runner, instead of leaving a stopped runtime.

### Security

- CI secrets never reach argv or the log: publication and DepManager passwords come from `OWL_DEPLOY_PASSWORD` / `OWL_REMOTE_PASSWORD`, exported by the TeamCity step only, and `--password` / `--remote_passwd` are refused.
- `ci.utils.secrets` masks every registered secret in every CI log record and the value of any sensitive flag in a logged command (`run_command`).
- Publication no longer downloads and runs the server's `api.py`: the upload client lives in `ci/utils/publish.py`, runs in process and requires HTTPS.
- `CodeStyle` gains a `secrets` sub-check that fails on a tracked `.env` or a committed key, token or URL password.
- CMake configure no longer prints `.env` values, only their keys.
- First Python tests of the CI tooling (`ci/tests/`, pytest) cover masking, publication and the secret scan.
- `.owlpack` reader hardened against forged packs: no write outside the extraction directory (`..`, absolute paths, symbolic links), sizes bounded by the file before any allocation, typed errors instead of escaping exceptions; `OWL_FUZZING` builds a libFuzzer target on `PackReader`.
- Lua sandbox hardened: chunks load as text only (bytecode refused, also through `load`), `string.dump` removed, `setmetatable` refuses `__gc`, `collectgarbage` restricted, string metatable locked.
- Lua quotas per `ScriptInstance` (`ScriptQuotas`): 64 MiB memory ceiling through a custom allocator and a 250 ms time budget per call through a watchdog thread; a script exceeding one is disabled instead of freezing or exhausting the game.
- Every engine call into Lua is protected with a stack trace, host reads of globals bypass script metatables, and bindings run behind an exception trampoline: no C++ exception crosses a Lua frame (PR-14: D-06, D-16).

## [0.2.1] - 2026-06-27

### Added

- **Voxel data model** — `owl::data::voxel`: `BlockRegistry`, 16³ `Chunk` (RLE encode/decode), sparse `VoxelWorld`. New `voxel_tests` category.
- **Chunk meshing** — `ChunkMesher`: greedy meshing with hidden-face culling, per-face atlas UVs, cross-chunk neighbour culling.
- **Block metadata** — 16-bit orientation + state per block (`BlockMeta`); `orientedFace` remaps per-face textures; RLE gained an optional `:meta` suffix (legacy-compatible).
- **Ambient occlusion** — per-vertex four-corner AO baked into the mesh, AO-aware greedy merge; toggle via `VoxelWorld.ambientOcclusion`.
- **Transparent / water rendering** — separate back-to-front pass with depth-write off (`ChunkMesher::meshByKind`), plus dynamic `RenderCommand::setDepthMask` on Vulkan.
- **3D forward rendering** — `Renderer3D`: depth-tested textured forward path with one directional light (`mesh3d` shader).
- **`RendererVoxel` layer + `VoxelWorld` component** — draws voxel entities (per-entity mesh cache, tileset atlas with shader `frac()` tiling); per-chunk CPU frustum culling (`FrustumCullingPass`).
- **Depth-aware rendering** — `Depth24Stencil8` on editor viewport + swapchain framebuffers; 3D depth on, 2D depth off.
- **Voxel worlds in the editor viewport** — rendered and navigable while editing, not only in Play.
- **Procedural terrain** — `math::PerlinNoise` (seeded 2D/3D + fBm) + `TerrainGenerator` (layering, shoreline, caves, biomes), seed-reproducible.
- **Terrain streaming** — `VoxelWorld.proceduralTerrain`: async chunk stream in/out around the camera (task `Scheduler`) with mesh pruning; demo `scenes/voxel_terrain.owl`.
- **Voxel player** — `VoxelPlayer`: first-person walk/run/jump with AABB-vs-voxel collision (`moveAabb`); mouse-look (opt-in cursor capture), double-tap-Space fly mode.
- **Block interaction** — break / place blocks via `raycastVoxel` (grid DDA), crosshair + wireframe highlight, dirties border neighbour chunks.
- **Voxel editor (Owl Nest)** — Voxel Palette brush (place/erase, undoable, stroke-coalesced), `.owlvoxstruct` structure capture/stamp, tile thumbnails.
- **3D cameras** — reusable `Camera3DController` (free-fly / first-person) + `FlyCamera` component (replaces the demo fly-camera Lua script).
- **Editor camera overhaul** — DCC-style navigation (Alt = look/pan/dolly, Ctrl = orbit/pan, wheel = zoom) on every viewport, corner XYZ gizmo, larger editor-only voxel view distance.
- **Window cursor mode** — `Window::setCursorMode` (Normal/Disabled) for first-person mouse-look.
- **Block + world textures** — new `voxel_blocks.owltileset` (16 faces); regenerated 2D platformer + world-map atlases (procedural generators).

### Changed

- **Module taxonomy reorganization** (breaking — public namespaces changed): `app` split from `core`
  (`Application` / `EntryPoint` / `Layer`); new `platform` module (`FileDialog`, `fileToString`, `openExternalUrl`);
  asset packing `io::pack` → `data::assets::pack` (`io` is now device-only); `CameraOrthoController` `input` →
  `renderer`; `physic` → `physics`; `data::component` → `data::meshrange`. Rules in `.claude/rules/module-layout.md`.

### Removed

- **Chunk Inspector panel** — voxel-editor chunk-diagnostics panel dropped (low value).

### Fixed

- **`math::Matrix` initializer-list ctor out-of-bounds read** — it copied a fixed `NCol*NRow` elements from any list (ASan stack-buffer-overflow on a short one); now clamps to the list size and zero-fills the rest.
- **Vulkan pipeline storm** — `pushPipeline` built a new pipeline per `DrawData` (thousands per voxel scene); now deduplicated + refcounted by pipeline signature, so identical chunks share one.
- **Vulkan `vkDestroyDevice` object leak** — the static `IconBank` atlas was freed after device destruction; now cleared in `EditorLayer::onDetach`. Added `vkSetDebugUtilsObjectName` instrumentation.
- **Vulkan `UPDATE_AFTER_BIND` cascade** — one shared per-frame descriptor set was rewritten mid-frame; replaced by a fence-recycled `DescriptorRing` (a distinct set per draw). Owl Nest now runs with zero validation messages.
- **Voxel atlas bleeding** — fixed via half-texel `tileRect` inset, seamless block textures, and `SampleGrad` tiling.
- **OpenGL: all `Renderer2D` content invisible** — `UniformBuffer::bind()` was a no-op while 2D/tilemap/3D share GL binding 0; `bind()` now re-binds and each renderer re-asserts its UBO before drawing (Vulkan unaffected).
- **Vulkan validation fixes** — distinct queue families (`EXCLUSIVE` swapchain sharing), enabled `shaderDrawParameters`, and clear a `StorageBuffer`'s descriptor binding before its `VkBuffer` is destroyed.
- **Vulkan batch-fence deadlock** — `beginBatch()` is now idempotent (was the freeze when entering a voxel scene).
- **Vulkan render-pass incompatibility flood** (`renderPass-02684`) — the swapchain framebuffer gained a matching `Depth24Stencil8` attachment so all scene pipelines share one layout.

## [0.2.0] - 2026-06-02

### Added

- **Renderer stack architecture** — composable per-scene renderer pipeline: `RenderLayer` / `RenderStack` /
  `RenderLayerFactory` + `RendererTag` component, YAML round-trip (`RendererStack:` / per-scene `EnabledRenderers:`
  with overrides), Project Settings composition UI, and a dockable per-scene Scene Settings panel.
- **Raycasting renderer** — `RendererRaycast` / `RendererRaycastLayer`, per-column DDA, configurable FOV / max distance
  / sky / floor, `Tileset.FilterMode`, and the `scenes/raycast_demo.owl` sample (Wolfenstein 3D E1L1, 64×64).
- **Raycast world detail** — textured floors / ceilings (`emitTexturedBackdrop`) and uniform distance fog
  (`RaycastConfig.fogColor` / `fogStart` / `fogEnd`).
- **Raycast sprites (billboards)** — `SpriteRenderer` / `AnimatedSpriteRenderer` rendered as camera-facing strips with
  per-column z-buffer occlusion, plus per-sprite world-size and Z-offset overrides; the same components stay 2D on
  `Renderer2D` layers.
- **Variable wall heights and transparent walls** — `TileMeta.wallHeight` (`[0, 8]`) and `TileMeta.transparent`
  (alpha-only, up to 8 back-to-front hits per ray).
- **Doors and pushwalls** — animated `RaycastDoor` / `RaycastPushWall` components with built-in `interactionKey` or Lua
  activation and auto-managed Box2D collision; editor tile pickers and gizmos.
- **Tilemap system** — `scene::Tileset` (`.owltileset`) and a standalone `.owltilemap` `TilemapAsset`, edited in the
  `TilemapDocument` (multi-layer, Properties / Canvas / Palette, paint-erase strokes, per-stroke undo).
- **In-viewport camera markers** — every `component::Camera` entity draws a dot + facing arrow + FOV cone.
- **Scene transition effects** — `Fade{In,Out}` + `Wipe{Left,Right,Up,Down}` with custom tint; Lua `ui.transition_play`
  (and back-compat `ui.transition_fade_in/out`).
- **GPU compute foundation** — Slang `[shader("compute")]` programs, `gpu::ComputeShader`, `gpu::StorageBuffer`
  (with `getData()` readback on all backends), and `RenderCommand::storageBufferMemoryBarrier()`.
- **Compute pre-pass utilities** — `WorldTransformPass` (#33) and `RaycastDDAPass` (#35), both adopted this release, plus
  `BitonicSortPass` (#32) and `FrustumCullingPass` + indirect-draw API (#34) shipped and headless-tested for adoption in
  v0.3.0.
- **Per-renderer Vulkan descriptor blocks** — backend-neutral `gpu::RendererDescriptors` (no-op on Null / OpenGL).
- **Renderer test coverage** — `RendererTilemap_test` (instanced-path stats) and `BitonicSortPass` padding / empty-input
  cases.
- **Editor & build niceties** — snap-to-grid translation gizmo (`Snap` toggle + `Step`), ribbon dropdown buttons, ccache
  compiler launcher (`cmake/CompilerCache.cmake`), and the Lua `physics.set_gravity_scale(entity, scale)` API.

### Changed

- **Renderer modernization** — `Renderer2D` rewritten as instanced + per-instance SSBO (Phase 1); per-frame world
  matrices produced by the `WorldTransformPass` compute pre-pass, with instances carrying an `int32_t worldIndex`
  instead of a `mat4` (Phase 2); raycast walls drawn by `RaycastDDAPass` + `raycast_stripe.slang` in one instanced
  call (Phase 3). Public draw APIs preserved.
- **Tilemap rendering** routes through the instanced `RendererTilemap`: deferred into `Renderer2D::flush` (after the
  background, before sprites) and combined into a single drawcall for the whole scene — per-instance `layerZ` / atlas /
  `textureSlot`, distinct tilesets across the shader's 32-texture array. Replaces the per-cell `Renderer2D::drawQuad`
  fallback.
- **2D entity draws** drop the redundant `getWorldTransform` when `worldIndex` covers them; the world-transform cache
  stays for the raycast DDA, physics, `EntityLink`, text glyphs and the editor.
- **Performance** — scene loading ~550× faster (memory-tracker O (N²) fix, UUID→entity cache, in-memory SPIR-V cache;
  `raycast_demo` ~22 s → ~40 ms) plus async load; in-game wins via scratch-buffer pooling and per-pass render-loop
  caches.
- **`Scene.EnabledRenderers`** now overrides layer order, not just enable / disable.
- **Editor keyboard shortcuts** (Ctrl+S, Ctrl+Z, …) bypass `WantCaptureKeyboard`; modifier-less keys still yield to
  focused text widgets.
- **Tooling** — renderer sources reorganized by renderer kind; `CodeStyle` CI action bundled into one read-only command;
  CMake globbing made configure-aware; TeamCity switched to plugin-event triggers; Oxford English spelling sweep across
  comments and prose; version bumped to `0.2.0` and `doc/pages/roadmap.md` reorganized.

### Fixed

- **Tilemaps (and raycast door / pushwall tilesets) were missing from packaged
  games.** `AssetScanner` never followed the `Tilemap` component's
  `tilemapPath` nor the `RaycastDoor` / `RaycastPushWall` `tilesetPath`, so the
  `.owltilemap`, its `.owltileset`, and the atlas textures were absent from the
  `.owlpack`; and `Scene::resolveAllTilemapAssets` loaded those YAML assets
  from disk only. Both are now wired: the scanner packs the whole
  tilemap → tileset → texture chain (and standalone door / pushwall tilesets),
  and asset resolution tries the open pack before the filesystem (like the
  Lua-script path). Editor / loose-file runs were unaffected.
- **Vulkan compute pipeline** — `ComputeShader::dispatch` now records on a one-shot command buffer (was recording into a
  closed buffer → SIGSEGV); entry-point name corrected to `main`; Slang source lookup fixed to
  `shaders/<renderer>/slang/<name>.slang`; `world_transform.slang` wraps its matrix in a struct for correct
  column-major SPIR-V decorations.
- **Raycaster** — player rotation now applied; walls actually drawn; HUD no longer rotates with the camera; broken
  empty-cell convention, wall blurriness, stripe flicker under motion, cell-coordinate half-extent and FOV-aspect bugs
  fixed.
- **Editor** rendered tilemaps in 2D regardless of `RendererTag` — fixed.
- **Empty render-stack layers** caused multi-scene flicker — fixed in `Scene::renderWithStack`.
- **Hidden triggers** no longer fire — `Scene::onUpdateRuntime` skips `gameVisible == false` triggers, clearing timers
  and overlap state with a synthetic `onTriggerExit`.
- **Packaging** — packaged games now ship the engine assets they need (`PackReader::entrySize()` reports uncompressed
  size); asymmetric `CameraOrtho` projections fixed on Vulkan.
- **Build / CI** — mingw-gcc 15 link passes `-Wa,-mbig-obj` on Windows GCC; `get_git_hash` made resilient; the
  `HelpIndex` badge test skips cleanly offline.

## [0.1.1] - 2026-04-30

### Added

- **Full Markdown rendering for the in-editor help and live preview**
    - New `codeEditor::MarkdownDocument` parser backed by **md4c 0.5.2** (new
      DepManager recipe at `OwlDependencies/Libs/md4c/`) — CommonMark + GFM
      tables / strikethrough / autolinks. Public block model
      (`MdHeading`, `MdParagraph`, `MdCodeBlock`, `MdImage`, `MdTable`,
      `MdList`, `MdBlockQuote`, `MdHRule`) covers everything the help pages use.
      Lazy implicit-paragraph creation handles md4c's tight-list quirk where
      `MD_BLOCK_P` is skipped inside `MD_BLOCK_LI`.
    - Rewritten `codeEditor::MarkdownPreview` walks the parsed block list and
      emits ImGui draw calls directly: scaled headings (1.60× / 1.30× / 1.15× of
      body via `PushFont(font, size)`), inline emphasis / strong / strikethrough
      / inline code, GFM tables with `BeginTable` (borders + row stripes), code
      blocks rendered through cached read-only `TextEditor` widgets with full
      syntax highlighting (Lua / C / C++ / Python / YAML / JSON / Markdown / XML
      / **Bash** — new `Language::Bash` definition with POSIX/bash keywords and
      common shell built-ins), block / inline images loaded via `lunasvg` (SVG)
      and `stb_image` (raster) with on-disk cache per source path. PNG/JPG
      textures load through the engine's `pat:` serialized form and are
      displayed with flipped UVs to compensate for stb_image's bottom-up loading.
    - External links and `https://` images are preserved in the rendered output;
      clicks open the user's default browser via the new
      `core::utils::openExternalUrl` helper (Linux: `xdg-open` via fork+execvp;
      Windows: `ShellExecuteW`; URL scheme restricted to `http(s)://` and
      `mailto:` for safety).
    - `cmake/HelpAssets.cmake` now scrubs Doxygen syntax at bundle time:
      `# Title {#page-anchor}` → `# Title`, `[TOC]` lines dropped,
      `(../images/foo.svg)` rewritten to `(images/foo.svg)`,
      `(engine_assets/<dir>/foo.png)` (used by the README logo) rewritten to
      `(images/foo.png)`, and `doc/images/` plus `engine_assets/logo/` copied
      into `engine_assets/help/images/`. HTTPS images referenced from any
      bundled markdown (e.g. the 24 shields.io badges in the README) are
      downloaded once via `file(DOWNLOAD)` into
      `engine_assets/help/images/badges/<sha1>.svg` and the references
      rewritten to local paths so the runtime renderer never has to fetch.
    - `nest::panel::HelpPanel` now defaults to the project README on first
      open; a draggable splitter between the page tree and the content pane
      lets the user resize the navigation column (clamped to a 120 px minimum
      per side).
    - `imgui_markdown` removed from `depmanager.yml` (replaced).
- **Live preview for markup documents**
    - `codeEditor::SvgPreview` rasterises the live SVG buffer through `lunasvg`
      into a `Texture2D` (cap 2048 px / side, ARGB-premul → RGBA-straight).
    - `CodeEditorDocument` now offers a vertical splitter with a draggable
      handle when the active language is Markdown or XML/SVG. Auto-enabled on
      load, toggleable via the new **Text → Preview** ribbon button.
- **In-editor help pages**
    - `cmake/HelpAssets.cmake` bundles `doc/pages/*.md` plus root README /
      CHANGELOG / CONTRIBUTING into `engine_assets/help/` at configure time and
      generates an `index.yml` (id, title, category, path).
    - `nest::panel::HelpPanel` reads the index, renders the selected page via
      `MarkdownPreview`, supports search, categorised navigation, back/forward
      history. Internal `[link](other.md)` clicks navigate within the panel.
    - `help.context` action (default shortcut **F1**) opens the help page that
      documents the SceneHierarchy component header most recently hovered
      (`SceneHierarchy::lastHoveredComponentName`); falls back to the editor
      overview when nothing is hovered.
    - File ribbon tab gained a **Help** group; the Welcome screen surfaces a
      **Getting Started** entry pointing to the new
      `doc/pages/getting_started.md`.
- **Animation editor**
    - New reusable `.owlanim` asset (`scene::AnimationClip` —
      `source/owl/public/scene/AnimationClip.h`): texture, grid, frame range, frame
      duration, loop, optional speed curve, with YAML round-trip and unit tests in
      `test/scene_tests/AnimationClip_test.cpp`.
    - `nest::AnimationDocument` opens as a 4th document type (alongside Scene, Code,
      NodeGraph). Three-panel layout: live spritesheet preview, properties (texture
      drop target, columns / rows / first / last frame, frame duration, loop, speed
      curve via `gui::widgets::curveEditor`), and a frame-range timeline.
    - New `gui::widgets::sequencer()` widget (`source/owl/public/gui/widgets/Sequencer.h`)
      wraps `ImSequencer` from the existing imguizmo bundle — owl-friendly API
      (`SequencerEntry` / `SequencerOptions`), no third-party types leak through the
      public surface.
    - Contextual ribbon `Animation` tab when an `AnimationDocument` is active:
      Playback (Play / Pause / Stop), Frame (Previous / Next) and File (Save / Save
      As / Close) groups. Ribbon File → "New Animation" entry creates an untitled
      clip; double-click / drag-drop in the Content Browser routes `.owlanim` files to
      the document. Dedicated browser icon (filmstrip glyph).
    - `sample_project/animations/coin.owlanim` showcases the new asset (mirrors the
      level 2 coin animation, including the Smooth speed curve).
- **Scene Flow editing**
    - Visual create of teleport links — every scene node carries a ghost `+ Add teleport`
      output pin. Dragging it onto another scene's entry pin spawns a `Trigger`
      (`Type=Teleport`, `LevelName=<dest>`) entity in the source scene at world origin and
      wires the canvas link in one undoable step.
    - Visual delete of teleport links — pressing Delete on a Teleport link destroys the
      matching `Trigger` entity, removes the canvas pin, and erases the link in one undoable
      step.
    - Per-pin `targetName` editing — right-click a scene node → `Edit teleport target →
      <pin>` opens a modal that mutates the live `Trigger.targetName` and pushes a
      `ModifyEntityCommand` on the source scene's undo manager (rapid keystrokes coalesce).
    - New `commands::SceneFlowCompositeCommand` glues a `SceneUndoCommand` with a
      `NodeGraphUndoCommand` so a single undo step reverses both halves; complemented by
      `AddPinAndLinkCommand` / `RemovePinAndLinkCommand` for the canvas pin+link bundle.
    - New `EditorLayer::loadOrOpenSceneDocument(path)` synchronously opens a `SceneDocument`
      for an on-disk scene without yanking focus — used by Scene Flow link edits to mutate a
      scene that may not currently be in a tab.
    - Per-layer vertical centring in the BFS auto-layout so single-node layers no longer hug
      the top edge.
- **NodeCanvas widget polish**
    - Text level-of-detail: pin labels stop drawing below `0.6` zoom and node titles below
      `0.3`, leaving the silhouette + connectors visible for graph-overview navigation.
      Exposed via free helpers `gui::widgets::shouldDrawPinLabels` /
      `shouldDrawNodeTitles` for downstream reuse and unit tests.
    - New public pin-manipulation API on `NodeCanvas`: `addOutputPin` / `addInputPin` /
      `removeOutputPin` / `removeInputPin`. Pin removal automatically strips dangling links.
- **Inspector field interactions**
    - Drag-drop from Content Browser to inspector fields via shared
      `gui::widgets::assetDropTarget` helper (`source/owl/public/gui/widgets/AssetField.h`).
      Per-extension validation through a new `AssetKind` enum (Texture, Font, Sound,
      LuaScript, AnyScript, Scene, Prefab, Any). Reuses the existing `CONTENT_BROWSER_ITEM`
      ImGui payload — no new wire format. Drops accepted on Sprite/AnimatedSprite/
      BackgroundTexture/UIImage textures, Text font, SoundSource asset, LuaScript path.
    - `gui::widgets::textureField()` consolidates the previously inlined texture-picker
      pattern (thumbnail / popup / "Remove texture") into one helper used by every
      texture-aware component. Removes ~150 lines of duplicated drag-drop boilerplate
      from `gui/component/render.cpp`.
    - Texture thumbnails now show a `(loading...)` overlay while an async-loaded texture
      decodes (`LoadState::Pending`) and a red `(failed)` overlay on `LoadState::Failed`.
    - Font preview: Text component shows a sample-string rendering of the selected font
      ("Aa Bb 1!? éàüÇ" — covers lower/upper case, digits, punctuation and accented
      Latin-1 glyphs). Backed by a new `gui::FontPreviewCache` that lazily renders each
      font into a 256x64 framebuffer via `Renderer2D::drawString` and caches the result;
      pumped from `EditorLayer::onUpdate` and freed on `UiLayer::onDetach`.
    - Latin-1 glyph rendering fix — `Font::getGlyphBox` and `Renderer2D::drawString` no
      longer sign-extend `char` codepoints into garbage 32-bit values; the renderer
      additionally decodes UTF-8 source text into Latin-1 codepoints up front, so
      accented glyphs (`éàüÇ`…) coming from YAML scenes or Lua strings render correctly
      everywhere instead of falling back to `?` or `Ã©` byte pairs.
- **Sample project showcases v0.1.1 features**
    - Main-menu subtitle exercises UTF-8 / Latin-1 rendering
      (`Démo des fonctionnalités… caractères éàüÇ`).
    - Level-2 coins use a Smooth `AnimatedSpriteRenderer.speedCurve` so the rotation
      pulses (slow at the loop boundary, fast in the middle).
- **Curve editor for animated properties**
    - New `math::Curve` (`source/owl/public/math/Curve.h`) — sorted keyframe list with
      Constant / Linear / Smooth interpolation, flat-hold extrapolation, and YAML
      round-trip (handled inline in component serializers, default-empty curves omit
      `speedCurve` from the YAML to preserve byte-identical scene files).
    - New `gui::widgets::curveEditor` widget (`gui/widgets/CurveEditor.h`) — wraps
      ImCurveEdit from the existing imguizmo bundle (no new DepManager dependency).
      Drag points, double-click to add, right-click to remove; companion combo selects
      the interpolation mode.
    - First end-to-end consumer: `AnimatedSpriteRenderer.speedCurve` remaps per-frame
      animation advancement (`Scene::onUpdate` multiplies dt by
      `speedCurve.evaluate(progress)` when the curve is non-empty).
- **Scene Flow refinements**
    - Pin labels are now drawn **inside** the node frame (via a new `NodePin::labelColor` field
      and a `CustomDraw` override on `NodeCanvas`). GraphEditor receives `nullptr` for slot names
      so it stops rendering them outside the rect — labels stay aligned with their slot circles
      regardless of zoom
    - Compact pin labels: **just the source identifier** prefixed by a single-glyph kind hint
      (`† DangerZone`, `★ VictoryZone`, `λ checkpoint`, plain `LevelPortal` for Teleport).
      Destination is implicit from the link, no need to repeat in the label
    - **Right-click hit-test fix** — `GraphEditor` only sets `nodeOver` when a slot is hovered;
      clicks on the node body returned `-1`. `NodeCanvas` now does its own canvas-space hit-test
      against stored node rects when GraphEditor reports no node. Right-click on the body shows
      the Edit / Delete menu as expected
    - **Layered layout** replaces the random 4-column grid: BFS from `Project::firstScene`
      assigns each scene a column = its depth, orphans land in a "limbo" column on the right.
      Within each column, scenes are sorted alphabetically. Reduces link crossings substantially
      on the sample project
- **Scene Flow UX pass**
    - Nodes auto-size from their title and pin labels (`NodeCanvas::measureNode` exposes the
      `ImGui::CalcTextSize`-based size); the SceneFlow grid layout uses **actual** node widths /
      heights to compute column / row offsets so nothing overlaps regardless of label length
    - `NodeCanvas` now sets `GraphEditor::Options::mDrawIONameOnHover = false` — pin labels are
      drawn permanently (they used to only appear on hover, making the graph illegible at a glance)
    - Scene titles drop the `.owl` extension (implicit since every node is a scene)
    - Output pin labels show the **source trigger entity name** parsed from the scene's YAML
      (e.g. `LevelPortal → level2 @ SpawnPoint`); transitions of type `Death` and `Victory` are
      now extracted too (they also serialize a `LevelName`) and styled distinctly:
      `[death] DangerZone → game_over`, `[victory] VictoryZone → victory`
    - Best-effort scan of attached Lua scripts for `scene.load_scene("...")` calls — adds extra
      output pins tagged `scene_lua_exit` so Lua-driven transitions appear alongside Trigger ones
    - Right-click menu: on a node → **Edit scene** (opens it in a new tab) / **Delete scene**
      (with confirmation modal); on empty space → **Add new scene...** (creates an empty `.owl`
      under `scenes/NAME.owl` and opens it). Canvas auto-rescans after create/delete
    - The global **Scene Hierarchy** and **Properties** panels now host SceneFlow content while
      that document is active: the hierarchy lists every scene (orphans drawn red, click-to-select,
      double-click to open), the properties panel shows the selected scene's path, transitions
      (colour-coded: red = death, green = victory, blue = Lua) and an "Open this scene" button.
      Wired through new `Document::overridesGlobalPanels()` / `renderHierarchyPanel()` /
      `renderPropertiesPanel()` virtuals — generic for future node-graph documents
- **Node graph framework + Scene Flow view**
    - Reusable `gui::widgets::NodeCanvas` widget (public header
      `source/owl/public/gui/widgets/NodeCanvas.h`) — nodes with typed input/output pins, UUID-based
      identity, pan/zoom/selection, link validator, callbacks for link create/delete, node move,
      node select and **double-click** (detected in the wrapper since GraphEditor has no native
      double-click signal). Pimpl over `GraphEditor` from the existing `imguizmo` 1.92.7 DepManager
      package — the bundle ships GraphEditor + ImSequencer + ImCurveEdit + ImGradient + ImZoomSlider
        + ImLightRig alongside ImGuizmo, so no new dependency was needed
    - `gui::widgets::NodeCanvasSerializer` — domain-agnostic YAML round-trip (`.owlflow` format)
      for full canvas save/load, plus `serializeSubset`/`pasteSubset` for copy/paste with fresh UUIDs
    - `NodeGraphDocument` — third `DocumentType` alongside Scene and Code, generic node-graph
      document with its own `NodeGraphUndoManager` (a `UndoManager` typed for `NodeCanvas`); pastes, saves
      and loads through the serializer. Content Browser wires `.owlflow` double-click to a new
      `EditorLayer::openNodeGraphFile` handler; ribbon contextual tab "Graph" with save/close
    - Node-graph undo commands (`source/owlnest/sources/commands/NodeGraphCommands.{h,cpp}`):
      `AddNodeCommand`, `RemoveNodeCommand` (restores attached links on undo), `MoveNodeCommand`
      (merge-coalesces rapid drag into a single step), `AddLinkCommand`, `RemoveLinkCommand`
    - `SceneFlowDocument` — first consumer of the framework, specialises `NodeGraphDocument`
      to render the project's scenes as a graph: one node per `.owl` file, entry pin + one output
      pin per Teleport trigger found in that scene, links wired from output → destination entry,
      orphan scenes (unreachable from `Project::firstScene`) drawn with a red title. Double-click
      navigates to a scene via `EditorLayer::openScene`. Exposed from the ribbon File tab via
      a new "Views → Scene Flow" button. Link create/delete and per-pin target editing (writing
      new trigger entities back into the source scene) are deferred — they require composite
      `SceneUndo + NodeGraphUndo` commands and stay out of this first slice
    - 16 new unit tests: `test/gui_tests/NodeCanvas_test.cpp` (topology, link validator veto,
      cascaded link removal, selection round-trip, pin→node lookup, clear) and
      `test/gui_tests/NodeCanvasSerializer_test.cpp` (empty/full canvas round-trip, custom data
      preservation, malformed YAML rejection, subset serialize, paste with fresh UUIDs)
- **Undo system templatized over its target type**
    - Templatized `UndoCommand` + `UndoManager` (parameterized over the edited `Target`) in
      `source/owlnest/sources/UndoCommand.h` /
      `UndoManager.h` — both now header-only templates, `IUndoTarget` as common marker base
    - `SceneUndoCommand` / `SceneUndoManager` aliases preserve the current editor behaviour one-to-one;
      every existing command (`Entity*`, `Component*`, `Hierarchy*`, `Prefab*`) migrated to
      `SceneUndoCommand`
    - Unblocks a future node-graph undo stack parallel to the scene one (workstream B-5/B-6)
- **Async texture loading with placeholders**
    - `TextureDecoder` helper (`renderer/TextureDecoder.h`) — CPU-only decode primitive
      (`peekImageSize`, `decodeImageBytes`, `decodeImageFile`) with per-thread stb_image flip state
      so multiple workers can decode concurrently
    - New `Texture2D::createFromSerializedAsync(name, scheduler)` returns immediately with a
      placeholder-sized Rgba8 texture filled white; worker decodes, termination callback uploads
      real pixels on the main thread and flips `LoadState` to `Ready` (or `Failed`, keeping the
      placeholder visible)
    - Dimensions peeked from the PNG/JPG header up front, so every texture is created at its real
      size from frame 0 — binding, UV coords and atlas math unaffected
    - `Texture2D::createFromSerializedForDeserialize()` wrapper used by scene components
      (`SpriteRenderer`, `AnimatedSpriteRenderer`, `BackgroundTexture`, `UIImage`) — async when an
      `Application` is alive, synchronous fallback for `PackWriter` and unit tests
    - `RunnerLayer::finishTransition()` logs the count of still-pending textures after a teleport
      as a diagnostic trace — smooth scene changes in the runner, no more frame hitch on rich
      scenes
- **Async operations in editor** with progress modal (`AsyncProgressModal` panel)
    - Pack Game / Pack Scene: async with per-entry progress bar and cancel support
    - Save Scene: serialize on main thread, write file on background thread
    - Open Scene: read file on background thread, deserialize on main thread in callback
    - Content Browser: directory scanning cached and refreshed asynchronously (no more per-frame
      `directory_iterator`)
    - Runner scene transitions (`handleTeleportRequest`): load scene bytes in background, deserialize
      and swap on main thread, old scene stays rendered during load
- **Deferred shader compilation** with ImGui loading screen at startup
    - `Renderer::initContext()` / `Renderer::initShaders(callback)` split
    - Per-shader progress displayed before editor/runner starts
- **Packaging wizard panel**
    - Dedicated dialog opens when clicking Pack Game (replaces the bare folder dialog)
    - Fields: destination path with Browse button, target platform (read-only), compress/obfuscate options
    - Pre-packaging validation phase runs async and collects warnings for unresolvable texture,
      sound, font, script, and trigger-scene references (`AssetScanner::scanScene/scanProject`
      accept an optional `std::vector` of warning strings as output)
    - Validation modal shown if warnings: bullet list + "Proceed anyway" / "Cancel"
    - Async validation phase reuses its scanned assets for the pack phase (avoids double scan)
    - Detects missing `OwlRunner` executable and empty asset list
    - Post-pack build report: asset count, pack size (MiB), and total duration displayed in the
      completion modal
- **Ribbon menu + generic code editor**
    - New `gui::widgets::Ribbon` widget (`source/owl/public/gui/widgets/Ribbon.h` +
      `private/gui/widgets/Ribbon.cpp`) — Office-style horizontal banner with top-level tabs →
      groups → large buttons (32 px icon + label underneath) and small buttons (16 px icon + label
      on the right, 3 per column). Button callbacks drive enabled/checked/click state
    - `UiLayer::setTopBarCallback()` reserves space between `ImGui::Begin(OwlDockSpace)` and
      `ImGui::DockSpace()` for the ribbon. The classic `ImGuiWindowFlags_MenuBar` is gone
    - `Ribbon::setTabHighlighted()` renders the tab title with the theme accent (secondary) colour
      — applied to the **File** tab so it reads as the primary entry point
    - Tab-bar padding (`FramePadding {14, 6}`) gives the titles breathing room; `TabSelected`
      pushed brighter so the active tab stands out clearly against dimmed siblings
    - `EditorLayer` replaces the menu bar + floating Play/Pause toolbar + gizmo `ButtonBar` with
      a single ribbon, a contextual **Scene** / **Text** tab switches on the active document type
        - **File**: Project (New / Open large, Save / Save As / Close small), Recent (large button
          opens a popup listing `EditorSettings::recentProjects`), Package (Pack Game large),
          Session (**Exit large**)
        - **Edit**: History (Undo / Redo large), Settings (Engine / Editor / Project small)
        - **Scene**: File (New / Open large, Save / Save As / Import small, **Close large** last),
          Playback (Play / Stop large, Pause / Step small), Gizmo (Translate / Rotate / Scale
          **large**), Package (Pack Scene large)
        - **Text**: File (Save / **Close** large)
    - New `saveProjectAs()`: picks a destination folder, duplicates the current project
      recursively via `std::filesystem::copy`, then switches the editor to the new location
    - New `CodeEditorDocument` (`DocumentType::Code`): a second kind of `Document` that opens
      text/source files as their own tab. Backed by **imgui_color_text_edit** 1.92.6 pulled via
      DepManager (bumped `imgui` to 1.92.6-docking to match)
    - Built-in syntax highlighting: **Lua**, **C**, **C++**, **Python**, **JSON**, **Markdown**
      (from the library); custom definitions added for **YAML** and **SVG/XML** in
      `source/owlnest/sources/document/codeEditor/LanguageDefinitions.{h,cpp}`
    - ContentBrowser double-click routes
      `.lua`/`.py`/`.c`/`.cpp`/`.cc`/`.cxx`/`.h`/`.hpp`/`.hxx`/`.yml`/`.yaml`/`.json`/`.md`/`.markdown`/`.svg`/`.xml`
      to a new code-editor tab (or re-activates the existing one)
    - Status footer in each code editor shows language + line/column + INS/OVR. Ctrl+S saves;
      dirty tracked via `TextEditor::GetText() != savedText`
    - Rich rendering of Markdown and SVG inside the editor is deferred to a future release —
      added to the roadmap
- **Editor fonts**
    - Engine fonts (Roboto Regular / Bold / Italic, JetBrains Mono Regular) ship as loose TTFs in
      `engine_assets/fonts/{roboto,jetbrainsmono}/` and are loaded via `AddFontFromFileTTF`
      (resolved through `Application::getAssetDirectories()`). The previous `.embed` hex-array
      headers have been dropped from the binary
    - Dedicated code-editor font: **JetBrains Mono** monospace for column-aligned glyphs in the
      TextEditor widget, rasterised at the user-configured size
    - `UiLayer::setUiFontSize()` + `setCodeFontSize()` static setters applied from `main.cpp`
      before the `Application` is constructed — the atlas is built once in `UiLayer::onAttach`;
      size changes take effect on the next startup
    - New `EditorSettings::uiFontSize` (14–24, default **18**) and `codeEditorFontSize`
      (8–48, default **17**) sliders in the Editor Settings panel
    - Modal button widths (close-doc, welcome, pack wizard, pack validation, async progress) are
      now `ImGui::GetFontSize() * N` so they scale with the UI font size instead of clipping
- **Multi-document architecture**
    - New abstractions in `source/owlnest/sources/document/`: `Document` (interface),
      `DocumentManager` (open list + active tracking), `SceneDocument` (scene wrapper)
    - Scene-scoped state (`editorScene`, `activeScene`, path, Play/Pause/Stop, teleport request,
      save/load request, undo stack, **the Viewport panel**) moved from `EditorLayer` into
      `SceneDocument`
    - `EditorLayer` becomes a host of `DocumentManager`; all scene operations delegate to the
      active document
    - **Per-document Viewport**: each `SceneDocument` owns its own `Viewport` instance with its
      own framebuffer and a stable ImGui window id (`##scene_UUID`). ImGui's docking groups
      viewports that share a dock node as tabs automatically, and users can tear a tab off to
      see several scenes side-by-side
    - Dirty marker rendered via `ImGuiWindowFlags_UnsavedDocument`. No close `x` and no collapse
      button on viewport tabs (`ImGuiWindowFlags_NoCollapse`, `p_open = nullptr`) — scenes are
      closed via `Current > Close Scene` or `Ctrl+W`, which route through a "Discard changes /
      Cancel" modal when the document is dirty. New viewports auto-dock to the central node on
      first open
    - `File > Open Scene` opens in a new tab (or switches to the existing one when already open),
      `File > New Scene` creates an Untitled tab (or reuses a pristine one)
    - Play/Pause/Stop/Step toolbar + gizmo control bar are hidden when the active tab is not the
      document currently running; the toolbar positions itself over the *active* viewport's
      bounds (updated when the user switches tabs)
    - Per-document undo/redo stack (Ctrl+Z / Ctrl+Y acts on the active doc)
    - New shortcuts: `Ctrl+W` close active document, `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle
    - Background simulation: non-active tabs in Play mode advance their physics/scripts without
      rendering (new `iRender` flag on `Scene::onUpdateRuntime`); only the active viewport
      writes to its framebuffer
    - `gui::BasePanel::onRender()` becomes virtual so specialised panels (the per-document
      Viewport) can provide their own `ImGui::Begin` with close button + unsaved-document flag
      while reusing the focus/hover/size bookkeeping
- **File type icons + icon buttons**
    - Per-extension content-browser icons built from the `base_file_ext_icon` template with a
      ribbon label and a central type glyph
    - Sound (`wav`, `mp3`, `ogg`, `flac`) — speaker; mesh (`obj`, `gltf`, `glb`, `fbx`) — isometric
      cube; source (`py`, `cpp`, `h`, `c`) — language-specific silhouette; docs (`md`) — markdown
      mark with down arrow
    - Central glyphs added to existing icons: `png`/`jpg` (image with sun + mountains), `svg`
      (vector anchor points), `glsl` (GPU chip), `owl` (isometric scene floor), `yml` (gear),
      `ttf` ("Aa" sample), `lua` (crescent moon + star), `json` (curly braces with colon dots)
    - Theme accent (secondary) now resolves to an amber/gold matching the Owl Nest brand (fixed
      `#ffc726`), no longer tied to `ImGuiCol_ButtonActive`
    - Clarity pass on non-browser icons for readability at 64 px atlas size:
        - Thickened strokes previously under 3 atlas-px: `save` internal lines, `circle` radius,
          `interaction` key outline
        - Redesigned `content_browser` (grid of four tiles, selected tile filled), `animated_sprite`
          (filmstrip with sprocket holes), `new_scene` (clean "+" in place of 8-point spark)
        - Replaced SVG `text`-rendered glyphs with paths in `interaction` (letter E) and
          `lua_callback` (code-block chevron + amber dot)
        - Disambiguated overlapping silhouettes: `add_child_entity` now shows a parent → child
          tree; `background` (night sky with stars + horizon), `sprite` (outlined landscape),
          `ui_image` (amber inner tile) are visually distinct instead of three image variants
    - Icons added to Welcome screen (New / Open Project), Pack Wizard (Browse / Start / Cancel),
      validation modal (Proceed / Cancel), AsyncProgressModal (Close / Cancel), Content Browser
      rename and delete dialogs, Log panel Clear, Settings/Parameters/Project Settings OK/Cancel
- **Sound preview in inspector**
    - Play / Stop button on the SoundSource component to preview the sound using the current
      volume and pitch settings (non-spatial)
- **Recent projects** persisted in `EditorSettings::recentProjects` (up to 10 entries)
    - "Open Recent" submenu in File menu with full-path tooltip per entry
    - Welcome screen modal when no project is loaded (New / Open / Recent list, closable via `x` or
      `File > Welcome Screen`)
- **Editor menu replaced by the ribbon** — the former `ImGui::BeginMenuBar` drop-downs, the
  floating Play/Pause toolbar, and the gizmo `ButtonBar` are all gone. All actions now live in
  the ribbon (File / Edit / Scene|Text tabs). "Show Stats" moved into the Editor Settings panel
- **Tooltips everywhere** with hover delay (~0.4s)
    - Descriptive tooltips on all 7 trigger types (Victory, Death, Target, Teleport, Timer,
      Interaction, LuaCallback) and their sub-fields
    - Tooltips on PhysicBody, Player, Camera, SoundSource, SoundListener, Canvas, UIRect,
      UIButton, UISlider fields
    - Reusable `fieldTooltip()` helper
- **Scene transitions** and packaging quality-of-life
    - `scene.quit()` Lua API for clean application exit
    - `SettingsManager::loadDefaultsFromString()` for pack-based settings loading
    - `PackWriter` progress callback + cancel check
    - `AssetScanner` now scans UIImage, AnimatedSpriteRenderer, all trigger LevelNames,
      `sound.play()` in Lua, and deferred `scene.load_scene` patterns
- **Documentation**
    - Mermaid diagram support in Doxygen (via CDN mermaid.js in header.html)
    - Mermaid diagrams in architecture, scene, scripting, editor, sound, physics doc pages
    - Doxygen favicon using the owl logo (32x32 + 16x16)
    - GitHub community files: CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md, CHANGELOG.md
    - `.claude/rules/documentation.md` with format conventions for markdown tables, Mermaid,
      roadmap, and CHANGELOG
- **Testing**
    - AssetScanner unit tests (18 tests covering all asset types, recursion, deduplication,
      cancellation)
- **Code quality**
    - Component-scoped `PushID(T::name())` in the templated `drawComponent` helper to prevent ImGui label collisions
      across components that share field names (e.g., "Colour" in multiple renderers)
    - Index-based `PushID` in LuaScript property loop

### Changed

- Version bumped to **0.1.1**
- Tests that touch process-level resources (freetype, GLFW, OpenAL, msdfgen, script) now serialize
  via CMake `RESOURCE_LOCK` to avoid sporadic SEGFAULTs in parallel ctest runs
- LuaBindings: removed unused `iArgIndex` parameter from `findEntity`
- `ContentBrowser`: mutation methods (create/import/rename/delete/drop) now flag `m_rescanRequested`
  instead of relying on per-frame directory scans
- UI rounding reduced across all theme presets — `windowRounding` / `tabRounding` /
  `controlsRounding` now 2–3 px (was 4–10) for a crisper, more technical look
- `UiLayer::codeFontSize()` is no longer a `constexpr 13.f` — it returns the user-configured size
  set via `UiLayer::setCodeFontSize()` at startup

### Fixed

- **Sample project main menu Lua flood** — `string.format("%d, %d", mx, my)` on the float mouse
  coordinates now `math.floor`s them first; was triggering
  `bad argument #2 to 'format' (number has no integer representation)` on every frame in
  `main_menu.lua:89`
- **ARM64 Linux CI restored** — Poetry's default venv cache (`~/.cache/pypoetry/virtualenvs/`)
  names venvs from `(project, pyproject, python version)` without the architecture. CI agents
  running different archs on a shared `$HOME` (or bind-mounted workspace) collided on the same
  venv path; the ARM64 runner then loaded the x86_64 `cryptography/_rust.abi3.so` and crashed
  at import with `cannot open shared object file`. `ci.utils.venv` now layers three checks in
  cheapest-first order: (1) no venv → skip; (2) compare an `arch-OS-impl-pyver` marker file
  inside the venv against the live host; (3) if the marker is missing/mismatched, run a
  functional `from cryptography.fernet import Fernet` test under `poetry run python`. When the
  test fails, `ci_action.py` exports `OWL_CI_REFRESH_VENV=1`; `cmake/Poetry.cmake` consumes it
  via `poetry env remove --all` before sync and (re)writes the marker afterwards. The check
  runs unconditionally (TC-via-Docker doesn't propagate `TEAMCITY_VERSION`, so an env-var gate
  would no-op), yet same-arch reruns still pay almost nothing thanks to the marker fast path.
- **Windows Debug test binaries no longer fail to load** (was
  `STATUS_DLL_NOT_FOUND` / `0xc0000135`). The helper
  `owl_target_link_libraries()` was setting `CMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release`
  only around `find_package()`, but that variable is read at generate time (link resolution +
  `TARGET_RUNTIME_DLLS` generator-expression evaluation), not at find time. In Debug builds the linker picked
  `glfw3d.lib` (imports `glfw3d.dll`) while the post-build copy grabbed the release `glfw3.dll`
  — every test binary then failed to load. The mapping is now applied at top-level directory
  scope in `CMakeLists.txt`, gated by `OWL_USE_RELEASE_THIRD_PARTY`, so link and DLL copy agree
- Tests now get a `TARGET_RUNTIME_DLLS` generator-expression post-build copy next to their binary
  (belt-and-braces — picks up any DLL surfaced through the test's own link graph that the
  engine's post-build didn't cover)
- **Use-after-free SIGSEGV when closing a document tab** — closing a `SceneDocument` inline from
  `onImGuiRender` freed its Vulkan colour-attachment while ImGui's draw list still referenced it
  (the sampler then read freed GPU memory at `UiLayer::end()` submit). `EditorLayer::closeDocument`
  now queues the id in `m_deferredCloseIds` and drains at the start of the next frame, after the
  prior frame's ImGui commands are submitted
- **SIGSEGV in the `ImGuiLayer.creation` unit test** — `UiLayer::onAttach` called
  `OWL_CORE_ERROR` when the Roboto/JetBrains Mono TTFs couldn't be resolved, but `disableApp()`
  left the Log singleton uninitialised, so the error path deref'd null. The external-font block
  is now gated on `m_withApp`; standalone uses fall back to ImGui's built-in default font
- Pack filesystem errors no longer crash the editor: `create_directories`, `copy_file`, and
  `permissions` now use `std::error_code` with a user-facing error modal when the destination path
  conflicts with an existing file of the same name
- AssetScanner: absolute path resolution for sounds and scenes (`resolveSound`, `resolveScene`)
- AssetScanner: skip nonexistent scene files instead of adding them to the asset list
- `recursive_directory_iterator` crash on missing directories (AssetLibrary, FontLibrary)
- Mouse Y inversion in packed runner UI (UIInputSystem coordinates)
- `writeLinuxLauncher` undefined on Windows (guarded with an `OWL_PLATFORM_LINUX` ifdef)
- `game_settings.yml` not included in packed game
- Markdown table alignment across all doc pages
- Clang-tidy cognitive complexity in UIInputSystem and AssetScanner (extracted helper functions)
- LuaBindings: `const lua_State*` incompatibility with `lua_CFunction` signature (added NOLINT)
- Flaky tests: shared `output/test_tmp` directory replaced with unique per-test paths in
  `std::filesystem::temp_directory_path()`
- First-time Slang shader compilation no longer freezes the window — loading screen animates
  between each shader
- Test scene textures (`source/owlnest/assets/scenes/test_levels{,2}.owl`) used absolute
  `pat:/source/.../mario.png` paths inherited from a previous host layout — converted to
  portable `nam:textures/mario.png` so they resolve through the engine's asset directories.

## [0.1.0] - 2026-04-16

### Added

- Lua 5.5 scripting with full engine API (13 tables, 58 functions)
- In-game Canvas UI system (8 widget types: Text, Image, Button, Slider, ProgressBar, Panel, Rect)
- Game state and save/load system with multiple slots
- Two-layer settings system (game defaults + user overrides)
- Prefab system with UUID remapping and override tracking
- Undo/Redo command system with merge coalescing
- Extended trigger system (7 types: Victory, Death, Teleport, Timer, Interaction, LuaCallback, Target)
- Asset packing pipeline (.owlpack format with zstd compression)
- Runner with packaging (renamed executable, launcher script, metadata)
- Scene fade transitions (fade in/out)
- Sample game demonstrating all engine features (6 scenes, 12 scripts)
- AnimatedSpriteRenderer component
- SpriteRenderer tiling factor separated into X/Y
- scene.quit() Lua API for clean exit
- Sound pause/resume/volume control from Lua

### Fixed

- Mouse Y inversion in packed runner UI
- Assets not found in packed game (AssetScanner now scans UIImage, AnimatedSpriteRenderer,
  Death/Victory triggers, sound.play in Lua)
- recursive_directory_iterator crash on missing directories
- OpenAL source leak on scene end
- GameState lost on scene transitions
- UISlider callback never fired
- writeLinuxLauncher undefined on Windows

## [0.0.3] - 2026-04-09

### Added

- Sound effects, music, and spatial audio (OpenAL backend)
- Animated sprites with spritesheet grid support
- Mesh loading (OBJ, glTF, GLB, FBX)
- Configurable keymap
- Scene hierarchy with parent-child entities
- Asset packing (.owlpack binary format)
- Task system (Taskflow backend)
- Project system in Owl Nest
- Icon system with runtime SVG rendering

## [0.0.2] - 2026-03-03

### Added

- Backgrounds / skyboxes
- Slang shader migration
- Pause / unpause / frame stepping
- General settings management
- Scene-to-scene jumping

## [0.0.1] - 2025-02-06

### Added

- Initial release: minimal viable engine with scene-based games
