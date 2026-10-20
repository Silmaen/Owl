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
