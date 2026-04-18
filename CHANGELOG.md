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
- **Multi-document architecture**
    - New abstractions in `source/owlnest/sources/document/`: `Document` (interface),
      `DocumentManager` (open list + active tracking), `SceneDocument` (scene wrapper),
      `DocumentTabBar` (tab bar rendered inside the Viewport header, with dirty `*`, play/pause
      badge, close prompt)
    - Scene-scoped state (`editorScene`, `activeScene`, path, Play/Pause/Stop, teleport request,
      save/load request, undo stack) moved from `EditorLayer` into `SceneDocument`
    - `EditorLayer` becomes a host of `DocumentManager`; all scene operations delegate to the
      active document
    - Each open scene is its own tab rendered **inside the Viewport panel** (no separate
      "Documents" window). `File > Open Scene` opens in a new tab (or switches to the existing
      one when already open), `File > New Scene` creates an Untitled tab
    - Play/Pause/Stop/Step toolbar + gizmo control bar are hidden when the active tab is not the
      document currently running (only one document can Play at a time)
    - Per-document undo/redo stack (Ctrl+Z / Ctrl+Y acts on the active doc)
    - New shortcuts: `Ctrl+W` close active document, `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle
    - Dirty close prompt: closing a tab with unsaved changes opens a "Discard changes / Cancel"
      modal
    - Background simulation: non-active tabs in Play mode advance their physics/scripts without
      rendering (new `iRender` flag on `Scene::onUpdateRuntime`); only the active tab issues the
      Renderer2D draw pass, which eliminates visual flicker between tabs
    - `gui::BasePanel` gains an optional `onHeaderRender()` hook so panels can inject tab bars or
      toolbars above their content region
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
- **Reorganized menu structure**
    - **File**: project operations (New, Open, Recent, Save, Close, Pack Game, Welcome, Exit)
    - **Edit**: Undo/Redo + Engine Settings, Editor Settings, Project Settings
    - **Current**: scene operations (New Scene, Open Scene, Save, Save As, Import Scene, Pack Scene)
    - "Show Stats" moved into Editor Settings panel (General section)
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

### Fixed

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
