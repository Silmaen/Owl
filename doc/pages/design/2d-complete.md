# 2D complete {#page-design-2d-complete}

[TOC]

Design page of the 2D work once planned as v0.5.0; the 2026-10-16 replan spread it over the releases, see the
[Roadmap](../roadmap.md): the draw-order sort key, scroll views and lists, and tile-graph pathfinding in v0.4.0; 2D
lights, the minimap and "look through scene camera" in v0.5.0; the tileset editor and the procedural brick in v0.6.0;
full 2D physics and the enemies demo in v0.7.0. The 2D physics part has its own page,
[Physics API](physics-api.md); the editor part is in [Owl Nest UI](nest-ui.md).

## 2D lighting system (v0.5.0)

- Point lights, spotlights in 2D scenes
- Normal-mapped sprites for dynamic 2D lighting
- Shadow casting from 2D occluders

## Text and HUD

Aspect-correct text with alignment and fit modes, and the dedicated HUD layer with its editor, moved to v0.4.0:
see [HUD system](hud-system.md). Built on them:

- v0.4.0: scroll views with clipping and virtualised lists (vehicle lists of a management game), done in HUD7
- v0.5.0: tables (columns and sortable headers) over the virtualised list
- v0.5.0: minimap of a tilemap on a render target, shown in a `UiImage`
- v0.5.0: live scene backdrop in the HUD editor, with "look through scene camera"

## Draw-order sort key (B-10, v0.4.0, done)

A sort key on every 2D primitive instead of the primitive type and the storage order; it follows the depth convention
of the v0.4.0 isometric layer ([Isometric](isometric.md)) so 2D and isometric layers order the same way: `depth` on each
data struct of `Renderer2D`, larger is nearer; the rule is depth first, then type (quads, circles, lines, text), then
submission order. `Scene` feeds it the world z of sprites,
circles and text, the HUD pass the window rank. An authored layer on the components (a field in the inspector) is not
there yet: z is the only way to order a scene.

## 2D lighting on isometric sprites (v0.5.0)

The 2D lights also light isometric sprites through the normal and depth atlases produced by the v0.4.0 generator; a
global ambient tint gives day / night.

## Tile-graph pathfinding (v0.4.0)

Moved from the v0.8.0 navigation work: vehicles of a Transport Tycoon-like game follow a graph of tiles and tracks,
not a navigation mesh.

- A* on a tile grid or on a graph built from tilemap overlays (tracks, roads), with per-edge costs and one-way edges
- Hierarchical search over the 32×32 chunks of the v0.4.0 tilemap storage, so 1024² maps stay cheap
- Incremental update when a tile changes; queries spread over ticks on the task system
- Lua API and an editor overlay drawing the graph and a test path

## Tileset editor (v0.6.0) — compose atlas by inserting / removing source images

- Today `TilesetDocument` only edits per-tile metadata (name, collidable, wallHeight, transparent). The atlas itself is
  authored offline — drop a PNG, edit metadata. There's no way to **build** or **mutate** an atlas from inside the
  editor.
- Add tile-slot mutation in the document: drop an image onto an empty (or existing) slot to insert/replace it;
  right-click a slot to clear it. The underlying atlas PNG is rebuilt on disk (or kept in memory until the asset is
  saved).
- Bulk insert: select multiple PNGs from the Content Browser and drop them onto the tileset grid — fill consecutive
  empty slots, growing the grid if needed (configurable: extend columns vs add rows).
- "Remove" doesn't delete the slot index (would shift every downstream tilemap's tile indices); it just clears the
  pixels to fully transparent and clears the meta, so existing tilemaps that referenced that index render an empty
  cell instead of mis-pointing at the next tile.
- Asset-pipeline hook: when the atlas changes the tilemap previews, door/pushwall thumbnails, and any open
  `TilemapDocument` reload automatically (the tileset is shared via the per-scene cache, so a single invalidation
  propagates).
- Undo/redo: every insert / replace / remove pushes a single `ModifyTilesetCommand` (mirrors `ModifyEntityCommand`
  semantics) with the standard 1 s merge-coalescing for rapid drag chains.

## First procedural brick for tilemaps (v0.6.0)

A seeded noise / rule fill of a tilemap region, undoable — the first consumer and the seed of the v0.6.0
[PCG graphs](pcg-graphs.md).

## Sample project demos (moved out of the engine)

- Inventory demo — moved to v0.4.0 as the showcase of the new HUD widgets ([HUD system](hud-system.md))
- Enemies demo (v0.7.0) — patrol / chase enemies in Lua in `sample_project/`; engine-side AI arrives with v0.7.0
