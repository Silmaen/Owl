# Roadmap {#page-roadmap}

[TOC]

This page tracks planned and completed features across Owl releases.

**Ongoing across all releases:**

These cross-cutting efforts are never "done" — they are maintained and improved continuously across every release. No
feature should regress the baseline on these axes; each release is expected to move the needle forward.

- ![Planned][planned] Code quality
    - Keep clang-tidy / clang-format clean (no new warnings, no `// NOLINT` without justification)
    - Refactor away duplication and dead code as it appears (no abstractions for hypothetical needs)
    - Respect the conventions in `.claude/rules/cpp-style.md` (naming, trailing return types, smart-pointer aliases,
      `@brief` on every public API)
    - Treat every PR as an opportunity to leave the touched files cleaner than found
- ![Planned][planned] Test coverage
    - Grow coverage alongside every new feature (no new public API without tests)
    - Maintain coverage trend upward, never downward — use
      `poetry run python ci_action.py Coverage <preset>` to measure
    - Unit tests for pure logic, integration tests for systems that cross module boundaries
    - Fill gaps in existing modules opportunistically (backfill tests for untested legacy paths)
- ![Planned][planned] Performance
    - Profile before optimizing — measure with the in-editor profiler or external tools
    - Watch for regressions in hot paths (renderer, physics step, scene update, script tick)
    - Prefer algorithmic wins over micro-optimizations; document non-obvious perf tradeoffs
    - Memory: avoid per-frame allocations, reuse buffers, stream large assets
- ![Planned][planned] Documentation quality
    - Every public class, method, enum value, and struct field has a `@brief` / `///` comment
    - Private members get at least a `///` one-liner
    - Keep `doc/pages/*.md` in sync with behaviour — update pages in the same PR as the feature
    - Prefer mermaid diagrams over ASCII art or external images for architecture/flow/sequence
    - Update `CHANGELOG.md` (Unreleased section) and this roadmap as features land
- ![Planned][planned] Editor coverage for every authored object
    - Any new object the engine lets the user author (component, asset, sub-object inside a component) ships with full
      editor support in Owl Nest **in the same PR** as the engine-side feature. "Editor support" means, at minimum:
        - **Selection** — clickable in the viewport / hierarchy / asset browser (whichever is the natural surface for
          the object), with visible highlight
        - **Inspection** — read every property in the inspector panel
        - **Editing** — modify every property from the inspector, undo/redo via
          `ModifyEntityCommand` (or the asset-equivalent)
        - **Bulk operations** — when the object is part of a collection (tilemap cells, animation keyframes, node-graph
          nodes, …), at least *select-many* +
          *move-group* / *delete-group*; resizing a parent must preserve / shift contents instead of clipping
        - **Discoverability** — context-menu / drag-drop / keyboard shortcut where comparable objects already have one
    - "Working in YAML or via Lua only" is **not** acceptable for any object the user is expected to author by hand;
      treat that gap as a regression of the same severity as a missing test or missing public API doc
- ![Planned][planned] Profiling tools
    - In-editor frame profiler (CPU/GPU timeline)
    - Memory usage breakdown by asset type
    - Entity/component count statistics
- ![Planned][planned] Rendering optimizations
    - Spatial partitioning (quadtree/octree) for culling
    - Batched draw calls, texture atlasing
    - Multithreaded render preparation
- ![Planned][planned] Per-pass uniform buffers in Vulkan
    - `Renderer2D::beginScene` writes the camera VP to a single per-frame uniform buffer that the GPU descriptor set
      still references for all passes in the same frame. When two layers in the same frame use
      *different* VPs (e.g. world layer with the scene camera + screen-space
      `Renderer2DLayer` with a pixel-ortho), the second `beginScene`'s host memcpy can race the first pass's GPU read on
      Vulkan, producing visible flicker on the world layer.
    - Workaround in v0.2.0: keep both passes in the same coordinate frame by default (project's `ui` layer is
      `Space: World`), and have only scenes with a rotated player camera (raycast) opt into `Space: Screen` via a
      scene-level `Overrides` block. Race still exists in those scenes but is masked when both VPs project the active
      camera near the same orientation.
    - Proper fix: rotate through a pool of uniform buffers (one per
      `beginScene` call), bind the descriptor set with a dynamic offset, and reset the rotation at frame end. Or move
      the camera VP to push constants. Either approach makes the per-pass VP per-draw-call and eliminates the host/GPU
      race.

## v0.5.0 -- Expected 2027-06-01

**Goal:** Let players extend and modify games built with Owl. Bring Owl games to more platforms. Handle large game
worlds efficiently.

- Editor UX
    - ![Planned][planned] Session restore (persisted open tabs)
        - Remember the list of open documents between launches (per project)
        - Restore active tab, selection, and viewport layout
        - Stored in `EditorSettings` or `owl_project.yml`

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
        - Analogue stick dead zone and curve configuration

## v0.4.0 -- Expected 2027-04-01

**Goal:** Give game designers tools to create intelligent NPCs, richer physical interactions, polished audio/narrative
experiences, and networked multiplayer.

- Editor UX
    - ![Planned][planned] Node-graph link waypoints
        - Right-click on a coloured link in `NodeCanvas` (Scene Flow today, future graphs too)
          to insert an intermediate point at the cursor position
        - Drag to relocate, Delete to remove — waypoints persist in the `.owlflow` YAML
        - The Bezier routing splits at each waypoint into a series of cubic segments so the user can manually steer
          links around node clusters when the auto-deflection heuristic isn't enough
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
    - ![Planned][planned] Behaviour trees
        - Visual behaviour tree editor in Owl Nest (node graph)
        - Standard nodes: sequence, selector, parallel, decorator, condition, action
        - Lua-scriptable leaf nodes for custom actions/conditions
        - Saveable `.owlbt` behaviour tree assets
    - ![Planned][planned] Steering behaviours
        - Seek, flee, arrive, wander, pursue, evade
        - Flocking (separation, alignment, cohesion)
        - Composable via behaviour tree or Lua
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

## v0.3.0 -- Expected 2027-02-01

**Goal:** Full 3D rendering pipeline with lighting, materials, post-processing, mesh-based scene authoring, and
cross-platform packaging from any host.

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
        - Configurable: lifetime, velocity, colour over time, size, gravity
        - Emitter shapes: point, sphere, cone, box
        - Editor: visual particle preview and property curves
    - ![Planned][planned] Post-processing pipeline
        - Configurable post-process stack per camera
        - Effects: bloom, vignette, colour grading (LUT), chromatic aberration, film grain
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
- Scripting
    - ![Planned][planned] Script debugging aids
        - Breakpoint markers (visual only — log-based, not a step debugger); the underlying `TextEditor::AddMarker` API
          is already exposed by the
          `imgui_color_text_edit` package
        - Live variable watch panel (read globals from running `ScriptInstance`
          via `lua_pushglobaltable` + `lua_next`)
        - Deferred from v0.1.1 — needs the watch UI design pinned down before implementation
- Cross-Platform Packaging
    - ![Planned][planned] Cross-compile packaging from any host
        - Package a Linux game from Windows and a Windows game from Linux
        - Pre-built runner binaries per target platform (downloaded or bundled)
        - Cross-platform shared library bundling (resolve target-platform `.so`/`.dll`)
    - ![Planned][planned] Target platform selector in Pack Game
        - Choose target: Linux x64, Windows x64 (independently of host)
        - Automatic runner binary selection for target platform
        - Platform-specific post-processing (launcher script for Linux, .zip for Windows)
