# 2D complete {#page-design-2d-complete}

[TOC]

Design of the 2D features of the engine. The 2D physics has its own page, [Physics API](physics-api.md); the HUD is in
[HUD system](hud-system.md).

## 2D lighting system

The `Light2D` component and `renderer/Lighting2D.h`; the user guide is in
[Renderer > 2D lighting](../renderer.md#renderer-lighting2d), the fields in [Scene](../scene.md#light2d-component).

| Decision      | Choice                                                                                     |
|---------------|--------------------------------------------------------------------------------------------|
| Light types   | Ambient, Point, Spot; up to 64 per frame, summed in the fragment shader                    |
| Default look  | No enabled light: unlit; lights without an ambient one: white ambient, lights only add     |
| Normal-mapped | `SpriteRenderer.normalMap`, tangent space, OpenGL green-up, slanted by the light `height`  |
| Shadows       | Sprites flagged `castShadows` are occluders; a polar shadow map per light built on the CPU |
| Tilemaps      | Lit without normal, shadows received                                                       |
| Editor        | Inspector, create menu (`entity.create_light2d`), hierarchy icons, viewport markers        |

The `Light2D` lights also light the isometric layer through the `_normal` and `_depth` atlases produced by
`tools/iso_atlas`, found beside an albedo atlas.

| Decision      | Choice                                                                             |
|---------------|------------------------------------------------------------------------------------|
| Texture slots | Albedo 0 to 9, normal atlas +10, depth atlas +20 (`common/isometric_light.slangh`) |
| Map position  | Solved per pixel from the view position and the height read in the depth atlas     |
| Ground tiles  | Lit per pixel too                                                                  |
| Shadows       | None in the isometric layer                                                        |
| Ambient       | The `Ambient` light gives day and night                                            |

## Draw-order sort key

Every 2D primitive carries a sort key that follows the depth convention of the [Isometric](isometric.md) layer, so 2D
and isometric layers order the same way: `depth` on each data struct of `Renderer2D`, larger is nearer; the rule is
depth first, then type (quads, circles, lines, text), then submission order. `Scene` feeds it the world z of sprites,
circles and text, the HUD pass the window rank. There is no authored layer on the components: z is the only way to
order a scene.

## Tile-graph pathfinding

User guide in [Pathfinding](../pathfinding.md). Vehicles of a Transport Tycoon-like game follow a graph of tiles and
tracks, not a navigation mesh.

- A* on a tile grid or on a graph built from tilemap overlays (tracks, roads), with per-edge costs and one-way edges
- Hierarchical search over the 32×32 chunks of the tilemap storage, so 1024² maps stay cheap
- Incremental update when a tile changes; queries spread over ticks on the task system
- Lua API and an editor overlay drawing the graph and a test path

## Mipmapped pixel-art atlases {#pixel-art-atlases}

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

## Tileset editor

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

The seeded generation of a tilemap region runs a [PCG graph](pcg-graphs.md) from a seed, undoable, from the
**Procedural** section of the tilemap editor.

## Walkable areas {#walkable-areas-2d}

Free walking on a 2D floor for point-and-click and top-down games, beside the tile graphs above
([Walkable areas](../pathfinding.md#pathfinding-walk-areas)):

- `WalkArea`: polygons with holes on the plane of its entity, cut into triangles; the shortest path across them is A*
  over the triangles, then the corridor pulled tight (funnel)
- A scale by depth: two scales at two heights of the plane, and the Z of the agents from their height
- `WalkBehind` masks: a sprite of the backdrop takes the Z of its baseline and hides the actors walking behind it, 2D
  sprites and meshes alike
- One depth per actor: a mesh actor of a depth-sorted area is drawn flat at the depth of its feet, so a mask or
  another actor hides all of it or none of it, never a leg in front and the body behind
  ([Static meshes](../renderer.md#renderer-meshes))
- `WalkAgent` walks the path, turns and drives the speed of its `Animator`; Lua `walk.*`; the navigation overlay draws
  the floors and the paths; the vertices and holes are edited in the Owl Nest viewport (select-many, move, insert,
  delete, add a hole or a floor), each gesture a `walk.*` command
