# Windowing and input {#page-design-windowing-input}

[TOC]

Design page for platform windowing (Wayland), the SDL3 evaluation and input actions, summarised in the
[Roadmap](../roadmap.md).

## Wayland (v0.3.0, platform correctness) — Done

Goal: full Wayland support for the editor and the runner, X11 kept as an option; the Owl icon on Wayland; editor
multi-window behaving on both; tested on GNOME and KDE, with a CI smoke test under a headless compositor if feasible.

### Platform selection

GLFW 3.5.1 is built with both the Wayland and the X11 backends (local Conan recipe `conan/recipes/glfw`, against the
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
| Decorations            | Server-side when offered (libdecor then skipped), else libdecor            | Window manager                    |

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

GNOME (Mutter) passes the same checks.

### CI smoke test

`test/wayland_smoke.sh` (CTest `owl_wayland_smoke`, labels `render` and `wayland`) starts a headless weston (pixman
renderer, private `XDG_RUNTIME_DIR`), then plays 30 frames of `test/render_tests/scenes/sprites.owl` with
`OwlRunner --frame-bench` on the Wayland platform, Vulkan on lavapipe then OpenGL on llvmpipe; it fails when the
runner crashes, logs an error or did not pick Wayland. Without `weston` it reports *Skipped*. It found that GLFW 3.4
dereferenced a missing `wl_seat` (a compositor without input devices): the local recipe patched it until GLFW 3.5.1
fixed it upstream.

### Known GLFW 3.5.1 limits under Wayland

- **No window icon.** `glfwSetWindowIcon` is still unavailable under Wayland in 3.5.1: GLFW does not implement
  `xdg-toplevel-icon-v1` (KWin 6 supports it, `wayland-protocols` 1.45 in the image ships it) and does not expose its
  `xdg_toplevel`, so the engine cannot add it either. The desktop entry is the only route, and it requires writing
  into the user's data directory.
- **No global window positions.** `glfwSetWindowPos` / `glfwGetWindowPos` are unavailable, so ImGui cannot place
  detached viewports (the ImGui GLFW backend itself refuses viewports under Wayland). Detached panels stay inside the
  main window; X11 (`OWL_WINDOW_PLATFORM=x11`, XWayland) keeps native detached windows. This is a protocol limit, not
  a GLFW one: SDL3 has the same.
- **Fractional scaling is there, Owl does not use it yet.** Since 3.4 GLFW implements `wp_fractional_scale_v1` with
  `wp_viewporter` when `GLFW_SCALE_FRAMEBUFFER` is on (its default). Owl turns that hint off because the swapchain,
  the viewports and ImGui are sized from the window size: the buffer stays at 1× and the compositor upscales it
  (correct size, slightly soft at 125 % or 150 %). Turning it on needs framebuffer-size resize events, ImGui's
  `DisplayFramebufferScale` and picking in framebuffer pixels: engine work, not a GLFW gap.
- **Vsync on hidden surfaces.** GLFW 3.5.1 no longer blocks: under Wayland it drives the swap interval itself, waiting
  for the frame callback with a 20 ms timeout. Owl still paces OpenGL itself: on the headless weston of the smoke
  test, interval 1 gives irregular frames (median 13.3 ms, p95 20.8 ms, the timeout firing every other frame) where
  the pacer holds 16.67 ms ± 0.1 ms. To be measured again on a visible desktop before switching.
- **libdecor loads GTK.** With libdecor, `glfwInit` loads its GTK plugin (GTK, Pango, the glycin image loader and its
  threads) even when the compositor draws the decorations itself. The engine asks the compositor first
  (`zxdg_decoration_manager_v1`, KWin, wlroots, COSMIC) and tells GLFW to skip libdecor then: start-up drops from
  309 ms to 279 ms on KDE Plasma 6 (Vulkan, warm, median of 3). GNOME and weston have no server-side decorations
  and keep libdecor.

## SDL3 evaluation (v0.3.0, Done): rejected

- ![Done][done] Evaluation of SDL3 for windowing, input, dialogues and audio, possibly SDL GPU as an Owl RHI backend
- **Decision (October 2026): Owl stays on GLFW. SDL3 is rejected for the window, the input, the dialogues and the
  gamepads alike; OpenAL Soft is kept; SDL GPU is rejected.** Another alternative may be looked at later, not SDL3.

Why:

- GLFW 3.5.1 closes most of the Wayland gaps the evaluation started from: fractional scaling exists (Owl's choice to
  turn it off, see above), vsync no longer hangs on hidden surfaces. The remaining ones (window icon, global
  positions) are protocol limits SDL3 shares or depends on the compositor for.
- The native modal file dialogues of NFD (portal, GTK, Win32) are preferred over SDL's asynchronous
  `SDL_Show*Dialog`, which would change the editor's Open / Save As flow.
- Gamepads stay on GLFW too (`glfwGetGamepadState`, SDL-compatible mapping database through
  `glfwUpdateGamepadMappings`); rumble and hot-plug details are not worth a second windowing library.
- A backend swap of about 12 files plus a release with two backends to test, for no feature Owl lacks.

### What SDL3 would have brought

| Owl pain (GLFW)                 | SDL 3.4                                                                                        | GLFW 3.5.1                                     |
|---------------------------------|------------------------------------------------------------------------------------------------|------------------------------------------------|
| No Wayland window icon          | `SDL_SetWindowIcon` uses `xdg-toplevel-icon-v1` (needs a compositor that has it: KWin, Mutter) | Not supported: desktop entry                   |
| Fractional scaling              | `wp_fractional_scale_v1` + `wp_viewporter`; `SDL_GetWindowPixelDensity` / `DisplayScale`       | Same protocols with `GLFW_SCALE_FRAMEBUFFER`   |
| Blocking vsync on hidden window | frame-callback swap with a timeout                                                             | Same since 3.5.1 (20 ms timeout)               |
| Window placement                | Same protocol limit: no global position under Wayland (`SDL_GetWindowPosition` is relative)    | Same limit                                     |
| Gamepad (I-03, D-25)            | Joystick / gamepad API, mapping database, rumble and hot-plug built in                         | Gamepad API and mapping database, no rumble    |
| File dialogs                    | `SDL_ShowOpenFileDialog` / `SaveFileDialog` / `OpenFolderDialog`, asynchronous                 | None: NFD, modal (kept)                        |
| Text input, IME, clipboard      | Built in (IME and composition are better than GLFW)                                            | Char callback and clipboard                    |

SDL audio is a stream API without 3D positioning, EFX or HRTF: it never was a replacement for OpenAL Soft.

### Prototype (scratchpad, throwaway)

SDL 3.4.16 compiled through Conan in the build image (profile `linux-clang`, Release, shared). With the Wayland
driver forced under headless weston 14: window created with `SDL_WINDOW_VULKAN | HIGH_PIXEL_DENSITY`, driver
`wayland`, pixel density 1.0, events pumped, clean quit. `SDL_SetWindowIcon` failed with *required
xdg_toplevel_icon_v1 protocol not supported*: weston 14 lacks the protocol.

### Cost that was weighed

- `libSDL3.so` is 3.8 MB in Release with Wayland, X11, OpenGL, Vulkan and dbus (GLFW is well under 1 MB, NFD small).
- Recipe options to tune (`pulseaudio`, `alsa`, `sndio`, `libusb` off), `libudev-dev` in the build image for
  joystick hot-plug.
- About 12 files behind the `window`, `input` and `GraphContext` seams, the ImGui backend, the key-code translation,
  and a release with both backends.

### SDL GPU: rejected

SDL GPU has no OpenGL backend and its own shader and resource model: it would be a third RHI path next to the
existing OpenGL and Vulkan ones, for no feature Owl lacks. The Owl RHI work (Phase C) stays on Vulkan and OpenGL.

## Input actions and basic gamepad (v0.4.0)

- Named actions bound to keys, mouse buttons and gamepad buttons / axes; Lua reads actions, not keys (K-30)
- Gamepad backend on GLFW (`glfwGetGamepadState`, mappings through `glfwUpdateGamepadMappings`), so the gamepad
  mentioned in the README becomes true (I-03, D-25)
- Editor: action map edited in Project Settings, undoable

## Gamepad improvements (v0.10.0)

- Controller remapping UI
- Haptic feedback / vibration API
- Analogue stick dead zone and curve configuration

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square
