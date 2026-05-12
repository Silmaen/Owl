# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Doors and pushwalls — animated raycast walls.** Two new components and a
  dedicated renderer pass close out the Wolf3D-style feature set for v0.2.0.
    - `scene::component::RaycastDoor` — Wolf3D-style sliding door
      occupying a **1×1 cell**. Both the laterals (cube inside faces
      perpendicular to the opening direction) and the moving plate are
      rendered with **zero thickness** — they're just textures on the
      cube's inside faces. The plate's surface normal is perpendicular
      to the opening direction (a N-opening door has plate surfaces
      facing east and west), so the player approaches the door head-on
      along the axis perpendicular to the opening direction. The plate
      slides exactly one cell (plus 1 pixel scaled with the open
      progress for hermetic closure) into the pocket when open, at
      which point only the laterals remain visible.
      `OpeningDirection` is an enum (`North` / `South` / `East` /
      `West`). State machine: Idle → Opening → Open (`holdTime` s) →
      Closing → Idle. Open and close speeds are independent.
      `Transform.translation` stays put at the cell centre; only the
      internal plate offset animates. `faceTexture`'s U=1 always lands
      on the opening-direction side, regardless of which side the
      player views the door from.
    - `scene::component::RaycastPushWall` — **full-cell cube** that
      slides one shot (Idle → Moving → Final). A single tile covers
      every face; the entity's `Transform` translates with the wall.
    - **Texture sourcing via shared tileset.** Both components
      reference textures through `tilesetPath` (string, drag-drop in
      the inspector) + tile indices (`faceTileIndex` /
      `lateralTileIndex` for doors, `tileIndex` for pushwalls). A
      per-scene cache in `Scene::resolveAllTilemapAssets` deduplicates
      `.owltileset` loads — a door whose tileset path matches the
      world tilemap's tileset reuses the same `shared<Tileset>` (and
      its atlas texture), no double-load. Inspector ships a **visual
      tile picker**: click the thumbnail to open a grid popup of the
      tileset's tiles with the current selection highlighted.
      Pre-extracted Wolf3D tiles (24/25 = door faces, 98–105 = jambs)
      are dropped under `sample_project/textures/wolf3d_doors/` for
      reference but the API is tileset-driven.
    - **Hermetic closure.** The +1-pixel slide margin is scaled by the
      open progress (`offset * (1 + 1/64)`) so a closed door sits
      exactly at the cell centre — without the scaling the plate
      leaked a 1-pixel gap on the side opposite to the opening
      direction.
    - **Auto-kinematic body — closed doors are not traversable.**
      `PhysicCommand::init` auto-creates a kinematic Box2D body for
      any door / pushwall entity that doesn't already carry a
      `PhysicBody`. The body's collider matches the moving surface
      (thin plate for doors, full cube for pushwalls) and tracks the
      plate position each tick via `PhysicCommand::setTransform`. No
      manual physics wiring required.
    - **Built-in activation: proximity + key.** When the primary player
      is within `interactionRange` cells of the entity and
      `interactionKey` was pressed this tick, the state machine kicks
      out of Idle. Setting `interactionKey = 0` disables the engine
      path entirely; Lua drives activation through the `door` and
      `pushwall` tables (`activate / close / is_open / get_state` and
      `activate / has_moved / get_state` respectively).
    - **Renderer API.** Two new entry points:
      `RendererRaycast::drawDynamicWalls(span<RaycastDynamicWallData>)`
      (pushwall cubes) and
      `RendererRaycast::drawDoors(span<RaycastDoorData>)`. Per screen
      column, slab-method ray/AABB intersection writes the closest
      hit to the same per-column zBuffer the static-tilemap pass
      populated, so sprites drawn afterwards occlude correctly. The
      lateral is biased a hair closer to the camera so it always wins
      against the pocket-side wall sitting at the same depth.
      Stripes carry a `uvRect` (atlas sub-rectangle) so the renderer
      reads the right tile from the shared atlas.
    - **Editor 2D view.** Doors draw as a thin plate strip oriented
      along the slide axis (vertical for N/S, horizontal for E/W) so
      the designer reads the opening direction at a glance. Pushwalls
      get a **green outline** highlighting every block in the
      viewport. Selecting a door or pushwall adds a yellow
      destination line with a circle endpoint pointing at the
      open-pose position.
    - **Menu gating.** The hierarchy's "Add Component" popup hides
      `Raycast*` entries unless the entity's renderer layer (resolved
      via `RendererTag`) is a `RendererRaycast`. The same gating
      hides `Ui*` entries unless the layer is a `Renderer2D`.
    - **Tests:** 8 scene-level tests (state-machine progressions, YAML
      round-trip per component, full Scene serializer round-trip) + 5
      raycast renderer tests (`drawDynamicWalls` empty / in-front /
      occluded by static wall / culled past maxDistance / missing
      texture skipped) + 4 `drawDoors` tests (empty / closed shows
      laterals + plate / open shows only laterals / no texture
      skipped) + 4 Lua binding tests.
