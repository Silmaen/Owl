# Isometric renderer {#page-design-isometric}

[TOC]

Design of the isometric renderer, a rendering mode in the **Transport Tycoon Deluxe** tradition, mixable with the 2D,
raycast, voxel and 3D layers through the renderer stack. It carries a Transport Tycoon-like game: large tile maps with
slopes, tracks and roads on the tiles, buildings wider and taller than a tile, thousands of moving vehicles, zoom and
smooth scroll. The user guide is in [Renderer > Isometric layer](../renderer.md#renderer-isometric); the 2D lights on
the isometric sprites are in [2D complete](2d-complete.md).

## Layer and camera

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
- Layer config keys `TileWidth`, `TileHeight`, `ZStep` and `Zoom` (power-of-two level, −4 to 4); a script changes the
  zoom of the running level with `isometric.set_zoom`. In Play the camera
  target is the scene camera's position, Z dropped; the editor hands its own `IsometricCamera`, used as is.

## Draw order

- Depth test with alpha cutout: every sprite writes its isometric depth (diagonal, height, sub-layer), so opaque and
  cut-out sprites — the whole Transport Tycoon look — draw without sorting. Only translucent sprites (smoke, glass,
  previews) are sorted back to front on the CPU.
- Terrain chunks have a static order and are never sorted per frame; they live in persistent GPU buffers
  ([Terrain](#isometric-terrain)).
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

## Objects

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
  sprites. The vehicle sheet of `tools/iso_atlas` is an 8 × 2 sheet of 64 × 64 cells (car and truck, one frame every
  45° from +x towards +y); the vehicle is centred on its tile, whose front corner is the bottom centre of the cell and
  where the entity sits (`body + 0.5`). On screen +x runs to the lower right and +y to the lower left, so the right hand
  of a heading `(dx, dy)` is `(-dy, dx)`; a script picks the frame with `entity.set_sprite_frame`.

## Terrain {#isometric-terrain}

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
  (64×48 in the demo, two steps of headroom).
- Track and road overlays are the overlay ids of the tilemap cells: drawn right after the terrain of the same cell,
  half a sub-layer above it, through the same (id, slope) table, so they follow the slope.
- Map storage is the chunked tilemap format (structure of arrays, binary chunks), version 3 with the corner heights
  after the chunks.
- Chunk buffers: per layer and 32 x 32 chunk, a persistent instance buffer of 32 bytes per tile (foot in map space,
  height of the lowest corner, depth bias, texture rectangle; `tile.slang`). What the cells of a map share (origin,
  sprite size, depth slope, entity id) is in the per-draw uniform block, so a moved map, a new camera or another
  presentation rebuild nothing. `Scene::prepareTilemapRenderData` builds and uploads, before any layer draws, the
  chunks whose cell generation, corner-height generation (`TilemapAsset::getChunkHeightGeneration`, stamped on the up to
  four chunks around a changed corner) or tileset (layout, slope tables) changed: a static frame uploads nothing, a
  one-cell edit one chunk, and the frame draws one instanced call per chunk in view. A map drawn without a prepare is
  built at draw time and counted as a late build.

## Road junctions

The road overlay has 16 shapes, one per neighbour mask (bits N = -y, E = +x, S = +y, W = -x): ids 3 (E+W) and 11
(N+S), then 12 alone, 13-16 dead ends N/E/S/W, 17-20 corners NE/ES/SW/WN, 21-24 T missing W/N/E/S (NES, ESW, SWN, WNE),
25 cross. The asphalt fills the cell, an edge line closes every side that leads nowhere and the dashed centre line runs
to every joined side. All of them exist on every slope: the pattern is drawn in world coordinates, so a junction on a
ramp is as well defined as a straight piece. `iso_town.lua` tiles a cell from its mask and re-tiles the four neighbours
when a road is built or bulldozed; the `navigation` rules of the scene list the arms of each tile (`exits` the arms,
`enters` their opposites).

## Picking

- Tiles: `scene::terrain::pickTerrain` walks the cells under the screen ray (a 2D traversal clipped to the height range
  of the map) and intersects it with the two triangles of each candidate cell, split along the diagonal whose corners are
  the closest in height; it returns the cell, the nearest corner and the world point. Pure CPU, tested headless on the 19
  slopes, bench `iso/pick`.
- Entities keep the entity-id framebuffer picking.

## Editor

- Isometric viewport mode when the scene's stack holds an isometric layer: dimetric projection locked in Edit,
  world-axis cursor, tile highlight; the gizmo receives the layer's view and projection.
- `TilemapDocument`: isometric preview when the tileset is `kind: isometric`, raise / lower / level corner tools,
  overlay brush; `TilesetDocument`: kind and (terrain, slope) table. Every edit is undoable
  (`commands::ModifyTerrainHeightsCommand` keeps only the corners a gesture moved).

## Atlas generator

`tools/iso_atlas` is a seeded, deterministic Python generator (Poetry, Pillow), output committed, run from the
repository root with `./docker_run.sh poetry run python -m tools.iso_atlas [--project engine_demo] [--seed 1]
[--levels 1x,2x,half]` (default `1x`). It writes into `textures/iso/`, `tilesets/` and `tilemaps/` of the project:

| Output                                 | Content                                                                                                                                    |
|----------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| `tiles.png`                            | 16-column atlas of 64×48 tiles: flat terrains (road shapes included), slopes, foundation sets                                              |
| `buildings.png`, `buildings.json`      | Per-footprint-tile slices (64×128) of the buildings, with each slice's 3D box and rectangle                                                |
| `cottage`, `tower`, `oak`, `pine`, `…` | Whole sprites, bottom centre on the front corner of the footprint                                                                          |
| `vehicles.png`                         | The vehicle sheet                                                                                                                          |
| `smoke.png`                            | Translucent puff                                                                                                                           |
| `*_normal.png`, `*_depth.png`          | Same grid as the albedo; normals are world space (`n * 0.5 + 0.5`), depth is the height above the sprite's ground (`z / 8 * 255`)          |
| `@2x`, `@0.5x` suffixes                | `--levels 2x,half`: atlases drawn natively at twice the size, and the mip at half the size, with their tilesets (the demo keeps `1x` only) |

The same seed always gives the same pixels (pytest hash in `tools/tests/`).

## Demo and tests

`engine_demo/scenes/iso_town.owl` is a Transport Tycoon-style town on a sloped map: a ring road and streets, buildings
from the generated sheet, vehicles looping on the ring, a "build road" and bulldoze tool in Lua with its own `.owlhud`,
and a 2D sky behind the isometric layer. Game rules stay in Lua; the Lua `isometric` table (pick, cells, slope, height)
is the only engine support. A raycast or voxel layer cannot join the stack of this scene: both cast from a first-person
camera, the town has an orthographic one.

Tests cover the layer factory registration, the scene round-trip with
`EnabledRenderers: [{ Name: iso, Type: RendererIsometric }]`, the projection helpers on both Y conventions, slope
derivation and picking on the 19 slopes, tileset and tilemap format migrations, image tests on lavapipe and llvmpipe
with zero validation message, and the `iso/*` benches.
