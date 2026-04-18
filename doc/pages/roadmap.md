# Roadmap {#page-roadmap}

[TOC]

This page tracks planned and completed features across Owl releases.

**Ongoing across all releases:**

- ![Planned][planned] Profiling tools
    - In-editor frame profiler (CPU/GPU timeline)
    - Memory usage breakdown by asset type
    - Entity/component count statistics
- ![Planned][planned] Rendering optimizations
    - Spatial partitioning (quadtree/octree) for culling
    - Batched draw calls, texture atlasing
    - Multithreaded render preparation

## v0.5.0 -- Expected 2027-02-01

**Goal:** Let players extend and modify games built with Owl. Bring Owl games to more platforms.
Handle large game worlds efficiently.

- Modding
    - ![Planned][planned] Mod loading system
        - Discover and load mod packs (`.owlmod`) at runtime
        - Mod manifest: name, version, dependencies, load order
        - Override or extend game assets (textures, scripts, scenes)
    - ![Planned][planned] Lua mod API
        - Register custom components from Lua
        - Hook into game events (on_scene_load, on_entity_spawn, etc.)
        - Sandboxed execution (no filesystem access by default)
    - ![Planned][planned] In-game mod manager
        - UI to enable/disable/reorder mods
        - Mod conflict detection
- Performance
    - ![Planned][planned] Level streaming
        - Load/unload scene chunks on demand based on player position
        - Background loading via task system
        - Seamless transitions (no loading screen)
    - ![Planned][planned] Asset pipeline (cooking)
        - Offline asset preprocessing: texture compression, atlas generation, mesh optimization
        - Cooked assets for faster runtime loading
        - Incremental cooking (only changed assets)
- Platforms
    - ![Planned][planned] Web export (WebAssembly + WebGL/WebGPU)
        - Emscripten build target
        - HTML5 template with loading screen
        - Async asset loading adapted for web
    - ![Planned][planned] Android support
        - Touch input backend
        - OpenGL ES / Vulkan mobile renderer
        - APK packaging from Owl Nest
    - ![Planned][planned] Gamepad improvements
        - Controller remapping UI
        - Haptic feedback / vibration API
        - Analog stick dead zone and curve configuration

## v0.4.0 -- Expected 2026-12-01

**Goal:** Give game designers tools to create intelligent NPCs, richer physical interactions,
polished audio/narrative experiences, and networked multiplayer.

- Networking
    - ![Planned][planned] Network transport layer
        - UDP-based reliable messaging (or integrate a library like ENet/GameNetworkingSockets)
        - Client-server model with authoritative server
        - Connection management: connect, disconnect, reconnect, timeout
    - ![Planned][planned] Entity replication
        - Mark components as replicated (sync from server to clients)
        - Interpolation and prediction for smooth movement
        - Authority model: server-owned vs client-owned entities
    - ![Planned][planned] RPC system
        - Lua API: `rpc_server(func, args)`, `rpc_client(func, args)`, `rpc_all(func, args)`
        - Reliable and unreliable RPC channels
    - ![Planned][planned] Lobby and session management
        - Host/join game, player list, ready state
        - Lua callbacks: `on_player_join`, `on_player_leave`
    - ![Planned][planned] Network debugging tools
        - Latency/packet-loss simulation in editor
        - Network stats overlay (ping, bandwidth, entity count)
- AI
    - ![Planned][planned] Pathfinding
        - Navigation mesh generation from scene geometry
        - A* pathfinding on NavMesh
        - Dynamic obstacle avoidance
    - ![Planned][planned] Behavior trees
        - Visual behavior tree editor in Owl Nest (node graph)
        - Standard nodes: sequence, selector, parallel, decorator, condition, action
        - Lua-scriptable leaf nodes for custom actions/conditions
        - Saveable `.owlbt` behavior tree assets
    - ![Planned][planned] Steering behaviors
        - Seek, flee, arrive, wander, pursue, evade
        - Flocking (separation, alignment, cohesion)
        - Composable via behavior tree or Lua