- **Variable wall heights and transparent walls in the raycast renderer.**
  Two new fields on `TileMeta` (per-tile, in the `.owltileset` asset) drive
  Wolf3D-style map effects:
    - `wallHeight: float = 1.0` — vertical scale of the wall in cell units.
      Walls are bottom-anchored at floor level so a tall wall pokes into the
      sky (towers, pillars) and a low one stays on the floor (half-walls).
      Range clamped to `[0, 8]`. Ignored by the legacy 2D rendering path.
    - `transparent: bool = false` — when set, the raycast DDA does not stop
      at this tile. It records the hit and keeps stepping (capped at 8
      transparent layers per ray) so further walls render behind the
      transparent one. The collected hits are drawn back-to-front per
      column so alpha-blended textures composite correctly. The closest
      *opaque* hit is what's latched into the per-column z-buffer used by
      the sprite occluder, so sprites remain visible through transparent
      walls (with a known v0.2.0 limitation: a sprite that sits behind a
      transparent wall still draws on top of it because sprites and walls
      aren't merged per column yet).
  YAML round-trip emits both fields sparsely (only when they differ from
  the default). Editor: the `TilesetDocument` properties panel gains a
  **Raycast** section with a `Wall height` drag and a `Transparent`
  toggle, with tooltips. Transparency is **alpha-channel only** — author
  tiles with a proper PNG alpha channel; the previously-captured
  `TileMeta::chromaKey*` fields are removed (legacy `chromaKey: [r,g,b]`
  YAML entries are silently dropped on load). Tests: 3 new headless
  raycast tests (multi-hit on transparent rows, budget cap, opaque-only
  behaviour matches pre-PR3) plus 3 new tileset round-trip tests
  (per-tile fields, clamp on deserialize, legacy chromaKey drop).
- **Per-sprite raycast size and Z-offset overrides.** New fields on
  `SpriteRenderer` and `AnimatedSpriteRenderer`:
    - `raycastSize: vec2 = {0, 0}` — when both components are `> 0`,
      this is the world size used as the billboard width × height in
      raycast view, decoupled from `Transform.scale.xy` (which keeps
      driving the 2D editor preview). Default `(0, 0)` keeps the PR2
      behaviour of inheriting from `Transform.scale`.
    - `raycastZOffset: float = 0` — added on top of
      `Transform.translation.z` when computing the on-screen vertical
      centre. Positive raises the sprite (lamps, ceiling decals),
      negative lowers it (floor stains). `0` (default) preserves the
      PR2 behaviour exactly so existing scenes stay byte-identical.
  Fields are emitted sparsely in YAML (only when they differ from the
  default) and surface in the inspector under a new collapsible
  `Raycast (billboard)` section on both components, with tooltips.
- **Sprites (billboards) in the raycast renderer.** Entities carrying
  `SpriteRenderer` or `AnimatedSpriteRenderer` are now rendered as
  camera-facing billboards when they sit on a `RendererRaycast` layer —
  the same components stay 2D-rendered everywhere else, no new component
  needed. The `Transform.translation` X/Y is the world position,
  `translation.z` shifts the sprite up/down on screen (lamps, ceiling
  decals, floor decals), and `scale.xy` is the world width × height
  (scale `{1,1}` matches a 1-cell wall at the same depth). Architecture:
    - `RendererRaycast` gains a per-column z-buffer latched by
      `drawTilemapWalls` (one entry per cast ray), so sprites are
      occluded column-by-column by walls — no GPU depth test needed.
    - New `RendererRaycast::drawSprites(span<RaycastSpriteData>)` batch:
      camera-space projection, cull behind / beyond `maxDistance`,
      back-to-front sort, then per-column emission of 1-pixel-wide
      strips against the wall z-buffer. Sprite-on-sprite occlusion
      falls out of the painter's order naturally.
    - `Scene::render` collects sprite + animated-sprite components and
      dispatches them through the new batch when the active layer is
      raycast; `Circle` / `Text` components are silently dropped on
      raycast layers (HUDs belong on a separate `Renderer2D` layer).
    - `BackgroundTexture` is skipped on raycast layers — the layer's
      own sky / floor backdrop covers the same frame.
    - `RaycastConfig`-aware: sprites past `maxDistance` are culled.
  Stats expose `spriteCount` (visible sprites), `spriteStripeCount`
  (stripes emitted) and `spriteOccludedCount` (stripes rejected by
  walls). Demo: `raycast_demo.owl` ships a barrel sprite (static) and
  an animated coin (using the existing `animated_coin.png` spritesheet)
  placed in front of the player spawn — both authored with regular
  `SpriteRenderer` / `AnimatedSpriteRenderer` components, just tagged
  `raycast_world` via `RendererTag`.
- **In-viewport camera markers.** Every `component::Camera` entity now draws
  a small camera icon, a forward arrow and a FOV cone in the editor viewport
  (Edit mode only). The marker uses the entity's world transform — local +Y
  is the camera forward, matching `RendererRaycast::beginScene`'s convention
  — so a raycast scene's player camera, previously invisible in the
  top-down preview, now shows where it sits and which way it faces. The
  primary camera is highlighted in gold (slightly larger), secondary
  cameras in cyan. The cone uses the perspective FOV when available and a
  fixed 75° hint for orthographic cameras (the actual raycast FOV lives on
  the layer config, not on the camera entity). Selectable via the existing
  picking pipeline (the icon writes the entity id into the framebuffer
  attachment), manipulable through the standard Translate / Rotate gizmo.
  Toggleable via:
    - **Ribbon `Scene > Show > Cameras`** — small toggle button alongside
      the existing Gizmo group.
    - **Floating viewport overlay** — a compact toolbar in the top-left of
      every viewport, sharing the same toggle plus icon-only Translate /
      Rotate / Scale gizmo selectors so the user doesn't have to leave the
      viewport to switch tools. Persisted in `EditorSettings.showCameraGizmos`
      (default `true`).
- **Tilemap & Tileset editor UX iteration #2.**
    - **Open-in-editor buttons.** TilemapDocument hierarchy panel now hosts
      an "Open in Tileset Editor" icon-button next to the tileset slot.
      SceneHierarchy entity context menu gains "Open in Tilemap Editor"
      for any entity carrying a `Tilemap` component with a non-empty path.
      Resolves the relative asset path against the project's asset
      directories before delegating to `EditorLayer::openTilemapFile` /
      `openTilesetFile`.
    - **Cross-document live refresh.** When a `TilemapDocument` saves to
      disk, every open `SceneDocument` whose `Tilemap` components reference
      the same path drops its cached `asset` so the next frame's
      `Scene::resolveAllTilemapAssets` reloads from disk. When a
      `TilesetDocument` saves, every open `TilemapDocument` and
      `SceneDocument` referencing that tileset path drops the cached
      `tileset` and reloads. Path comparison is canonicalised so symlinks
      and `./..` segments don't cause false negatives.
    - **Per-document ribbon tabs.** New contextual `Tilemap` and `Tileset`
      ribbon tabs replace the stale Scene tab when a document of that
      type is active — Save / Save As / Close in the File group, Undo /
      Redo in the Edit group. Wiring matches the existing
      `buildAnimationTab` / `buildCodeTab` pattern.
    - **Tilemap canvas: cell selection.** Clicking any cell on the canvas
      (including empty cells) selects it; the persistent highlight remains
      until another cell is picked. The Properties panel now shows the
      cell's coordinates, current tile type, and `Passable` flag derived
      from the tileset's `collidable` metadata. Tile-type metadata edits
      are deferred to the dedicated tileset editor (open via the new
      button next to the tileset slot).
    - **Tile palette: full-tile wrapping.** Atlas grid wraps using the
      true button stride (`button width + frame padding + item spacing`)
      so the right column never displays a clipped fragment. Help texts
      under the brush summary are now smaller (0.85x font scale) and
      word-wrapped; tooltips on the layer-manager icon buttons run through
      a wrapped `BeginTooltip` for the same look.
    - **Layer manager polish.** Each layer row now uses a radio button to
      pick the active painting layer, an inline `InputText` for renaming,
      and icon-only buttons for show/hide / move-up / move-down / delete.
      The reserved width for the buttons is computed from the icon stride
      so the last button no longer falls off the right edge of the panel
      regardless of width.
    - **Tileset editor fixes.** Atlas preview UVs flipped (`{0,1}, {1,0}`)
      to match `widgets::textureField`'s convention so the texture is no
      longer upside-down on Vulkan. Texture drop slot now properly pushes
      a `ModifyTilesetCommand` into the undo stack on drop. Grid edits
      replaced by non-destructive `+/-` buttons backed by new
      `Tileset::addRow` / `removeRow` / `addColumn` / `removeColumn` /
      `swapTiles` methods (existing per-tile metadata is preserved across
      grow / shrink).
    - **Hierarchy panel cleanup.** Removed the redundant `file: …` line
      in both Tilemap and Tileset hierarchy overrides — the file name is
      already shown on the document tab.
- **Tilemap & Tileset editor UX iteration.**
    - `TilemapDocument` now overrides the global Scene Hierarchy & Properties
      panels (`overridesGlobalPanels()`): the hierarchy panel hosts the
      tilemap's general properties (tileset slot, grid size, layer manager,
      save / undo / redo) and the properties panel hosts the per-tile
      metadata editor (collidable flag, designer name) for the inspected
      tile. The document tab itself is dedicated to the canvas — no more
      cramped left side panel.
    - **Two-brush tile palette.** `TilePalette` carries an independent
      primary brush (left mouse on the canvas) and secondary brush (right
      mouse), each one of `pick` (no selection — clicking on the canvas
      samples the cell under the cursor and assigns it to that brush),
      `eraser` (writes empty), or a tile index. Click on a palette tile
      with the left button to set the primary, right button for the
      secondary; re-clicking the same selection toggles back to pick mode.
      Primary highlight is the editor accent (orange), secondary is blue,
      both = magenta when the same tile is in both brushes.
    - **Eraser icon** (`icons/actions/eraser.svg`) and **layer reorder
      icons** (`move_up.svg`, `move_down.svg`); layer manager rows now
      use icon buttons for visibility / move / delete and an inline
      `InputText` for renaming. The active painting layer is selected
      via a radio button on each row.
    - **`TilesetDocument` (6th `DocumentType`)** — dedicated `.owltileset`
      editor. Texture drop slot + grid configuration (columns, rows, tile
      pixel size) + filter mode combo (Linear / Nearest) in the hierarchy
      panel; per-tile collidable + name editor in the properties panel;
      atlas preview canvas (zoomable / pannable) in the document tab with
      click-to-select per slot, hover preview, collidable cells tinted
      red. Save / undo / redo via `TilesetUndoManager` +
      `ModifyTilesetCommand`. New "New Tileset" small button in the File
      ribbon, double-click `.owltileset` from the Content Browser opens
      the document.
- **Tilemap is now a standalone `.owltilemap` asset, edited in a dedicated document.**
    - `scene::TilemapAsset` — standalone YAML asset (mirrors `scene::Tileset`)
      holding `width × height × cellSize` + `tilesetPath` + ordered list of
      `TilemapLayer`s. Round-trip API (`serializeToString` /
      `deserializeFromString` / `saveToFile` / `loadFromFile`), `addLayer`,
      `resize`, `getTile`, `setTile`. 13 unit tests in
      `test/scene_tests/TilemapAsset_test.cpp` cover round-trips, malformed
      YAML, short / oversized tile buffers, file I/O.
    - `scene::component::Tilemap` is now a *reference*: it carries
      `tilemapPath` + a runtime `asset` shared pointer. Render paths
      (2D in `Scene::render`, raycast in `RendererRaycast::drawTilemapWalls`)
      and physics (`PhysicCommand`) all read from the resolved asset.
      `Scene::resolveAllTilemapAssets` performs a two-phase load (asset →
      tileset). The deserializer keeps a defensive reader for the legacy
      inline form so existing scenes still parse.
    - `nest::TilemapDocument` (5th `DocumentType`) opens `.owltilemap`
      files in a dedicated tab with three sub-panels: properties (tileset
      slot, grid size, cell size, layer manager — add / delete / reorder /
      visibility / parallax), zoom-and-pan canvas (left-click paints,
      right-click erases, scroll zooms, middle-drag pans), and the existing
      `TilePalette`. Each stroke pushes one `ModifyTilemapAssetCommand`
      (full-asset YAML before/after) on the document's
      `TilemapUndoManager`. Save / dirty marker integrated.
    - The Tilemap component inspector becomes a read-only summary plus a
      drop slot for `.owltilemap`; in-scene paint mode and grid resize
      removed (the `Viewport::processTilemapPaint` path is gone).
    - `gui::widgets::AssetKind::Tilemap` added; `assetDropTarget` filters
      `.owltilemap` payloads. Content Browser gets a dedicated icon and
      double-click handler. File ribbon gains a small "New Tilemap"
      button next to "New Animation".
    - Sample project: `world_map.owl`, `platformer_house.owl` and
      `raycast_demo.owl` migrated to path-based form via the one-shot
      `tools/migrate_inline_tilemaps.py` script; their inline data now
      lives in `sample_project/tilemaps/*.owltilemap`.
    - `TilePalette` is fully decoupled from the scene hierarchy — it now
      takes a `TilemapAsset*` directly and is hosted by `TilemapDocument`
      instead of the editor layer.
