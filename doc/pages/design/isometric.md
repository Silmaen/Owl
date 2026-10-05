# Isometric renderer {#page-design-isometric}

[TOC]

Design page for the v0.4.0 isometric renderer, summarised in the [Roadmap](../roadmap.md).

## Goal

Add a third non-2D rendering mode — an isometric pseudo-3D renderer in the **Transport Tycoon Deluxe** tradition —
slotted between the existing 2D / raycast / voxel options and mixable with them through the renderer stack.

A first step exists on the unmerged `Feature/KickoffIsometricRenderer` branch (layer skeleton, projection helpers,
tests). It is rebased onto the v0.3.0 phased systems so the renderer lives outside `Scene.cpp` (A-02).

## Specification

- Add `RendererIsometricLayer` (factory key `"RendererIsometric"`) so scenes can mix the isometric mode with the
  existing 2D / raycast / voxel stack just by listing it in `owl_project.yml` and tagging entities with the matching
  `RendererTag`.
- **Pseudo-3D presentation** in the Transport Tycoon Deluxe style: a fixed 2:1 dimetric projection (no free-look
  camera), pre-rendered sprite tiles drawn back-to-front by world-space Y then Z. World axes map to screen as
  `screen.x = (worldX − worldY) · (tileW / 2)` and `screen.y = (worldX + worldY) · (tileH / 2) − worldZ · zStep`.
  Tile sprites are 64×32 px by default (configurable on the layer's `DefaultConfig`).
- **Heightmap-aware tilemap**: extend `scene::Tileset` with optional per-tile slope/ramp metadata (flat,
  N/S/E/W/NE/NW/SE/SW slope) and a `cornerHeights` quad. The renderer composites the corresponding ramp / cliff
  sprite variant so the world has gentle slopes like TTD without modelling actual 3D meshes.
- **Multi-Z stacking**: entities sort first by their projected Y (depth), then by world Z (elevation) so buildings
  stack cleanly on top of terrain tiles and over each other. The painter's order keeps the Renderer2D batching path;
  no new GPU pipeline required for this cut.
- **Editor support**: dedicated isometric viewport mode (locked to the dimetric projection, world-axis cursor + tile
  highlight), `TilemapDocument` gains an isometric preview when the target tileset is flagged `kind: isometric`, and
  gizmos use the isometric basis so click-drag of an entity feels native instead of zooming around in cartesian XY.
- **Demo scene** in `sample_project/scenes/`: a small TTD-style town with a couple of buildings, ramps connecting two
  height plateaus, and a player entity that walks along the grid using `world_player.lua` (z-aware variant).
- **Tests**: layer factory registration, scene round-trip with
  `EnabledRenderers: [{ Name: iso, Type: RendererIsometric }]`, the projection helpers (`worldToScreen` /
  `screenToWorld`), the depth-sort comparator, and an image test on the headless backends.

## Later

Isometric terrain heights are one of the consumers of the v0.7.0 procedural graphs, see [PCG graphs](pcg-graphs.md).
