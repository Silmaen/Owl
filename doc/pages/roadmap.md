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
          nodes, …), at least *select-many* + *move-group* / *delete-group*; resizing a parent must preserve / shift
          contents instead of clipping
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
        - Savable `.owlbt` behaviour tree assets
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
        - Directional light (sun), point lights, spotlights
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
    - ![Planned][planned] Compute-driven frustum / occlusion culling pre-pass feeding indirect draws. The
      `renderer::utils::FrustumCullingPass` utility + `frustum_culling.slang` shader ship, alongside the
      `RenderCommand::drawIndexedIndirect` API (Vulkan `vkCmdDrawIndexedIndirectCount`, OpenGL
      `glMultiDrawElementsIndirectCount`, Null no-op). `extractFrustumPlanes(viewProj)` Gribb-Hartmann helper for the
      CPU side. Headless tests on the Null backend pass. With the SSBO-indexed instanced pipeline landed in v0.2.0,
      adoption here is a drop-in: populate the AABB SSBO from the scene graph and replace the per-entity draw loop with
      one `drawIndexedIndirect`. Pays off when 3D meshes start filling scenes.
    - ![Planned][planned] GPU raycast sprite stripes + `BitonicSortPass` adoption (#32, deferred from v0.2.0). The
      `BitonicSortPass` utility + `bitonic_sort.slang` shipped and are headless-tested in v0.2.0, but adoption was held
      back because `RendererRaycast::drawSprites` still emits per-column via `Renderer2D::drawQuad` (only the wall
      stripes moved to `raycast_stripe.slang` in v0.2.0 Phase 3). The remaining work: move sprite stripe emission to a
      GPU instanced shader fed by a GPU-side `zBuffer[]` occlusion read (drop the per-frame readback the wall path
      currently does), then dispatch `BitonicSortPass` on the sprite depths so the back-to-front order stays on the GPU
      and feeds the stripe shader directly — zero CPU readback. Same applies to the door / pushwall stripe consumers.
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
          via `lua_pushglobaltable` + `lua_next`) — needs the watch UI design pinned down before implementation
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
          `TilemapDocument` reload automatically (the tileset is shared via the per-scene cache, so a single
          invalidation propagates).
        - Undo/redo: every insert / replace / remove pushes a single
          `ModifyTilesetCommand` (mirrors `ModifyEntityCommand` semantics)
          with the standard 1 s merge-coalescing for rapid drag chains.
