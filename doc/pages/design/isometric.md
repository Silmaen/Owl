# Isometric renderer {#page-design-isometric}

[TOC]

Design page for the v0.4.0 isometric renderer, summarised in the [Roadmap](../roadmap.md). The pull requests, their
order and the budgets are in [Isometric worlds](isometric-worlds.md) (ISO1 to ISO5, on top of TM1 / TM2).

## Goal

Add a third non-2D rendering mode — an isometric renderer in the **Transport Tycoon Deluxe** tradition — slotted
between the existing 2D / raycast / voxel options and mixable with them through the renderer stack. The target is an
engine able to carry a Transport Tycoon-like game: large tile maps with slopes, tracks and roads on the tiles,
buildings wider and taller than a tile, thousands of moving vehicles, zoom and smooth scroll, and modern rendering
later (lights, day / night, shadows, 3D meshes under the same camera).

The first step on the unmerged `Feature/KickoffIsometricRenderer` branch (2026-06) is **rewritten** on v0.3.0: only
`worldToScreen` / `screenToWorld` and their tests are ported, then the branch is deleted.

## Specification

### Layer and camera

- `RendererIsometricLayer`, factory key `"RendererIsometric"`: scenes mix it with the other layers by listing it in
  `owl_project.yml` and tagging entities with the matching `RendererTag`. It draws from its own `onRender`, not from
  `Scene::render`.
- Fixed 2:1 dimetric presentation, no free-look camera. The camera is a real view (45° yaw, 30° pitch, Z scaled by
  `zStep`) times `CameraOrtho`: pan, power-of-two zoom steps and the Vulkan Y flip come from the existing camera code,
  and depth stays usable. Pixel snapping at integer zoom.
- World axes map to screen as `screen.x = (worldX − worldY) · (tileW / 2)` and
  `screen.y = (worldX + worldY) · (tileH / 2) − worldZ · zStep`; tiles are 64×32 px by default (layer config).
  This layout is the mirror image of a right-handed world seen from above, so the view mirrors world Y before the
  rotation: distances and depth are unchanged, the pitch is `asin(tileH / tileW)` (30° for 2:1).
- Layer config keys `TileWidth`, `TileHeight`, `ZStep` and `Zoom` (power-of-two level, −4 to 4). In Play the camera
  target is the scene camera's position, Z dropped; the editor hands its own `IsometricCamera`, used as is.

### Draw order

- Depth test with alpha cutout: every sprite writes its isometric depth (diagonal, height, sub-layer), so opaque and
  cut-out sprites — the whole Transport Tycoon look — draw without sorting. Only translucent sprites (smoke, glass,
  previews) are sorted back to front on the CPU.
- Terrain chunks have a static order and are never sorted per frame.
- Pre-rendered sprites are billboards anchored at their world foot point; they are never pushed through the dimetric
  matrix a second time. Each corner takes the depth of a plane through the foot: vertical, facing the viewer, for an
  upright sprite; the ground plane for a flat terrain tile. Tilemap layers are depth sub-layers a sixteenth of a view
  pixel apart, upright sprites one view pixel above the ground under them; a tint alpha below 1 marks a translucent
  sprite. An entity sprite is `scale.xy × TileWidth` pixels large.
- Objects wider than a tile are cut into one slice per footprint tile, as Transport Tycoon does; each slice carries a
  3D box that gives its depth, which avoids the sorting artefacts of long vehicles and tall buildings.
- Moving sprites use an anchor-plus-size billboard instance without a per-sprite matrix, so thousands of vehicles stay
  on the instanced path.

### Terrain

- Heights belong to the map: `TilemapAsset` holds a `(W+1)×(H+1)` grid of corner heights (`uint8_t` steps of 0.5
  world unit, 8 pixels at 2:1, empty while the map is flat); a cell's slope is derived from its four corners — 15
  regular slopes plus 4 steep ones, 19 in all (`scene::terrain`, `scene/TilemapTerrain.h`) — so terraforming is a
  corner edit. Corners next to each other never differ by more than one step: `setCornerHeight`, `raiseCorner`,
  `lowerCorner` and the area calls `raiseArea`, `lowerArea`, `levelArea` pull the neighbours along and return the
  corners that moved (`CornerHeightChange`), which `applyCornerChanges` plays back or undoes.
- The 19 slopes, as heights of the back, right, front and left corners above the lowest: flat `0000`; one corner up
  `1000`, `0100`, `0010`, `0001`; three up `0111`, `1011`, `1101`, `1110`; an edge up `1100`, `0110`, `0011`, `1001`;
  saddles `1010`, `0101`; steep `2101`, `1210`, `0121`, `1012`.