- Physics
    - ![Planned][planned] Physics queries
        - Raycast, box cast, circle/sphere cast with filtering
        - Overlap queries (find all entities in area)
        - Lua API: `physics.raycast(origin, direction, distance)`
    - ![Planned][planned] Joints and constraints
        - Distance, revolute, prismatic, weld joints
        - Motor joints for vehicles/mechanisms
        - Visual joint editor in Owl Nest
    - ![Planned][planned] Collision callbacks
        - `on_collision_enter`, `on_collision_exit`, `on_trigger_enter`, `on_trigger_exit`
        - Collision layers and masks for filtering
- Audio
    - ![Planned][planned] Audio mixer
        - Bus system: Master → Music / SFX / Ambient / Voice
        - Per-bus volume, mute, effects
        - Crossfade between music tracks
    - ![Planned][planned] Audio effects
        - Reverb zones (e.g. cave vs outdoor)
        - Low-pass/high-pass filters (e.g. underwater, behind walls)
- Narrative
    - ![Planned][planned] Dialogue system
        - Dialogue tree asset (`.owldlg`) with branching conversations
        - Visual dialogue editor in Owl Nest (node graph)
        - Lua hooks for conditions and consequences
        - Subtitle rendering via in-game UI

## v0.3.0 -- Expected 2026-10-01

**Goal:** Full 3D rendering pipeline with lighting, materials, post-processing, mesh-based
scene authoring, and cross-platform packaging from any host.

- Graphics
    - ![Planned][planned] 3D render pipeline
        - Forward rendering with depth buffer
        - Camera: perspective projection, free-look, orbit controls
    - ![Planned][planned] Lighting system
        - Directional light (sun), point lights, spot lights
        - Shadow mapping (at least for directional light)
        - Ambient light and basic global illumination approximation
    - ![Planned][planned] Material system
        - PBR materials (albedo, normal, metallic, roughness, AO)
        - Material editor in Owl Nest inspector
        - Material library / reusable material assets
    - ![Planned][planned] Mesh rendering
        - Static mesh component with transform
        - Instanced rendering for repeated meshes
        - LOD support (multiple detail levels per mesh)
    - ![Planned][planned] Skeletal animation
        - Bone hierarchy and skinning
        - Animation clips with blending and transitions
        - Animation state machine component
    - ![Planned][planned] Particle system
        - GPU-accelerated particle emitters
        - Configurable: lifetime, velocity, color over time, size, gravity
        - Emitter shapes: point, sphere, cone, box
        - Editor: visual particle preview and property curves
    - ![Planned][planned] Post-processing pipeline
        - Configurable post-process stack per camera
        - Effects: bloom, vignette, color grading (LUT), chromatic aberration, film grain
        - Motion blur, depth of field
    - ![Planned][planned] Weather and environment effects
        - Rain, snow, fog, day-night cycle
        - Configurable via Lua or scene properties
- Scene
    - ![Planned][planned] 3D physics
        - 3D rigid body component (replace or extend Box2D with a 3D engine)
        - Collision shapes: box, sphere, capsule, mesh
        - Raycasting queries for gameplay (line-of-sight, ground detection)
        - Joints and constraints
    - ![Planned][planned] 3D scene editing in Owl Nest
        - 3D viewport with orbit/fly camera
        - 3D gizmos (translate, rotate, scale in 3 axes)
        - Grid snapping, vertex snapping
        - Mesh import preview
- Cross-Platform Packaging
    - ![Planned][planned] Cross-compile packaging from any host
        - Package a Linux game from Windows and a Windows game from Linux
        - Pre-built runner binaries per target platform (downloaded or bundled)
        - Cross-platform shared library bundling (resolve target-platform `.so`/`.dll`)
    - ![Planned][planned] Target platform selector in Pack Game
        - Choose target: Linux x64, Windows x64 (independently of host)
        - Automatic runner binary selection for target platform
        - Platform-specific post-processing (launcher script for Linux, .zip for Windows)

## v0.2.0 -- Expected 2026-08-01

**Goal:** Introduce new rendering modes alongside the existing 2D renderer: a raycasting renderer
(pseudo-3D like Wolfenstein 3D / DOOM), a voxel engine (block-based worlds like Minecraft), and
a 2D lighting system.

