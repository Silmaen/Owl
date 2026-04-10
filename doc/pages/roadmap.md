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

**Goal:** Full 3D rendering pipeline with lighting, materials, post-processing, and mesh-based
scene authoring.

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

## v0.1.0 -- Expected 2026-06-01

**Goal:** Users can design a complete game in Owl Nest and package it as a standalone distributable
application (Linux / Windows).

- Scripting (Lua) — See [Lua Scripting](@ref page-scripting)
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
  - ![Planned][planned] Canvas UI system
    - `Canvas` component attached to camera or as screen overlay
    - Coordinate space: screen pixels or viewport percentage
    - Rendered via existing 2D pipeline (textured quads + text)
  - ![Planned][planned] Base widgets
      - UIText (font, size, color, alignment), UIImage (sprite, tint)
      - UIButton (normal/hover/pressed states, Lua callback)
      - UIPanel (background, border, vertical/horizontal layout)
      - UISlider, UIProgressBar
  - ![Planned][planned] UI input handling
      - Focus/navigation system (mouse + keyboard/gamepad)
      - UI consumes click events before scene, custom cursor support
  - ![Planned][planned] UI editor in Owl Nest
      - Canvas mode in Viewport: visual widget placement (drag, resize, anchors)
      - Real-time preview, properties editable in inspector
  - ![Planned][planned] Standard game screens
      - Predefined templates: Main Menu, Pause, Game Over, Settings
      - Screen transitions (fade, slide)
- Game State & Save System
  - ![Planned][planned] Serializable GameState
      - Key-value dictionary (int, float, string, bool) for progression, inventory, flags
      - Lua API: `gamestate.set("key", value)`, `gamestate.get("key", default)`
  - ![Planned][planned] Save / Load
      - Save to user directory (`~/.local/share/<game>/` on Linux, `%APPDATA%/<game>/` on Windows)
      - Multiple save slots, list available saves
      - Lua API: `save_game(slot)`, `load_game(slot)`, `list_saves()`
  - ![Planned][planned] Full scene state save
      - Serialize active scene + player position + physics state on save
      - Restore complete scene on load (not just variables)
- Runner & Distribution
  - ![Planned][planned] Extended project configuration
    - `owl_project.yml` fields: version, author, icon, window size, fullscreen, resizable
    - Runner reads all config at startup
  - ![Planned][planned] Improved Runner
      - Window title and icon from project config
      - Optional splash screen during loading
      - Fullscreen / windowed / borderless support
      - Fallback system menu if no Main Menu scene defined
  - ![Planned][planned] Persistent player settings
      - `settings.yml` in user directory: resolution, fullscreen, volume (master/music/SFX)
      - Lua API: `get_setting(key)`, `set_setting(key, value)`
  - ![Planned][planned] Improved packaging pipeline
      - Rename Runner executable to game name
      - Linux: launcher script with proper `LD_LIBRARY_PATH`
      - Windows: distributable `.zip` with all DLLs
      - Include metadata (version, author) in package
  - ![Planned][planned] Packaging wizard in Owl Nest
      - Panel/dialog: target platform, output directory, progress bar, build report
      - Pre-packaging validation: check firstScene exists, all assets found
- Editor Improvements (Owl Nest)
  - ![Planned][planned] Prefab system
    - `.owlprefab` files: serialized entity subtrees
    - Create from entity (context menu), instantiate from Content Browser
    - Prefab ↔ instance link: edits to prefab propagate, per-instance overrides
  - ![Planned][planned] Animation editor
      - Timeline for `AnimatedSpriteRenderer`: frame-by-frame preview
      - Visual configuration of frame count, frame rate, loop mode
  - ![Planned][planned] Scene flow view
      - Graph showing scenes and their connections (via teleporters)
      - Click to navigate to a scene, detect orphaned/unreachable scenes
  - ![Planned][planned] Enhanced inspector
      - Curve editor for animated properties
      - Texture/sound preview in inspector, drag-drop assets to fields
  - ![Planned][planned] Undo / Redo
      - Reversible command system for all editor actions
      - Configurable undo stack depth, Ctrl+Z / Ctrl+Y
- Scene & Triggers
  - ![Planned][planned] More scene events and triggers
      - On enter, on interact, on timer, etc.
      - Trigger actions: play sound, change scene, modify objects, etc.
      - Event parameters: trigger volume, interaction range, timer duration
      - Editor support for placing triggers and defining events
  - ![Planned][planned] Common properties across scenes
      - Player spawn point, lighting, background, etc.
- Gameplay
  - ![Planned][planned] Inventory system
      - Collectible objects
      - Key-locked switches
  - ![Planned][planned] Enemies

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
