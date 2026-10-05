# Game export {#page-design-game-export}

[TOC]

Design page for exporting a game authored in Owl Nest, summarised in the [Roadmap](../roadmap.md).

## v0.3.0 — export that works end to end (correctness)

Exporting exists (Pack Game, packaging wizard, runner), but nothing proves it works end to end. Phase A of
[Foundations](foundations.md) finalises and tests it:

- Packaging: runner binary, shared libraries, `runner.yml`, `game_info.yml`, launcher script (Linux) / `.zip` (Windows)
- Pack: every referenced asset found by `AssetScanner` and readable from the `.owlpack`
- Runner: voxel scenes meshed in the runner (D-03), shaders precompiled in the pack, window title and icon
- Automated test: CI exports `sample_project/`, launches the packaged runner headless, plays every scene for N frames
  and fails on any error log or missing asset

## v0.10.0 — cross-platform packaging

- Cross-compile packaging from any host
    - Package a Linux game from Windows and a Windows game from Linux
    - Pre-built runner binaries per target platform (downloaded or bundled)
    - Cross-platform shared library bundling (resolve target-platform `.so`/`.dll`)
- Target platform selector in Pack Game
    - Choose target: Linux x64, Windows x64, Web (independently of host)
    - Automatic runner binary selection for target platform
    - Platform-specific post-processing (launcher script for Linux, .zip for Windows, HTML template for the Web)
