# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] (0.1.1-dev)

### Added

- Async operations in editor with progress modal (AsyncProgressModal panel)
  - Pack Game and Pack Scene: async with per-entry progress bar and cancel support
  - Save Scene: serialize on main thread, write file on background thread
  - Open Scene: read file on background thread, deserialize on main thread
- Deferred shader compilation with ImGui loading screen at startup
  - `Renderer::initContext()` / `Renderer::initShaders(callback)` split
  - Per-shader progress displayed before editor/runner starts
- PackWriter progress callback and cancel check support
- AssetScanner improvements: scans UIImage, AnimatedSpriteRenderer, all trigger LevelNames, sound.play() in Lua, deferred scene.load_scene patterns
- AssetScanner unit tests (18 tests covering all asset types, recursion, deduplication, cancellation)
- Mermaid diagram support in Doxygen documentation (via CDN mermaid.js)
- Mermaid diagrams in architecture, scene, scripting, editor, sound, physics doc pages
- GitHub community files (CONTRIBUTING, CODE_OF_CONDUCT, SECURITY, CHANGELOG)
- `scene.quit()` Lua API for clean application exit
- `SettingsManager::loadDefaultsFromString()` for pack-based settings loading
- Recent projects: persisted list (up to 10) in `EditorSettings::recentProjects`
- "Open Recent" submenu in File menu with per-entry tooltip
- Welcome screen modal shown when no project is loaded, closable via `x` button or `File > Welcome Screen`
- Reorganized menu structure:
  - **File**: project operations (New, Open, Recent, Save, Close, Pack Game, Welcome, Exit)
  - **Edit**: Undo/Redo + Engine Settings, Editor Settings, Project Settings
  - **Current**: scene operations (New, Open, Save, Save As, Import, Pack Scene)
- "Show Stats" moved into Editor Settings panel (General section) — no longer a menu item
- Doxygen favicon using owl logo (32x32 + 16x16)

### Fixed

- AssetScanner: absolute path resolution for sounds and scenes (resolveSound, resolveScene)
- AssetScanner: skip nonexistent scene files instead of adding them to the asset list
- `recursive_directory_iterator` crash on missing directories (AssetLibrary, FontLibrary)
- Mouse Y inversion in packed runner UI (UIInputSystem coordinates)
- `writeLinuxLauncher` undefined on Windows (guarded with `#ifdef OWL_PLATFORM_LINUX`)
- `game_settings.yml` not included in packed game
- Markdown table alignment in all doc pages
- Clang-tidy cognitive complexity in UIInputSystem (extracted helper functions)
- LuaBindings: removed unused `iArgIndex` parameter, fixed `const lua_State*` incompatibility

### Changed

- Version bumped to 0.1.1-dev

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
- Assets not found in packed game (AssetScanner now scans UIImage, AnimatedSpriteRenderer, Death/Victory triggers,
  sound.play in Lua)
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
