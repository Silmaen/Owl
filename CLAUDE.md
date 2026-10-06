# CLAUDE.md

Owl is a C++23 game engine (`OwlEngine`) with an editor (Owl Nest), a game runner, a Python CI wrapper and
TeamCity pipelines. Version in development: see `project(... VERSION ...)` in `CMakeLists.txt` (0.3.0 in
development at the time of writing; 0.2.1 is the last release). Backends: graphics OpenGL 4.5 / Vulkan 1.4 / Null, input
GLFW / Null, sound OpenAL / Null. ECS on EnTT with parent-child hierarchy. Platforms: Linux x64/arm64,
Windows x64 (MinGW).

Detailed, path-scoped rules load automatically from `.claude/rules/` when you touch matching files
(C++ style, CMake, tests, renderer, Slang, scene, editor, script, task system, dependencies, Python CI,
docs, module layout). User documentation lives in `doc/pages/*.md` — read the relevant page before
changing a subsystem. An ongoing repository audit lives in `doc/audit/` (entry point: `AUDIT.md`).

## Build, test, run — Docker only

Every compiler, CMake, CTest, Poetry, clang-tidy or clang-format call runs **inside the build image**
through `docker/run.sh`, which mirrors CLion's *Docker Owl* toolchain (image
`registry.argawaen.net/builder/devel-ubuntu2404:latest`, your UID/GID, repo mounted at its host path,
`$HOME` = `../fake_home` → `/fhome` holding the Poetry venv, the DepManager cache and ccache). Never build
natively, never install a tool on the host; if a tool is missing from the image, say so.

```bash
docker/run.sh cmake --preset linux-clang-release -S .          # configure (fetches deps)
docker/run.sh cmake --build output/build/linux-clang-release   # ~1 min full build on 32 cores
docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure -j8
docker/run.sh output/build/linux-clang-release/bin/owl_<cat>_tests_unit_test --gtest_filter='Suite.*'
docker/run.sh --gui output/build/linux-clang-release/bin/OwlNest   # GPU + display + audio
docker/run.sh --perf perf ...                                      # ptrace / perf events
```

- **Clang presets only** for dev, tests and coverage: `linux-clang-release` (default),
  `linux-clang-debug` (coverage on). GCC presets exist for CI parity only.
- Other presets: `linux-clang-tidy`, `linux-sanitizer-{address,thread,undefined-behavior,leak}`,
  `windows-{gcc,clang}-{release,debug}`, `package-{engine,app-nest}-{linux,windows}`.
- Output: `output/build/<preset>/{bin,lib}`, install in `output/install/<preset>/`.
- If a build dir ends up root-owned, chown it back through a throwaway root container.
- Test binaries are `owl_<folder>_unit_test` (e.g. `owl_scene_tests_unit_test`); new `.cpp` files in
  `test/<cat>_tests/` and anywhere in `source/` are picked up by `GLOB_RECURSE` (re-run configure).

## CI actions (`ci_action.py`)

`docker/run.sh poetry run python ci_action.py <Action> <preset> [-- --opt=value]`

| Action                                             | What it does                                                                                   |
|----------------------------------------------------|------------------------------------------------------------------------------------------------|
| `Build`                                            | Configure + build the preset.                                                                  |
| `Test`                                             | CTest with reports.                                                                            |
| `Coverage`                                         | gcovr through `gcovr.cfg` (never pass filters on the CLI); use `linux-clang-debug`.            |
| `CodeStyle`                                        | Read-only gate: clang-format, codespell, comment quality, `m_*` docs, cpp-style bans, headers. |
| `ClangTidy`                                        | clang-tidy (or `--tool=analyzer`) over `compile_commands.json`; PR = diff-scoped.              |
| `Documentation`                                    | Doxygen with `WARN_AS_ERROR=YES`.                                                              |
| `Package`, `Clean`, `PublishDoc`, `PublishPackage` | As named; also `DefineTeamCityVariables`, `Help`.                                              |

