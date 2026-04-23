# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] (0.1.1-dev)

### Added

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
      accept an optional `std::vector<std::string>* oWarnings` output)
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
    - `Ribbon::setTabHighlighted()` renders the tab title with the theme accent (secondary) color
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
    - ContentBrowser double-click routes `.lua/.py/.c/.cpp/.cc/.cxx/.h/.hpp/.hxx/.yml/.yaml/.json/
      .md/.markdown/.svg/.xml` to a new code-editor tab (or re-activates the existing one)
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
      own framebuffer and a stable ImGui window id (`##scene_<uuid>`). ImGui's docking groups
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
        - Replaced `<text>`-rendered glyphs with paths in `interaction` (letter E) and
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
    - Component-scoped `PushID(T::name())` in `drawComponent<T>` to prevent ImGui label collisions
      across components that share field names (e.g., "Color" in multiple renderers)
    - Index-based `PushID` in LuaScript property loop

### Changed

- Version bumped to **0.1.1-dev**
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
- **Windows Debug test binaries no longer fail with `STATUS_DLL_NOT_FOUND` (0xc0000135)** —
  the helper `owl_target_link_libraries()` was setting `CMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release`
  only around `find_package()`, but that variable is read at generate time (link resolution +
  `$<TARGET_RUNTIME_DLLS>` evaluation), not at find time. In Debug builds the linker picked
  `glfw3d.lib` (imports `glfw3d.dll`) while the post-build copy grabbed the release `glfw3.dll`
  — every test binary then failed to load. The mapping is now applied at top-level directory
  scope in `CMakeLists.txt`, gated by `OWL_USE_RELEASE_THIRD_PARTY`, so link and DLL copy agree
- Tests now get a `$<TARGET_RUNTIME_DLLS:<test>>` post-build copy next to their binary (belt-
  and-braces — picks up any DLL surfaced through the test's own link graph that the engine's
  post-build didn't cover)
- **Use-after-free SIGSEGV when closing a document tab** — closing a `SceneDocument` inline from
  `onImGuiRender` freed its Vulkan color-attachment while ImGui's draw list still referenced it
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
- `writeLinuxLauncher` undefined on Windows (guarded with `#ifdef OWL_PLATFORM_LINUX`)
- `game_settings.yml` not included in packed game
- Markdown table alignment across all doc pages
- Clang-tidy cognitive complexity in UIInputSystem and AssetScanner (extracted helper functions)
- LuaBindings: `const lua_State*` incompatibility with `lua_CFunction` signature (added NOLINT)
- Flaky tests: shared `output/test_tmp` directory replaced with unique per-test paths in
  `std::filesystem::temp_directory_path()`
- First-time Slang shader compilation no longer freezes the window — loading screen animates
  between each shader

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