- Raycasting Renderer
    - ![Planned][planned] Raycasting core
        - DDA raycasting algorithm rendering a 2D grid map as pseudo-3D
        - Textured walls with perspective-correct mapping
        - Configurable field of view and resolution
    - ![Planned][planned] Floors and ceilings
        - Textured floor/ceiling casting
        - Skybox or solid color above horizon
    - ![Planned][planned] Sprites (billboards)
        - Entities rendered as camera-facing sprites in the raycast view
        - Distance-based sorting and scaling
    - ![Planned][planned] Map features
        - Doors (opening/closing with animation)
        - Thin walls and transparent walls
        - Variable wall heights
    - ![Planned][planned] Raycasting map editor in Owl Nest
        - 2D grid editor for wall placement and texture assignment
        - Top-down preview alongside first-person preview
        - Entity placement on the grid
    - ![Planned][planned] Lighting for raycasting
        - Distance-based shading (fog/darkness)
        - Optional point lights with falloff
- Voxel Engine
    - ![Planned][planned] Voxel world core
        - Chunk-based world (e.g. 16x16x256 chunks)
        - Block type registry with textures per face
        - Chunk loading/unloading around camera
    - ![Planned][planned] Chunk meshing
        - Greedy meshing or similar algorithm for efficient geometry
        - Only exposed faces rendered (hidden face culling)
        - Frustum culling per chunk
    - ![Planned][planned] Terrain generation
        - Procedural terrain via noise functions (Perlin/Simplex)
        - Configurable biomes, terrain height, cave generation
        - Seed-based reproducible worlds
    - ![Planned][planned] Block interaction
        - Block placement and destruction
        - Block picking (raycast from camera to find targeted block)
        - Block metadata (orientation, state)
    - ![Planned][planned] Voxel rendering
        - Ambient occlusion per vertex for block edges
        - Basic directional lighting
        - Water/transparent block rendering with proper sorting
    - ![Planned][planned] Voxel editor in Owl Nest
        - Brush tools for painting blocks
        - Prefab structures (trees, buildings) as reusable block templates
        - Chunk inspector for debugging
- 2D Lighting
    - ![Planned][planned] 2D lighting system
        - Point lights, spot lights in 2D scenes
        - Normal-mapped sprites for dynamic 2D lighting
        - Shadow casting from 2D occluders
- Renderer Architecture
    - ![Planned][planned] Renderer abstraction
        - Clean separation so scenes can select their renderer type
        - Scene property: renderer mode (2D, Raycasting, Voxel)
        - Shared resource management (textures, shaders) across renderers
    - ![Planned][planned] Tilemap system for 2D
        - Tile-based map component as an alternative to individual sprite entities
        - Tileset definition (texture atlas + tile properties)
        - Tile layers with collision, parallax scrolling
        - Tilemap editor in Owl Nest (paint, fill, erase tools)
- Gameplay
    - ![Planned][planned] Inventory system
        - Collectible objects
        - Key-locked switches
    - ![Planned][planned] Enemies
    - ![Planned][planned] Scene transition effects
        - Configurable fade, wipe, or custom shader transitions between scenes
        - Lua API to trigger transitions with parameters (duration, type)
- Editor Infrastructure
    - ![Planned][planned] Custom ImGui-based file picker
        - Replace the native file dialog (NFD/GTK) which briefly freezes the UI on Linux
          when GTK initializes (triggers IDE "antiloop" detection)
        - Pure ImGui implementation integrated with the task scheduler for async folder scanning
        - Benefits: consistent look-and-feel, truly non-blocking, theme-aware
        - Replaces the current sync `FileDialog::openFile/saveFile/pickFolder` blocking calls

## v0.1.1 -- Editor Polish & Multi-Document

**Goal:** Transform the editor from a single-scene tool into a multi-document workspace
with dedicated editors for different asset types. All long-running operations become
asynchronous with progress feedback.