Run `CodeStyle` (and `ClangTidy linux-clang-tidy -- --diff_base=main` after a `linux-clang-tidy` build when C++
changed) before calling C++ work done.

## Python

Poetry only (`pyproject.toml`): `poetry run …`, `poetry sync --no-root`. Never `pip`, never the system
Python, never a bare `depmanager`. Inside the container the venv lives in `/fhome/.cache/pypoetry`.

## Repository map

| Path                      | Content                                                                                   |
|---------------------------|-------------------------------------------------------------------------------------------|
| `source/owl/public/`      | Public API, one folder per module = one namespace (see `.claude/rules/module-layout.md`). |
| `source/owl/private/`     | Implementation, mirrors `public/`; third-party header wrappers in `core/external/`.       |
| `source/owlnest/`         | Editor (`sources/`: panels, documents, undo commands) and runner (`runner/`).             |
| `test/<cat>_tests/`       | Google Test, 16 categories, helpers in `test/test_helper/`.                               |
| `engine_assets/`          | Fonts, Slang shaders (`shaders/<renderer>/slang/`), textures, logo.                       |
| `sample_project/`         | Feature showcase game: every engine feature must be demonstrated there.                   |
| `ci/`, `ci_action.py`     | Python CI (`ci/actions/*` extend `BaseAction`, auto-discovered).                          |
| `.teamcity/`              | TeamCity Kotlin DSL (validate with Maven in Docker).                                      |
| `cmake/`, `CMakePresets*` | Build modules and presets; `depmanager.yml` pins ~36 dependencies.                        |
| `doc/pages/`              | User documentation (Doxygen + GitHub): `roadmap.md`, `changelog.md`, `design/` pages.     |

## Workflow

- Commits: one short imperative line, no body unless needed, **no `Co-Authored-By` or any attribution**.
  Commit locally on a branch (never on `main`); **never `git push` nor `gh pr create`** — the user does.
- PR descriptions (when asked): a few bullets, what and why.
- Every PR updates `doc/pages/changelog.md` (`[Unreleased]`, one line per change), the `doc/pages/roadmap.md` badges
  and the relevant `doc/pages/*.md` / `doc/pages/design/*.md`; root
  `CHANGELOG.md` / `ROADMAP.md` change at release. Moving items between release sections is the user's call.
- A release cycle starts with a kickoff PR (version bump + roadmap reorg) before any feature code.
- No new public API without tests; every authored object ships full editor support in the same PR
  (`.claude/rules/ongoing-quality.md`).
- When the user says "CI is green" / "main is clean", the merged code is the truth, even if the roadmap
  says otherwise. Confirm before modifying a working render path.

## Pitfalls that already cost a debugging session

- `math::mat4{1.f}` is **not** identity (positional fill). Use `math::identity<float, 4>()`.
- Slang matrices default to row-major: declare `column_major float4x4` for C++ uploads.
- `getRootPath()` (test helper) loops forever when CWD is the project root.
- Slang: ~74 ms cold and ~20 ms per shader in Release (`bench/`, 2026-10); still share the session across
  tests with `SetUpTestSuite`. Debug + coverage builds are much slower (not measured yet).
- After changing the layout of a widely included public header (`Scene.h`, components), an incremental
  build can keep a stale object → heap corruption in an unrelated test. Rebuild with `--clean-first`
  before trusting the results.
- Renderer backend invariants (GL global bindings, Vulkan batch/descriptor rules, shared model UBO):
  `.claude/rules/renderer.md`.
- Comment-stripping scripts must keep `// NOLINT*`, `// clang-format on/off`, `// IWYU pragma` lines;
  check that every `NOLINTBEGIN` / `clang-format off` stays balanced.
- `std::cerr` / iostreams are banned: logger macros, or `std::println(stderr, …)` before the logger exists.
- Third-party code only through DepManager, never vendored.
- Editor SVG icons (`source/owlnest/assets_sources/icons/`) are hand-made: never edit them by script.
