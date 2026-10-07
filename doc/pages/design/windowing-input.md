# Windowing and input {#page-design-windowing-input}

[TOC]

Design page for platform windowing (Wayland), the SDL3 evaluation and input actions, summarised in the
[Roadmap](../roadmap.md).

## Wayland (v0.3.0, platform correctness) — In Progress

Goal: full Wayland support for the editor and the runner, X11 kept as an option; the Owl icon on Wayland; editor
multi-window behaving on both; tested on GNOME and KDE, with a CI smoke test under a headless compositor if feasible.

### Platform selection

GLFW 3.4 is built with both the Wayland and the X11 backends (local Conan recipe `conan/recipes/glfw`, against the
system Wayland: a libwayland shipped next to the binaries would hide the system one from the GPU drivers). The engine
picks one explicitly at `glfwInit`:

| Source                                 | Values                       | Priority |
|----------------------------------------|------------------------------|----------|
| `OWL_WINDOW_PLATFORM` environment var. | `auto`, `wayland`, `x11`     | Highest  |
| `windowPlatform` key in `config.yml`   | same (`AppParams`)           | Default  |
| `OWL_FORCE_X11=1` (deprecated)         | forces `x11`, logs a warning | Legacy   |

`auto` lets GLFW choose: Wayland when `WAYLAND_DISPLAY` is set, X11 otherwise. A platform that fails to initialise
falls back to `auto` with a warning instead of aborting. The platform in use is logged
(`GLFW: Using the wayland platform (requested auto).`) and exposed by `Window::getPlatform()`. The parsing helpers
(`window::parsePlatform`, `resolvePlatform`, `makeAppId`) are pure functions covered by `input_tests`.

### What the engine does per platform

| Concern                | Wayland                                                                    | X11 (native or XWayland)          |
|------------------------|----------------------------------------------------------------------------|-----------------------------------|
| Application identifier | `app_id` = `AppParams::appId`, or the application name made id-safe        | `WM_CLASS` set to the same id     |
| Icon                   | Hidden user desktop entry `<app_id>.desktop` whose `Icon=` is the icon (1) | `glfwSetWindowIcon`               |
| Vertical sync (OpenGL) | Swap interval 0 + frame pacer at the monitor refresh rate (2)              | Swap interval 1                   |
| Vertical sync (Vulkan) | Mailbox when offered, FIFO otherwise                                       | Same                              |
| HiDPI / scale          | `GLFW_SCALE_FRAMEBUFFER` off: framebuffer = window size, compositor scales | Pixels, no scaling                |
| Editor multi-viewports | Disabled (detached panels float inside the main window)                    | Enabled (native detached windows) |
| Cursor capture         | Pointer constraints + relative pointer (GLFW)                              | Grab + raw motion                 |
| Fullscreen             | `xdg_toplevel.set_fullscreen` on the primary monitor; position ignored     | Video mode switch                 |
| Title, resize          | Supported                                                                  | Supported                         |

1. `platform::installDesktopEntry` writes `$XDG_DATA_HOME/applications/<app_id>.desktop` (`NoDisplay=true`, so no
   menu entry) and only rewrites it when its content changes; opt out with `installDesktopEntry: false` in
   `config.yml`. The compositor reads the host's data directories: inside `docker/run.sh` the entry lands in the
   container's `$HOME` and the icon stays generic. KDE may need a second launch (or `kbuildsycoca6`) to notice a new
   entry.
2. `eglSwapInterval(1)` waits for the compositor frame callback, which never comes while the surface is not shown
   (minimised, another workspace, locked session): the main loop froze after one frame and could not even process
   the close request. The compositor never tears, so interval 0 plus a sleep to the refresh period keeps vsync's
   pacing without the hang.

### Diagnosis of the "frozen first frame" (October 2026)

Reported as `OwlRunner` freezing at the first frame in `docker/run.sh --gui`. Measured on a KDE Plasma 6.6 session
(Intel RPL-S + NVIDIA RTX 5000 Ada, PRIME offload), **with the session locked**, frames presented in 15 s:

| Run (OwlRunner, vsync on)            | Before the fix                     | After the fix       |
|--------------------------------------|------------------------------------|---------------------|
| Wayland, OpenGL, Intel or NVIDIA     | 1 frame, then blocked, kill needed | ~850-970 (60 Hz)    |
| Wayland, Vulkan, Intel or NVIDIA     | runs (mailbox)                     | 6400-6800           |
| X11 (XWayland), OpenGL, both GPUs    | ~20 (XWayland throttles to ~1 Hz)  | same, exits on TERM |
| X11 (XWayland), Vulkan, NVIDIA       | ~20                                | same                |
| X11 (XWayland), Vulkan, Intel        | runs (mailbox)                     | 5500                |
| X11 on a private Xvfb, Vulkan NVIDIA | —                                  | ~60 fps, scene OK   |