- Async Operations & Progress
    - ![Done][done] Async task integration in Owl Nest
        - `AsyncProgressModal` reusable panel: modal progress bar + cancel button + error display
        - `AsyncProgressState` thread-safe shared state (atomics + mutex)
        - Leverages existing Taskflow-based `Scheduler`, termination callbacks on main thread
    - ![Done][done] Async packaging
        - Pack Game and Pack Scene: asset scanning, compression, pack writing — all off main thread
        - Real-time progress bar (per-entry callback via `PackWriter::ProgressCallback`)
        - Cancel support via `PackWriter::CancelCheck`
    - ![Done][done] Async scene save
        - `saveSceneAs()`: serialize to string on main thread, write file on background thread
        - "Saving..." overlay with progress indicator
    - ![Done][done] Async scene loading
        - `openScene()`: read file bytes on background thread, deserialize on main thread in callback
        - "Loading Scene..." overlay while file I/O is in progress
    - ![Done][done] Deferred shader compilation with loading screen
        - `Renderer::init()` split into `initContext()` + `initShaders(callback)`
        - ImGui loading overlay displayed between each shader compilation (5 shaders)
        - Per-shader progress ("Compiling shader 3/5: quad...") with progress bar
        - Cache hit skips compilation (~1ms), first-time compile shows real progress (~50s total)
    - ![Planned][planned] Async texture loading with placeholders
        - Load textures from pack on background thread (decompress + decode)
        - Display 1x1 white placeholder until real texture is ready
        - Smooth scene transitions without frame hitches in the runner
    - ![Done][done] Async scene transitions in runner
        - `RunnerLayer::handleTeleportRequest()` reads scene bytes in background (from pack or file)
        - Deserializes and swaps the active scene on the main thread when bytes are ready
        - Old scene paused (onEndRuntime called) while loading — last rendered frame stays visible
        - GameState + velocity + target name preserved across the async transition
    - ![Done][done] Async content browser scanning
        - `ContentBrowser` caches directory entries (no more per-frame `directory_iterator`)
        - Scans are performed asynchronously via the task scheduler when path changes
        - Rescan triggered after create folder / import / rename / delete / drop / move
        - Sorted entries (folders first, then alphabetical)
- Multi-Document Architecture
    - ![Planned][planned] Document tab system
        - Tab bar for open documents (scenes, scripts, node graphs)
        - Each tab owns its own panel context (hierarchy, inspector, etc.)
        - Remember and restore open tabs between sessions
    - ![Planned][planned] Detachable panels
        - Panels can be dragged out into floating windows
        - Independent ImGui viewports per floating window
- Script Editor
    - ![Planned][planned] Integrated Lua script editor
        - Syntax-highlighted text editor for `.lua` files (open from Content Browser)
        - Save / reload with Ctrl+S, auto-reload on external change
        - Basic error display (line number + message from Lua compilation)
    - ![Planned][planned] Script debugging aids
        - Breakpoint markers (visual only — log-based, not a step debugger)
        - Live variable watch panel (read globals from running ScriptInstance)
- Node Graph Editor
    - ![Planned][planned] Node graph framework
        - Generic node canvas: nodes with typed input/output pins, bezier connections
        - Pan/zoom, selection, copy/paste, undo/redo integration
        - Serialization to/from YAML (reusable for animation, shaders, AI, etc.)
    - ![Planned][planned] Scene flow view (first node graph usage)
        - Scenes as nodes, teleport triggers as connections
        - Click to navigate to a scene, detect orphaned/unreachable scenes
        - Visual editing of teleport links
- Asset Editors
    - ![Planned][planned] Animation editor
        - Timeline for `AnimatedSpriteRenderer`: frame-by-frame preview
        - Visual configuration of frame count, frame rate, loop mode
        - Opens as a document tab (not a modal)
    - ![Planned][planned] Enhanced inspector
        - Curve editor for animated properties
        - Texture/sound preview in inspector, drag-drop assets to fields
- Packaging
    - ![In Progress][progress] Packaging wizard in Owl Nest
        - ![Done][done] Pre-packaging validation: `AssetScanner` warnings output for unresolvable texture/sound/script/scene/font references
        - ![Done][done] Validation modal before pack with issue list + "Proceed anyway" / "Cancel" buttons
        - ![Done][done] OwlRunner executable check + empty-assets check
        - ![Planned][planned] Dedicated wizard panel with target platform selection (currently just a folder dialog)
        - ![Planned][planned] Post-pack build report panel
- Menu & Project Workflow
    - ![Done][done] Reorganized menu structure
        - **File** menu: project operations (New, Open, Open Recent, Save, Close, Pack Game, Welcome Screen, Exit)
        - **Edit** menu: Undo/Redo + Engine Settings, Editor Settings, Project Settings
        - **Current** menu: scene operations (New Scene, Open Scene, Save, Save As, Import Scene, Pack Scene)
        - "Show Stats" moved into Editor Settings panel (General section)
    - ![Done][done] Recent projects
        - Persisted in `EditorSettings::recentProjects` (capped at 10 entries, most recent first)
        - "Open Recent" submenu in the File menu with per-entry tooltip showing full path
        - Welcome screen modal shown when no project is loaded: New/Open buttons + recent list
        - Double-click a recent entry to open, `x` button to remove individual entries
