# Windowing and input {#page-design-windowing-input}

[TOC]

Design page for platform windowing (Wayland), the SDL3 evaluation and input actions, summarised in the
[Roadmap](../roadmap.md).

## Wayland (v0.3.0, platform correctness)

- Full Wayland support for the editor and the runner, X11 kept as an option (environment variable or setting)
- The Owl icon shows on Wayland (window icon / desktop entry with the matching `app_id`)
- Editor multi-window works: ImGui detached windows (viewports) open, move and dock back under Wayland
- Tested on GNOME and KDE sessions; a CI smoke test runs under a headless Wayland compositor if feasible

### Known GLFW limits under Wayland

GLFW cannot set a window icon under Wayland (the protocol leaves it to the desktop entry) and cannot position windows
in global coordinates, which breaks the placement of ImGui detached windows. Workarounds exist (desktop file with
`app_id`, viewport placement relative to the main window), but these limits are a **concrete argument for the SDL3
evaluation** below.

## SDL3 evaluation (v0.3.0, To evaluate)

- ![To evaluate][evaluate] SDL3 for windowing, input, dialogues and audio, possibly SDL GPU as an Owl RHI backend
- Questions to answer: does SDL3 solve the Wayland icon and multi-window placement; what does it replace (GLFW, NFD,
  OpenAL Soft); cost of the switch; impact on the public dependency count
- No commitment: the maintainer is not convinced; the outcome is a short written verdict

## Input actions and basic gamepad (v0.4.0)

- Named actions bound to keys, mouse buttons and gamepad buttons / axes; Lua reads actions, not keys (K-30)
- Gamepad backend on GLFW (or SDL3 if adopted), so the gamepad mentioned in the README becomes true (I-03, D-25)
- Editor: action map edited in Project Settings, undoable

## Gamepad improvements (v0.10.0)

- Controller remapping UI
- Haptic feedback / vibration API
- Analogue stick dead zone and curve configuration

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