- **Snap-to-grid for translation gizmos** — new `Snap` toggle plus a `Step`
  preset dropdown in the Scene ribbon's Gizmo group, and an
  `Editor Settings > General > Gizmo Snap` section. Four new fields on
  `EditorSettings`: `snapEnabled`, `snapStep` (world units), `snapMultiplier`
  (× cell size), `snapAutoFromTilemap`. When auto-from-tilemap is on (default)
  and the active scene contains a `Tilemap` component, the snap step is
  `cellSize × multiplier`, and on drag-end the entity is finalised to the
  nearest cell-center grid point (with a half-cell offset for even-sized
  tilemaps) so entities visually align with cell centers; otherwise the manual
  step is used. The `Step` preset dropdown contextually shows fractions /
  multiples of the cell size (1/4, 1/2, 1, 2, 5, 10) when a tilemap exists, or
  raw world-unit values (0.25, 0.5, 1, 5, 10) otherwise. Holding `Ctrl` during
  a drag still forces snap regardless of the toggle (legacy behaviour
  preserved). Rotation snap stays at 45° and scale snap at 0.5.
- **Ribbon dropdown buttons** — `gui::widgets::Ribbon::Button` gained an
  optional `popupContents` callback. When set, clicking the button opens a
  popup whose body is filled by the callback (instead of firing `onClick`),
  and a small downward caret indicator is rendered to hint at the dropdown.
  Used for the new Snap step preset picker; reusable for any future ribbon
  drop-list (e.g. recent files, theme picker).
- **ccache compiler launcher** — new `cmake/CompilerCache.cmake` module
  detects `ccache` (or `sccache`) on the host and wires it as
  `CMAKE_C(XX)_COMPILER_LAUNCHER`. Controlled by the `OWL_USE_CCACHE`
  option (ON by default); silently no-ops when no binary is found so
  dev machines without ccache keep working unchanged. The CI Docker
  containers receive `CCACHE_DIR=/tmp/cache_dir/ccache` (persisted via
  the existing `cache_dir` agent volume mount), `CCACHE_COMPRESS=1`,
  `CCACHE_MAXSIZE=20G`, `CCACHE_BASEDIR=/home/user`. First build
  populates the cache; subsequent rebuilds short-circuit unchanged
  translation units even when the build directory is wiped.

### Changed

- **`file(GLOB ...)` calls** in `source/owl`, `source/owlnest`, `test`,
  and `cmake/HelpAssets.cmake` now pass `CONFIGURE_DEPENDS` so adding
  or removing source / header / help-page files triggers a CMake
  re-configure on the next build instead of silently going unnoticed
  until the next manual configure.

- **`CodeStyle` CI action** — single read-only command
  (`poetry run python ci_action.py CodeStyle <preset>`) bundling six
  inspections that no other tool covers in one place:
    - **clang-format** dry-run on every C++ source.
    - **typos** via `codespell` (with project-local
      `ci/codespell-ignore-words.txt` for legitimate identifiers like
      `nam:` / `siz:` texture prefixes).
    - **comment-quality** — `///` discipline (multi-line `///` blocks must
      use `/** */`); function declarations must use `/** */` (never a single
      `///` line above); non-`void` functions documented with `/** */` must
      carry a `@return` tag; Doxygen description paragraphs must end with
      `.`, `?` or `!`. TU-local helpers (`static` free functions in `.cpp`,
      anonymous namespaces) are skipped.
    - **private-member-docs** — every `m_*` / `mp_*` / `s_*` / `g_*` field
      needs a `///` line above or `///<` inline.
    - **cpp-style** — banned `std::shared_ptr` / `std::make_shared` /
      `std::unique_ptr` / `std::make_unique` / `std::weak_ptr` (use the
      project aliases `shared` / `mkShared` / `uniq` / `mkUniq` / `weak`),
      banned class suffixes `*Service` / `*Helper` / `*Util`, `UI*` vs
      `Ui*`, `enum class` vs `enum struct`, blank-line rules around
      `OWL_PROFILE_FUNCTION()` / `OWL_DIAG_PUSH/POP`, log-message
      formatting (`Subsystem: capitalized message ending with .`).
    - **structural** — file headers (`@file` + `Copyright (c) YYYY`),
      `OWL_API` warning on free functions in `source/owl/{public,private}`.

  Doxygen is **not** invoked here (run via the existing `Documentation`
  action). The CodeStyle action only inspects sources — it never rewrites
  them. Each sub-check can be disabled with `-- --no-format=true` /
  `--no-typos=true` / `--no-comment-quality=true` / `--no-doc-audit=true`
  / `--no-cpp-style=true` / `--no-structural=true`.
- **Per-scene settings panel in Owl Nest** — dockable
  `panel::SceneSettings` window (Edit > Settings > Scene) editing the active
  scene's `EnabledRenderers` block: per-layer enable toggle, up / down
  ordering, `Detach` to revert to project default, and an `Overrides`
  collapsible with typed widgets per layer type (`Space` combo for
  `Renderer2D`; `Fov` / `MaxDistance` / `NumRays` drags + `CeilingColor` /
  `FloorColor` pickers for `RendererRaycast`). Edits route through the new
  `commands::ModifyEnabledRenderersCommand` (undoable, with the standard
  1 s merge-coalescing window for rapid drags) and trigger
  `EditorLayer::syncActiveDocumentPanels` so the live `RenderStack` rebuilds
  immediately. Removes the only remaining "you have to hand-edit YAML"
  authoring step in the renderer-stack feature.
- **Scene transition effects (Lua-triggerable)** — extended
  `scene::ScreenTransition` from `Fade*` only to `Fade{In,Out}` +
  `Wipe{Left,Right,Up,Down}` with a configurable tint colour. New
  `play(type, duration, colour)` API; legacy `start(type, duration)` keeps
  the opaque-black default. New unified Lua dispatcher
  `ui.transition_play(type_string, duration, [r, g, b, a])` accepts
  `"fade"`, `"fade_in"`, `"fade_out"`, `"wipe_left"`, `"wipe_right"`,
  `"wipe_up"`, `"wipe_down"`. The legacy `ui.transition_fade_in` /
  `ui.transition_fade_out` shorthands remain. `sample_project/scripts/raycast_house_door.lua`
  switched to `wipe_left` so the world→raycast handoff demoes the new
  variants. Tests in `test/scene_tests/ScreenTransition_test.cpp` cover
  default state, custom-colour play, progress, completion, every wipe
  variant, reset and the < 1 ms duration clamp.
- **Raycasting renderer (core)** — first non-2D renderer in the stack, a
  Wolfenstein-style DDA raycaster.
    - New static facade `renderer::rendererraycast::RendererRaycast`
      (`init` / `shutdown` / `beginScene` / `drawTilemapWalls` / `endScene` +
      `Statistics`). The DDA traversal runs on the CPU per screen column and
      emits textured `Renderer2D` quads (one per stripe) — leveraging the
      existing 2D pipeline so no new Slang shader is required for v0.2.0. A
      future PR can move the inner loop to a dedicated full-screen shader.
    - `RendererRaycastLayer` adapter (`RenderLayer` interface, factory key
      `"RendererRaycast"`). Reads per-instance config from the project's
      `DefaultConfig` / scene's `Overrides` (`Fov`, `MaxDistance`,
      `CeilingColor`, `FloorColor`, `NumRays`).
    - `Tilemap` is reused as the wall grid — the same component drives the
      Renderer2D path and the raycast path. `Scene::render` dispatches each
      tilemap to whichever renderer matches the active layer's type key.
    - Sky / floor backdrop is emitted lazily on the first wall draw so a
      scene that doesn't route any tilemap to the raycaster keeps the layer
      genuinely no-op (no overdraw on the 2D pass underneath).
    - **Sample project**:
        - New tileset `tilesets/raycast_walls.owltileset` (4×4 atlas, 128 px/tile,
          NEAREST-filtered) built from 13 reference textures kept under
          `textures/raycast_refs/` (doorpattern / greystone / bluestone / wood
          for E1L1 walls + barrel / brickpattern / colorstone / eagle /
          greenlight / mossy / pillar / purplestone / redbrick reserved for
          future use).
        - New scene `scenes/raycast_demo.owl` — full **Wolfenstein 3D E1L1**
          (64×64) imported from the user's reference raycaster (cells +
          player start + initial direction). Player rotation derived from
          the reference's `playerStartDir` mapped through Owl's yaw
          convention (rotation = -π/2 → forward = +X, matching E1L1's
          (1,0) start direction). Objects (barrels, eagles, etc.) are
          intentionally not imported — only the wall layout.
        - New scripts `scripts/raycast_player.lua` (classic Wolfenstein
          controls — AZERTY-friendly: GLFW key positions `W`/`S` map to
          AZERTY `Z`/`S` for forward/back, `A`/`D` map to AZERTY `Q`/`D`
          for turn left/right, no strafing) and
          `scripts/raycast_house_door.lua` (transition into the raycast
          scene from the world map).
        - `world_map.owl` ships a second house structure (7-cell-wide, 4-cell-tall
          variant — distinct from the existing 5×5 house) painted directly into
          the tilemap (cols 22-28, rows 11-14 with `house_roof` / `house_wall`
          / `house_door` tiles). The existing east-going stone path is extended
          to reach the new door (row 14 cols 20-21, row 13 col 21).
          `RaycastHouseDoor` is the invisible interaction trigger sitting on
          the door tile.
        - `owl_project.yml` adds a `RendererRaycast` slot named
          `raycast_world` between `world` and `ui` — disabled by default in
          existing scenes (no-op pass) and explicitly enabled by
          `raycast_demo.owl` via `EnabledRenderers`.
    - Tests: `RendererRaycast_test` covers factory registration, YAML config
      parsing, viewport latching, empty-tilemap silence, all-rays-miss in an
      empty grid, all-rays-hit against a wall row, and stats reset.