- UX & Quality
    - ![Planned][planned] In-editor help pages
        - Built-in documentation browser (searchable, linked from panels)
        - Context-sensitive help (F1 on a component opens its doc page)
        - Getting started guide accessible from the welcome screen
    - ![Done][done] Tooltips everywhere with hover delay
        - Reusable `fieldTooltip()` helper with `DelayNormal` (~0.4s) hover delay
        - Tooltips on all 7 trigger types with descriptions
        - Tooltips on trigger sub-fields (Scene, Level Name, Target Name, Duration, Range, Callback)
        - Tooltips on PhysicBody fields (Type, Density, Restitution, Friction, Fixed Rotation)
        - Tooltips on Player fields (Primary, Linear/Jump Impulse, Can jump)
        - Tooltips on Camera fields (Primary, Projection type, FOV, Near/Far, Ortho Size, Fixed Aspect)
        - Tooltips on SoundSource fields (Asset, Category, Volume, Pitch, Loop, Spatial, Max Distance, Rolloff)
        - Tooltips on SoundListener (Primary)
        - Tooltips on Canvas (Space, Sort Order), UIRect (Anchor, Pivot, Size, Offset)
        - Tooltips on UIButton colors + On Click callback, UISlider value/min/max + On Value Changed
    - ![Done][done] Unique ImGui IDs audit
        - Component-scoped `PushID(T::name())` in `drawComponent<T>` — prevents label collisions
          between components that share field names (e.g. "Color" in SpriteRenderer and CircleRenderer)
        - Index-based `PushID` in LuaScript property loop — prevents collision if two properties share a name
        - Entity list in SceneHierarchy already uses UUID-based PushID (verified safe)
        - ContentBrowser and SettingsPanel already use unique per-item IDs (verified safe)
    - ![Planned][planned] Icon clarity pass
        - Review and redesign confusing or too-similar icons
        - Add distinct icons for new trigger types (timer, interaction, lua callback)
        - Consistent visual language across all icon categories

## v0.1.0 -- 2026-04-16

**Goal:** Users can design a complete game in Owl Nest and package it as a standalone distributable
application (Linux / Windows).

- Scripting (Lua) — See [Lua Scripting](scripting.md)
    - ![Done][done] Lua interpreter integration
        - Embed Lua 5.5 via DepManager, `LuaEngine` wrapper in `source/owl/private/script/`
        - Public API: `ScriptEngine`, `ScriptInstance` in `source/owl/public/script/`
    - ![Done][done] LuaScript component
        - Path to `.lua` file, exposed properties table editable in inspector
        - Lifecycle callbacks: `on_create`, `on_update`, `on_destroy`, `on_collision`
    - ![Done][done] Engine API exposed to Lua
        - Entity manipulation (get/has component, get name, destroy)
        - Transform, Physics (impulse, velocity), Input (keys, mouse), Sound (play, stop, volume)
        - Scene (find entity, create entity, destroy entity), Time, Logging
    - ![Done][done] Editor support for Lua scripts
        - Inspector: display/edit exposed script properties with Refresh button
        - Content Browser: `.lua` icon
        - Serialization: script path and property values saved in `.owl`
    - ![Done][done] Lua scripts in Runner
        - Runner loads and executes `.lua` files from `.owlpack`
        - Same lifecycle as editor (on_create, on_update, etc.)
