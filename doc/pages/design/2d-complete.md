# 2D complete {#page-design-2d-complete}

[TOC]

Design page for the v0.5.0 release, summarised in the [Roadmap](../roadmap.md). The 2D physics part has its own page,
[Physics API](physics-api.md); the editor part is in [Owl Nest UI](nest-ui.md).

## 2D lighting system

- Point lights, spotlights in 2D scenes
- Normal-mapped sprites for dynamic 2D lighting
- Shadow casting from 2D occluders

## Text rendering quality — aspect-correct glyphs

- Today text drawn inside a `UiText` / `UiRect` is scaled to fill its rectangle, so a square-ish box stretches or
  squashes the glyphs (characters look horizontally compressed or vertically elongated).
- Render glyphs at their **native aspect ratio**: advance the pen by each glyph's own metrics and use a single uniform
  scale (px-per-em) rather than independent X/Y scales derived from the rect. The rectangle should govern layout (wrap
  width, line height, alignment), not per-axis glyph distortion.
- Add text **fit / alignment** controls: horizontal (left / center / right) and vertical (top / middle / bottom)
  alignment, optional word-wrap, and a fit mode (clip / shrink-to-fit / overflow) — so a glyph keeps its shape and the
  box only decides where it sits and when it wraps.
- Applies to both the screen-space HUD path and world-space `Renderer2D::drawString`.

## Dedicated HUD layer, decoupled from the world renderer

- Today the sample project ships a two-layer stack `[Renderer2D(world), Renderer2D(ui)]` and entities are tagged `ui`
  by hand — fine, but authoring the HUD still happens in the same top-down viewport as the world, with no preview of
  how it will look stretched over a raycast scene
- Promote UI authoring to a **dedicated HUD layer** in the renderer stack that always renders on top, in pixel-space,
  regardless of what the layers underneath draw (raycast, voxel, isometric, 2D world, …)
- **HUD Editor mode** in Owl Nest: a viewport variant that shows the HUD over a configurable backdrop (solid colour, a
  snapshot of the target gameplay scene, or live preview) and snaps to screen-space coordinates by default. Drag-drop
  sprites / text / panels onto the HUD; the existing `Ui*` components are reused
- The HUD becomes a scene-level asset (`.owlhud` or a dedicated renderer-stack entry) referenced by gameplay scenes
  instead of being embedded as one renderer in each scene's stack — so the same HUD can ride on top of a raycast scene,
  a voxel scene, or a 2D scene without duplication

## Tileset editor — compose atlas by inserting / removing source images

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

## First procedural brick for tilemaps

A seeded noise / rule fill of a tilemap region, undoable — the first consumer and the seed of the v0.7.0
[PCG graphs](pcg-graphs.md).

## Sample project demos (moved out of the engine)

- Inventory demo — collectible objects and key-locked switches, written in Lua (later in visual scripting) in
  `sample_project/`, not as an engine system (A-02, C-14)
- Enemies demo — patrol / chase enemies in Lua in `sample_project/`; engine-side AI arrives with v0.8.0
