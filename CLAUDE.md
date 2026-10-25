# CLAUDE.md

Owl is a C++23 game engine (`OwlEngine`) with an editor (Owl Nest), a game runner, a Python CI wrapper and
TeamCity pipelines. Version: see `project(... VERSION ...)` in `CMakeLists.txt`; plan and released versions in
`doc/pages/roadmap.md`. Backends: graphics OpenGL 4.5 / Vulkan 1.3+ / Null, input
GLFW / Null, sound OpenAL / Null. ECS on EnTT with parent-child hierarchy. Platforms: Linux x64/arm64,
Windows x64 (MinGW).

Detailed, path-scoped rules load automatically from `.claude/rules/` when you touch matching files
(C++ style, CMake, tests, renderer, Slang, scene, editor, script, task system, dependencies, Python CI,
docs, module layout). User documentation lives in `doc/pages/*.md` — read the relevant page before
changing a subsystem. The repository audit of 2026-10-05 is archived in `doc/audit/` (entry point: `AUDIT.md`).

## Build, test, run — Docker only

Every compiler, CMake, CTest, Poetry, clang-tidy or clang-format call runs **inside the build image**
through `./docker_run.sh`, which mirrors CLion's *Docker Owl* toolchain (image
`registry.argawaen.net/builder/devel-ubuntu2604:latest`, your UID/GID, repo mounted at its host path,
`$HOME` = `../fake_home` → `/fhome` holding the Poetry venv, the Conan cache and ccache). Never build
natively, never install a tool on the host; if a tool is missing from the image, say so.

```bash
./docker_run.sh cmake --preset linux-clang-release -S .          # configure (fetches deps)
./docker_run.sh cmake --build output/build/linux-clang-release   # ~1 min full build on 32 cores
./docker_run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure -j8
./docker_run.sh bash -c 'cd output/build/linux-clang-release && bin/owl_<cat>_tests_unit_test --gtest_filter="Suite.*"'
./docker_run.sh --gui output/build/linux-clang-release/bin/OwlNest   # GPU + display + audio
./docker_run.sh --perf perf ...                                      # ptrace / perf events
```

- **Clang presets** for dev and tests: `linux-clang-release` (default), `linux-clang-debug`. Coverage is measured
  with both compilers: `linux-gcc-debug` (the pull-request gate) and `linux-clang-debug` (nightly).
- Other presets: `linux-clang-tidy`, `linux-sanitizer-{address,thread,undefined-behavior}`,
  `linux-cross-arm64` (arm64 release, nightly, tests under QEMU, packaged), `linux-clang-minimal` (every optional engine module off,
  `OWL_MODULE_*`, see `doc/pages/building.md`), `windows-{gcc,clang}-{release,debug}`; `cpack` works in any release tree.
- Dependencies: Conan 2 only (`conanfile.py`, `conan.lock`); in a worktree add `-DOWL_CONAN_HOME=/fhome/.conan2-owl`;
  see `.claude/rules/dependencies.md`.
- Output: `output/build/<preset>/{bin,lib}`, install in `output/install/<preset>/`.
- If a build dir ends up root-owned, chown it back through a throwaway root container.
- Test binaries are `owl_<folder>_unit_test` (e.g. `owl_scene_tests_unit_test`); new `.cpp` files in
  `test/unit/<cat>_tests/` and anywhere in `source/` are picked up by `GLOB_RECURSE ... CONFIGURE_DEPENDS` at the next build.

## CI actions (`ci_action.py`)

`./docker_run.sh poetry run python ci_action.py <Action> <preset> [-- --opt=value]`

| Action                                             | What it does                                                                                   |
|----------------------------------------------------|------------------------------------------------------------------------------------------------|
| `Build`                                            | Configure + build the preset.                                                                  |
| `Test`                                             | CTest with reports.                                                                            |
| `Coverage`                                         | gcovr via `gcovr.cfg` (never CLI filters); `linux-gcc-debug` or `linux-clang-debug`.           |
| `CodeStyle`                                        | Read-only gate: clang-format, codespell, comment quality, docs, cpp-style, headers, ruff/mypy. |
| `ClangTidy`                                        | clang-tidy (or `--tool=analyzer`) over `compile_commands.json`; PR = diff-scoped.              |
| `Documentation`                                    | Doxygen with `WARN_AS_ERROR=YES`.                                                              |
| `Bench`                                            | `owl_bench` against `test/bench/baseline/<preset>.json` (e.g. `linux-bench`).                  |
| `Fuzz`                                             | libFuzzer targets for a fixed time each (`linux-fuzz`).                                        |
| `IncludeCheck`                                     | Every header and source compiled alone on strict libc++, no PCH (`linux-include-check`).       |
| `DependencyReport`                                 | Informative list of dependencies with a newer version.                                         |
| `Package`, `Clean`, `PublishDoc`, `PublishPackage` | As named; also `DefineTeamCityVariables`, `Help`.                                              |