- In-Game UI
    - ![Done][done] Canvas UI system
        - `Canvas` component marks UI root, `UIRect` for screen-space layout (anchor, pivot, size, offset)
        - Screen-space orthographic rendering via existing Renderer2D pipeline
        - Sort order for layering multiple canvases
    - ![Done][done] Base widgets
        - UIText (font, size, color, alignment), UIImage (sprite, tint)
        - UIButton (normal/hover/pressed/disabled states, Lua callback)
        - UIPanel (background, border, vertical/horizontal layout)
        - UISlider (draggable, min/max, callback), UIProgressBar (value, colors)
    - ![Done][done] UI input handling
        - UIInputSystem: hit-test, hover/pressed state tracking
        - UI consumes mouse events before scene, Lua callback on button click
        - Lua `ui` table: set_text, set_visible, set_progress, slider get/set, button enable/disable
    - ![Done][done] UI editor in Owl Nest
        - Canvas and UIRect visible in editor viewport with cyan outlines
        - All widget properties editable in inspector
        - Component icons for Canvas, UIRect, UIText, UIImage, UIPanel, UIButton, UISlider, UIProgressBar
    - ![Done][done] Standard game screens
        - Template scenes: Main Menu, Pause Menu, Game Over (`engine_assets/templates/`)
        - Screen transitions: FadeIn/FadeOut via `ScreenTransition` + Lua `ui.transition_fade_in/out`
- Game State & Save System
    - ![Done][done] Serializable GameState
        - `GameState` key-value store (int, float, string, bool) on Scene
        - Lua API: `gamestate.set/get/remove/clear`, auto-type detection
        - Copied across scene transitions, serialized in save files
    - ![Done][done] Save / Load
        - `SaveManager`: save/load to `~/.local/share/<game>/saves/` (Linux) or `%APPDATA%/<game>/saves/` (Windows)
        - Multiple save slots, `save.save_game(slot)`, `save.load_game(slot)`, `save.list_saves()`
        - Deferred load pattern (safe mid-script), handled by RunnerLayer/EditorLayer
    - ![Done][done] Full scene state save
        - Complete scene + GameState + physics snapshots (velocities, wake state)
        - `PhysicCommand::getSnapshot/applySnapshot` for Box2D state capture/restore
        - `loaded_from_save` flag set in GameState on load
- Runner & Distribution
    - ![Done][done] Extended project configuration
        - `owl_project.yml` fields: version, author, description, icon, window size, fullscreen, resizable
        - ProjectSettings panel with full editing UI
    - ![Done][done] Improved Runner
        - Window title, icon, and size from project config (via extended `runner.yml`)
        - Fullscreen / resizable support via new Window API
        - `packGame()` exports all project settings + icon file
    - ![Done][done] Persistent game settings
        - Two-layer system: `game_settings.yml` (game defaults in assets) + `settings.yml` (user overrides)
        - Built-in keys: resolution, fullscreen, resizable, volume (master/music/sfx) + custom game keys
        - Lua API: `settings.get/set/save/load/reset/reset_all/apply`
        - Auto-apply builtins to window + sound listener gain
    - ![Done][done] Improved packaging pipeline
        - Renamed Runner executable to sanitized game name
        - Linux: `launch.sh` script with `LD_LIBRARY_PATH`
        - Windows: automatic `.zip` archive via PowerShell
        - `game_info.yml` metadata (name, version, author, engine version, platform, date)
- Editor Improvements (Owl Nest)
    - ![Done][done] Prefab system
        - `.owlprefab` files: serialized entity subtrees with UUID remapping
        - Create from entity (context menu), instantiate via Content Browser drag-drop
        - PrefabLink component: instance ↔ prefab link, update/revert with override preservation
    - ![Done][done] Undo / Redo
        - Reversible command system for all editor actions (entity, component, hierarchy, gizmo)
        - Configurable undo stack depth, Ctrl+Z / Ctrl+Y, merge coalescing, dirty flag in title
- Scene & Triggers
    - ![Done][done] More scene events and triggers
        - New trigger types: Timer (duration + repeat), Interaction (key press in range), LuaCallback (generic)
        - Edge detection: `on_trigger_enter`/`on_trigger_exit` callbacks on all trigger types
        - Lua API: `trigger.start_timer`/`stop_timer`/`reset_timer` for timer control
        - Editor: Duration/Repeating fields for Timer, Interaction Range for Interaction
    - ![Done][done] Common properties across scenes
        - Covered by `SettingsManager`: game defaults in `game_settings.yml`, shared across all scenes
- Sample Game
    - ![Done][done] Complete game demonstrator
        - 6 scenes: main menu (logo, save management), gameplay, level 2, settings (with reset),
          victory, game over — all with fade transitions
        - Full Lua API coverage: all 13 API tables exercised (transform, physics, input, scene,
          entity, ui, gamestate, save, settings, sound, log, trigger, time)
        - All 8 UI widget types, all 7 trigger types, animated sprites, sounds + music
          with pause/resume/volume, mouse input, runtime entity creation, entity inspection

