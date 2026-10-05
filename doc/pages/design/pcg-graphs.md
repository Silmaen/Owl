# Procedural generation graphs {#page-design-pcg-graphs}

[TOC]

Design page for the procedural content generation (PCG) graphs, summarised in the [Roadmap](../roadmap.md).

## v0.5.0 — first brick

A seeded noise / rule fill of a tilemap region, undoable, if it serves the tilemap workflow (see
[2D complete](2d-complete.md)).

## v0.7.0 — PCG graphs

- Non-destructive graph on `NodeCanvas`: noise → thresholds → point scattering → filters → placement of prefabs, tiles
  or voxels
- Live preview in the viewport, regeneration from a seed
- Consumers: voxel biomes and caves, tilemap dungeons, isometric terrain heights, set dressing
- Output is regular scene content (entities, tiles, voxels), so the generated result can be baked and edited by hand
