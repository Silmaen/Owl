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
- Terrain chunks have a static order and are never sorted per frame; they live in persistent GPU buffers (below).
- Pre-rendered sprites are billboards anchored at their world foot point; they are never pushed through the dimetric
  matrix a second time. Each corner takes the depth of a plane through the foot: vertical, facing the viewer, for an
  upright sprite; the ground plane for a flat terrain tile. Tilemap layers are depth sub-layers a sixteenth of a view
  pixel apart, upright sprites one view pixel above the ground under them; a tint alpha below 1 marks a translucent
  sprite. An entity sprite is `scale.xy × TileWidth` pixels large.
- Objects wider than a tile are cut into one slice per footprint tile, as Transport Tycoon does; each slice carries a
  3D box that gives its depth, which avoids the sorting artefacts of long vehicles and tall buildings.
- Moving sprites use an anchor-plus-size instance without a per-sprite matrix (`IsometricSprite`, 64 bytes against 96
  for an `IsometricBillboard`), so thousands of vehicles stay on the instanced path: `drawSprites` takes a span sharing
  one texture, drops the sprites outside the viewport on the CPU and batches the rest in one draw call.
- The foot of every sprite and tile snaps to whole screen pixels at an integer zoom (level 0 and above), so a moving
  sprite never shimmers; below zoom 1 the projection is left alone.

### Objects