## v0.0.3 -- 2026-04-09

- Sound
    - ![Done][done] Sound effects
        - One-shot and looping playback via SoundSource component
        - SoundHelper for gameplay-triggered sounds
    - ![Done][done] Music
        - Background music as SoundSource with loop and non-spatial settings
        - Category system (SFX, Music, Ambient) for future per-category mixing
    - ![Done][done] Sound management
        - Play, stop, pause, resume, loop, volume, pitch
        - SoundHandle-based control of individual sound sources
        - Extended format support (.wav, .ogg, .flac, .mp3)
    - ![Done][done] Sound spatialization
        - 3D positional audio via OpenAL with distance attenuation
        - SoundListener component marks the "ear" entity
        - Configurable max distance and rolloff per source
    - ![Done][done] Moving sound
        - Spatial source positions synced from entity world transforms each frame
- Graphics
    - ![Done][done] Animated sprites
        - AnimatedSpriteRenderer component with spritesheet grid support
        - Frame-by-frame animation with configurable speed, loop, and frame range
        - Custom UV coordinates in Renderer2D for sub-texture rendering
- Objects
    - ![Done][done] Mesh object support
    - ![Done][done] Mesh loading (OBJ, glTF, GLB, FBX)
        - Via tinygltf, tinyobjloader, ufbx
- Miscellaneous
    - ![Done][done] Configurable keymap
- Scene
    - ![Done][done] Scene hierarchy (parent-child entities)
        - Transform and visibility inheritance
        - Reparenting preserves world position
        - Delete orphans children to grandparent, or cascade delete
        - Duplicate entity or entire subtree
    - ![Done][done] Asset packing (`.owlpack` binary format)
        - zstd compression, XOR-obfuscated TOC
        - Pack-aware loading in runner
    - ![Done][done] Task system (Taskflow backend)
        - Async work scheduling, parallel utilities
- Game Designer (Owl Nest)
    - ![Done][done] Project system
        - Create, open, save, and close projects (`owl_project.yml`)
        - Dynamic asset directory management for project folders
        - Project name displayed in window title
    - ![Done][done] Project settings panel
        - Edit project name and first scene (dropdown of available scenes)
    - ![Done][done] Scene import into project
    - ![Done][done] Export game for runner
        - Standalone game runner package
    - ![Done][done] Scene hierarchy panel
        - Tree display with drag-drop reparenting
        - Context menu: create root/child entity, duplicate/subtree, unparent, delete/cascade
        - Visibility toggle buttons per entity (editor + game)
    - ![Done][done] Icon system with runtime SVG rendering
        - SVG sources organized by usage (toolbar, browser, components, actions, etc.)
        - Runtime rasterization via lunasvg with dynamic theme color substitution
        - White → theme text color, fuchsia → theme accent color
        - Atlas with mipmaps, rebuild on theme change
    - ![Done][done] View of level links
    - ![Done][done] Separate editor/game display
        - Entities rendered differently

## v0.0.2 -- 2026-03-03

- Developers
    - ![Done][done] Public engine headers 3rd-party independent
    - ![Done][done] Remove fmt public dependency
    - ![Done][done] Reduce needed public binaries
        - More static linking of 3rd-party libs
- Graphics
    - ![Done][done] Backgrounds / skyboxes
    - ![Done][done] Migrate shaders to Slang
- Miscellaneous
    - ![Done][done] Pause / unpause game
    - ![Done][done] Frame-by-frame stepping
        - When paused
    - ![Done][done] General settings management
- Gameplay
    - ![Done][done] Jump between scenes
- Game Designer (Owl Map)
    - ![Done][done] Global game settings
    - ![Done][done] Log frame
    - ![Done][done] In-game visibility
    - ![Done][done] In-editor visibility

## v0.0.1 -- 2025-02-06

First basic release: minimal viable engine with the ability to run simple games
defined in scenes.

## Badge Legend

- ![Done][done] Completed features
- ![In Progress][progress] Features currently being implemented
- ![Planned][planned] Features that are planned but not yet being worked on

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square

[progress]: https://img.shields.io/badge/-In_Progress-d29922?style=flat-square

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square
