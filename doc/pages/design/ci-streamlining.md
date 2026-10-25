# CI streamlining {#page-design-ci-streamlining}

[TOC]

Design page of the CI streamlining, summarised in the [Roadmap](../roadmap.md); the CI as it runs is described in
[Continuous integration](../continuous_integration.md).

## Plan

First work of v0.5.0, done before the rest of the cycle: a CI that answers a pull request in minutes. Starting point,
measured on 2026-10-19 and 2026-10-20:

| What                        | Today                                                     | Target                          |
|-----------------------------|-----------------------------------------------------------|---------------------------------|
| Pull request, first verdict | about 35 min (Linux x64 Clang, Debug + coverage + tests)  | under 10 min                    |
| Linux arm64 on `main`       | 48 to 58 min (tests 47.7 min, CTest serial, 53 scenarios) | under 20 min                    |
| Build step                  | 10 to 14 min per preset                                   | halved                          |
| `BuildArtefact.zip`         | 301 MB on Windows GCC, over the 300 MB server limit       | under 50 MB, release files only |

- **Parallelism**: `-j` wherever it applies (CTest on every preset, builds, Conan, the Python checks), CTest resource
  locks only where a test really shares a resource.
- **Lighter pull requests**: a Release build with its tests first, and only the configurations the diff can affect
  (a documentation or Lua-only change skips the C++ builds); Debug, coverage, sanitizers, Windows GCC and arm64 run
  on `main` and at night.
- **Full suites at night only**: every scenario, the export of every project, the sanitizers, emulated arm64, the
  benchmarks; a pull request runs a fast subset that still covers every feature.
- **Shorter tests**: scenarios play the fewest frames that prove their behaviour, on small fixture maps (today
  `hub_progress` takes 917 s and `iso_town_city` 649 s under qemu); heavy ones labelled `slow`; the end-to-end export
  split into one CTest case per project; the slowest unit binaries (`scene_tests`, `owlnest_tests`, `font_tests`)
  profiled and trimmed.
- **Shorter builds**: measure first (`.ninja_log`, ccache hit rate per agent, the share of the coverage build), then
  precompiled headers, unity builds, fewer includes in widely used public headers, less template code where the
  measure points; share the ccache and the Conan binaries between agents.
- **Smaller artefacts**: keep only what a later step or a person needs (packages, test reports, logs), no Debug
  binaries nor static libraries in `BuildArtefact.zip`, short retention for the rest.
- **Fewer duplicate builds**: no queue of identical Code Style runs on the same commit, a newer push to a branch
  cancels the older builds.

## Status

| Item                   | State                                                                                                            |
|------------------------|------------------------------------------------------------------------------------------------------------------|
| Parallelism            | Done: CTest `-j` on every preset, `RESOURCE_LOCK` removed, image and export tests sharded                        |
| Lighter pull requests  | Done: release tree configured only, GCC, ASan, arm64, whole-tree Clang-Tidy and Include Check off PRs            |
| Full suites at night   | Done: `slow` label, nightly full suite on Linux x64 Clang and arm64                                              |
| Clang-Tidy             | Done: configured, not compiled; diff scope from `clang-scan-deps`, CMake changes compared against the merge base |
| Compiler cache         | Done: PCH sloppiness passed to ccache (warm `linux-clang-release` 108 s → 4 s); ccache 4.14 on the Windows agent |
| Smaller artefacts      | Done: test reports, coverage, documentation and packages only, no binaries nor libraries                         |
| Fewer duplicate builds | Done: the bridge plugin cancels superseded builds and reuses a green verdict for the same commit                 |
| Shorter builds         | Done: measured (cold `linux-clang-release`: 2685 s CPU, tests 40 %, Owl Nest 29 %), ccache warm on every agent   |
| Single tree            | Done: one tree per configuration, tested then packaged; Linux Clang Release is the fast verdict                  |

## Cost review 2026-10 {#ci-streamlining-cost-review}

Measured on 20 merged pull requests (#307 to #326), `main` from 19 to 24 October and 14 nights; agent-minutes over
the window.

| Build                    | Trigger | Median (min) | Agent-min | Unique failures | Note                          |
|--------------------------|---------|-------------:|----------:|----------------:|-------------------------------|
| Code Style               | PR      |          1.3 |        52 |               0 |                               |
| Linux Clang              | PR      |           17 |       432 |               2 |                               |
| Windows Clang            | PR      |         25.5 |       541 |               6 | first to fail 6 times         |
| Clang-Tidy               | PR      |           16 |       462 |               3 |                               |
| Bench                    | PR      |           10 |       265 |               8 |                               |
| Linux GCC                | main    |           17 |       405 |               1 |                               |
| Windows GCC              | main    |           28 |       624 |               2 | duplicates Linux GCC          |
| Sanitizer Address        | main    |           18 |       362 |               2 |                               |
| Sanitizer Thread         | nightly |           22 |       317 |               0 |                               |
| Sanitizer UB             | nightly |         15.5 |       246 |               0 |                               |
| Static Analyzer          | nightly |           50 |       630 |               2 |                               |
| Fuzz                     | nightly |         14.5 |       181 |               0 |                               |
| Include Check            | nightly |           36 |       106 |               1 |                               |

Decisions:

- One GCC build only (Debug, tests and coverage, on pull requests and `main`); Windows GCC is removed.
- One tree per configuration: Release for the pull-request verdict, the tests and the packages, Debug only for GCC and the nightly Clang.
- The nightly suites and Bench stay as they are (the Bench reds were explained changes or real regressions).
- The single Windows agent `hecate` is the bottleneck (31 min median queue on `main`).
- GitHub branch protection "require branches to be up to date" is on, with the admin bypass kept for trivial pull
  requests.

Measured on the last green `main` builds before the single-tree layout:

| Build                   | Total (min) | Detail                                                                                 |
|-------------------------|------------:|----------------------------------------------------------------------------------------|
| Linux Clang             |          31 | debug tests 8 min against 2 min 20 for release tests                                   |
| Windows Clang           |        32.5 | debug build 17.5 min at 59 % ccache; debug tests 5 min 52 against 1 min 03 for release |
| Linux arm64             |          65 | debug tests 50 min under QEMU                                                          |
| Package arm64 (rebuild) |          16 | ccache hit rate 2.7 %                                                                  |

Done in this cycle:

- A fast stage 0: a draft pull request runs Code Style and Linux x64 Clang Release only.
- One GCC build (Debug, tests, coverage); the Clang Debug build with its coverage runs nightly.
- A `/check --quick` skill (with `--fix`) for agents; no git hook.

Planned, in parallel with v0.6.0 development:

- The Android, Web and arm64 runners (v0.8.0) build at night and at release, on a pull request only when its diff
  touches their backend.