- `Tileset::kind` (`Orthogonal`, `Isometric`); an isometric tileset maps (terrain id, slope) to a tile index through
  its `slopes` table (`SlopeSet`: 19 tiles and 19 foundation tiles per id; `-1` falls back on the flat tile, an id
  without a table is its own tile index) and holds the foundation sprites: a cell whose flag bit 0
  (`foundationFlag`) is set draws its foundation instead of the slope tile, under a building standing on a slope.
  A sloped tile is drawn on the lowest corner of its cell, so an isometric tileset has tiles taller than the diamond
  (the sample uses 64×48, two steps of headroom).
- Track and road overlays are the overlay ids of the tilemap cells (TM2): drawn right after the terrain of the same
  cell, half a sub-layer above it, through the same (id, slope) table, so they follow the slope.
- Map storage is the chunked format of TM2 (structure of arrays, binary chunks), now version 3 with the corner heights
  after the chunks, up to 1024² in v0.4.0; 4096² maps wait for the v0.7.0 content pipeline on the same storage interface.

### Picking

- Tiles: `scene::terrain::pickTerrain` walks the cells under the screen ray (a 2D traversal clipped to the height range
  of the map) and intersects it with the two triangles of each candidate cell, split along the diagonal whose corners are
  the closest in height; it returns the cell, the nearest corner and the world point. Pure CPU, tested headless on the 19
  slopes, 83 ns per pick on a 256² map with hills (bench `iso/pick`, budget 20 µs).
- Entities keep the entity-id framebuffer picking.

### Editor

- Isometric viewport mode when the scene's stack holds an isometric layer: dimetric projection locked in Edit,
  world-axis cursor, tile highlight; the gizmo receives the layer's view and projection.
- `TilemapDocument`: isometric preview when the tileset is `kind: isometric`, raise / lower / level corner tools,
  overlay brush; `TilesetDocument`: kind and (terrain, slope) table. Every edit is undoable
  (`commands::ModifyTerrainHeightsCommand` keeps only the corners a gesture moved).

### Assets and demo

- Procedural Python generator under `tools/` (Poetry, Pillow), seeded and deterministic, output committed: terrain ×
  19 slopes, foundations, tracks and roads, a few sliced buildings, vehicles. It writes albedo plus normal and depth
  atlases on the same grid, one set per zoom level or mips.
- `sample_project/scenes/iso_town.owl`: a Transport Tycoon-style town on two height plateaus linked by ramps, roads,
  a vehicle loop, a mouse "build road" tool in Lua, the shared HUD, and a scene mixing isometric with another style.
  Game rules stay in Lua in the sample project.

### Tests

Layer factory registration, scene round-trip with `EnabledRenderers: [{ Name: iso, Type: RendererIsometric }]`, the
projection helpers on both Y conventions, slope derivation and picking on the 19 slopes, tileset and tilemap format
migrations, image tests on lavapipe and llvmpipe with zero validation message, and the `iso/*` benches.

## Status

ISO1 (2026-10-14) shipped the layer, the camera, the billboards with depth and cutout, the translucent sort, the
ported projection helpers, `Tileset::kind`, the isometric viewport mode of Owl Nest, `sample_project/scenes/iso_test.owl`
and the `isometric` image test on both backends. Isometric tilemaps emit their cells per frame from the chunks in view;
persistent isometric chunk buffers come with ISO4.

ISO2 (2026-10-15) added the corner heights (tilemap format 3), the 19 slopes, terraforming, the (terrain, slope) table
and foundations of the tileset, overlays drawn along the slope, `pickTerrain` (bench `iso/pick`), the isometric preview
and the terraforming tools of `TilemapDocument`, the slope table of `TilesetDocument`, and the sloped `iso_test.owl`
(an 8×8 map with a hump under the house on a foundation, a ridge and a road climbing a ramp); the `isometric` image
references changed because the sample map is no longer flat.

## Later

- v0.5.0: normal-mapped isometric sprites under the 2D lights, from the generator's normal atlas; day / night as an
  ambient tint; tile-graph pathfinding for vehicles on the chunked map.
- v0.6.0: 3D meshes drawn under the same camera into the same depth buffer; shadows from the depth atlas or from
  shadow maps.
- v0.7.0: 4096² maps from the content pipeline; procedural terrain heights from the [PCG graphs](pcg-graphs.md).
