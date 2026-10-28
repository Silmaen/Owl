# 2D complete {#page-design-2d-complete}

[TOC]

Design page of the 2D work once planned as v0.5.0; the 2026-10-16 replan spread it over the releases, see the
[Roadmap](../roadmap.md): the draw-order sort key, scroll views and lists, and tile-graph pathfinding in v0.4.0; 2D
lights and the minimap (done) and "look through scene camera" in v0.5.0; the tileset editor, mipmapped pixel-art atlases and the procedural brick in v0.6.0;
full 2D physics and the enemies demo in v0.7.0. The 2D physics part has its own page,
[Physics API](physics-api.md); the editor part is in [Owl Nest UI](nest-ui.md).

## 2D lighting system (v0.5.0, done)

Built as the `Light2D` component and `renderer/Lighting2D.h`; the user guide is in
[Renderer > 2D lighting](../renderer.md#renderer-lighting2d), the fields in [Scene](../scene.md#light2d-component).

| Decision          | Choice                                                                                           |
|-------------------|--------------------------------------------------------------------------------------------------|
| Light types       | Ambient, Point, Spot; up to 64 per frame, summed in the fragment shader                          |
| Default look      | No enabled light: unlit; lights without an ambient one: white ambient, lights only add           |
| Normal-mapped     | `SpriteRenderer.normalMap`, tangent space, OpenGL green-up, slanted by the light `height`        |
| Shadows           | Sprites flagged `castShadows` are occluders; a polar shadow map per light built on the CPU      |
| Tilemaps          | Lit without normal, shadows received                                                             |
| Editor            | Inspector, create menu (`entity.create_light2d`), hierarchy icons, viewport markers              |

## Cost of the 2D lighting and of the reversed-Z frustum {#lighting-cost}

The 2D lighting and the reversed-Z frustum of v0.5.0 made three CPU paths slower. An A/B of `owl_bench` on one core
against the merge of the Hi-Z culling (just before the 2D lighting) gives what is left:

| Part     | Change                                                                                                    | Before the 2D lighting, with it, now                   |
|----------|-----------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| Quads    | `QuadInstance` at 80 bytes: the normal-map index and the lit / shadow-caster flags in its padding         | `quads_world_index/10000` 100, 127, 121 µs             |
| Tilemaps | `getLighting2DState` returns a reference (no `shared` copy); unlit tilemaps bind no light buffer          | `tilemap/static/32x32_layers1` 120, 158, 128 ns        |
| Frustum  | The tolerance folded into the near plane when the frustum is built: no per-box cost, near plane only      | `culling/octree/brute_force/10000` within the noise    |

The quads keep about 15 % more instructions per quad: reading the normal map and the flags, and a `Quad2DData` 24 bytes
larger (a second `shared` texture) built and destroyed by every caller. It is the cost of the feature, accepted.

## Text and HUD

Aspect-correct text with alignment and fit modes, and the dedicated HUD layer with its editor, moved to v0.4.0:
see [HUD system](hud-system.md). Built on them:

- v0.4.0: scroll views with clipping and virtualised lists (vehicle lists of a management game), done in HUD7
- v0.5.0: tables (columns and sortable headers) over the virtualised list
- v0.5.0: a minimap on a render target, shown in a `UiImage`, done: see [HUD system](hud-system.md#minimap)
- v0.5.0: live scene backdrop in the HUD editor, with "look through scene camera"

## Draw-order sort key (B-10, v0.4.0, done)

A sort key on every 2D primitive instead of the primitive type and the storage order; it follows the depth convention
of the v0.4.0 isometric layer ([Isometric](isometric.md)) so 2D and isometric layers order the same way: `depth` on each
data struct of `Renderer2D`, larger is nearer; the rule is depth first, then type (quads, circles, lines, text), then
submission order. `Scene` feeds it the world z of sprites,
circles and text, the HUD pass the window rank. An authored layer on the components (a field in the inspector) is not
there yet: z is the only way to order a scene.

## 2D lighting on isometric sprites (v0.5.0, done)

The `Light2D` lights also light the isometric layer through the `_normal` and `_depth` atlases produced by
`tools/iso_atlas`, found beside an albedo atlas.

| Decision         | Choice                                                                                            |
|------------------|---------------------------------------------------------------------------------------------------|
| Texture slots    | Albedo 0 to 9, normal atlas +10, depth atlas +20 (`common/isometric_light.slangh`)               |
| Map position     | Solved per pixel from the view position and the height read in the depth atlas                    |
| Ground tiles     | Lit per pixel too                                                                                 |
| Shadows          | None in the isometric layer                                                                       |
| Ambient          | The `Ambient` light gives day and night                                                           |

## Tile-graph pathfinding (v0.4.0, done)

Shipped as PATH1, user guide in [Pathfinding](../pathfinding.md). Vehicles of a Transport Tycoon-like game follow a graph of tiles and tracks,
not a navigation mesh.

- A* on a tile grid or on a graph built from tilemap overlays (tracks, roads), with per-edge costs and one-way edges
- Hierarchical search over the 32×32 chunks of the v0.4.0 tilemap storage, so 1024² maps stay cheap
- Incremental update when a tile changes; queries spread over ticks on the task system
- Lua API and an editor overlay drawing the graph and a test path

## Mipmapped pixel-art atlases (v0.6.0, done) {#pixel-art-atlases}

A tile atlas keeps its texels sharp up close and is filtered far away, without a cell bleeding into its neighbours.
User guide: [Renderer](../renderer.md), [Voxel Engine](../voxel.md).

| Part        | Decision                                                                                                                                                                                                            |
|-------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Filter mode | `FilterMode::PixelArt`: nearest magnification, trilinear minification, on OpenGL and Vulkan                                                                                                                         |
| Mipmaps     | `renderer::buildCellMips` builds them cell by cell, alpha-weighted box filter; the chain stops when a cell side is odd                                                                                              |
| Cook        | The cook keeps its own mips; the runtime keeps only the levels that hold whole cells                                                                                                                                |
| Loading     | A tileset with `filterMode: PixelArt` calls `Tileset::applyFilterMode()`, `Texture2D::setPixelArtCells(cell)`                                                                                                       |
| Shaders     | `voxel`, `tilemap_instanced` and `raycast_stripe` sample through `clampToAtlasCell` (`common/atlas.slangh`): the UV stays half a texel of the coarsest level read inside the cell; a one-level texture is unchanged |
| Demo        | The `voxel_blocks` and `raycast_w3d_walls` tilesets use `PixelArt`                                                                                                                                                  |
| Tests       | `AtlasMips_test.cpp` (cell isolation of the generated levels), the voxel and raycast render references                                                                                                              |

## Tileset editor (v0.6.0, done)

`TilesetDocument` composes the atlas from source images in Owl Nest; user guide in
[Tilemaps and Tilesets](../editor_tilemaps.md#composing-the-atlas).

- The atlas pixels live in `scene::TilesetAtlas`; the PNG is written on Save, and a tileset without texture creates
  `<name>.png` beside its file (it must be saved first). The texture and the tile size are locked while composed pixels
  are unsaved.
- An image dropped from the Content Browser fills a slot, scaled to the cell; a folder fills the slot and the next empty
  ones with its images sorted by name, adding rows at the bottom when full. **Insert Image...**, **Insert Folder...**,
  **Clear Image** and **Select All** are in the right-click menu.
- Clearing keeps the tile index: the pixels become transparent and the metadata is reset, so a tilemap that used the
  index shows an empty cell instead of pointing at the next tile.
- Ctrl / Shift click select several slots; dragging the selection onto another slot moves the pixels and the metadata
  (occupied slots swap).
- The grid **+** / **-** buttons resize the atlas, each cell keeping its column and row. Growth adds rows only, so the
  indices stay stable; extending the columns is not offered.
- Every gesture is one undo step.

## First procedural brick for tilemaps (v0.6.0, done)

The seeded generation of a tilemap region is the first consumer of the [PCG graphs](pcg-graphs.md): the generator of a
tilemap runs a graph from a seed, undoable, from the **Procedural** section of the tilemap editor.

## Engine Demo demos (moved out of the engine)

- Inventory demo — done in v0.4.0 as the showcase of the new HUD widgets ([HUD system](hud-system.md))
- Enemies demo (v0.7.0) — patrol / chase enemies in Lua in `engine_demo/`; engine-side AI arrives with v0.7.0
