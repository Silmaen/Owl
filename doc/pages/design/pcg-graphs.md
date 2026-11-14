# Procedural generation graphs {#page-design-pcg-graphs}

[TOC]

Design of the procedural content generation (PCG) graphs. The user guide is [PCG graphs](../pcg_graphs.md).

## Graphs

A `.owlpcg` graph, edited on the `NodeCanvas` of Owl Nest, fills a region of a tilemap from a seed.

| Part        | Content                                                                                                                  |
|-------------|--------------------------------------------------------------------------------------------------------------------------|
| Engine      | `scene::pcg` (`PcgGraph.h`, `PcgTilemap.h`): versioned YAML, 17 node types, `evaluate`, `generateTilemap`                |
| Pin types   | `Field` (a number per cell, a mask is 0 or 1) and `Points` (cells); a link joins the same type only                      |
| Determinism | Every random stream (SplitMix64) comes from the seed and the node identifier: same seed, same tiles                      |
| Outputs     | Applied from the top one on the canvas down, so a lower one wins; `g_UntouchedTile` keeps a cell, `-1` erases            |
| Diagnostics | Errors `Cycle` and `EmptyRegion`; warnings for unlinked inputs and unknown node types                                    |
| Heights     | `output.height` (**Write Heights**) fills `pcg::Result::height`, the height map of a `Planet` ([Planets](../planets.md)) |
| Tilemap     | `TilemapAsset::generator` (graph, seed, layer, origin, size, a zero side reaches the grid end), YAML `generator`         |
| Editor      | `PcgGraphDocument`: Nodes panel, Graph panel with the preview of the selected node, undo; tilemap **Procedural** section |
| Engine Demo | `engine_demo/pcg/caves.owlpcg` and `engine_demo/tilemaps/pcg_caves.owltilemap` (a cave automaton)                        |

The graph is non-destructive and the output is regular tile content, so a generated tilemap can be edited by hand
afterwards. The preview is drawn on the canvas of the tilemap document.

There are no prefab or voxel outputs, no isometric terrain height output (the planet height output exists) and no
live preview of the generation in the scene viewport.