Run `CodeStyle` (and `ClangTidy linux-clang-tidy -- --diff_base=main` after a `linux-clang-tidy` build when C++
changed) before calling C++ work done.

## Python

Poetry only (`pyproject.toml`): `poetry run …`, `poetry sync --no-root`. Never `pip`, never the system
Python, never a bare `conan`. Inside the container the venv lives in `/fhome/.cache/pypoetry`.

## Repository map

| Path                      | Content                                                                                   |
|---------------------------|-------------------------------------------------------------------------------------------|
| `source/owl/public/`      | Public API, one folder per module = one namespace (see `.claude/rules/module-layout.md`). |
| `source/owl/private/`     | Implementation, mirrors `public/`; third-party header wrappers in `core/external/`.       |
| `source/owlnest/`         | Editor (`sources/`: panels, documents, undo commands) and runner (`runner/`).             |
| `test/unit/<cat>_tests/`  | Google Test, 20 categories, helpers and all fixtures in `test/unit/test_helper/`.          |
| `test/{bench,fuzz,package}/` | `owl_bench` + baseline, libFuzzer targets, Conan package test (`test_package_folder`).  |
| `engine_assets/`          | What the engine ships: `fonts/`, Slang `shaders/<renderer>/`, `project_templates/`, `logo/`. |
| `engine_demo/`            | Engine Demo, the all-in-one showcase (not a game): `scenes/ huds/ prefabs/ tilesets/ tilemaps/`, `scripts/` `sounds/` `textures/` by area. |
| `tools/`                  | Python tools, not installed: `demo/`, `iso_atlas/` (demo assets), `editor/` (icons), `doc/` (Doxygen filter). |
| `docker_run.sh`           | Runs any command in the build image (all builds go through it).                           |
| `ci/`, `ci_action.py`     | Python CI (`ci/actions/*` extend `BaseAction`, auto-discovered).                          |
| `.teamcity/`              | TeamCity Kotlin DSL (validate with Maven in Docker).                                      |
| `cmake/`, `CMakePresets*` | Build modules and presets; `conanfile.py` lists the dependencies, `conan.lock` pins them. |
| `doc/pages/`              | User documentation (Doxygen + GitHub): `roadmap.md`, `changelog.md`, `design/` pages.     |

## Workflow

- Commits: one short imperative line, no body unless needed, **no `Co-Authored-By` or any attribution**.
  Commit locally on a branch (never on `main`); **never `git push` nor `gh pr create`** — the user does.
- Branch names: **`Feature/*` or `Experiment/*` only** (no `Fix/`, `Test/`, `Perf/`): one branch per PR.
- PR descriptions (when asked): a few bullets, what and why.
- Every PR updates `doc/pages/changelog.md` (`[Unreleased]`, one line per change), the `doc/pages/roadmap.md` badges
  and the relevant `doc/pages/*.md` / `doc/pages/design/*.md`; root
  `CHANGELOG.md` / `ROADMAP.md` change at release. Moving items between release sections is the user's call.
- Comments and docs describe what exists, never its history; only the roadmap and the changelog carry history.
- A release cycle starts with a kickoff PR (version bump + roadmap reorg) before any feature code.
- No new public API without tests; every authored object ships full editor support in the same PR
  (`.claude/rules/ongoing-quality.md`).
- When the user says "CI is green" / "main is clean", the merged code is the truth, even if the roadmap
  says otherwise. Confirm before modifying a working render path.

## Pitfalls that already cost a debugging session

- `math::mat4{1.f}` is **not** identity (positional fill). Use `math::identity<float, 4>()`.
- Slang matrices default to row-major: declare `column_major float4x4` for C++ uploads.
- `getRootPath()` (test helper) loops forever when CWD is the project root.
- Slang 2026.19: ~165 ms cold and 21–35 ms per shader in Release (`test/bench/`, 2026-10); engine shaders are
  precompiled at build time (`OwlShaderBake`), still share the session across tests with `SetUpTestSuite`. Debug + coverage builds are much slower (not measured).
- After changing the layout of a widely included public header (`Scene.h`, components), an incremental
  build can keep a stale object → heap corruption in an unrelated test. Rebuild with `--clean-first`
  before trusting the results.
- Renderer backend invariants (GL global bindings, Vulkan batch/descriptor rules, shared model UBO):
  `.claude/rules/renderer.md`.
- Comment-stripping scripts must keep `// NOLINT*`, `// clang-format on/off`, `// IWYU pragma` lines;
  check that every `NOLINTBEGIN` / `clang-format off` stays balanced.
- MinGW GCC does not export the `static constexpr` members of an `OWL_API` class: a test binding one to a reference
  (`EXPECT_EQ(x, Class::max)`) fails to link on Windows GCC only; pass a copy (`size_t{Class::max}`).
- `std::cerr` / iostreams are banned: logger macros, or `std::println(stderr, …)` before the logger exists.
- Third-party code only through Conan (ConanCenter, else `conan/recipes/`), never vendored.
- Editor SVG icons (`source/owlnest/assets_sources/icons/`) are hand-made: never edit them by script.