- Tileset Editor
    - ![Planned][planned] Compose atlas by inserting / removing source images
        - Today `TilesetDocument` only edits per-tile metadata (name, collidable, wallHeight, transparent). The atlas
          itself is authored offline — drop a PNG, edit metadata. There's no way to **build** or **mutate** an atlas
          from inside the editor.
        - Add tile-slot mutation in the document: drop an image onto an empty (or existing) slot to insert/replace it;
          right-click a slot to clear it. The underlying atlas PNG is rebuilt on disk (or kept in memory until the asset
          is saved).
        - Bulk insert: select multiple PNGs from the Content Browser and drop them onto the tileset grid — fill
          consecutive empty slots, growing the grid if needed (configurable: extend columns vs add rows).
        - "Remove" doesn't delete the slot index (would shift every downstream tilemap's tile indices); it just clears
          the pixels to fully transparent and clears the meta, so existing tilemaps that referenced that index render an
          empty cell instead of mis-pointing at the next tile.
        - Asset-pipeline hook: when the atlas changes the tilemap previews, door/pushwall thumbnails, and any open
          `TilemapDocument` reload automatically (the tileset is shared via the per-scene cache added in v0.2.0, so a
          single invalidation propagates).
        - Undo/redo: every insert / replace / remove pushes a single
          `ModifyTilesetCommand` (mirrors `ModifyEntityCommand` semantics)
          with the standard 1 s merge-coalescing for rapid drag chains.