Owl Nest opens, runs and closes cleanly on `SIGTERM` in the same eight combinations (Wayland / X11 × Vulkan /
OpenGL × Intel / NVIDIA). Hidden on XWayland it runs unthrottled on Intel (~130 fps) but stays at ~1 Hz on NVIDIA,
whose PRIME presents wait for the X server whatever the swap interval.

- **Cause: presentation, not Docker.** The compositor does not present hidden surfaces: native Wayland stops frame
  callbacks altogether, XWayland completes presents at about 1 Hz. Any vsync'ed swap then blocks; `vkcube` shows the
  same throttling on XWayland. Without vsync nothing blocks, which matches the frame-bench observations.
- **Engine part (fixed):** OpenGL under Wayland now paces itself instead of blocking in `eglSwapBuffers`;
  `SIGINT` / `SIGTERM` request a clean shutdown (a second signal kills), so a throttled loop still exits; the window
  logs `GLFW: Window closed after N presented frames.` and `Window::getPresentedFrames()` exposes the count.
- **Docker part:** the socket, `WAYLAND_DISPLAY`, `XAUTHORITY` and `/dev/dri` were already right. `docker/run.sh`
  gained `--gpu=intel|nvidia` (Vulkan ICD, EGL and GLX vendor, PRIME offload), `--platform=wayland|x11`, and warns
  when the desktop session is locked.
- **Not fixed:** X11 + vsync still crawls at ~1 Hz while the window is hidden — that is the X server's throttling.
  OpenGL on llvmpipe (Xvfb without a GPU) crashes on the missing `glSpecializeShader` (no `GL_ARB_gl_spirv`), and
  Mesa's Intel Vulkan driver crashes on Xvfb; neither is a supported target.

### Manual check on a visible desktop (October 2026)

Owl Nest on KDE Plasma 6.6 (Wayland session, Intel + NVIDIA), native and in `docker/run.sh`, in five combinations:
Wayland / X11 (XWayland) × Vulkan / OpenGL × Intel / NVIDIA. Platform selection, fractional scales (125 %, 150 %, text
blurry as expected until the DPI work), fullscreen, cursor capture, hidden / locked window, clean close (button,
Alt+F4, `SIGTERM`), detached panels (inside the window on Wayland, native windows on X11), keyboard (AZERTY, accents,
shortcuts), clipboard, file drag and drop and the file dialog all pass.

| Combination                     | Frames per second (vsync on) |
|---------------------------------|------------------------------|
| Native, Wayland, Vulkan         | ~100 (mailbox)               |
| Native, Wayland, OpenGL         | 60 (monitor rate)            |
| Native, X11 (XWayland), Vulkan  | ~30                          |
| Docker, Wayland, Vulkan, Intel  | ~250 (mailbox)               |
| Docker, Wayland, OpenGL, Intel  | 60                           |
| Docker, Wayland, Vulkan, NVIDIA | ~350 (mailbox)               |
| Docker, X11, Vulkan, NVIDIA     | 60                           |

- On the **first** launch KDE shows the window under the launching application (the IDE) and the generic icon, in
  the task bar only: the desktop entry is written during that launch and KWin reads it from the next one. From the
  second launch the icon, the Alt+Tab entry and the application id are right.
- Inside Docker, OpenAL Soft logs `Failed to create PipeWire event context` and falls back to PulseAudio (sound works):
  the build image lacks PipeWire's client configuration (`libpipewire-0.3-common`).

### Still to verify

GNOME (Mutter): icon, scales, cursor capture, detached panels and the libdecor decorations. The CI smoke test under a
headless compositor waits for `weston` in the build image.

### Known GLFW 3.4 limits under Wayland (argument for the SDL3 evaluation)

- **No window icon.** `glfwSetWindowIcon` is unavailable under Wayland, GLFW does not implement
  `xdg-toplevel-icon-v1` (KWin 6 supports it, `wayland-protocols` 1.45 in the image ships it) and does not expose its
  `xdg_toplevel`, so the engine cannot add it either. The desktop entry is the only route, and it requires writing
  into the user's data directory. SDL 3.2 sets the icon through `xdg-toplevel-icon-v1`.
- **No global window positions.** `glfwSetWindowPos` / `glfwGetWindowPos` are unavailable, so ImGui cannot place
  detached viewports (the ImGui GLFW backend itself refuses viewports under Wayland). Detached panels stay inside the
  main window; X11 (`OWL_WINDOW_PLATFORM=x11`, XWayland) keeps native detached windows. SDL3 hits the same protocol
  limit; whether its ImGui backend handles detached windows better is a question for the evaluation.
- **No fractional scaling.** GLFW 3.4 uses integer `wl_surface.set_buffer_scale` and ignores
  `wp_fractional_scale_v1`; at 125 % or 150 % it renders at 2× and lets the compositor downscale. The engine turns
  framebuffer scaling off and lets the compositor upscale (correct size, slightly soft). SDL3 implements
  fractional scaling with `wp_viewporter`.
- **Blocking vsync on hidden surfaces.** GLFW forwards the swap interval to EGL as is; SDL emulates it with a
  frame-callback wait and a timeout. Owl now does its own pacing for OpenGL.

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
