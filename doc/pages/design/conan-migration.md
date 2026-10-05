# Conan migration and dependencies {#page-design-conan-migration}

[TOC]

Design page for the v0.3.0 dependency work, summarised in the [Roadmap](../roadmap.md) and
[Foundations](foundations.md).

## Goal

Anyone can build Owl from public infrastructure. DepManager (packages served by the maintainer's server) is replaced by
**Conan 2**, preferring **ConanCenter** recipes; in-house recipes are avoided and listed honestly when they remain.
The engine itself is published as a Conan package. DepManager and `owl_engine.py` are removed at the end of the cycle.

## Place in the release

This is **Phase 0** of v0.3.0 ([Foundations](foundations.md)): it runs first, before the correctness work, so the
riskiest items (missing recipes, versions absent from ConanCenter, breaking upgrades) surface early. The reduction of
public dependencies comes later, in phase D, once the chain is stable.

## Steps

Order inside the phase: missing recipes and absent versions first, then the breaking upgrades, then the switch of
every preset, then the engine package.

- Fix the install tree first (PR-08: G-01, G-02, G-06, A-19, G-19, F-06): `#include <owl.h>` works, no
  `-Weverything` imposed on consumers, `CMAKE_INSTALL_PREFIX` honoured
- `conanfile.py` for the engine, with a `test_package` building a consumer against the installed package
  (PR-09: G-03); the test runs in the `Package` CI action
- Conan profiles for Linux GCC / Clang, Linux arm64 and Windows MinGW; the presets call Conan instead of DepManager
- Lockfile committed; an update report in CI replaces the manual check of lagging versions (G-08)
- DepManager, `depmanager.yml`, `cmake/Depmanager.cmake` and `owl_engine.py` removed once every preset builds

## In-house recipes likely to remain

Checked on 2026-10-05 against `conan-io/conan-center-index` (`recipes/` and `config.yml`, `master` branch).

| Dependency             | ConanCenter (2026-10-05)              | Consequence                                        |
|------------------------|---------------------------------------|----------------------------------------------------|
| Slang                  | absent                                | In-house recipe (or packaged upstream binaries)    |
| ufbx                   | absent                                | In-house recipe (single source file, simple)       |
| imgui_color_text_edit  | absent                                | In-house recipe                                    |
| ImGuizmo bundle 1.92.7 | `imguizmo` cci.20231114 and 1.83 only | In-house recipe while the bundle tracks imgui 1.92 |
| nfd (extended, btzy)   | only `nativefiledialog` 116 (mlabbe)  | In-house recipe, or gone with the v0.5.0 picker    |
| tinyobjloader          | 2.0.0-rc10 (Owl: rc13)                | Step back to rc10 or in-house recipe               |
| debugbreak             | absent                                | Made private or replaced                           |
| zeus                   | absent                                | Not needed: replaced by `std::expected`            |

EnTT 4.0.0 (released 2026-07-23) and Taskflow 4.1.0 are not on ConanCenter yet; OpenAL Soft 1.25 and msdfgen 1.13
neither. Upgrading them means waiting, or contributing the new versions to ConanCenter.

Available on ConanCenter at the pinned version or newer: box2d 3.1.1, cpptrace 1.0.4, entt 3.16.0, freetype,
glad 2.0.8, glfw, gtest, imgui 1.92.9b (with `-docking`), libdwarf, libpng, libsndfile 1.2.2, lua 5.5.0,
lunasvg 3.5.0, magic_enum 0.9.8, md4c 0.5.2, msdf-atlas-gen 1.3, spdlog 1.17.0, spirv-cross and vulkan-headers
1.4.357.0, vulkan-loader, vulkan-validationlayers, stb, taskflow 4.0.0, tinygltf 2.9.7, yaml-cpp 0.9.0, rapidyaml,
zlib, zstd — plus tracy, miniaudio, volk, vulkan-memory-allocator, joltphysics and sdl for later work.

## Upgrades (all of them, breaking ones included, in Phase 0)

- EnTT 4 (before the open component registry, PR-37), Taskflow 4.1, yaml-cpp 0.9 (or its replacement)
- The lagging ones (G-08): FreeType, libpng, msdfgen, nfd, OpenAL Soft, spdlog, tinygltf, ufbx, zlib
- tinyobjloader out of its release candidate if upstream allows

## Fewer public dependencies (phase D)

Target: `find_package(OwlEngine)` exposes **EnTT** only, plus **imgui** through the optional `Owl::Gui` target.

- spdlog behind an `owl::log` facade
- yaml-cpp out of the public headers (A-03, G-07); rapidyaml as a private replacement to evaluate (fiche 5)
- zeus replaced by `std::expected`; debugbreak private
- tinyxml2 removed (declared, never used)
- To evaluate: miniaudio instead of OpenAL Soft + libsndfile; a single image loader instead of stb_image + libpng