- In-Editor Documentation
    - ![Planned][planned] Mermaid diagram rendering in the help panel
        - Today the md4c-based renderer treats ` ```mermaid ` fences as plain code blocks; the actual diagrams (used by
          `architecture.md`, `editor.md`, `node_graph.md`, `physics.md`,
          `renderer.md`, `scene.md`, `scripting.md`, `sound.md`) only render on GitHub / Doxygen
        - Build-time pre-render of mermaid blocks → SVG (or PNG) files in
          `engine_assets/help/images/mermaid/`, with the markdown rewriter swapping each fence for an
          `![alt](images/mermaid/<sha>.svg)` reference. No runtime JS/Node dependency (constraint set in v0.1.1) —
          pre-rendering can run with a depmanager-shipped tool or a custom subset renderer in C++
        - Update `cmake/HelpAssets.cmake` to invoke the pre-renderer and surface the cache files
        - Tests: assert each bundled `engine_assets/help/*.md` no longer contains ` ```mermaid `
          after the bundle step and that the rasterised diagram files exist
    - ![Planned][planned] Help-panel rendering polish (V2)
        - Hanging indent in unordered/ordered lists (current V1 wraps to column 0 — see
          `MarkdownPreview::renderList` comment)
        - Inline `code` rendered as a flat span (today it goes through `SmallButton` for the tinted background — works
          but adds a clickable affordance that reads as a button)
        - Ordered-list numbering survives `is_tight` md4c quirks (current OL counter is renderer-side, not parser-side)
        - Native rendering of GFM task lists `[ ]` / `[x]` (currently shown as inline text)
        - Optional dark/light theme switch for the help panel content (today inherits from the editor theme via
          `ImGuiCol_Text` / `Owl::Theme::buttonHovered`)
        - Side-by-side preview of the source `.md` next to the rendered output (debug aid for contributors editing
          pages)
- Performance
    - ![Planned][planned] Binary scene format
        - Replace YAML with a binary format (MessagePack / flatbuffers / custom) for `.owl` scenes, `.owltilemap`,
          `.owltileset`. yaml-cpp allocates per node — a 1000-entity scene parses
          ~10× slower than the same data in binary. Keep YAML import / export as a one-shot migration path; runtime +
          editor load reads binary. *Deferred from v0.2.0: the tracker O (N²) fix already pulled YAML parse from seconds
          to milliseconds, so binary format is no longer urgent — but still worth ~10× on really large scenes.*
    - ![Planned][planned] Parallel pack `readEntry`
        - Wrap `PackReader::readEntry` so multiple entries can be zstd-decompressed concurrently (file seek serialised
          under a mutex, decompression off-mutex). Useful for packed games loading dozens of textures in parallel at
          startup.
          *Deferred from v0.2.0 — needs PackReader thread-safety audit + a worker-friendly API.*

## v0.2.3 -- Expected 2027-01-01

**Goal:** Round out the 2D experience with dynamic lighting, ship the long-awaited custom file picker, and add core
gameplay primitives (inventory, enemies).

- 2D Lighting
    - ![Planned][planned] 2D lighting system
        - Point lights, spot lights in 2D scenes
        - Normal-mapped sprites for dynamic 2D lighting
        - Shadow casting from 2D occluders
- Editor Infrastructure
    - ![Planned][planned] Custom ImGui-based file picker
        - Replace the native file dialog (NFD/GTK) which briefly freezes the UI on Linux when GTK initializes (triggers
          IDE "antiloop" detection)
        - Pure ImGui implementation integrated with the task scheduler for async folder scanning
        - Benefits: consistent look-and-feel, truly non-blocking, theme-aware
        - Replaces the current sync `FileDialog::openFile/saveFile/pickFolder` blocking calls
    - ![Planned][planned] Editor camera controls overhaul
        - Current `CameraEditor` is awkward to manipulate (orbit feels off-axis, pan/zoom thresholds are inconsistent,
          RMB-drag direction sometimes fights the user). Re-tune sensitivity per axis, add deadzones, support
          Maya/Blender-style middle-click navigation as an option, surface the settings under
          `Settings > Editor > Camera`.
        - **Standard navigation presets** — quick buttons (ribbon `View` group + viewport overlay) for: **Reset View**
          (snap back to the default editor pose); axis-aligned ortho views **XY** (top-down), **XZ** (front),
          **YZ** (side); **Frame Selection** (zoom to fit the selected entity);
          **Frame Scene** (zoom to fit the whole scene's bounds).
        - **Go to camera viewpoint** — snap the editor camera onto any selected `component::Camera` entity's pose
          (translation + rotation)
          for a quick preview. Disabled for cameras whose `RendererTag`
          targets a `RendererRaycast` layer (the editor renders those scenes flat — see "Look through scene camera"
          below for the proper first-person preview path).
    - ![Planned][planned] "Look through scene camera" mode
        - Let the user temporarily drive the editor viewport from any
          `component::Camera` entity in the scene (primary or otherwise), for previewing what the runtime camera will
          see without entering Play. Toggleable from the camera entity's context menu or a viewport overlay dropdown.
          Reverts to the editor camera on demand.
- UI / HUD Editor
    - ![Planned][planned] Dedicated HUD layer, decoupled from the world renderer
        - Today the sample project ships a two-layer stack `[Renderer2D(world),
          Renderer2D(ui)]` and entities are tagged `ui` by hand — fine, but authoring the HUD still happens in the same
          top-down viewport as the world, with no preview of how it will look stretched over a raycast scene
        - Promote UI authoring to a **dedicated HUD layer** in the renderer stack that always renders on top, in
          pixel-space, regardless of what the layers underneath draw (raycast, voxel, 2D world, …)
        - **HUD Editor mode** in Owl Nest: a viewport variant that shows the HUD over a configurable backdrop (solid
          colour, a snapshot of the target gameplay scene, or live preview) and snaps to screen-space coordinates by
          default. Drag-drop sprites / text / panels onto the HUD; the existing `Ui*` components are reused
        - The HUD becomes a scene-level asset (`.owlhud` or a dedicated renderer-stack entry) referenced by gameplay
          scenes instead of being embedded as one renderer in each scene's stack — so the same HUD can ride on top of a
          raycast scene, a voxel scene, or a 2D scene without duplication
- Gameplay
    - ![Planned][planned] Inventory system
        - Collectible objects
        - Key-locked switches
    - ![Planned][planned] Enemies

## v0.2.2 -- Expected 2026-12-01

**Goal:** Add a third non-2D rendering mode — an isometric pseudo-3D renderer in the
**Transport Tycoon Deluxe** tradition — riding on the renderer stack architecture established in v0.2.0 and slotted
between the existing 2D/raycast/voxel options.

- Isometric Renderer
    - ![Planned][planned] `RendererIsometric` layer (Transport Tycoon-style)
        - Add `RendererIsometricLayer` (factory key `"RendererIsometric"`) so scenes can mix the isometric mode with the
          existing 2D / raycast / voxel stack just by listing it in `owl_project.yml` and tagging entities with the
          matching `RendererTag`.
        - **Pseudo-3D presentation** in the Transport Tycoon Deluxe style: a fixed 2:1 dimetric projection (no free-look
          camera), pre-rendered sprite tiles drawn back-to-front by world-space Y then Z. World axes map to screen as
          `screen.x = (worldX − worldY) · (tileW / 2)` and
          `screen.y = (worldX + worldY) · (tileH / 2) − worldZ · zStep`. Tile sprites are 64×32 px by default
          (configurable on the layer's
          `DefaultConfig`).
        - **Heightmap-aware tilemap**: extend `scene::Tileset` with optional per-tile slope/ramp metadata (flat,
          N/S/E/W/NE/NW/SE/SW slope) and a `cornerHeights` quad. The renderer composites the corresponding ramp / cliff
          sprite variant so the world has gentle slopes like TTD without modelling actual 3D meshes.
        - **Multi-Z stacking**: entities sort first by their projected Y (depth), then by world Z (elevation) so
          buildings stack cleanly on top of terrain tiles and over each other. The painter's order keeps the Renderer2D
          batching path; no new GPU pipeline required for the v0.2.2 cut.
        - **Editor support**: dedicated isometric viewport mode (locked to the dimetric projection, world-axis cursor +
          tile highlight),
          `TilemapDocument` gains an isometric preview when the target tileset is flagged `kind: isometric`, and gizmos
          use the isometric basis so click-drag of an entity feels native instead of zooming around in cartesian XY.
        - **Demo scene** in `sample_project/scenes/`: a small TTD-style town with a couple of buildings, ramps
          connecting two height plateaus, and a player entity that walks along the grid using
          `world_player.lua` (z-aware variant).
        - **Tests**: layer factory registration, scene round-trip with
          `EnabledRenderers: [{ Name: iso, Type: RendererIsometric }]`, the projection helpers (`worldToScreen` /
          `screenToWorld`), and the depth-sort comparator.

## v0.2.1 -- Expected 2026-10-01

**Goal:** Add the second non-2D rendering mode — a voxel engine for block-based worlds (Minecraft-style), riding on the
renderer stack architecture established in v0.2.0.

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

## v0.2.0 -- Expected 2026-08-01

**Goal:** Introduce a composable **renderer stack** so scenes can mix and match rendering modes (e.g. raycasting world +
2D HUD), then deliver the first non-2D mode (raycasting), the tilemap system, and scene-to-scene transition effects.

- Renderer Stack Architecture
    - ![Done][done] Foundation (engine API + serialization + tests)
        - `RenderLayer` interface, `RenderStack` orchestrator, `RenderLayerFactory`
          registry in `source/owl/public/renderer/`. `Renderer2DLayer` adapter wraps the existing `Renderer2D` (no
          rewrite) and is auto-registered during `Renderer::initShaders`.
        - `RendererTag` component on entities (`{ rendererName: string }`), wired into `CopiableComponents` /
          `SerializableComponents` /
          `OptionalComponents` and auto-handled by `SceneSerializer`.
        - YAML round-trip: `RendererStack:` block in `owl_project.yml`
          (project-level layer definitions with `Type` / `Name` /
          `DefaultConfig`) and `EnabledRenderers:` block in `.owl` files (per-scene enable + `Overrides` deep-merged on
          top of project defaults). Iterative merge keeps `misc-no-recursion` clean.
        - Backward compatible by construction: project without `RendererStack`
          → implicit `[Renderer2D(default)]`; scene without `EnabledRenderers`
          → all renderers active; entity without `RendererTag` → first renderer in stack.
        - Tests cover factory, stack build, scene-override merge, frame callback order, find-by-name, plus per-scene and
          per-entity serialization round-trips.
    - ![Done][done] Runtime install + per-entity dispatch
        - `EditorLayer::syncActiveDocumentPanels` builds the `RenderStack` from
          `Project::rendererStack` + `Scene::getEnabledRenderers()` and installs it via `Renderer::setRenderStack` on
          every active-document change; the runner does the equivalent at scene load.
        - `Scene::renderWithStack` orchestrates per-layer passes (`onBeginFrame`
          → `render` → `renderUI` → `onEndFrame`) and `Scene::layerAccepts`
          routes entities by their `RendererTag` (untagged → first layer; backgrounds draw only on the first pass).
    - ![Done][done] Editor UI for the stack
        - **Project Settings** modal: ordered list of layers with Type combo (factory keys), Name input, up/down reorder
          buttons, remove button,
          `+ Add Layer`. Stack changes hot-reload without reopening the project.
        - **Scene Hierarchy** inspector: editable `RendererTag` dropdown of the active stack's layer names + a
          `(default — first layer)` choice; surfaces a one-shot warning when the chosen name doesn't match any active
          layer.
        - **Sample project** (`sample_project/owl_project.yml`) opts into a two-layer stack
          `[Renderer2D(world), Renderer2D(ui)]` and tags all UI entities across every scene with
          `RendererTag: { Name: ui }` to dissociate scene rendering from HUD/menu rendering end-to-end.
    - ![Done][done] Per-scene settings panel in Owl Nest
        - Dockable `Scene Settings` window (Edit > Settings > Scene) editing the active scene's `EnabledRenderers`
          block: per-layer enable toggle, up / down ordering, `Detach` to revert to project default, and an `Overrides`
          collapsible with typed widgets per layer type (`Space` combo for `Renderer2D`; `Fov` / `MaxDistance` /
          `NumRays`
          drags + `CeilingColor` / `FloorColor` pickers for `RendererRaycast`).
        - Edits route through `commands::ModifyEnabledRenderersCommand` — undoable, with the standard 1 s
          merge-coalescing window for rapid drags.
        - Live preview: every commit triggers `syncActiveDocumentPanels` so the active `RenderStack` rebuilds without
          reopening the scene.
    - ![Done][done] Tilemap system for 2D
        - `scene::Tileset` asset (`.owltileset`): texture atlas + tile size +
          `columns × rows` grid + per-tile metadata (collidable flag, name). Sparse YAML round-trip.
        - `scene::component::Tilemap`: tileset reference, `width × height`
          cell grid, multi-layer support (each with name / visibility / parallax / tile data). Comma-separated tile
          encoding in YAML for compact storage on large grids.
        - Renderer: per-cell quads from atlas UVs in `Scene::render`.
        - Physics: one static Box2D body per Tilemap entity, one box fixture per collidable cell, generated at
          `onStartRuntime`.
        - Editor: inspector with tileset drop slot + grid resize + layer add/delete/reorder + per-layer
          parallax/visibility; **Tile Palette**
          panel for tile + active-layer + eraser selection; viewport paint mode (left-click paints, right-click erases)
          with `ModifyEntityCommand`
          undo per click. Fill / flood / rect tools deferred to a follow-up.
- Raycasting Renderer
    - ![Done][done] Raycasting core
        - `RendererRaycast` static facade and `RendererRaycastLayer`
          (`RenderLayer` adapter, factory key `"RendererRaycast"`).
        - CPU per-column DDA driving textured wall stripes through the existing `Renderer2D` quad batch — no new Slang
          shader required for v0.2.0 (deferred to a future PR).
        - Wall walls reuse the existing `scene::component::Tilemap` and
          `scene::Tileset` — `Scene::render` dispatches each tilemap to either the 2D path or the raycast path based on
          the active layer's type key.
        - Configurable FOV / max distance / sky / floor colours via the project's `DefaultConfig` (or per-scene
          `Overrides`); ray count defaults to the viewport width.
        - `renderer/{stack,renderer2d,rendererraycast}/` sub-folders + matching sub-namespaces opened up the renderer
          module for the future voxel layer.
        - Sample project ships `scenes/raycast_demo.owl` — full **Wolfenstein 3D E1L1** layout (64×64) imported from a
          reference raycaster, walls textured from the original Wolfenstein art (greystone / bluestone / wood /
          doorpattern, 4×4 atlas at 128 px/tile, NEAREST-filtered).
        - Reachable from `world_map.owl`'s second house structure (cols 22-28, rows 11-14, distinct shape from the
          existing house, accessed via the extended stone path).
        - Texture filtering: new `renderer::FilterMode` enum, `Tileset` can declare `filterMode: Nearest` in YAML so the
          wall atlas stays pixel-crisp at any distance (no LINEAR/mipmap blur on far walls).
    - ![Done][done] Tilemap editor as dedicated asset (`.owltilemap`)
        - `scene::TilemapAsset` class with full YAML round-trip (`serializeToString` / `deserializeFromString` /
          `saveToFile` /
          `loadFromFile`), `addLayer` / `resize` / `getTile` / `setTile` API, 13 unit tests covering round-trips,
          malformed input, short / oversized tile buffers, and file I/O.
        - `scene::component::Tilemap` is a path-reference component (`tilemapPath` + runtime `asset` shared pointer);
          render paths (2D + raycast) and physics read from the resolved asset.
          `Scene::resolveAllTilemapAssets` performs a two-phase load.
        - `nest::TilemapDocument` (5th `DocumentType`) — three-pane editor (Properties / Canvas / Palette). Zoom + pan,
          paint / erase strokes, full layer manager, undo via per-stroke `ModifyTilemapAssetCommand`. Fill / flood /
          rect tools deferred — open as follow-up. The in-scene paint mode is removed; the inspector shows a read-only
          summary with a drop slot for `.owltilemap` references.
        - `world_map.owl`, `platformer_house.owl` and `raycast_demo.owl`
          migrated via `tools/migrate_inline_tilemaps.py`; their inline data lives in
          `sample_project/tilemaps/*.owltilemap`. The legacy inline reader on `scene::component::Tilemap` is kept as a
          defensive fallback for unmigrated projects and will be removed in a future release.
    - ![Done][done] Floors and ceilings
        - Per-screen-row textured backdrop in `emitTexturedBackdrop`: each row projects its pixel back to the floor (or
          ceiling) plane using the camera-plane basis, then emits a 1-pixel-tall quad whose UVs interpolate linearly
          between the left / right ends of the visible cone. Combined with the texture's REPEAT wrap, this paints the
          entire backdrop in one quad per scanline (≈ `viewport.y` quads / frame for both halves).
        - `RaycastConfig` carries `floorTilesetPath` + `floorTileIndex` and the matching ceiling pair;
          `RendererRaycastLayer` resolves them on first `onBeginFrame` via the asset library and populates the runtime
          `floorTexture` / `ceilingTexture` + UV-rect fields. Same tileset reference convention as doors / pushwalls —
          pointing the floor at the world tilemap's atlas reuses the shared
          `shared<Tileset>` instance.
        - Falls back to the existing solid-colour quads (`ceilingColor` / `floorColor`) when the corresponding texture
          is absent — backward compatible with every existing demo scene.
        - Stats: `backdropScanlineCount` reports the per-frame backdrop quad budget.
    - ![Done][done] Sprites (billboards)
        - Entities carrying `SpriteRenderer` / `AnimatedSpriteRenderer` are rendered as camera-facing billboards on
          `RendererRaycast` layers without any new component — the same components stay 2D-rendered on `Renderer2D`
          layers, so a single entity is authored top-down in the editor and shown first-person at runtime.
        - `RendererRaycast` gained a per-column z-buffer latched by
          `drawTilemapWalls`; the new `drawSprites(span<RaycastSpriteData>)`
          batch projects each sprite to camera space, culls behind / beyond `maxDistance`, back-to-front-sorts the
          survivors and emits 1-pixel-wide strips that the per-column z-test rejects when a wall sits in front.
          Sprite-on-sprite occlusion falls out of painter's order.
        - `Transform.translation.z` is the world Z-offset (lamps, ceiling decals); `Transform.scale.xy` is the world
          size in cells. A sprite of size `{1,1}` reaches the same screen height as a 1-cell wall at the same depth.
          Stats: `spriteCount`,
          `spriteStripeCount`, `spriteOccludedCount`.
        - Demo: `raycast_demo.owl` ships a barrel + animated-coin sprite near the player spawn, both authored with the
          regular
          `SpriteRenderer` / `AnimatedSpriteRenderer` components and tagged on the `raycast_world` layer via
          `RendererTag`.
        - 6 new headless tests in `RendererRaycast_test.cpp` cover empty span, camera cull, in-front emission, wall
          occlusion, max-distance cull and texture-less skip.
    - ![Done][done] Map features
        - ![Done][done] Variable wall heights — `TileMeta.wallHeight: float
          = 1.0` (clamped to `[0, 8]`). Walls are bottom-anchored at floor level. Editor: Raycast section in the
          `TilesetDocument`
          properties panel with a wall-height drag + tooltip. YAML round-trip is sparse (only emitted when ≠ 1.0).
        - ![Done][done] Transparent walls — `TileMeta.transparent: bool`. DDA continues past transparent tiles, collects
          up to 8 hits per ray, renders back-to-front so alpha-blended textures composite correctly. The closest opaque
          hit drives the sprite-occlusion z-buffer, so sprites stay visible through transparent walls. Transparency is *
          *alpha-channel only** — the project intentionally does not support chroma keying; author tiles with a proper
          PNG alpha channel. Known v0.2.0 limitation: a sprite sitting behind a transparent wall draws on top of it
          (sprites and walls aren't merged per column yet).
        - ![Done][done] Doors and pushwalls — dedicated entity components (`component::RaycastDoor` +
          `component::RaycastPushWall`), not tile flags, so each instance is addressable from scripts. Pushwalls are
          full-cell cubes routed through
          `RendererRaycast::drawDynamicWalls`. Doors are 1×1 cells with zero-thickness laterals (cube inside faces
          perpendicular to the opening direction) and a zero-thickness sliding plate (normal perpendicular to the
          opening direction) routed through `RendererRaycast::drawDoors`. `OpeningDirection` is a cardinal enum (N / S /
          E / W); the door always slides exactly one cell with a +1-pixel hermetic-closure margin that scales with the
          open progress. Both components reference textures via `tilesetPath` + tile indices — a per-scene tileset cache
          reuses the world tilemap's
          `shared<Tileset>` instance so the atlas texture isn't double-loaded. Box2D collision is handled automatically:
          `PhysicCommand::init` auto-creates a kinematic body matching the moving surface (thin box for the door's
          plate, full cube for pushwalls), so a closed door is never traversable out of the box. Activation is hybrid:
          built-in
          `interactionKey` (default `E`) + `interactionRange`
          (default 1.5 cells) for out-of-the-box use, set
          `interactionKey = 0` to disable the built-in path and drive activation from Lua via `door.activate / door.close /
          door.is_open / door.get_state` and `pushwall.activate /
          pushwall.has_moved / pushwall.get_state`. State machines:
          doors cycle Idle → Opening → Open (held for `holdTime`) → Closing → Idle; pushwalls go Idle → Moving → Final
          once. Editor: visual tile picker in the inspector (click a thumbnail to open a grid popup of the tileset's
          tiles); green outline around every pushwall in the 2D viewport; yellow destination line + endpoint circle for
          the selected door / pushwall; thin plate strip in the door's 2D preview oriented along the slide axis. `Add Component ▸ Raycast
          Door / Raycast PushWall` is gated by layer type (only shown for entities on a `RendererRaycast` layer; the
          same gating hides `Ui*` entries unless the entity sits on a
          `Renderer2D`). **Thin walls** were dropped from scope.
    - ![Done][done] Raycasting map editor in Owl Nest
        - ![Done][done] 2D grid editor for wall placement and texture assignment (`TilemapDocument` ships in v0.2.0)
        - ![Done][done] Entity placement on the grid via the existing scene editor (entities, triggers, lights, … are
          already authorable on top of any tilemap)
        - ![Done][done] Top-down preview alongside first-person preview — the editor viewport is the top-down view; Play
          mode is the first-person view
        - ![Done][done] In-viewport camera marker — a small dot + forward arrow + FOV cone for every `component::Camera`
          entity, drawn in the top-down editor view so the level designer can see where the player will spawn and which
          direction it faces. Toggleable from a viewport overlay button and from the ribbon's `Show` group.
    - ![Done][done] Lighting for raycasting (v0.2.0 scope — global fog only; point lights deferred)
        - ![Done][done] Distance fog — `RaycastConfig::fogColor` + `fogStart` +
          `fogEnd`. Every wall / dynamic-wall / door / sprite stripe and every backdrop scanline is lerped toward
          `fogColor` between the two distances. Disabled when `fogEnd <= fogStart` (default), so existing scenes keep
          their natural tint. Same `applyFog` helper drives all paths so a wall and the floor pixels right below it
          converge to the identical tint at `fogEnd` — no visible seam.
        - *Deferred to v0.2.x* Optional point lights with falloff — the fog system already covers global atmosphere;
          per-light shading needs a per-column light-list buffer and a deferred-style accumulation pass.
- Gameplay
    - ![Done][done] Scene transition effects
        - `scene::ScreenTransition::Type` extended from `Fade*` only to
          `Fade{In,Out}` + `Wipe{Left,Right,Up,Down}`; `play(type, duration,
          colour)` accepts a custom tint (legacy `start(type, duration)` keeps the opaque-black default for
          back-compat).
        - Lua API: new `ui.transition_play(type_string, duration, [r, g, b, a])`
          dispatcher (accepts `"fade"`, `"fade_in"`, `"fade_out"`,
          `"wipe_left"`, `"wipe_right"`, `"wipe_up"`, `"wipe_down"`).
          `ui.transition_fade_in(d)` / `ui.transition_fade_out(d)` remain as back-compat shorthands.
        - Sample project: `sample_project/scripts/raycast_house_door.lua` now uses `wipe_left` so the world→raycast
          handoff shows off a non-fade variant.
        - Tests: `ScreenTransition_test` covers default state, custom-colour play, progress advancement, completion, all
          wipe variants, reset, and the `< 1 ms` duration clamp.
- Editor Performance
    - ![Done][done] Faster scene loading (target exceeded 550×)
        - ![Done][done] **Target met by a 550× factor**, the tracker O (N²) fix above pulled the load from 22 s to 40ms.
          Prefab YAML caching + PrefabSerializer round-trip drop were considered but **dropped** as the perf target is
          already exceeded by orders of magnitude.
        - ![Done][done] **Memory tracker O (N²) regression fixed.**
          Global `operator new`/`delete` overrides disabled in release builds; debug builds use an O (1) `unordered_map`
          index instead of the linear list scan. Effect on the reference editor session: opening the raycast_demo scene
          (110 entities, 55 KB YAML) drops from ~22 s of frozen UI to ~40 ms. Every burst-allocation workload (YAML
          parse, Lua compile, scene deserialisation, undo replay) speeds up proportionally.
        - ![Done][done] UUID → `entt::entity` cache on Scene, kept warm across the scene's lifetime, invalidated on
          create/destroy. `Scene::findEntityByUUID` is now O (1)
          with a self-healing fallback. Eliminates the O (N²) walk in `rebuildHierarchyChildren`, prefab instantiation,
          and
          `EntityLink` warmup. Test:
          `SceneRuntimeTest.FindEntityByUUIDCacheSurvivesDestroy`.
        - ![Done][done] In-memory SPIR-V cache. Mutex-guarded
          `unordered_map<string, vector<uint32_t>>` in
          `shaderFileUtils.cpp` — the second scene of a session skips disk reads of every `.spv` blob. The persistent
          on-disk cache stays as the cold-launch warmup. Target: opening `sample_project/scenes/raycast_demo.owl`
          in under half its current time on the reference dev preset.
    - ![Done][done] Async scene loading (v0.2.0 scope — the heavy parse-and-decode path is now off the main thread;
      cancellation UI + parallel decode were considered and dropped because the load is already ~40 ms)
        - ![Done][done] Two-phase `SceneSerializer::parseBuffer` /
          `applyParsed` split — YAML parsing runs on a Taskflow worker, entity creation + GPU placeholder upload stays
          on the main thread. `EditorLayer::openScene`,
          `SceneDocument::handleTeleportRequest`, and
          `RunnerLayer::finishTransition` all route through the new path. Progress overlay shows "Loading…" → "Parsing…"
          → "Materialising entities…".
        - ![Done][done] Parallel-execution scaffolding —
          `Scheduler::getImpl()` + `parallelForEach(Scheduler&, …)`
          / `parallelForIndex(Scheduler&, …)` overloads in
          `ParallelUtils.h`. Engine call sites no longer have to name `tf::Executor`; Taskflow stays a PRIVATE
          dependency.
- In-Game Performance
    - ![Done][done] Entity / system update budget
        - *Note* The original "profile worst-case scene first" item was overtaken by the tracker O (N²) fix, which
          reduced the dominant cost from seconds-per-tick to microseconds. Remaining sub-items below all shipped.
        - ![Done][done] Skip dormant entities in the hot loops: entities with `Visibility.gameVisible = false` (or whose
          ancestor is hidden) bypass script/physics ticks instead of being filtered per-loop — generalised the trigger
          pattern to native scripts, Lua scripts, and `EntityLink` resolution
        - ![Done][done] Frame-pool the per-render scratch buffers, mirroring the per-column `zBufferPerColumn`
          pattern: `vector<RaycastSpriteData>` / `vector<RaycastDoorData>` / `vector<RaycastDynamicWallData>`
          in `Scene::renderRaycast*`, `vector<CanvasEntry>` in `Scene::renderUI`, plus
          `vector<ColumnHit>` and `vector<Projected>` inside `RendererRaycast` all use `thread_local` reuse — allocation
          is paid once per session, not once per frame. The original audit's "trigger overlap list / animated-sprite
          frame index list" turned out to not exist in code (those loops iterate views directly); the real per-frame
          allocations were the render-side vectors above.
    - ![Done][done] Entity-management hot paths
        - ![Done][done] Cache the primary-player lookup result on Scene (invalidated on entity destroy / `Player`
          add/remove) instead of re-scanning the full registry every tick from
          `Scene::updateRaycastDynamicWalls` + `Scene::onUpdateRuntime` + trigger overlap check + sound listener pose.
        - ![Done][done] Keep the UUID → entity cache warm — the map landed under *Faster scene loading* and serves every
          per-tick lookup (`rebuildHierarchyChildren`, prefab instantiation, Lua `scene.find_entity`, editor hierarchy
          drag-drop, `Ctrl+D` duplicate) at O (1).
        - ![Done][done] Pre-resolve `EntityLink.linkedEntity` once per scene start (`Scene::resolveAllEntityLinks`,
          called from
          `onStartRuntime`) so the per-frame loop never falls into the O (N²) `view<Tag>` rescan path on the first tick
    - ![Done][done] Render-loop hygiene
        - ![Done][done] `getWorldTransform` per-pass cache (`m_worldTransformCache`, gated by
          `m_worldTransformCacheActive`). Armed only after the mutating phases (scripts / physics / EntityLinks) have
          completed so the sound + render reads dedupe against the first walk per entity — the same world matrix was
          previously rebuilt up to ~30× per frame across sprites + circles + text + tilemaps + raycast sprites + dynamic
          walls + doors + physics sync + sound listener/source. Cache bypassed outside the pass so tests / inspector
          always see fresh state.
        - ![Done][done] `layerHasContent` per-pass cache (`m_layerContentCacheFirst` /
          `m_layerContentCacheNotFirst`). Each layer's 7-view scan now runs once per render-stack walk; the
          second-and-subsequent calls hit a hashmap lookup. Gated by `m_inUpdatePass`.
        - ![Done][done] `isEffectivelyVisible` per-pass cache (`m_visibilityCache`, key packs the entity id +
          editor-mode bit). The parent-chain walk runs once per (entity, mode) per pass; sibling entities sharing the
          same root all hit O (1) cache after the first walk. Gated by `m_inUpdatePass`.
        - ![Done][done] `resolveAllTilemapAssets` dirty flag (`m_tilemapAssetsDirty`). Set true in
          `onComponentAdded<Tilemap | RaycastDoor | RaycastPushWall>` and by the public
          `invalidateTilemapAssets()` (for inspector path-edit code). The function early-returns when clean, so the
          per-frame call from `Scene::render` is now a single bool check on warm scenes instead of three empty view
          iterations.
    - ![In Progress][progress] GPU offload — moved into v0.2.0 to get the biggest CPU→GPU wins shipped with the renderer
      stack itself (deferred from "longer horizon" status — user committed to completing v0.2.0 with the GPU push)
        - ![Planned][planned] Tilemap rendering via instanced quads + SSBO instead of one `drawQuad` per cell. Cleanest
          self-contained win: rewrite `RenderableTilemap` to push per-cell `{positionXY, uvIndex, tint}` into an SSBO
          and issue a single `vkCmdDraw(4, cellCount, …)` instanced quad call. Expected 5–15× on 64×64 maps (the editor
          tilemap currently issues 4096 calls per frame).
        - ![Planned][planned] GPU sprite Z-sort via radix shader. Replace the per-frame CPU sort in `RendererRaycast`
          (and
          `Renderer2D` if it sorts) with a compute-shader radix sort over the sprite SSBO. Expected 3–8× on 100+ sprite
          scenes.
        - ![Planned][planned] Hierarchical world-transform pre-pass in a compute shader. Each entity dispatches one
          thread that reads its local transform + parent index, walks up, and writes the flat `mat4` into a per-frame
          world-matrix SSBO. Renderer reads SSBO directly. Expected 5–10× on deep hierarchies and removes the ~30
          redundant CPU rebuilds of the same matrix per frame.
        - ![Planned][planned] Compute-driven frustum / occlusion culling pre-pass feeding indirect draws. Compute shader
          tests each entity's AABB against the frustum and writes a packed visible-index buffer; the draw call becomes
          `vkCmdDrawIndirect`. Foundation for the bigger v0.3.0 3D scenes; gives 2–4× on scenes with lots of off-screen
          entities even in 2D.
        - ![Planned][planned] Raycast DDA in a compute shader, one thread per column. Biggest single CPU→GPU win (20–50×
          theoretical) but also the most invasive — touches the entire raycast renderer architecture, the per-column
          zBuffer, dynamic walls, sprite occlusion. Plan to land last after the smaller GPU items have shaken out the
          SSBO / compute-shader plumbing.
- Known bug fixes (deferred from v0.2.0 — all closed during v0.2.0)
    - ![Done][done] Editor keyboard shortcuts unreliable — *fixed in v0.2.0*
        - Modifier-based shortcuts (Ctrl+S, Ctrl+Z, …) now bypass
          `ImGui::GetIO().WantCaptureKeyboard`, matching the convention used by VS Code / Blender / Unity. Modifier-less
          shortcuts still yield to focused text widgets. When a shortcut would have matched but is suppressed by
          capture, `ActionRegistry::dispatch` logs a TRACE entry.
    - ![Done][done] World-map top-down player drifts vertically — *fixed in v0.2.0*
        - New `physics.set_gravity_scale(entity, scale)` Lua API exposes
          `b2Body_SetGravityScale`. `world_player.lua` now sets scale = 0 in
          `on_create` instead of the per-frame `+9.81 * dt` cancellation hack.
    - ![Done][done] Hidden triggers fired regardless of visibility — *fixed in v0.2.0*
        - `Scene::onUpdateRuntime` now skips trigger entities whose
          `Visibility.gameVisible` is false (or whose ancestor is hidden); any in-progress timer is stopped and prior
          overlap state is cleared with a synthetic `onTriggerExit`.

## v0.1.1 -- 2026-04-30

**Goal:** Transform the editor from a single-scene tool into a multi-document workspace with dedicated editors for
different asset types. All long-running operations become asynchronous with progress feedback.

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
    - ![Done][done] Async texture loading with placeholders
        - `TextureDecoder` helper (`peekImageSize`/`decodeImageBytes`/`decodeImageFile`) with per-thread stb_image flip
          state for safe concurrent decoding
        - `Texture2D::createFromSerializedAsync` returns immediately with a placeholder-sized Rgba8 texture filled
          white; dimensions peeked cheaply from the PNG/JPG header so the real size is correct from frame 0 (not a 1×1
          bump later)
        - Worker thread decodes, termination callback uploads real pixels and flips
          `LoadState` to `Ready` (or `Failed`, leaving the placeholder visible)
        - `createFromSerializedForDeserialize` wrapper lets `SpriteRenderer`,
          `AnimatedSpriteRenderer`, `BackgroundTexture`, `UiImage` stay a single-line call that goes async under an
          `Application`, synchronous for `PackWriter` / tests
        - Runner diagnostic trace after each teleport lists the count of still-pending textures
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
    - ![Done][done] Document tab system
        - `Document` / `DocumentManager` / `SceneDocument` / `DocumentTabBar` in
          `source/owlnest/sources/document/`
        - Per-document undo stack, dirty marker, Play/Pause/Stop state
        - Active document concept; global panels (hierarchy, inspector) follow it
        - Tab bar rendered **inside the Viewport header** with dirty `*`, play/pause badge, close button with
          confirmation prompt (no separate "Documents" window)
        - Play/Gizmo toolbars hidden when viewing a tab that is not the one running
        - `Ctrl+W` close, `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle, `File > Open Scene` opens in a new tab (or reuses an
          already-open one)
        - Background simulation: non-active tabs in Play mode advance physics/scripts without rendering
          (`Scene::onUpdateRuntime` gained an `iRender` flag)
    - ![Done][done] Per-document viewport (side-by-side via docking)
        - Each `SceneDocument` owns its own `Viewport` with its own framebuffer and a stable
          `##scene_<uuid>` ImGui window id
        - ImGui docking groups viewports as tabs automatically; tear one off to see scenes side-by-side. Dirty marker
          via `ImGuiWindowFlags_UnsavedDocument`, close via native
          `p_open`. New viewports auto-dock to the central node on first open
        - Active document = last-focused viewport; hierarchy / inspector follow it
    - ![Done][done] Detachable panels
        - Fournit par le docking natif d'ImGui (`ImGuiConfigFlags_DockingEnable` +
          `ImGuiConfigFlags_ViewportsEnable` activés dans `UiLayer`)
        - N'importe quel panneau (hierarchy, viewport, content browser, log…) peut être drag-out en fenêtre OS
          indépendante ou docké dans un autre nœud
- Script / Code Editor
    - ![Done][done] Generic code editor document
        - New `CodeEditorDocument` (DocumentType::Code) — second kind of document after
          `SceneDocument`, opens from ContentBrowser double-click on a text/source file
        - Powered by **imgui_color_text_edit** 1.92.6 fetched via DepManager (`depmanager.yml`);
          `imgui` aligned to the same docking branch 1.92.6-docking
        - Syntax highlighting: **Lua**, **C**, **C++**, **Python**, **JSON**, **Markdown**
          (built-in) plus **YAML** and **SVG/XML** (custom definitions in
          `source/owlnest/sources/document/codeEditor/LanguageDefinitions.*`)
        - Dedicated **JetBrains Mono** font for the editor buffers (monospace column alignment), shipped externally in
          `engine_assets/fonts/jetbrainsmono/` and rasterised at the user- configured size
          (`EditorSettings::codeEditorFontSize`, 8–48, default 17; restart required — the atlas is built once in
          `UiLayer::onAttach`)
        - Matching `EditorSettings::uiFontSize` slider (14–24, default 18) for the main Roboto UI font; both are applied
          from `main.cpp` before `Application` construction via
          `UiLayer::setUiFontSize` / `setCodeFontSize`
        - Dirty via `ImGuiWindowFlags_UnsavedDocument`, Ctrl+S to save, close via
          `Ctrl+W` / `Scene > Close` modal
    - ![Done][done] Live preview for markup documents
        - `MarkdownPreview` (`source/owlnest/sources/document/codeEditor/`) is a full Markdown renderer:
          `MarkdownDocument` parses CommonMark + GFM via
          **md4c** (new DepManager recipe `OwlDependencies/Libs/md4c/`, replaces the previous `imgui_markdown`
          integration) and the renderer walks the parsed block list to emit ImGui draw calls — scaled headings, GFM
          tables (`BeginTable`), code blocks rendered with a cached read-only
          `TextEditor` (full syntax highlighting), local images via `lunasvg` /
          `stb_image`, external `https://` images and links open in the user's default browser via the new
          `core::utils::openExternalUrl`. Update is debounced (~250 ms); auto-enabled on `.md`, toggleable from the
          **Text → Preview** ribbon button.
        - `SvgPreview` rasterizes the live SVG via `lunasvg` into a `Texture2D`
          (debounced, capped at 2048 px per side, ARGB-premul → RGBA-straight conversion shared with `IconBank`);
          auto-enabled on `.svg` / `.xml`
        - `CodeEditorDocument` gained a vertical splitter between the editor and the preview pane, with a draggable
          handle and per-document split ratio
- Node Graph Editor
    - ![Done][done] Node graph framework
        - Reusable `gui::widgets::NodeCanvas` widget — UUID-based nodes/pins/links, typed pins, link validator,
          pan/zoom/selection, double-click detection, callbacks for create/delete/move. Pimpl wrapper over `GraphEditor`
          from the ImGuizmo bundle (no new DepManager dependency)
        - `UndoCommand<Target>` / `UndoManager<Target>` templatized, with `SceneUndoCommand`
          alias preserving editor behaviour — also `NodeGraphUndoManager` for canvas edits
        - `NodeCanvasSerializer` — `.owlflow` YAML round-trip (full + subset for copy/paste with fresh UUIDs)
        - `NodeGraphDocument` as a third `DocumentType`, ribbon contextual "Graph" tab,
          `.owlflow` content-browser handling + drag-drop routing
        - Node-graph undo commands: AddNode / RemoveNode (restores attached links) / MoveNode (drag-coalesced) /
          AddLink / RemoveLink
    - ![Done][done] Scene flow view (first node graph usage)
        - ![Done][done] Scenes as nodes, teleport triggers as output pins, links wired from output → destination scene
          entry, orphan detection (BFS from `Project::firstScene`, unreachable scenes drawn in red). Exposed from the
          File ribbon tab → Views → Scene Flow
        - ![Done][done] Double-click a node → navigates to that scene via `EditorLayer::openScene`
        - ![Done][done] Visual create of teleport links — every scene node carries a ghost
          `+ Add teleport` output pin; dragging it onto another scene's entry creates a `Trigger`
          (`Type=Teleport`, `LevelName=<dest>`) entity in the source scene at world origin and wires the canvas link in
          one undoable step. Source scene is opened silently via
          `EditorLayer::loadOrOpenSceneDocument` if not already in a tab.
        - ![Done][done] Visual delete of teleport links — pressing Delete on a Teleport link destroys the matching
          `Trigger` entity, removes the canvas pin, and erases the link in a single undoable step.
        - ![Done][done] Per-pin `targetName` editing — right-click a scene node → `Edit teleport
          target → <pin>` opens a modal that mutates the live `Trigger.targetName` and pushes a
          `ModifyEntityCommand` on the source scene's undo manager (rapid keystrokes coalesce).
        - ![Done][done] New `commands::SceneFlowCompositeCommand` glues a `SceneUndoCommand` and a
          `NodeGraphUndoCommand` so a single undo step reverses both halves; complemented by
          `AddPinAndLinkCommand` / `RemovePinAndLinkCommand` for the canvas pin+link bundle.
        - ![Done][done] Canvas polish — text level-of-detail (pin labels hide below 0.6 zoom, node titles below 0.3)
          plus per-layer vertical centring so single-node columns align around the same horizontal mid-line as the rest
          of the graph.
- Asset Editors
    - ![Done][done] Animation editor
        - New reusable asset format `.owlanim` (`scene::AnimationClip`) — texture, grid, frame range, frame duration,
          loop, optional speed curve. YAML round-trip with unit-test coverage in
          `test/scene_tests/AnimationClip_test.cpp`
        - `AnimationDocument` opens as a document tab (4th `DocumentType`) with three panels: live spritesheet preview,
          properties (texture drop, columns/rows, first/last, frame duration, loop, embedded `curveEditor` for the speed
          curve), and a frame-range timeline backed by the new `gui::widgets::sequencer()` wrapper around `ImSequencer`
          from the imguizmo bundle (no new DepManager dep)
        - Ribbon contextual `Animation` tab with Playback (Play/Pause/Stop), Frame (Previous/Next) and File (Save / Save
          As / Close) groups
        - Content Browser double-click + drag-drop on `.owlanim`, dedicated icon, ribbon File → "New Animation" entry to
          spawn an untitled clip
    - ![Done][done] Enhanced inspector
        - ![Done][done] Sound preview button on SoundSource component (Play / Stop, uses current volume and pitch)
        - ![Done][done] Texture thumbnail preview, font preview
            - Texture rows show a 100x100 thumbnail with a `(loading...)` / `(failed)` overlay while async-loaded
              textures are still decoding (`renderer::LoadState`)
            - Font preview: small sample-string strip rendered through a new
              `gui::FontPreviewCache` (lazy off-screen render of `Aa Bb 1!? éàüÇ` via
              `Renderer2D::drawString`, cached per font name); pumped from
              `EditorLayer::onUpdate` and freed on `UiLayer::onDetach`. First frame falls back to the MSDF atlas image
            - Latin-1 / UTF-8 glyph rendering fixed in `Font::getGlyphBox` and
              `Renderer2D::drawString` so accented characters render correctly everywhere (not just in the inspector
              preview)
        - ![Done][done] Drag-drop assets from content browser to inspector fields
            - Reusable `gui::widgets::assetDropTarget(AssetKind, path)` helper layered on the existing
              `CONTENT_BROWSER_ITEM` payload — per-extension validation via a new
              `AssetKind` enum (Texture / Font / Sound / LuaScript / AnyScript / Scene / Prefab / Any)
            - `gui::widgets::textureField()` consolidates the previously inlined thumbnail/popup/remove pattern into one
              helper used by every texture-aware component (Sprite / AnimatedSprite / BackgroundTexture / UiImage)
            - Drop targets wired on Text font, SoundSource asset and LuaScript path on top of their existing widgets
        - ![Done][done] Curve editor for animated properties
            - New `math::Curve` (`source/owl/public/math/Curve.h`) — sorted keyframe list with Constant / Linear /
              Smooth interpolation, flat-hold extrapolation, and YAML round-trip (default-empty curves are omitted from
              `.owl` output to preserve byte-identical scenes)
            - `gui::widgets::curveEditor()` widget wraps ImCurveEdit from the existing imguizmo bundle (no new
              DepManager dependency); auto-fits the canvas viewing range (X always shows `[0, 1]`, Y auto-fits keys with
              20% margin, always includes the zero baseline)
            - First end-to-end consumer: `AnimatedSpriteRenderer.speedCurve` remaps per-frame `dt` by
              `speedCurve.evaluate(progress)` where `progress` is the normalized position inside
              `[firstFrame, lastFrame]`
- Packaging
    - ![Done][done] Packaging wizard in Owl Nest
        - Pre-packaging validation: `AssetScanner` warnings output for unresolvable texture/sound/script/scene/font
          references
        - Validation modal before pack with issue list + "Proceed anyway" / "Cancel" buttons
        - OwlRunner executable check + empty-assets check
        - Dedicated wizard panel: destination input + Browse, target platform (read-only), compress/obfuscate options
        - Post-pack build report: asset count, pack size (MiB), duration shown on completion
- Menu & Project Workflow
    - ![Done][done] Ribbon replaces the classic menu bar (see the **Ribbon-style main menu**
      entry below) — all project / scene actions now live in its File / Edit / Scene|Text tabs, and "Show Stats" moved
      into the Editor Settings panel
    - ![Done][done] Recent projects
        - Persisted in `EditorSettings::recentProjects` (capped at 10 entries, most recent first)
        - "Recent" button in the ribbon File tab opens a popup listing the projects (full path as shortcut text, click
          to open)
        - Welcome screen modal shown when no project is loaded: New/Open buttons + recent list
        - Double-click a recent entry to open, `x` button to remove individual entries
    - ![Done][done] Save Project As
        - `EditorLayer::saveProjectAs()` prompts for a destination folder and duplicates the current project recursively
          via `std::filesystem::copy`, then switches the editor to the new directory
- UX & Quality
    - ![Done][done] In-editor help pages
        - `cmake/HelpAssets.cmake` bundles `doc/pages/*.md` plus `README` /
          `CHANGELOG` / `CONTRIBUTING` into `engine_assets/help/` at configure time and writes an `index.yml` describing
          every page (title parsed from the first H1 line, Doxygen anchor stripped). The bundle ships inside packaged
          builds via the existing `engine_assets/` install rule
        - `panel::HelpPanel` reads the index, renders the selected `.md`
          through `codeEditor::MarkdownPreview`, supports search, categorised navigation, and a back/forward history.
          Internal `[link](other.md)`
          clicks navigate within the panel; external `http(s)://` links log the URL for now
        - F1 (`help.context` action) opens the page that documents the component header most recently hovered in the
          SceneHierarchy inspector (`SceneHierarchy::lastHoveredComponentName`), falling back to the editor overview
          when nothing is hovered
        - The Welcome screen surfaces a **Getting Started** entry pointing to
          `getting_started.md`, and the File ribbon tab gained a **Help** group
    - ![Done][done] Tooltips everywhere with hover delay
        - Reusable `fieldTooltip()` helper with `DelayNormal` (~0.4s) hover delay
        - Tooltips on all 7 trigger types with descriptions
        - Tooltips on trigger sub-fields (Scene, Level Name, Target Name, Duration, Range, Callback)
        - Tooltips on PhysicBody fields (Type, Density, Restitution, Friction, Fixed Rotation)
        - Tooltips on Player fields (Primary, Linear/Jump Impulse, Can jump)
        - Tooltips on Camera fields (Primary, Projection type, FOV, Near/Far, Ortho Size, Fixed Aspect)
        - Tooltips on SoundSource fields (Asset, Category, Volume, Pitch, Loop, Spatial, Max Distance, Rolloff)
        - Tooltips on SoundListener (Primary)
        - Tooltips on Canvas (Space, Sort Order), UiRect (Anchor, Pivot, Size, Offset)
        - Tooltips on UiButton colours + On Click callback, UiSlider value/min/max + On Value Changed
    - ![Done][done] Unique ImGui IDs audit
        - Component-scoped `PushID(T::name())` in `drawComponent<T>` — prevents label collisions between components that
          share field names (e.g. "Colour" in SpriteRenderer and CircleRenderer)
        - Index-based `PushID` in LuaScript property loop — prevents collision if two properties share a name
        - Entity list in SceneHierarchy already uses UUID-based PushID (verified safe)
        - ContentBrowser and SettingsPanel already use unique per-item IDs (verified safe)
    - ![Done][done] Icon clarity pass
        - Content-browser icons are per-extension (sound: `wav`/`mp3`/`ogg`/`flac`, mesh:
          `obj`/`gltf`/`glb`/`fbx`, source: `py`/`cpp`/`h`/`c`, docs: `md`) with a ribbon label and a central type glyph
          sharing the `base_file_ext_icon` template
        - Existing icons (`png`, `jpg`, `svg`, `glsl`, `owl`, `yml`, `ttf`, `lua`, `json`) now carry a central type
          glyph
        - Secondary accent colour is a fixed amber/gold (`#ffc726`) matching the Owl Nest brand
        - `IconBank::iconButton(name, label, size)` helper renders an icon-prefixed button, reused across Welcome,
          Packaging Wizard, validation modal, AsyncProgressModal, Content Browser dialogs, Log panel,
          Settings/Parameters/Project Settings
    - ![Done][done] Ribbon-style main menu
        - `gui::widgets::Ribbon` widget with tabs → groups → large / small buttons (3 small = 1 large height) in
          `source/owl/public/gui/widgets/`
        - `UiLayer::setTopBarCallback` reserves space above the DockSpace for the ribbon
        - Replaces the former `ImGui::BeginMenuBar` drop-downs, the floating Play/Pause toolbar, and the gizmo
          `ButtonBar`
        - File / Edit / Scene|Text tabs built from the existing `ActionRegistry` (shortcuts preserved and shown in
          tooltips); the contextual last tab switches Scene ↔ Text based on the active document type
        - `Ribbon::setTabHighlighted` renders the File tab title in the theme accent colour; tab bar padding and a
          brighter `TabSelected` make the active tab clearly identifiable
        - Theme presets: `windowRounding` / `tabRounding` / `controlsRounding` reduced to 2–3 px for a crisper look
          across Dark / Light / DarkBlue / Nord / Solarized
- Build & CI
    - ![Done][done] Linux ARM64 CI restored
        - Poetry venvs were colliding across architectures because the default cache path
          (`~/.cache/pypoetry/virtualenvs/`) ignores the host arch when naming venvs — a shared
          `$HOME` mount between x86_64 and ARM64 agents caused ARM64 to load x86_64 wheels and crash at
          `cryptography/_rust.abi3.so` import
        - New `ci/utils/venv.py` runs every invocation with three layered checks: no venv → skip; platform-signature
          marker matches → skip (fast path, one file read); marker missing/mismatched → run a functional
          `from cryptography.fernet import Fernet` test under `poetry run python`. Only when that import fails does
          `ci_action.py` export
          `OWL_CI_REFRESH_VENV=1`, which `cmake/Poetry.cmake` consumes to run
          `poetry env remove --all` before the next `poetry sync` and re-stamp the marker
        - Check is not gated on TeamCity detection: TC Docker jobs don't propagate
          `TEAMCITY_VERSION` into the container, so an env-var gate would silently no-op — the layered approach keeps
          same-arch reruns nearly free while reliably self-healing on arch switches or corrupted venvs
    - ![Done][done] Windows Debug builds fixed
        - `owl_target_link_libraries` forced Release third-party imports via a helper save/restore of
          `CMAKE_MAP_IMPORTED_CONFIG_DEBUG` around `find_package` — but that variable is read at generate time, not find
          time, so the mapping was lost. Debug builds linked against
          `*d.lib` (binaries imported `*d.dll`) while the `TARGET_RUNTIME_DLLS` generator expression copied the Release
          variants → every test exited with `STATUS_DLL_NOT_FOUND` (0xc0000135)
        - Mapping now applied at top-level directory scope in `CMakeLists.txt`, gated by
          `OWL_USE_RELEASE_THIRD_PARTY`, so link and DLL copy agree in Debug

## v0.1.0 -- 2026-04-16

**Goal:** Users can design a complete game in Owl Nest and package it as a standalone distributable application (Linux /
Windows).

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
        - `Canvas` component marks UI root, `UiRect` for screen-space layout (anchor, pivot, size, offset)
        - Screen-space orthographic rendering via existing Renderer2D pipeline
        - Sort order for layering multiple canvases
    - ![Done][done] Base widgets
        - UiText (font, size, colour, alignment), UiImage (sprite, tint)
        - UiButton (normal/hover/pressed/disabled states, Lua callback)
        - UiPanel (background, border, vertical/horizontal layout)
        - UiSlider (draggable, min/max, callback), UiProgressBar (value, colours)
    - ![Done][done] UI input handling
        - UiInputSystem: hit-test, hover/pressed state tracking
        - UI consumes mouse events before scene, Lua callback on button click
        - Lua `ui` table: set_text, set_visible, set_progress, slider get/set, button enable/disable
    - ![Done][done] UI editor in Owl Nest
        - Canvas and UiRect visible in editor viewport with cyan outlines
        - All widget properties editable in inspector
        - Component icons for Canvas, UiRect, UiText, UiImage, UiPanel, UiButton, UiSlider, UiProgressBar
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
        - 6 scenes: main menu (logo, save management), gameplay, level 2, settings (with reset), victory, game over —
          all with fade transitions
        - Full Lua API coverage: all 13 API tables exercised (transform, physics, input, scene, entity, ui, gamestate,
          save, settings, sound, log, trigger, time)
        - All 8 UI widget types, all 7 trigger types, animated sprites, sounds + music with pause/resume/volume, mouse
          input, runtime entity creation, entity inspection

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
        - Runtime rasterization via lunasvg with dynamic theme colour substitution
        - White → theme text colour, fuchsia → theme accent colour
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

First basic release: minimal viable engine with the ability to run simple games defined in scenes.

## Badge Legend

- ![Done][done] Completed features
- ![In Progress][progress] Features currently being implemented
- ![Planned][planned] Features that are planned but not yet being worked on

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square

[progress]: https://img.shields.io/badge/-In_Progress-d29922?style=flat-square

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square
