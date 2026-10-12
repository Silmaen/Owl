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

### Draw order

- Depth test with alpha cutout: every sprite writes its isometric depth (diagonal, height, sub-layer), so opaque and
  cut-out sprites — the whole Transport Tycoon look — draw without sorting. Only translucent sprites (smoke, glass,
  previews) are sorted back to front on the CPU.
- Terrain chunks have a static order and are never sorted per frame.
- Pre-rendered sprites are billboards anchored at their world foot point; they are never pushed through the dimetric
  matrix a second time.
- Objects wider than a tile are cut into one slice per footprint tile, as Transport Tycoon does; each slice carries a
  3D box that gives its depth, which avoids the sorting artefacts of long vehicles and tall buildings.
- Moving sprites use an anchor-plus-size billboard instance without a per-sprite matrix, so thousands of vehicles stay
  on the instanced path.

### Terrain

- Heights belong to the map: `TilemapAsset` holds a `(W+1)×(H+1)` grid of corner heights; a cell's slope is derived
  from its four corners — 15 regular slopes plus 4 steep ones, 19 in all — so terraforming is a corner edit.
- `Tileset::kind` (`Orthogonal`, `Isometric`); an isometric tileset maps (terrain type, slope) to a tile index and
  holds foundation sprites, drawn under objects standing on a slope.
- Track and road overlays are tilemap layers whose tile follows the slope of the cell under them; they draw right
  after the ground of the same cell.
- Map storage is the chunked v2 format of TM2 (structure of arrays, binary chunks), up to 1024² in v0.4.0; 4096² maps
  wait for the v0.7.0 content pipeline on the same storage interface.

### Picking

- Tiles: for each candidate cell under the cursor, front to back, the screen ray is intersected with the two triangles
  of the cell's corner heights; pure CPU, tested headless on the 19 slopes.
- Entities keep the entity-id framebuffer picking.

### Editor

- Isometric viewport mode when the scene's stack holds an isometric layer: dimetric projection locked in Edit,
  world-axis cursor, tile highlight; the gizmo receives the layer's view and projection.
- `TilemapDocument`: isometric preview when the tileset is `kind: isometric`, raise / lower / level corner tools,
  overlay brush; `TilesetDocument`: kind and (terrain, slope) table. Every edit is undoable.

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

## Later

- v0.5.0: normal-mapped isometric sprites under the 2D lights, from the generator's normal atlas; day / night as an
  ambient tint; tile-graph pathfinding for vehicles on the chunked map.
- v0.6.0: 3D meshes drawn under the same camera into the same depth buffer; shadows from the depth atlas or from
  shadow maps.
- v0.7.0: 4096² maps from the content pipeline; procedural terrain heights from the [PCG graphs](pcg-graphs.md).