- An object sheet is the `buildings.json` of the generator: for each building its footprint and one slice per footprint
  tile (`cell`, `rect` in the sheet, `anchor`, `box` = the 3D box of the slice in tiles). `renderer::IsometricObjectSet`
  parses it (`zoom` gives the art's tile width, 64 at `1x`) and holds the texture.
- A slice is an upright sprite whose bottom centre sits on the front corner of its tile, at the object's origin plus
  `(cell + 1)`: each one writes the depth of its own plane, so a vehicle drives between the slices of a station and a
  tree stands behind or in front of a tower per pixel. The box gives the object's height (`IsometricObjectDef::getHeight`);
  the sprites are cut against the viewport.
- The `IsometricObject` component (`atlasPath`, `building`, `color`) puts one on an entity: the translation is the back
  corner of the footprint, rotation and scale are ignored, only the isometric layer draws it. Its inspector has a sheet
  drop field (`.json`), the building list and a footprint summary; edits go through the usual component command, so they
  are undoable, and the entity is picked in the viewport through the entity id the slices write. A pack includes the
  sheet and the `.png` beside it.
- Vehicles are plain `SpriteRenderer` / `AnimatedSpriteRenderer` entities: opaque ones leave the layer as compact
  sprites. `tools/iso_atlas` writes `vehicles.png`, a 4 × 1 sheet of 64 × 64 cells (car and truck, heading along x and
  along y); the bottom centre of a cell is the front ground corner of the vehicle.

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
  after the chunks, up to 1024² in v0.4.0; 4096² maps wait for the v0.6.0 content pipeline on the same storage interface.

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
- `engine_demo/scenes/iso_town.owl`: a Transport Tycoon-style town on a 24×24 sloped map (a hill, a one-step embankment under the north street), a ring
  road and two streets along both axes, buildings from the generated sheet, three vehicles looping on the ring, a mouse
  / keyboard / gamepad "build road" and bulldoze tool in Lua, its own `.owlhud`, and a 2D sky with clouds behind the
  isometric layer. Game rules stay in Lua in the Engine Demo; the Lua `isometric` table (pick, cells, slope,
  height) is the only engine support. A raycast or voxel layer cannot join the stack of this scene: both cast from a
  first-person camera, the town has an orthographic one.

### Tests

Layer factory registration, scene round-trip with `EnabledRenderers: [{ Name: iso, Type: RendererIsometric }]`, the
projection helpers on both Y conventions, slope derivation and picking on the 19 slopes, tileset and tilemap format
migrations, image tests on lavapipe and llvmpipe with zero validation message, and the `iso/*` benches.

## Status

ISO1 (2026-10-14) shipped the layer, the camera, the billboards with depth and cutout, the translucent sort, the
ported projection helpers, `Tileset::kind`, the isometric viewport mode of Owl Nest, `engine_demo/scenes/iso_test.owl`
and the `isometric` image test on both backends. Isometric tilemaps keep one persistent GPU instance buffer per 32 x 32
chunk and layer, uploaded in the prepare phase only when a chunk changed (see the ISO5 chunk buffers below).

ISO2 (2026-10-15) added the corner heights (tilemap format 3), the 19 slopes, terraforming, the (terrain, slope) table
and foundations of the tileset, overlays drawn along the slope, `pickTerrain` (bench `iso/pick`), the isometric preview
and the terraforming tools of `TilemapDocument`, the slope table of `TilesetDocument`, and the sloped `iso_test.owl`
(an 8×8 map with a hump under the house on a foundation, a ridge and a road climbing a ramp); the `isometric` image
references changed because the sample map is no longer flat.

ISO3 (2026-10-15) added the atlas generator `tools/iso_atlas`, run from the repository root with
`docker/run.sh poetry run python -m tools.iso_atlas [--project engine_demo] [--seed 1] [--levels 1x,2x,half]`.
It writes into `textures/iso/`, `tilesets/` and `tilemaps/` of the project:

| Output                                 | Content                                                                                                                           |
|----------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| `tiles.png`                            | 16-column atlas of 64×48 tiles: 12 flat terrains, 18 slopes of 10 terrains, 2 foundation sets (road along Y included)                                      |
| `buildings.png`, `buildings.json`      | Per-footprint-tile slices (64×128) of five buildings, with each slice's 3D box and rectangle                                      |
| `cottage`, `tower`, `oak`, `pine`, `…` | Whole sprites, bottom centre on the front corner of the footprint                                                                 |
| `smoke.png`                            | Translucent puff                                                                                                                  |
| `*_normal.png`, `*_depth.png`          | Same grid as the albedo; normals are world space (`n * 0.5 + 0.5`), depth is the height above the sprite's ground (`z / 8 * 255`) |
| `@2x`, `@0.5x` suffixes                | Atlases drawn natively at twice the size, and the mip at half the size, with their tilesets                                       |

The same seed always gives the same pixels (pytest hash in `tools/tests/`); the sample map `iso_test.owltilemap` is
written by the same run. Nothing reads the normal and depth atlases yet (v0.5.0 lights).

ISO4 (2026-10-15) added the objects: `IsometricObjectSet` and the `IsometricObject` component with its inspector, the
compact `IsometricSprite` and `RendererIsometric::drawSprite` / `drawSprites` / `drawObject` (`sprite.slang`), pixel
snapping at integer zoom in both shaders, the vehicle sheet of the generator, the `iso_test.owl` buildings from slices
and three vehicles driven by `iso_vehicle.lua`, and the `isometric_objects` image test (640 × 360, both backends; the
`isometric` references did not change). Measured on the Null backend (1920 × 1080, 512² map with hills, one P-core):

| Case                                             | Median  | Budget |
|--------------------------------------------------|---------|--------|
| Frame, zoom 1, no vehicle                        | 0.21 ms | < 1 ms |
| Frame, zoom 1, 5 000 vehicles over the whole map | 0.26 ms | < 2 ms |
| Frame, zoom 1, 5 000 vehicles in view            | 0.31 ms | < 2 ms |
| Frame, zoomed out ×4 (51 200 cells in 3 chunks)  | 1.42 ms | < 4 ms |
| 1 000 buildings of 3 slices (3 000 sprites)      | 63 µs   | -      |

The baseline of these entries in `bench/baseline/linux-bench.json` is an estimate (this run scaled by the agent ratio
of `renderer2d/quads_color`) until a nightly run replaces it.

ISO5 chunk buffers (2026-10-16): a tilemap of the isometric layer keeps, per layer and 32 x 32 chunk, a persistent
instance buffer of 32 bytes per tile (foot in map space, height of the lowest corner, depth bias, texture rectangle;
`tile.slang`). What the cells of a map share (origin, sprite size, depth slope, entity id) is in the per-draw uniform
block, so a moved map, a new camera or another presentation rebuild nothing. `Scene::prepareTilemapRenderData` builds
and uploads, before any layer draws, the chunks whose cell generation, corner-height generation
(`TilemapAsset::getChunkHeightGeneration`, stamped on the up to four chunks around a changed corner) or tileset
(layout, slope tables) changed: a static frame uploads 0 byte, a one-cell edit one chunk, and the frame draws one
instanced call per chunk in view. A map drawn without a prepare is built at draw time and counted as a late build.
Same Null-backend measures as above:

| Case                                             | Before  | After   | Budget |
|--------------------------------------------------|---------|---------|--------|
| Frame, zoom 1, no vehicle                        | 218 µs  | 16 µs   | < 1 ms |
| Frame, zoom 1, 5 000 vehicles over the whole map | 267 µs  | 125 µs  | < 2 ms |
| Frame, zoom 1, 5 000 vehicles in view            | 308 µs  | 178 µs  | < 2 ms |
| Frame, zoomed out ×4 (51 200 cells in 3 chunks)  | 1.43 ms | 17 µs   | < 4 ms |

ISO5 demo (2026-10-16): `engine_demo/scenes/iso_town.owl` (**O** from the main menu), the road along Y, a `marker`
prop and `iso_town.owltilemap` from the generator, `scripts/iso_town.lua`, `iso_town_traffic.lua` and `iso_cloud.lua`,
`huds/iso_town.owlhud`, the `build`, `bulldoze`, `town_tool` and `cursor` actions, the Lua `isometric.*` functions and
`ui.is_mouse_over_ui`, the `iso_town` and `iso_town_actions` scenarios and the `iso_town` image test (640 × 360, both
backends).

## Later

- v0.5.0: normal-mapped isometric sprites under the 2D lights, from the generator's normal atlas; day / night as an
  ambient tint; tile-graph pathfinding for vehicles on the chunked map.
- v0.5.0: 3D meshes drawn under the same camera into the same depth buffer; shadows from the depth atlas or from
  shadow maps.
- v0.6.0: 4096² maps from the content pipeline; procedural terrain heights from the [PCG graphs](pcg-graphs.md).