- In-Editor Documentation
    - ![Planned][planned] Mermaid diagram rendering in the help panel
        - Today the md4c-based renderer treats ` ```mermaid ` fences as plain code blocks; the actual diagrams (used by
          `architecture.md`, `editor.md`, `node_graph.md`, `physics.md`,
          `renderer.md`, `scene.md`, `scripting.md`, `sound.md`) only render on GitHub / Doxygen
        - Build-time pre-render of mermaid blocks → SVG (or PNG) files in
          `engine_assets/help/images/mermaid/`, with the Markdown rewriter swapping each fence for an
          `![alt](images/mermaid/<sha>.svg)` reference. No runtime JS/Node dependency — pre-rendering can run with a
          depmanager-shipped tool or a custom subset renderer in C++
        - Update `cmake/HelpAssets.cmake` to invoke the pre-renderer and surface the cache files
        - Tests: assert each bundled `engine_assets/help/*.md` no longer contains ` ```mermaid `
          after the bundle step and that the rasterized diagram files exist
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
          editor load reads binary. Not urgent (tracker O (N²) fix already brought parse to ms scale) but still worth ~
          10× on really large scenes.
    - ![Planned][planned] Parallel pack `readEntry`
        - Wrap `PackReader::readEntry` so multiple entries can be zstd-decompressed concurrently (file seek serialized
          under a mutex, decompression off-mutex). Useful for packed games loading dozens of textures in parallel at
          startup. Needs PackReader thread-safety audit + a worker-friendly API.

## v0.2.3 -- Expected 2027-01-01

**Goal:** Round out the 2D experience with dynamic lighting, ship the long-awaited custom file picker, and add core
gameplay primitives (inventory, enemies).

- 2D Lighting
    - ![Planned][planned] 2D lighting system
        - Point lights, spotlights in 2D scenes
        - Normal-mapped sprites for dynamic 2D lighting
        - Shadow casting from 2D occluders
- Editor Infrastructure
    - ![Planned][planned] Custom ImGui-based file picker
        - Replace the native file dialogue (NFD/GTK) which briefly freezes the UI on Linux when GTK initializes
          (triggers IDE "antiloop" detection)
        - Pure ImGui implementation integrated with the task scheduler for async folder scanning
        - Benefits: consistent look-and-feel, truly non-blocking, theme-aware
        - Replaces the current sync `FileDialog::openFile/saveFile/pickFolder` blocking calls
    - ![Planned][planned] Editor camera controls overhaul
        - Current `CameraEditor` is awkward to manipulate (orbit feels off-axis, pan/zoom thresholds are inconsistent,
          RMB-drag direction sometimes fights the user). Re-tune sensitivity per axis, add dead zones, support
          Maya/Blender-style middle-click navigation as an option, surface the settings under
          `Settings > Editor > Camera`.
        - **Standard navigation presets** — quick buttons (ribbon `View` group + viewport overlay) for: **Reset View**
          (snap back to the default editor pose); axis-aligned ortho views **XY** (top-down), **XZ** (front), **YZ**
          (side); **Frame Selection** (zoom to fit the selected entity); **Frame Scene** (zoom to fit the whole scene's
          bounds).
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

**Goal:** Add a third non-2D rendering mode — an isometric pseudo-3D renderer in the **Transport Tycoon Deluxe**
tradition — slotted between the existing 2D/raycast/voxel options.

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
          batching path; no new GPU pipeline required for this cut.
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

**Goal:** Add the second non-2D rendering mode — a voxel engine for block-based worlds (Minecraft-style).

- Voxel Engine
    - ![In Progress][progress] Voxel world core
        - Chunk-based world (cubic 16³ chunks, sparse `VoxelWorld` map) — ![Done][done] data model
        - Block type registry with textures per face — ![Done][done] `BlockRegistry`
        - Chunk loading/unloading around camera — ![Planned][planned] (streaming, lands with terrain generation)
    - ![In Progress][progress] Chunk meshing
        - Greedy meshing or similar algorithm for efficient geometry — ![Done][done] `ChunkMesher` (greedy, per-axis)
        - Only exposed faces rendered (hidden face culling) — ![Done][done] visible-face-only, cross-chunk via provider
        - Frustum culling per chunk — ![Planned][planned] (renderer-side, lands with `RendererVoxel`)
    - ![Planned][planned] Terrain generation
        - Procedural terrain via noise functions (Perlin/Simplex)
        - Configurable biomes, terrain height, cave generation
        - Seed-based reproducible worlds
    - ![Planned][planned] Block interaction
        - Block placement and destruction
        - Block picking (raycast from camera to find targeted block)
        - Block metadata (orientation, state)
    - ![Done][done] Voxel rendering (Vulkan)
        - Generic `Renderer3D` forward foundation (depth, perspective, textured, directional light, `mesh3d` shader)
          — ![Done][done] (reusable base)
        - `RendererVoxel` stack layer + `VoxelWorld` scene component (per-entity chunk mesh cache, Play-mode dispatch)
          — ![Done][done]
        - **Depth-aware renderer** (symmetric `Depth24Stencil8` attachment on the editor viewport *and* swapchain
          framebuffers + depth clear + per-batch dynamic depth test/write; 2D depth-off, 3D depth-on) — ![Done][done]
          (fixed the batch-fence deadlock and the `renderPass-02684` incompatibility flood along the way)
        - **Tileset-atlas texturing**: `VoxelWorld` references a `.owltileset`; per-face indices are atlas tile indices
          mapped to UV sub-rects, with shader-side `frac()` tiling so greedy-merged quads tile rather than stretch
          — ![Done][done]
        - **Editor viewport rendering**: voxel worlds render and are navigable (orbit/pan/zoom) while editing, not only
          in Play — ![Done][done]
        - **Play-mode camera**: `voxel_fly_camera.lua` (WASD / Space-Shift / arrows) so the world is explorable in Play
          — ![Done][done] (movement feel still rough — see follow-ups)
        - Basic directional lighting — ![Done][done] (in `mesh3d` / `voxel`)
        - Ambient occlusion per vertex for block edges — ![Planned][planned]
        - Water/transparent block rendering with proper sorting — ![Planned][planned]
    - ![Planned][planned] Voxel rendering follow-ups (deferred from the voxel PR)
        - Repair the **OpenGL** backend for the depth/voxel path (the depth attachment + 3D draw path was validated on
          Vulkan only; OpenGL regressed and needs fixing)
        - **Richer block textures** for the demo world (distinct grass / earth / rock / … tiles) instead of the
          placeholder platform atlas
        - **Improve camera movement** — likely a reusable C++ 3D camera controller / movable camera object shared by the
          editor viewport and Play, replacing the ad-hoc Lua fly script
        - **Investigate the remaining Vulkan validation messages**: the single-shared-descriptor-set
          `UPDATE_AFTER_BIND` cascade (needs per-batch/ring descriptor sets, not the `UPDATE_AFTER_BIND` flag) and the
          5-object `vkDestroyDevice` teardown leak (`whiteTexture` + a descriptor-set layout)
    - ![Planned][planned] Voxel editor in Owl Nest
        - Brush tools for painting blocks
        - Prefab structures (trees, buildings) as reusable block templates
        - Chunk inspector for debugging

## v0.2.0 -- 2026-06-02

**Goal:** Introduce a composable **renderer stack** so scenes can mix and match rendering modes (e.g. raycasting world +
2D HUD), deliver the first non-2D mode (raycasting), the tilemap system, scene-to-scene transition effects, and
**modernize the renderer foundation** — drop the legacy CPU-vertex-transform batching pattern in favour of a GPU-driven
path (per-frame SSBOs + instanced rendering + compute pre-passes), so the engine is ready for the 3D pipeline landing in
v0.3.0.

- Renderer Stack Architecture
    - ![Done][done] Foundation — `RenderLayer` / `RenderStack` / `RenderLayerFactory` + `RendererTag` component, YAML
      round-trip (`RendererStack:` / per-scene `EnabledRenderers:` with overrides), backward-compatible defaults.
    - ![Done][done] Runtime dispatch — `Scene::renderWithStack` runs per-layer passes; `Scene::layerAccepts` routes
      entities by `RendererTag`.
    - ![Done][done] Editor UI — Project Settings stack composition, Scene Hierarchy `RendererTag` dropdown, dockable
      per-scene `Scene Settings` panel (enable / reorder / detach / typed overrides via `ModifyEnabledRenderersCommand`).
    - ![Done][done] Tilemap system — `scene::Tileset` (`.owltileset`) + `.owltilemap` `TilemapAsset` (multi-layer, YAML
      round-trip), `TilemapDocument` editor (Properties / Canvas / Palette, paint-erase, layers, per-stroke undo),
      static Box2D body, viewport paint mode.
- Raycasting Renderer
    - ![Done][done] Core — `RendererRaycast` + `RendererRaycastLayer`, per-column DDA, configurable FOV / max distance /
      sky / floor. `Tileset.FilterMode` keeps wall atlases pixel-crisp. Sample `scenes/raycast_demo.owl` (Wolfenstein 3D
      E1L1, 64×64, original art).
    - ![Done][done] Floors & ceilings — per-scanline textured backdrop (`emitTexturedBackdrop`); solid-colour fallback.
    - ![Done][done] Sprites (billboards) — `SpriteRenderer` / `AnimatedSpriteRenderer` as camera-facing strips, z-buffer
      occluded; the same components stay 2D on `Renderer2D` layers (author top-down, view first-person).
      `Transform.translation.z` = world Z-offset, `scale.xy` = world size in cells.
    - ![Done][done] Map features — variable wall heights (`TileMeta.wallHeight`, `[0, 8]`); transparent walls (alpha
      only, up to 8 back-to-front hits/ray); doors & pushwalls (`RaycastDoor` / `RaycastPushWall` components,
      built-in `interactionKey` or Lua activation, auto-managed Box2D, editor tile pickers). Thin walls dropped.
    - ![Done][done] Map editor in Owl Nest — `TilemapDocument` grid; top-down viewport in edit mode, first-person in
      Play; per-camera viewport marker (dot + arrow + FOV cone), ribbon `Show` toggle.
    - ![Done][done] Distance fog — `RaycastConfig.fogColor` / `fogStart` / `fogEnd`, uniform across walls / doors /
      sprites / backdrop. Point lights deferred.
- Gameplay
    - ![Done][done] Scene transition effects — `ScreenTransition::Type` covers `Fade{In,Out}` +
      `Wipe{Left,Right,Up,Down}`;
      `play(type, duration, colour)` accepts a custom tint. Lua: `ui.transition_play(type_string, duration, [rgba])`
      plus back-compat `ui.transition_fade_in/out`.
- Performance
    - ![Done][done] Scene loading ~550× faster — memory-tracker O (N²) fix (release disables the global `new`/`delete`
      overrides; debug uses an O (1) index), UUID→`entt::entity` cache, in-memory SPIR-V cache. `raycast_demo` opening
      dropped ~22 s → ~40 ms. Plus async load (YAML on a Taskflow worker, entity creation on the main thread).
    - ![Done][done] In-game hot paths — skip hidden entities in script / physics / `EntityLink` loops, frame-pooled
      scratch buffers, cached primary-player + UUID→entity lookups, pre-resolved `EntityLink`, per-pass
      `getWorldTransform` / visibility caches, `resolveAllTilemapAssets` dirty-flag gate.
- Renderer Modernization — replace the legacy Hazel CPU-vertex batch (capped ~few-thousand quads, fragile Vulkan
  descriptor state) with a uniform GPU-driven instanced-SSBO pipeline (compute pre-pass → SSBO → instanced draw, zero
  CPU vertex transform), unblocking the v0.3.0 3D pipeline.
    - ![Done][done] GPU compute foundation — `gpu::ComputeShader` + `gpu::StorageBuffer` (+ `getData()` readback) +
      `storageBufferMemoryBarrier()` + Slang `[shader("compute")]` entry points (headless `SlangCompute_test.cpp`).
    - ![Done][done] Phase 0 — per-renderer Vulkan descriptor blocks (`gpu::RendererDescriptors`, no-op on Null /
      OpenGL): fixes the NVIDIA `vkCmdBindPipeline` crash and the per-pass UBO race.
    - ![Done][done] Phase 1 — `Renderer2D` instanced rewrite (quad / circle / line / text → one drawcall per batch over
      a shared unit quad + per-instance SSBO). Public API preserved.
    - ![Done][done] Phase 2 — `WorldTransformPass` (#33): world matrices computed in a compute pre-pass into
      `sceneWorlds[]`; instances carry an `int32_t worldIndex` instead of a `mat4` (128→80 / 96→48 bytes each).
    - ![Done][done] Phase 3 — `RaycastDDAPass` (#35): per-column DDA on GPU (`raycast_dda.slang`), wall stripes in one
      instanced draw (`raycast_stripe.slang`). `zBuffer[]` read back for the CPU sprite / door occlusion path.
    - ![Done][done] Phase 4 — `BitonicSortPass` utility (#32) shipped + headless-tested. **Adoption deferred to v0.3.0**:
      raycast sprite stripes are still CPU-emitted, so a GPU sort would need a readback and regress — it lands together
      with GPU sprite emission, alongside the frustum-culling adoption.
    - ![Done][done] Phase 5 — clean-up & docs:
        - Tilemap rendering re-routed to the instanced `RendererTilemap`: deferred into `Renderer2D::flush` (after
          background, before sprites) and combined into **one drawcall** for the whole scene (per-instance `layerZ` /
          atlas / `textureSlot`, distinct tilesets across `gTextures[32]`, cap 32). Validated on GPU (Vulkan + OpenGL):
          single-layer regression, multi-layer stacking, multi-entity / multi-tileset combine.
        - Dropped the now-dead `getWorldTransform` calls in the 2D quad / circle / sprite loops (covered by
          `worldIndex`); the cache stays for the raycast DDA / physics / `EntityLink` / text / inspector callers.
        - Renderer test coverage up (`RendererTilemap_test`, `BitonicSortPass` padding / empty cases). The headless
          suite is Null-backend, so `StorageBuffer` readback is an API-contract oracle, not a compute-output verifier.
        - `renderer.md` + `architecture.md` document the instanced pipeline as canonical; perf-bench methodology shipped
          (real-GPU numbers need a manual run).
- Known bug fixes
    - ![Done][done] Editor keyboard shortcuts — modifier-based shortcuts (Ctrl+S, Ctrl+Z, …) bypass
      `ImGui::GetIO().WantCaptureKeyboard`; modifier-less still yield to focused text widgets.
    - ![Done][done] World-map top-down player drift — new `physics.set_gravity_scale(entity, scale)` Lua API;
      `world_player.lua` zeroes gravity in `on_create` instead of the per-frame cancellation hack.
    - ![Done][done] Hidden triggers no longer fire — `Scene::onUpdateRuntime` skips trigger entities whose
      `Visibility.gameVisible` is false; in-progress timers are stopped and overlap state cleared with a synthetic
      `onTriggerExit`.

## v0.1.1 -- 2026-04-30

**Goal:** Transform the editor from a single-scene tool into a multi-document workspace with dedicated editors for
different asset types. All long-running operations become asynchronous with progress feedback.

- Async Operations & Progress
    - ![Done][done] Async task integration in Owl Nest — `AsyncProgressModal` + `AsyncProgressState`, layered on the
      Taskflow-backed `Scheduler` with main-thread termination callbacks.
    - ![Done][done] Async packaging — asset scanning, compression, pack writing all off-main-thread with progress bar
      (`PackWriter::ProgressCallback`) and cancel support.
    - ![Done][done] Async scene save / load — file I/O off main thread; deserialize / serialize on main thread in the
      termination callback. "Loading…" / "Saving…" overlays during the I/O phase.
    - ![Done][done] Deferred shader compilation with loading screen — `Renderer::init` split into `initContext()`
        + `initShaders(callback)` with per-shader progress overlay; cache hit skips compilation.
    - ![Done][done] Async texture loading with placeholders — `TextureDecoder` helper, `createFromSerializedAsync`
      returns immediately with a correctly-sized placeholder; worker decodes then uploads on the main thread.
      `createFromSerializedForDeserialize` keeps `SpriteRenderer` / `AnimatedSpriteRenderer` / `BackgroundTexture` /
      `UiImage` call-sites simple.
    - ![Done][done] Async scene transitions in runner — `RunnerLayer::handleTeleportRequest` reads bytes in background,
      swaps scene on the main thread; GameState + velocity preserved across the transition.
    - ![Done][done] Async content browser scanning — directory entries cached and scanned via the task scheduler; rescan
      triggered on create/import/rename/delete/drop/move.
- Multi-Document Architecture
    - ![Done][done] Document tab system — `Document` / `DocumentManager` / `SceneDocument` / `DocumentTabBar` with
      per-document undo stack, dirty marker, Play/Pause/Stop state. Tab bar rendered inside the Viewport header;
      `Ctrl+W` close, `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle. Background simulation in non-active Play tabs.
    - ![Done][done] Per-document viewport (side-by-side via docking) — each `SceneDocument` owns its viewport with its
      own framebuffer and stable `##scene_<uuid>` window id; ImGui docking groups them as tabs.
    - ![Done][done] Detachable panels — ImGui native docking (`DockingEnable` + `ViewportsEnable` in `UiLayer`).
- Script / Code Editor
    - ![Done][done] Generic code editor document — `CodeEditorDocument` powered by **imgui_color_text_edit**.
      Highlighting: Lua, C, C++, Python, JSON, Markdown (built-in) + YAML, SVG/XML (custom). JetBrains Mono for buffers;
      configurable UI / code font sizes in `EditorSettings`.
    - ![Done][done] Live preview for markup documents — `MarkdownPreview` (CommonMark + GFM via **md4c**) walks the
      parsed blocks to emit ImGui draw calls with cached `TextEditor` for code blocks; external links open via
      `platform::openExternalUrl`. `SvgPreview` rasterize live SVG via `lunasvg`. Vertical splitter between editor
      and preview, debounced ~250 ms.
- Node Graph Editor
    - ![Done][done] Node graph framework — reusable `gui::widgets::NodeCanvas` (UUID-based nodes/pins/links, typed pins,
      link validator, pan/zoom/selection) over `GraphEditor` from the ImGuizmo bundle. Templatized
      `UndoCommand<Target>` / `UndoManager<Target>` (Scene + NodeGraph aliases), `NodeCanvasSerializer` for
      `.owlflow` round-trip, `NodeGraphDocument` with ribbon "Graph" tab.
    - ![Done][done] Scene flow view — scenes as nodes, teleport triggers as output pins, orphan detection (BFS from
      `Project::firstScene`). Double-click navigates to a scene; ghost `+ Add teleport` pin creates a
      `Trigger` entity + canvas link in one undoable step; per-pin `targetName` modal. `SceneFlowCompositeCommand`
      glues scene + canvas undo halves. Canvas polish: Zoom-based text LOD + per-layer vertical centring.
- Asset Editors
    - ![Done][done] Animation editor — `.owlanim` asset (`AnimationClip`: texture, grid, frame range, duration, loop,
      optional speed curve). `AnimationDocument` three-pane (preview / properties / sequencer timeline backed by
      `gui::widgets::sequencer()` over `ImSequencer`). Ribbon contextual `Animation` tab; Content Browser entries
        + ribbon `New Animation`.
    - ![Done][done] Enhanced inspector
        - Sound preview button on `SoundSource` (uses current volume + pitch).
        - Texture thumbnails (100×100 with `(loading…)` / `(failed)` overlay during async decode) + font preview strip
          via `gui::FontPreviewCache`. Latin-1 / UTF-8 glyphs render correctly everywhere.
        - Drag-drop from Content Browser via `gui::widgets::assetDropTarget(AssetKind, path)`, with per-extension
          validation. `textureField()` consolidates the texture-aware widget shared by Sprite / AnimatedSprite /
          BackgroundTexture / UiImage.
        - Curve editor — new `math::Curve` (Constant / Linear / Smooth interpolation, flat-hold extrapolation, YAML
          round-trip omitting empty curves) + `gui::widgets::curveEditor()` over ImCurveEdit. First consumer:
          `AnimatedSpriteRenderer.speedCurve` remaps per-frame `dt`.
- Packaging
    - ![Done][done] Packaging wizard in Owl Nest — pre-packaging validation modal (`AssetScanner` warnings, Runner-exec
      check, empty-assets check), destination + options panel, post-pack report (asset count, size, duration).
- Menu & Project Workflow
    - ![Done][done] Ribbon-style main menu — `gui::widgets::Ribbon` (tabs → groups → large / small buttons) with File /
      Edit / Scene|Text tabs built from `ActionRegistry`; contextual last tab switches Scene ↔ Text per active document.
      Replaces the menu bar + floating Play/Pause + gizmo bar.
    - ![Done][done] Recent projects — persisted in `EditorSettings::recentProjects` (cap 10); "Recent" popup in File
      tab; Welcome modal when no project loaded.
    - ![Done][done] Save Project As — recursive copy via `std::filesystem::copy`, then switch the editor to the new
      directory.
- UX & Quality
    - ![Done][done] In-editor help pages — `cmake/HelpAssets.cmake` bundles `doc/pages/*.md` + `README` /
      `CHANGELOG` / `CONTRIBUTING` into `engine_assets/help/` with an `index.yml`. `panel::HelpPanel` renders via
      `MarkdownPreview` with search, categorized nav, back/forward history. F1 opens the page documenting the
      most-recently-hovered component header.
    - ![Done][done] Tooltips everywhere with hover delay — reusable `fieldTooltip()` helper (~0.4s `DelayNormal`)
      wired across all trigger types + every authored component field.
    - ![Done][done] Unique ImGui IDs audit — component-scoped `PushID(T::name())` in `drawComponent<T>`; index-based
      `PushID` in the LuaScript property loop.
    - ![Done][done] Icon clarity pass — per-extension Content Browser icons (sound / mesh / source / docs) sharing
      `base_file_ext_icon` template with a central glyph + amber/gold accent. `IconBank::iconButton(name, label,
      size)` helper reused across Welcome, Packaging Wizard, modals, Content Browser dialogues, panels.
- Build & CI
    - ![Done][done] Linux ARM64 CI restored — Poetry venvs colliding across architectures (shared `$HOME`, ARM64 loading
      x86_64 wheels → `cryptography/_rust.abi3.so` crash). New `ci/utils/venv.py` layered check (no venv / signature
      marker / functional import test) drives an inline refresh via
      `OWL_CI_REFRESH_VENV` + `cmake/Poetry.cmake`.
    - ![Done][done] Windows Debug builds fixed — `CMAKE_MAP_IMPORTED_CONFIG_DEBUG` mapping now applied at top-level
      directory scope (was being set inside `find_package` after generate time), so link and DLL copy agree in Debug.

## v0.1.0 -- 2026-04-16

**Goal:** Users can design a complete game in Owl Nest and package it as a standalone distributable application (Linux /
Windows).

- Scripting (Lua) — See [Lua Scripting](scripting.md)
    - ![Done][done] Lua 5.5 integration — `LuaEngine` (private), `ScriptEngine` / `ScriptInstance` (public).
    - ![Done][done] LuaScript component — `.lua` path + exposed properties; lifecycle callbacks `on_create`,
      `on_update`, `on_destroy`, `on_collision`.
    - ![Done][done] Engine API exposed to Lua — entity / transform / physics / input / sound / scene / time / log.
    - ![Done][done] Editor support — inspector property edit + Refresh, `.lua` icon, serialization round-trip.
    - ![Done][done] Lua scripts in Runner — load + execute from `.owlpack`, same lifecycle as editor.
- In-Game UI
    - ![Done][done] Canvas UI system — `Canvas` + `UiRect` (anchor / pivot / size / offset), screen-space ortho via
      `Renderer2D`, sort order for layering.
    - ![Done][done] Base widgets — `UiText` / `UiImage` / `UiButton` / `UiPanel` / `UiSlider` / `UiProgressBar`
      with full state + style coverage and Lua callback hooks.
    - ![Done][done] UI input handling — `UiInputSystem` (hit-test, hover/pressed tracking, mouse consumed before scene).
      Lua `ui` table: `set_text`, `set_visible`, `set_progress`, slider/button helpers.
    - ![Done][done] UI editor — Canvas/UiRect cyan outlines, full inspector coverage, per-widget icons.
    - ![Done][done] Standard game screens — Main Menu / Pause / Game Over templates in `engine_assets/templates/`
      with `ScreenTransition` + Lua `ui.transition_fade_in/out`.
- Game State & Save System
    - ![Done][done] Serializable `GameState` — key-value store (int/float/string/bool) on Scene; Lua
      `gamestate.set/get/remove/clear`; copied across transitions, serialized in saves.
    - ![Done][done] Save / Load — `SaveManager` to `~/.local/share/<game>/saves/` (Linux) or `%APPDATA%/…`
      (Windows); multi-slot, deferred-load pattern (safe mid-script).
    - ![Done][done] Full scene state save — scene + GameState + physics snapshots via
      `PhysicCommand::getSnapshot/applySnapshot`; `loaded_from_save` flag in GameState on load.
- Runner & Distribution
    - ![Done][done] Extended project configuration — `owl_project.yml` (version, author, description, icon, window size,
      fullscreen, resizable) + Project Settings panel.
    - ![Done][done] Improved Runner — title / icon / size from `runner.yml`, fullscreen + resizable Window API,
      `packGame()` exports project settings + icon.
    - ![Done][done] Persistent game settings — two-layer (`game_settings.yml` defaults + `settings.yml` overrides);
      built-in keys (resolution, fullscreen, resizable, volumes); Lua `settings.*`; auto-applied to Window + sound.
    - ![Done][done] Improved packaging pipeline — sanitized runner name, `launch.sh` (Linux) / `.zip` (Windows),
      `game_info.yml` metadata.
- Editor Improvements (Owl Nest)
    - ![Done][done] Prefab system — `.owlprefab` entity subtrees with UUID remapping; create-from-entity context action;
      Content Browser drag-drop; `PrefabLink` component with update/revert preserving overrides.
    - ![Done][done] Undo / Redo — reversible commands covering entity / component / hierarchy / gizmo; configurable
      depth, Ctrl+Z / Ctrl+Y, merge coalescing, dirty flag in title.
- Scene & Triggers
    - ![Done][done] More scene events and triggers — Timer (duration + repeat), Interaction (key + range), LuaCallback.
      Edge detection (`on_trigger_enter` / `on_trigger_exit`) on all types. Lua
      `trigger.start_timer` / `stop_timer` / `reset_timer`.
    - ![Done][done] Common properties across scenes — covered by `SettingsManager`'s `game_settings.yml`.
- Sample Game
    - ![Done][done] Complete game demonstrator — 6 scenes (main menu, gameplay, level 2, settings, victory, game over)
      with fade transitions; exercises all 13 Lua API tables, all 8 UI widget types, all 7 trigger types.

## v0.0.3 -- 2026-04-09

- Sound
    - ![Done][done] Sound effects + music — `SoundSource` (one-shot + looping), `SoundHelper` for gameplay-triggered
      sounds, category system (SFX / Music / Ambient).
    - ![Done][done] Sound management — play / stop / pause / resume / loop / volume / pitch via `SoundHandle`;
      `.wav` / `.ogg` / `.flac` / `.mp3`.
    - ![Done][done] Sound spatialization — 3D positional audio via OpenAL, `SoundListener` component, configurable max
      distance + rolloff; spatial positions synced from entity world transforms each frame.
- Graphics
    - ![Done][done] Animated sprites — `AnimatedSpriteRenderer` with spritesheet grid, configurable speed / loop / frame
      range; custom UVs in `Renderer2D`.
- Objects
    - ![Done][done] Mesh loading — OBJ, glTF, GLB, FBX (tinygltf, tinyobjloader, ufbx).
- Miscellaneous
    - ![Done][done] Configurable keymap.
- Scene
    - ![Done][done] Scene hierarchy (parent-child entities) — transform + visibility inheritance, reparenting preserves
      world position, delete orphans-to-grandparent or cascades, duplicate entity or subtree.
    - ![Done][done] Asset packing — `.owlpack` binary format (zstd compression + XOR-obfuscated TOC); pack-aware loading
      in runner.
    - ![Done][done] Task system — Taskflow-backed `Scheduler` + parallel utilities.
- Game Designer (Owl Nest)
    - ![Done][done] Project system — create / open / save / close (`owl_project.yml`); dynamic asset directory
      management; project name in title.
    - ![Done][done] Project settings panel — edit project name + first scene.
    - ![Done][done] Scene import into project.
    - ![Done][done] Export game for runner — standalone runner package.
    - ![Done][done] Scene hierarchy panel — tree with drag-drop reparenting; context menu (create / duplicate /
      unparent / delete / cascade); visibility toggles (editor + game).
    - ![Done][done] Icon system with runtime SVG rendering — SVGs rasterized via lunasvg with dynamic theme colour
      substitution (white → text, fuchsia → accent), packed in a mipmap atlas, rebuilt on theme change.
    - ![Done][done] View of level links.
    - ![Done][done] Separate editor / game display.

## v0.0.2 -- 2026-03-03

- Developers
    - ![Done][done] Public engine headers third-party independent; fmt public dependency removed; more static linking of
      third-party libs.
- Graphics
    - ![Done][done] Backgrounds / sky boxes.
    - ![Done][done] Migrate shaders to Slang.
- Miscellaneous
    - ![Done][done] Pause / unpause game; frame-by-frame stepping when paused.
    - ![Done][done] General settings management.
- Gameplay
    - ![Done][done] Jump between scenes.
- Game Designer (Owl Map)
    - ![Done][done] Global game settings, log frame, in-game + in-editor visibility.

## v0.0.1 -- 2025-02-06

First basic release: minimal viable engine with the ability to run simple games defined in scenes.

## Badge Legend

- ![Done][done] Completed features
- ![In Progress][progress] Features currently being implemented
- ![Planned][planned] Features that are planned but not yet being worked on

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square

[progress]: https://img.shields.io/badge/-In_Progress-d29922?style=flat-square

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square