- **Renderer stack runtime + editor UI** — the foundation laid earlier in this
  cycle is now actually wired to rendering and exposed in the editor.
    - `Scene::renderWithStack` orchestrates per-layer passes for the active
      `RenderStack` (or falls back to the legacy single-pass when the stack is
      empty). Each pass calls `onBeginFrame` / `render` / `renderUI` /
      `onEndFrame` on the layer and routes entities by their `RendererTag`
      (untagged entities go to the first layer; backgrounds draw only on the
      first pass to keep the legacy z-order).
    - `EditorLayer::syncActiveDocumentPanels` now installs the project's
      `RendererStack` (filtered by the active scene's `EnabledRenderers`) on
      the engine `Renderer` whenever the active document changes — the
      runner does the equivalent at scene load.
    - **Project Settings** modal (`source/owlnest/sources/panel/ProjectSettings.cpp`)
      now has a "Renderer Stack" editor — ordered list of layers with a Type
      combo (factory keys), Name input, up/down reorder buttons, remove
      button, and `+ Add Layer`.
    - **Scene Hierarchy inspector** renders an editable `RendererTag`
      component (dropdown of the active stack's layer names + a `(default —
      first layer)` choice). Surfaces a one-shot warning when the chosen name
      doesn't match any active layer.
    - **Scene Hierarchy panel** groups root entities by renderer layer when
      the active stack has more than one layer (each layer is a tree node
      under `root`, untagged roots land under the first layer, entities with
      an unknown tag fall into a trailing `(unrouted)` group). Dragging a
      root entity onto a layer node both unparents it (if needed) and sets
      its `RendererTag.rendererName` in a single `ModifyEntityCommand` (one
      Ctrl+Z reverts the whole drop).
    - **Sample project** (`sample_project/owl_project.yml`) opts into the
      stack with two `Renderer2D` layers — `world` and `ui`. Every UI entity
      across `main_menu`, `settings_menu`, `world_map`, `platformer_house`,
      `victory`, `game_over` carries `RendererTag: { Name: ui }` so the HUD
      / menu UI is rendered on the dedicated layer.
- **Lua `physics.set_gravity_scale(entity, scale)`** — exposes
  `b2Body_SetGravityScale` for dynamic bodies. Setting `scale = 0` cancels
  world gravity for that body without touching mass; the top-down
  `world_player.lua` now calls this once in `on_create` instead of the
  per-frame `+9.81 * dt` cancellation hack.
- **Hidden triggers don't fire** — `Scene::onUpdateRuntime` now skips trigger
  entities whose `Visibility.gameVisible` is false (or whose ancestor is
  hidden). Any in-progress timer is stopped and prior overlap state is
  cleared with a synthetic `onTriggerExit`, so re-showing the trigger
  requires a fresh enter to fire it.
- **Tests**: `Scene.InvisibleTriggerDoesNotFire`,
  `PhysicCommand.GravityScaleZeroCancelsFalling`,
  `PhysicCommand.GravityScaleNoOpEdgeCases`.

### Changed

- **Scene `EnabledRenderers` now overrides layer order**, not just enable/disable.
  Layers listed in the scene appear in the order written; project layers the scene
  does not mention are appended afterwards in project order with their default
  config. A scene that omits `EnabledRenderers` keeps the previous behaviour
  (project order, all enabled). Three new tests in `RenderStack_test`:
  `sceneOverridesLayerOrder`, `sceneSilenceKeepsProjectOrder`,
  `sceneIgnoresUnknownLayerName`.
- **Oxford English spelling sweep across all comments and prose** — 251 word
  substitutions across 74 files (`.h` / `.cpp` comments and `.md` prose).
  Identifiers, YAML keys, code spans inside backticks, and string literals are
  preserved verbatim — only doxygen and Markdown prose were rewritten (colour /
  behaviour / centre / labelled / cancelled / neighbour / favour / honour /
  catalogue / analogue / etc.). `-ize` / `-yse` endings kept per Oxford.

### Fixed

- **Packaged games shipped without the engine assets they actually needed** —
  three independent issues compounded so a `.owlpack` produced by the editor
  never carried (and never extracted) the engine fonts, icons, and shader
  sources the runner expects on disk.
    - `AssetScanner::collectEngineAssets` only added `OpenSans-Regular.ttf` and
      the shader directory. Roboto / JetBrainsMono (loaded by
      `UiLayer::resolveAssetFile` from `engine_assets/fonts/`) and the engine
      logo (`logo_owl.png`, the runner's window-icon fallback) were silently
      omitted, so the runner logged
      `"could not load Roboto-Regular.ttf from engine_assets/fonts/roboto/"`
      and fell back to ImGui's bundled font. The scanner now collects every
      `.ttf`/`.otf` under `fonts/` and every image under `logo/` & `icons/`.
    - `Application::Application` extracted only entries whose path started
      with `shaders/` or `fonts/`, dropping every other engine entry on the
      floor even when it *was* in the pack. The whitelist is gone — every pack
      entry is now mirrored into `assets/` so the on-disk lookup paths
      (`AssetLibrary::find`, `TextureLibrary::find`, the slang compiler) keep
      working. A size-equality check skips re-extraction on subsequent
      launches.
    - The post-init cleanup loop deleted every non-`.spv` file from `assets/`
      after shader compilation, wiping the freshly-extracted Roboto, OpenSans,
      window icon, and shader sources. The cleanup is removed: extracted
      assets persist for the lifetime of the install.
- New `PackReader::entrySize()` reports the original (uncompressed) entry
  size, which the extractor uses to detect already-extracted files.
- **Asymmetric `CameraOrtho` projections were broken on Vulkan** — the
  Vulkan Y-flip in `CameraOrtho::setProjection` only multiplied the matrix's
  `(1, 1)` (Y scale) by `-1`. That trick is sufficient for symmetric ortho
  (where the offset `(1, 3)` is `0`) but breaks for any asymmetric pixel-space
  ortho such as `(0, vw, 0, vh)` used by `RendererRaycastLayer`,
  `ScreenTransition`, and `Renderer2DLayer` (Space=Screen): with the offset
  left at `-1`, the entire `[0, vh]` world-Y range mapped outside Vulkan's
  `[-1, 1]` clip volume, pinning every screen-space quad to the very top
  (in framebuffer coords) and clipping the rest. The raycaster's wall stripes
  ended up at "world y=0" → user perception **bottom of screen** ("only the
  feet of the walls"). Fix: also negate `(1, 3)` so the whole Y row of the
  projection flips coherently. Symmetric ortho behaviour is unchanged
  (`(1, 3) == 0` so negating it is a no-op).
- **Raycast player rotation had no effect** — `raycast_player.lua` updated
  the player's `Transform.rotation` via `transform.set_rotation`, but the
  Box2D body's rotation stayed at the scene-file initial value. Every
  physics step then re-synced body→Transform, silently overwriting the
  Lua-set rotation. The script now routes rotation through
  `physics.set_transform` (which calls `b2Body_SetTransform`), so the body
  itself rotates and the new rotation survives the sync. Same fix in
  `on_create` for the gamestate-restore path.
- **Raycast layer drew only the sky/floor backdrop, no walls** — both the
  backdrop quads and the wall stripes were emitted at z=0 in pixel space, so
  the default `GL_LESS` depth function let the backdrop write `0` to the depth
  buffer first and rejected every stripe drawn afterwards. The user saw a
  uniform sky/floor with no 3D walls. `RendererRaycastLayer::onBeginFrame`
  now disables the depth test for the layer (painter's order is the right
  semantic for a screen-space pass) and `onEndFrame` restores it before the
  next layer runs.
- **HUD rotated with the player camera in the raycast scene** — `Scene::renderUI`
  used to compute pixel-to-world via the world camera's inverse VP and the
  *world-axis-aligned* bounding box of the resulting corners, so a rotated
  player camera (the raycaster's player faces +X via a -π/2 Z rotation)
  produced a HUD whose origin tracked the world's bottom-left, not the
  screen's. The fix:
    - New `Renderer2DLayer` config `Space: Screen` (vs default `World`) — when
      set, the layer binds a pixel-space ortho instead of the scene camera so
      HUD overlays draw in screen pixels regardless of the player camera's
      rotation. The sample project's `ui` layer opts in.
    - New `RenderLayer::getEffectiveViewProjection(camera)` virtual — each
      layer reports the VP it just bound to `Renderer2D` (world VP for sprite
      layers, pixel-ortho VP for raycast / screen-overlay layers).
    - `Scene::renderUI` now takes that VP, derives camera-aligned right / up
      axes (no more world-axis-aligned bbox), and inherits the layer's screen
      rotation onto each UI quad's transform. `UIText` / `UIProgressBar` /
      `UISlider` now shift their fill / handle along the layer's local +X axis
      rather than world +X so the bar grows along the HUD instead of sliding
      diagonally.

- **Empty render-stack layers caused multi-scene flicker** — `Scene::renderWithStack`
  invoked every layer in the active stack regardless of whether any entity was
  routed to it. On Vulkan, an empty `Renderer2D::beginScene/endScene` pair around
  zero draw calls still triggers a render pass that clears state mid-frame, which
  manifested as background-vs-world flicker on `world_map.owl` and per-object
  flicker on `platformer_house.owl` (both have the project's `raycast_world` slot
  in their stack but no entity tagged for it). New `Scene::layerHasContent` scans
  visible renderable components (`Tilemap` / `SpriteRenderer` /
  `AnimatedSpriteRenderer` / `CircleRenderer` / `Text` / `Canvas` /
  `BackgroundTexture`) and skips any layer with zero matches before the
  begin/end-frame cycle. Lets every legacy 2D scene drop the
  `EnabledRenderers: raycast_world: false` workaround — the engine now figures
  it out.
- **Raycaster broken empty-cell convention** — the demo CSV used `0` for
  empty cells but Owl's `Tilemap` convention is `-1` (`g_EmptyTileIndex`).
  The DDA terminated on every traversed cell because cell 0 was treated as
  a hit on tile 0, returning a transparent stripe at perpDist ~0.6 — i.e.
  the player could only ever "see" the immediate neighbour cell, masked by
  the backdrop. Causes the entire view to render as flickering shapes
  hovering over the sky/floor. Conversion of the E1L1 import + the test
  scene's CSV now uses `-1` everywhere.
- **Raycaster wall texture blurriness** — Owl's default texture sampler
  uses `GL_LINEAR` for minification with mipmaps, so any wall further than
  ~1.5 cells got bilinearly blurred into mush. New `renderer::FilterMode`
  enum (`Linear` / `Nearest`); `Texture::Specification::filterMode` honoured
  by the OpenGL backend (NEAREST + no mipmap when `Nearest`); `Tileset`
  YAML accepts a `filterMode: Nearest` key. The raycast atlas declares
  `Nearest` so wall textures stay pixel-crisp at every distance, matching
  the original Wolfenstein 3D look.
- **Raycaster stripe flicker** — under camera motion the wall stripes
  visibly shimmered. Two root causes: (1) a 1.5 % stripe-width overlap
  introduced sub-pixel double-coverage at every column boundary, and the
  GPU's top-left rule alternated which neighbour won per frame; (2) the
  `lineHeight = vpH / perpDist` value was a fractional pixel count that
  oscillated as the player walked, so the top / bottom edges of every
  stripe popped between neighbour pixel rows. Fix: emit each stripe as
  exactly `viewportWidth / numRays` pixels wide with no overlap, snap the
  horizon to an integer pixel and `lineHeight` to an even integer so every
  stripe's pixel coverage is deterministic per frame. Sky / floor backdrop
  shares the same horizon constant.
- **Editor renders tilemaps in 2D regardless of `RendererTag`** —
  `Scene::renderWithStack` now short-circuits to the legacy single-pass
  when `status == Editing`, so the level designer keeps the top-down
  Renderer2D view even when the project's stack contains a
  `RendererRaycast` layer (editing a tilemap from inside a first-person
  raycast view is unusable). The multi-layer pipeline kicks back in when
  the scene enters Play mode.
- **Raycaster cell-coordinate conversion** — the half-extent in
  `RendererRaycast::drawTilemapWalls` was `(W − 1) / 2` instead of `W / 2`,
  shifting the camera by half a cell in cell-coord space (e.g. world X = 0.5
  was mapped to cellCoord 8.0 — the edge between cells 7 and 8 — instead
  of cellCoord 8.5, the centre of cell 8). Walls still rendered, but the
  player's apparent position drifted by half a cell. Same fix on Y.
- **Raycaster FOV** — the camera plane was multiplied by `viewport aspect`,
  which over-stretched the view cone on wide displays (a 75° configured
  FOV became ~107° at 16:9). Removed the aspect multiplier to match the
  classical Wolfenstein convention (`plane = perp · tan(fov/2)`). The
  configured `Fov` is now the actual horizontal FOV at any aspect ratio.
- **mingw-gcc 15 link**: pass `-Wa,-mbig-obj` globally on Windows GCC builds so
  template-heavy translation units (notably `Scene.cpp` after the renderer-stack
    + tilemap additions) no longer overflow the 16-bit PE/COFF section table —
      silenced the cascade of "relocation truncated to fit" + "undefined reference
      to `std::move_iterator<...>`" link errors.
- **`HelpIndex.BadgesAreFetchedAndCachedLocally`** now skips cleanly when the
  configure-time badge fetch produced no cached SVGs (e.g. the build agent has
  no outbound HTTPS — the typical ARM64 / sandboxed CI runner). The rewriting
  assertions still fire whenever at least one badge was downloaded.

### Changed

- **Renderer folder reorganized** — files split by renderer kind into
  matching sub-namespaces, in preparation for additional renderer types
  (voxel, future raycast variants).
    - `source/owl/{public,private}/renderer/stack/` houses the stack-level
      orchestration (`RenderLayer`, `RenderLayerFactory`, `RenderStack`,
      plus the YAML config structs `RendererStackEntry`,
      `RendererStackConfig`, `EnabledRenderersConfig`) under namespace
      `owl::renderer::stack`.
    - `source/owl/{public,private}/renderer/renderer2d/` houses the existing
      `Renderer2D` and its layer adapter under `owl::renderer::renderer2d`.
    - `source/owl/{public,private}/renderer/rendererraycast/` houses the new
      raycaster under `owl::renderer::rendererraycast`.
    - All call sites (engine, editor, runner, tests) updated to the new
      qualified names. Backward-compat shims are intentionally **not**
      provided: every consumer was renamed in this PR so the diff stays
      explicit.
    - `RenderLayer` gained a virtual `setViewport(vec2ui)` hook (no-op
      default) so layers that need pixel-accurate ortho projections (the
      raycaster) can latch the viewport size each frame.
- **Editor shortcuts** — modifier-based shortcuts (Ctrl+S, Ctrl+Z, …) now
  fire even when an ImGui text widget would otherwise capture the keyboard,
  matching the convention used by VS Code / Blender / Unity. Modifier-less
  shortcuts still yield to text input (so typing `s` in a name field doesn't
  trigger Save). When a shortcut would have matched but is suppressed by the
  capture check, `ActionRegistry` logs a `TRACE` line so the failure is
  diagnosable.

- **Renderer stack foundation** — composable per-scene renderer pipeline.
    - New public API in `source/owl/public/renderer/`: `RenderLayer` (interface),
      `RenderStack` (ordered orchestrator), `RenderLayerFactory` (string-keyed
      registry), `RendererStackConfig` and `EnabledRenderersConfig` (YAML
      round-trip data structures).
    - `Renderer2DLayer` adapter wraps the existing `Renderer2D` static facade
      and is auto-registered with the factory under the type key `"Renderer2D"`
      during `Renderer::initShaders`.
    - `Renderer::setRenderStack` / `getRenderStack` expose the active stack;
      `Renderer::reset` clears it.
    - New `scene::component::RendererTag` component (`{ rendererName: string }`)
      pins individual entities to a specific layer in the stack. Untagged
      entities fall back to the first layer (= legacy `Renderer2D` behaviour
      in mono-renderer projects). Component is added to `CopiableComponents`,
      `SerializableComponents`, and `OptionalComponents` and is auto-handled
      by `SceneSerializer`.
    - `Project` (editor) and `Scene` round-trip the stack configuration:
      `RendererStack:` block in `owl_project.yml` (project-level layer
      definitions with `Type` / `Name` / `DefaultConfig`) and
      `EnabledRenderers:` block in `.owl` files (per-scene enable flag and
      `Overrides` map merged on top of the project defaults).
    - **Backward compatible**: project without `RendererStack` falls back to
      an implicit `[Renderer2D(default)]` stack; scene without
      `EnabledRenderers` activates every project layer; entity without
      `RendererTag` is rendered by the first layer. Existing `.owl` and
      `owl_project.yml` files load unchanged.
    - Tests: `RenderLayerFactory_test` (4 cases), `RenderStack_test`
      (8 cases — config round-trip, scene override merge, frame callback
      order, find-by-name), `RendererTag_test` (4 cases — round-trip on
      entity and on `EnabledRenderers`).
- **`get_git_hash` resilience** in `ci/utils/publish.py`.
    - Falls back to TeamCity's `BUILD_VCS_NUMBER` env var, then to direct
      reading of `.git/HEAD` and the matching ref file (or `packed-refs`).
      Survives a stale `.git/objects/info/alternates` (e.g. a Windows path
      on a Linux agent when a TeamCity workspace is reused across OSes).
    - Also captures `stderr` from the `git` subprocess so the noisy
      `fatal: bad object HEAD` output no longer leaks to the build log.
- **Tilemap system (engine + editor)** — grid-based level authoring with
  shared tile atlases.
    - New `scene::Tileset` asset (`.owltileset`): texture + tile size +
      `columns × rows` grid + per-tile metadata (collidable flag, optional
      designer name). Sparse YAML serialization (only non-default tiles
      emitted). Public API: `getTileUv`, `getTileMeta`, `isCollidable`,
      `serializeToString` / `deserializeFromString`, `saveToFile` /
      `loadFromFile`.
    - New `scene::component::Tilemap` component: tileset reference (path),
      `width × height` cell grid, multi-layer support with per-layer name,
      visibility, parallax factor, and a flat row-major tile-index buffer.
      Tile data is encoded as a comma-separated string in YAML to keep
      large grids (4k+ cells) readable. Lazy resolution of the tileset asset
      via `Application::getAssetDirectories()` at first render.
    - Renderer integration: `Scene::render` walks Tilemap entities, draws one
      `Renderer2D::drawQuad` per non-empty cell with the tileset's atlas UVs.
      Layer order is back-to-front, invisible layers are skipped.
    - Physics integration: `PhysicCommand::init` generates one static Box2D
      body per Tilemap entity with one box fixture per collidable cell.
      Tilemaps with no collidable tile in their tileset are skipped (no
      empty body created).
    - Editor — Inspector: tileset asset slot with `.owltileset` drag-drop
      target, grid resize (preserves overlapping cells), cell-size field,
      layer list with name / visibility / parallax / occupancy + add /
      delete / move-up / move-down per layer.
    - Editor — Tile Palette panel: shows the active tileset's atlas as a
      clickable tile grid, eraser button, active-layer dropdown. Tracks
      selection from the SceneHierarchy panel automatically.
    - Editor — Viewport paint mode: when a Tilemap entity is selected and
      the Tile Palette has a brush, left-click paints the selected tile and
      right-click erases. Each paint operation pushes a `ModifyEntityCommand`
      on the per-document undo stack (rapid clicks coalesce via merge).
    - ContentBrowser: `.owltileset` icon + drag-drop to inspector field
      (filtered through `widgets::AssetKind::Tileset`).
    - Tests: `Tileset_test` (9 cases — resize, UV math, sparse round-trip,
      file round-trip, malformed input, out-of-range tile metadata),
      `Tilemap_test` (6 cases — layer add, get/set, resize-with-preservation,
      multi-layer YAML round-trip including parallax).

- **Sample game rebuilt around the tilemap system** with a two-stage world
  flow:
    - **`world_map.owl`** (NEW): top-down 32×24 tilemap (`world_topdown.owltileset`,
      16 tiles) — grass plain bordered by mountains, central house with
      door, water + lava hazards (instant death via `Trigger::Death`),
      hidden teleporter that appears once every house has been explored.
      Top-down player using `physics.set_velocity` with per-frame gravity
      cancellation (Box2D world gravity is hardcoded to -9.81 in v0.2.x).
    - **`platformer_house.owl`** (NEW): side-scroller 28×16 inside the house
      (`world_platform.owltileset`, 8 tiles) — jumping puzzle with brick
      walls, platforms, ladder, lava pits (death), spike traps (damage),
      victory zone. Reuses Player component for jump impulse.
    - **Cross-scene flow**: Main menu → world map → enter house → platformer
      → victory closes that house → world map → teleporter (visible once
      `houses_visited == houses_total`) → Victory screen.
    - **ESC continue mechanic**: ESC in any gameplay scene saves a snapshot
      (`continue_scene`/`continue_x/y/z`/`has_continue` keys in gamestate)
      and returns to the menu. The menu's primary button label flips
      between **"Démarrer"** (fresh run) and **"Continuer"** (resume); the
      destination scene's player script restores position from the snapshot.
    - **Shared HUD**: `HUDCanvas` + `HealthBar` (UIProgressBar) + control
      hint (UIText) embedded directly in both gameplay scenes; player
      scripts push `gamestate.health` to the bar each frame. Spike hits in
      the platformer deduct ~0.34 of the health bar per contact.
    - New scripts: `world_player.lua`, `platformer_player.lua`,
      `house_door.lua`, `teleporter.lua`, `spike_damage.lua`,
      `level_complete.lua`. Reworked `main_menu.lua` (dynamic button label
        + clear continue snapshot on fresh start) and `game_over.lua`
          (retry now returns to the world map).

### Changed

- Version bumped to **0.2.0**.
- `doc/pages/roadmap.md` re-organised: v0.2.0 narrowed to renderer stack +
  raycasting + tilemap + scene transitions; voxel engine moved to v0.2.1;
  custom file picker, 2D lighting, inventory, and enemies moved to v0.2.2.
  v0.3.0 / v0.4.0 / v0.5.0 expected dates shifted by one cycle.

## [0.1.1] - 2026-04-30

### Added

- **Full Markdown rendering for the in-editor help and live preview**
    - New `codeEditor::MarkdownDocument` parser backed by **md4c 0.5.2** (new
      DepManager recipe at `OwlDependencies/Libs/md4c/`) — CommonMark + GFM
      tables / strikethrough / autolinks. Public block model
      (`MdHeading`, `MdParagraph`, `MdCodeBlock`, `MdImage`, `MdTable`,
      `MdList`, `MdBlockQuote`, `MdHRule`) covers everything the help pages use.
      Lazy implicit-paragraph creation handles md4c's tight-list quirk where
      `MD_BLOCK_P` is skipped inside `MD_BLOCK_LI`.
    - Rewritten `codeEditor::MarkdownPreview` walks the parsed block list and
      emits ImGui draw calls directly: scaled headings (1.60× / 1.30× / 1.15× of
      body via `PushFont(font, size)`), inline emphasis / strong / strikethrough
      / inline code, GFM tables with `BeginTable` (borders + row stripes), code
      blocks rendered through cached read-only `TextEditor` widgets with full
      syntax highlighting (Lua / C / C++ / Python / YAML / JSON / Markdown / XML
      / **Bash** — new `Language::Bash` definition with POSIX/bash keywords and
      common shell built-ins), block / inline images loaded via `lunasvg` (SVG)
      and `stb_image` (raster) with on-disk cache per source path. PNG/JPG
      textures load through the engine's `pat:` serialized form and are
      displayed with flipped UVs to compensate for stb_image's bottom-up loading.
    - External links and `https://` images are preserved in the rendered output;
      clicks open the user's default browser via the new
      `core::utils::openExternalUrl` helper (Linux: `xdg-open` via fork+execvp;
      Windows: `ShellExecuteW`; URL scheme restricted to `http(s)://` and
      `mailto:` for safety).
    - `cmake/HelpAssets.cmake` now scrubs Doxygen syntax at bundle time:
      `# Title {#page-anchor}` → `# Title`, `[TOC]` lines dropped,
      `(../images/foo.svg)` rewritten to `(images/foo.svg)`,
      `(engine_assets/<dir>/foo.png)` (used by the README logo) rewritten to
      `(images/foo.png)`, and `doc/images/` plus `engine_assets/logo/` copied
      into `engine_assets/help/images/`. HTTPS images referenced from any
      bundled markdown (e.g. the 24 shields.io badges in the README) are
      downloaded once via `file(DOWNLOAD)` into
      `engine_assets/help/images/badges/<sha1>.svg` and the references
      rewritten to local paths so the runtime renderer never has to fetch.
    - `nest::panel::HelpPanel` now defaults to the project README on first
      open; a draggable splitter between the page tree and the content pane
      lets the user resize the navigation column (clamped to a 120 px minimum
      per side).
    - `imgui_markdown` removed from `depmanager.yml` (replaced).
- **Live preview for markup documents**
    - `codeEditor::SvgPreview` rasterises the live SVG buffer through `lunasvg`
      into a `Texture2D` (cap 2048 px / side, ARGB-premul → RGBA-straight).
    - `CodeEditorDocument` now offers a vertical splitter with a draggable
      handle when the active language is Markdown or XML/SVG. Auto-enabled on
      load, toggleable via the new **Text → Preview** ribbon button.
- **In-editor help pages**
    - `cmake/HelpAssets.cmake` bundles `doc/pages/*.md` plus root README /
      CHANGELOG / CONTRIBUTING into `engine_assets/help/` at configure time and
      generates an `index.yml` (id, title, category, path).
    - `nest::panel::HelpPanel` reads the index, renders the selected page via
      `MarkdownPreview`, supports search, categorised navigation, back/forward
      history. Internal `[link](other.md)` clicks navigate within the panel.
    - `help.context` action (default shortcut **F1**) opens the help page that
      documents the SceneHierarchy component header most recently hovered
      (`SceneHierarchy::lastHoveredComponentName`); falls back to the editor
      overview when nothing is hovered.
    - File ribbon tab gained a **Help** group; the Welcome screen surfaces a
      **Getting Started** entry pointing to the new
      `doc/pages/getting_started.md`.
- **Animation editor**
    - New reusable `.owlanim` asset (`scene::AnimationClip` —
      `source/owl/public/scene/AnimationClip.h`): texture, grid, frame range, frame
      duration, loop, optional speed curve, with YAML round-trip and unit tests in
      `test/scene_tests/AnimationClip_test.cpp`.
    - `nest::AnimationDocument` opens as a 4th document type (alongside Scene, Code,
      NodeGraph). Three-panel layout: live spritesheet preview, properties (texture
      drop target, columns / rows / first / last frame, frame duration, loop, speed
      curve via `gui::widgets::curveEditor`), and a frame-range timeline.
    - New `gui::widgets::sequencer()` widget (`source/owl/public/gui/widgets/Sequencer.h`)
      wraps `ImSequencer` from the existing imguizmo bundle — owl-friendly API
      (`SequencerEntry` / `SequencerOptions`), no third-party types leak through the
      public surface.
    - Contextual ribbon `Animation` tab when an `AnimationDocument` is active:
      Playback (Play / Pause / Stop), Frame (Previous / Next) and File (Save / Save
      As / Close) groups. Ribbon File → "New Animation" entry creates an untitled
      clip; double-click / drag-drop in the Content Browser routes `.owlanim` files to
      the document. Dedicated browser icon (filmstrip glyph).
    - `sample_project/animations/coin.owlanim` showcases the new asset (mirrors the
      level 2 coin animation, including the Smooth speed curve).
- **Scene Flow editing**
    - Visual create of teleport links — every scene node carries a ghost `+ Add teleport`
      output pin. Dragging it onto another scene's entry pin spawns a `Trigger`
      (`Type=Teleport`, `LevelName=<dest>`) entity in the source scene at world origin and
      wires the canvas link in one undoable step.
    - Visual delete of teleport links — pressing Delete on a Teleport link destroys the
      matching `Trigger` entity, removes the canvas pin, and erases the link in one undoable
      step.
    - Per-pin `targetName` editing — right-click a scene node → `Edit teleport target →
      <pin>` opens a modal that mutates the live `Trigger.targetName` and pushes a
      `ModifyEntityCommand` on the source scene's undo manager (rapid keystrokes coalesce).
    - New `commands::SceneFlowCompositeCommand` glues a `SceneUndoCommand` with a
      `NodeGraphUndoCommand` so a single undo step reverses both halves; complemented by
      `AddPinAndLinkCommand` / `RemovePinAndLinkCommand` for the canvas pin+link bundle.
    - New `EditorLayer::loadOrOpenSceneDocument(path)` synchronously opens a `SceneDocument`
      for an on-disk scene without yanking focus — used by Scene Flow link edits to mutate a
      scene that may not currently be in a tab.
    - Per-layer vertical centring in the BFS auto-layout so single-node layers no longer hug
      the top edge.
- **NodeCanvas widget polish**
    - Text level-of-detail: pin labels stop drawing below `0.6` zoom and node titles below
      `0.3`, leaving the silhouette + connectors visible for graph-overview navigation.
      Exposed via free helpers `gui::widgets::shouldDrawPinLabels` /
      `shouldDrawNodeTitles` for downstream reuse and unit tests.
    - New public pin-manipulation API on `NodeCanvas`: `addOutputPin` / `addInputPin` /
      `removeOutputPin` / `removeInputPin`. Pin removal automatically strips dangling links.
- **Inspector field interactions**
    - Drag-drop from Content Browser to inspector fields via shared
      `gui::widgets::assetDropTarget` helper (`source/owl/public/gui/widgets/AssetField.h`).
      Per-extension validation through a new `AssetKind` enum (Texture, Font, Sound,
      LuaScript, AnyScript, Scene, Prefab, Any). Reuses the existing `CONTENT_BROWSER_ITEM`
      ImGui payload — no new wire format. Drops accepted on Sprite/AnimatedSprite/
      BackgroundTexture/UIImage textures, Text font, SoundSource asset, LuaScript path.
    - `gui::widgets::textureField()` consolidates the previously inlined texture-picker
      pattern (thumbnail / popup / "Remove texture") into one helper used by every
      texture-aware component. Removes ~150 lines of duplicated drag-drop boilerplate
      from `gui/component/render.cpp`.
    - Texture thumbnails now show a `(loading...)` overlay while an async-loaded texture
      decodes (`LoadState::Pending`) and a red `(failed)` overlay on `LoadState::Failed`.
    - Font preview: Text component shows a sample-string rendering of the selected font
      ("Aa Bb 1!? éàüÇ" — covers lower/upper case, digits, punctuation and accented
      Latin-1 glyphs). Backed by a new `gui::FontPreviewCache` that lazily renders each
      font into a 256x64 framebuffer via `Renderer2D::drawString` and caches the result;
      pumped from `EditorLayer::onUpdate` and freed on `UiLayer::onDetach`.
    - Latin-1 glyph rendering fix — `Font::getGlyphBox` and `Renderer2D::drawString` no
      longer sign-extend `char` codepoints into garbage 32-bit values; the renderer
      additionally decodes UTF-8 source text into Latin-1 codepoints up front, so
      accented glyphs (`éàüÇ`…) coming from YAML scenes or Lua strings render correctly
      everywhere instead of falling back to `?` or `Ã©` byte pairs.
- **Sample project showcases v0.1.1 features**
    - Main-menu subtitle exercises UTF-8 / Latin-1 rendering
      (`Démo des fonctionnalités… caractères éàüÇ`).
    - Level-2 coins use a Smooth `AnimatedSpriteRenderer.speedCurve` so the rotation
      pulses (slow at the loop boundary, fast in the middle).
- **Curve editor for animated properties**
    - New `math::Curve` (`source/owl/public/math/Curve.h`) — sorted keyframe list with
      Constant / Linear / Smooth interpolation, flat-hold extrapolation, and YAML
      round-trip (handled inline in component serializers, default-empty curves omit
      `speedCurve` from the YAML to preserve byte-identical scene files).
    - New `gui::widgets::curveEditor` widget (`gui/widgets/CurveEditor.h`) — wraps
      ImCurveEdit from the existing imguizmo bundle (no new DepManager dependency).
      Drag points, double-click to add, right-click to remove; companion combo selects
      the interpolation mode.
    - First end-to-end consumer: `AnimatedSpriteRenderer.speedCurve` remaps per-frame
      animation advancement (`Scene::onUpdate` multiplies dt by
      `speedCurve.evaluate(progress)` when the curve is non-empty).
- **Scene Flow refinements**
    - Pin labels are now drawn **inside** the node frame (via a new `NodePin::labelColor` field
      and a `CustomDraw` override on `NodeCanvas`). GraphEditor receives `nullptr` for slot names
      so it stops rendering them outside the rect — labels stay aligned with their slot circles
      regardless of zoom
    - Compact pin labels: **just the source identifier** prefixed by a single-glyph kind hint
      (`† DangerZone`, `★ VictoryZone`, `λ checkpoint`, plain `LevelPortal` for Teleport).
      Destination is implicit from the link, no need to repeat in the label
    - **Right-click hit-test fix** — `GraphEditor` only sets `nodeOver` when a slot is hovered;
      clicks on the node body returned `-1`. `NodeCanvas` now does its own canvas-space hit-test
      against stored node rects when GraphEditor reports no node. Right-click on the body shows
      the Edit / Delete menu as expected
    - **Layered layout** replaces the random 4-column grid: BFS from `Project::firstScene`
      assigns each scene a column = its depth, orphans land in a "limbo" column on the right.
      Within each column, scenes are sorted alphabetically. Reduces link crossings substantially
      on the sample project
- **Scene Flow UX pass**
    - Nodes auto-size from their title and pin labels (`NodeCanvas::measureNode` exposes the
      `ImGui::CalcTextSize`-based size); the SceneFlow grid layout uses **actual** node widths /
      heights to compute column / row offsets so nothing overlaps regardless of label length
    - `NodeCanvas` now sets `GraphEditor::Options::mDrawIONameOnHover = false` — pin labels are
      drawn permanently (they used to only appear on hover, making the graph illegible at a glance)
    - Scene titles drop the `.owl` extension (implicit since every node is a scene)
    - Output pin labels show the **source trigger entity name** parsed from the scene's YAML
      (e.g. `LevelPortal → level2 @ SpawnPoint`); transitions of type `Death` and `Victory` are
      now extracted too (they also serialize a `LevelName`) and styled distinctly:
      `[death] DangerZone → game_over`, `[victory] VictoryZone → victory`
    - Best-effort scan of attached Lua scripts for `scene.load_scene("...")` calls — adds extra
      output pins tagged `scene_lua_exit` so Lua-driven transitions appear alongside Trigger ones
    - Right-click menu: on a node → **Edit scene** (opens it in a new tab) / **Delete scene**
      (with confirmation modal); on empty space → **Add new scene...** (creates an empty `.owl`
      under `scenes/NAME.owl` and opens it). Canvas auto-rescans after create/delete
    - The global **Scene Hierarchy** and **Properties** panels now host SceneFlow content while
      that document is active: the hierarchy lists every scene (orphans drawn red, click-to-select,
      double-click to open), the properties panel shows the selected scene's path, transitions
      (colour-coded: red = death, green = victory, blue = Lua) and an "Open this scene" button.
      Wired through new `Document::overridesGlobalPanels()` / `renderHierarchyPanel()` /
      `renderPropertiesPanel()` virtuals — generic for future node-graph documents
- **Node graph framework + Scene Flow view**
    - Reusable `gui::widgets::NodeCanvas` widget (public header
      `source/owl/public/gui/widgets/NodeCanvas.h`) — nodes with typed input/output pins, UUID-based
      identity, pan/zoom/selection, link validator, callbacks for link create/delete, node move,
      node select and **double-click** (detected in the wrapper since GraphEditor has no native
      double-click signal). Pimpl over `GraphEditor` from the existing `imguizmo` 1.92.7 DepManager
      package — the bundle ships GraphEditor + ImSequencer + ImCurveEdit + ImGradient + ImZoomSlider
        + ImLightRig alongside ImGuizmo, so no new dependency was needed
    - `gui::widgets::NodeCanvasSerializer` — domain-agnostic YAML round-trip (`.owlflow` format)
      for full canvas save/load, plus `serializeSubset`/`pasteSubset` for copy/paste with fresh UUIDs
    - `NodeGraphDocument` — third `DocumentType` alongside Scene and Code, generic node-graph
      document with its own `NodeGraphUndoManager` (a `UndoManager` typed for `NodeCanvas`); pastes, saves
      and loads through the serializer. Content Browser wires `.owlflow` double-click to a new
      `EditorLayer::openNodeGraphFile` handler; ribbon contextual tab "Graph" with save/close
    - Node-graph undo commands (`source/owlnest/sources/commands/NodeGraphCommands.{h,cpp}`):
      `AddNodeCommand`, `RemoveNodeCommand` (restores attached links on undo), `MoveNodeCommand`
      (merge-coalesces rapid drag into a single step), `AddLinkCommand`, `RemoveLinkCommand`
    - `SceneFlowDocument` — first consumer of the framework, specialises `NodeGraphDocument`
      to render the project's scenes as a graph: one node per `.owl` file, entry pin + one output
      pin per Teleport trigger found in that scene, links wired from output → destination entry,
      orphan scenes (unreachable from `Project::firstScene`) drawn with a red title. Double-click
      navigates to a scene via `EditorLayer::openScene`. Exposed from the ribbon File tab via
      a new "Views → Scene Flow" button. Link create/delete and per-pin target editing (writing
      new trigger entities back into the source scene) are deferred — they require composite
      `SceneUndo + NodeGraphUndo` commands and stay out of this first slice
    - 16 new unit tests: `test/gui_tests/NodeCanvas_test.cpp` (topology, link validator veto,
      cascaded link removal, selection round-trip, pin→node lookup, clear) and
      `test/gui_tests/NodeCanvasSerializer_test.cpp` (empty/full canvas round-trip, custom data
      preservation, malformed YAML rejection, subset serialize, paste with fresh UUIDs)
- **Undo system templatized over its target type**
    - Templatized `UndoCommand` + `UndoManager` (parameterized over the edited `Target`) in
      `source/owlnest/sources/UndoCommand.h` /
      `UndoManager.h` — both now header-only templates, `IUndoTarget` as common marker base
    - `SceneUndoCommand` / `SceneUndoManager` aliases preserve the current editor behaviour one-to-one;
      every existing command (`Entity*`, `Component*`, `Hierarchy*`, `Prefab*`) migrated to
      `SceneUndoCommand`
    - Unblocks a future node-graph undo stack parallel to the scene one (workstream B-5/B-6)
- **Async texture loading with placeholders**
    - `TextureDecoder` helper (`renderer/TextureDecoder.h`) — CPU-only decode primitive
      (`peekImageSize`, `decodeImageBytes`, `decodeImageFile`) with per-thread stb_image flip state
      so multiple workers can decode concurrently
    - New `Texture2D::createFromSerializedAsync(name, scheduler)` returns immediately with a
      placeholder-sized Rgba8 texture filled white; worker decodes, termination callback uploads
      real pixels on the main thread and flips `LoadState` to `Ready` (or `Failed`, keeping the
      placeholder visible)
    - Dimensions peeked from the PNG/JPG header up front, so every texture is created at its real
      size from frame 0 — binding, UV coords and atlas math unaffected
    - `Texture2D::createFromSerializedForDeserialize()` wrapper used by scene components
      (`SpriteRenderer`, `AnimatedSpriteRenderer`, `BackgroundTexture`, `UIImage`) — async when an
      `Application` is alive, synchronous fallback for `PackWriter` and unit tests
    - `RunnerLayer::finishTransition()` logs the count of still-pending textures after a teleport
      as a diagnostic trace — smooth scene changes in the runner, no more frame hitch on rich
      scenes
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
      accept an optional `std::vector` of warning strings as output)
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
    - `Ribbon::setTabHighlighted()` renders the tab title with the theme accent (secondary) colour
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
    - ContentBrowser double-click routes
      `.lua`/`.py`/`.c`/`.cpp`/`.cc`/`.cxx`/`.h`/`.hpp`/`.hxx`/`.yml`/`.yaml`/`.json`/`.md`/`.markdown`/`.svg`/`.xml`
      to a new code-editor tab (or re-activates the existing one)
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
      own framebuffer and a stable ImGui window id (`##scene_UUID`). ImGui's docking groups
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
        - Replaced SVG `text`-rendered glyphs with paths in `interaction` (letter E) and
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
    - Component-scoped `PushID(T::name())` in the templated `drawComponent` helper to prevent ImGui label collisions
      across components that share field names (e.g., "Colour" in multiple renderers)
    - Index-based `PushID` in LuaScript property loop

### Changed

- Version bumped to **0.1.1**
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

- **Sample project main menu Lua flood** — `string.format("%d, %d", mx, my)` on the float mouse
  coordinates now `math.floor`s them first; was triggering
  `bad argument #2 to 'format' (number has no integer representation)` on every frame in
  `main_menu.lua:89`
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
- **Windows Debug test binaries no longer fail to load** (was
  `STATUS_DLL_NOT_FOUND` / `0xc0000135`). The helper
  `owl_target_link_libraries()` was setting `CMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release`
  only around `find_package()`, but that variable is read at generate time (link resolution +
  `TARGET_RUNTIME_DLLS` generator-expression evaluation), not at find time. In Debug builds the linker picked
  `glfw3d.lib` (imports `glfw3d.dll`) while the post-build copy grabbed the release `glfw3.dll`
  — every test binary then failed to load. The mapping is now applied at top-level directory
  scope in `CMakeLists.txt`, gated by `OWL_USE_RELEASE_THIRD_PARTY`, so link and DLL copy agree
- Tests now get a `TARGET_RUNTIME_DLLS` generator-expression post-build copy next to their binary
  (belt-and-braces — picks up any DLL surfaced through the test's own link graph that the
  engine's post-build didn't cover)
- **Use-after-free SIGSEGV when closing a document tab** — closing a `SceneDocument` inline from
  `onImGuiRender` freed its Vulkan colour-attachment while ImGui's draw list still referenced it
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
- `writeLinuxLauncher` undefined on Windows (guarded with an `OWL_PLATFORM_LINUX` ifdef)
- `game_settings.yml` not included in packed game
- Markdown table alignment across all doc pages
- Clang-tidy cognitive complexity in UIInputSystem and AssetScanner (extracted helper functions)
- LuaBindings: `const lua_State*` incompatibility with `lua_CFunction` signature (added NOLINT)
- Flaky tests: shared `output/test_tmp` directory replaced with unique per-test paths in
  `std::filesystem::temp_directory_path()`
- First-time Slang shader compilation no longer freezes the window — loading screen animates
  between each shader
- Test scene textures (`source/owlnest/assets/scenes/test_levels{,2}.owl`) used absolute
  `pat:/source/.../mario.png` paths inherited from a previous host layout — converted to
  portable `nam:textures/mario.png` so they resolve through the engine's asset directories.

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
