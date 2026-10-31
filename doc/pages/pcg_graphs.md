# PCG Graphs {#page-pcg-graphs}

[TOC]

A **PCG graph** (`.owlpcg`) generates content procedurally: noise, rules and scattered points flow through nodes down to
outputs that write tiles. The graph is data, edited on the [node canvas](node_graph.md) of Owl Nest; a tilemap runs
it from a seed, so the same seed always gives the same tiles. Design notes and what is planned:
[PCG graphs](design/pcg-graphs.md).

## What a graph is

Nodes carry two kinds of value over the cells of the region being generated, and a link joins pins of the same kind
only:

| Pin type   | Content                                               |
|------------|-------------------------------------------------------|
| **Field**  | One number per cell; a **mask** is a field of 0 and 1 |
| **Points** | A list of cells (trees, rocks, chests)                |

Evaluation starts from the output nodes and pulls the nodes they depend on:

- The result is deterministic: every random stream (SplitMix64) comes from the seed of the generation and the identifier
  of its node, so two nodes of the same settings still differ.
- Outputs write from the top one on the canvas down: **a lower output wins** on a cell both write.
- A cell no output writes keeps its tile (`g_UntouchedTile` in the engine); **Erase Tiles** and tile `-1` empty a cell.
- A graph whose nodes depend on themselves fails with `Cycle`, a region without cell with `EmptyRegion`. An unlinked
  input or an unknown node type is a warning, the rest still evaluates.

## Nodes

| Node (`id`)                                 | Inputs        | Output | Parameters (default)                                                |
|---------------------------------------------|---------------|--------|---------------------------------------------------------------------|
| **Noise** (`source.noise`)                  | none          | Field  | `scale` 8, `octaves` 3, `persistence` 0.5, `seed` 0                 |
| **Random** (`source.random`)                | none          | Field  | `seed` 0                                                            |
| **Constant** (`source.constant`)            | none          | Field  | `value` 1                                                           |
| **Gradient** (`source.gradient`)            | none          | Field  | `direction`: `horizontal`, `vertical`, `radial`                     |
| **Threshold** (`field.threshold`)           | Field         | Mask   | `min` 0.5, `max` 1                                                  |
| **Combine** (`field.combine`)               | A, B          | Field  | `operation`: `add`, `subtract`, `multiply`, `min`, `max`, `average` |
| **Invert** (`field.invert`)                 | Field         | Field  | none                                                                |
| **Blur** (`field.blur`)                     | Field         | Field  | `radius` 1                                                          |
| **Cellular Automaton** (`field.cellular`)   | Mask          | Mask   | `iterations` 4, `birth` 5, `survival` 4, `edges`: `solid`, `open`   |
| **Edge** (`field.edge`)                     | Mask          | Mask   | `neighbours`: `8`, `4`                                              |
| **Scatter** (`points.scatter`)              | Mask          | Points | `density` 0.1, `spacing` 1, `seed` 0                                |
| **Filter Points** (`points.filter`)         | Points, Field | Points | `min` 0.5, `max` 1                                                  |
| **Points to Mask** (`points.to_mask`)       | Points        | Mask   | `radius` 0                                                          |
| **Write Tiles** (`output.tiles`)            | Mask          | none   | `tile` 0                                                            |
| **Write Tiles at Points** (`output.points`) | Points        | none   | `tile` 0                                                            |
| **Erase Tiles** (`output.erase`)            | Mask          | none   | none                                                                |
| **Write Heights** (`output.height`)         | Field         | none   | none                                                                |

Sources and `Combine` (`Field`) give fields; `Threshold` turns a field into a mask between `min` and `max`;
`Cellular Automaton` smooths a random mask into caves; `Scatter` picks cells of a mask apart from each other by
`spacing`; `Points to Mask` and the two point outputs bring points back onto tiles. `Write Heights` writes no tiles: its
field (clamped to 0..1, the lowest such node on the canvas wins) is the height map of a [Planet](planets.md).

## Editing a graph

Create one with **New PCG Graph** in the Content Browser context menu (a starter graph: noise, threshold, tiles), then
open it.

- The toolbar of the tab frames every node (**Home**) and switches the view: **Graph** (the nodes), **Split** (the
  nodes beside the tiles the whole graph writes) or **Output** (those tiles alone); one colour per tile, black for an
  erased cell. Its badge reports the evaluation: the size generated, the warnings (an unlinked input) or a node
  depending on itself.
- The **Nodes** panel sets the preview (width, height and seed) and holds the node library: folded categories, each
  node with its glyph, and a search field. Drag a node onto the canvas to add it there; the canvas context menu adds
  one at the mouse too.
- Drag from a pin to link; incompatible pins refuse the link. **Delete** removes the selected nodes and their links.
- Right-click a link for its menu (**Add Waypoint**, **Delete Link**); drag a waypoint to steer the link, **Delete**
  over it removes it. The links keep the colour of their type. The magnet of the toolbar snaps nodes and waypoints to
  the grid.
- **Alt**+click a link cuts it.
- The **Graph** panel edits the parameters of the selected node and previews its output: a field in grey, points in
  white.
- Every edit is one undo step.

## Generating a tilemap

A tilemap holds a **generator**: the graph, a seed, the layer written, the origin and the size of the region (a zero
side reaches the end of the grid). In the tilemap editor, the **Procedural** section of the Scene Hierarchy column:

| Control                   | Effect                                                        |
|---------------------------|---------------------------------------------------------------|
| **Add Generator**         | Gives the tilemap a generator                                 |
| Graph drop field          | The `.owlpcg` to run, dropped from the Content Browser        |
| Seed, Layer, Origin, Size | The seed and the region written                               |
| **Preview**               | Draws the result over the canvas without touching the tilemap |
| **Generate**              | Writes the tiles into the layer                               |
| **New Seed**              | Picks another seed                                            |
| **Open**                  | Opens the graph                                               |
| **Remove**                | Removes the generator                                         |

Each button is one undo step. The Engine Demo ships `pcg/caves.owlpcg` (a cave automaton) and the tilemap
`tilemaps/pcg_caves.owltilemap` that uses it.

## File format

A `.owlpcg` is YAML, versioned like the other assets. A node's `values` hold its parameters as text; a missing one takes its default:

```yaml
FormatVersion: 1
PcgGraph:
  nodes:
    - id: 1001
      type: source.random
      position: [0, 0]
      values:
        seed: 0
    - id: 1002
      type: field.threshold
      position: [220, 0]
      values:
        min: 0.55
  links:
    - {from: 1001, fromPin: Field, to: 1002, toPin: Field}
```

The generator of a tilemap is the `generator` key of its header:

```yaml
generator:
  graph: pcg/caves.owlpcg
  seed: 7
  layer: 0
  origin: [0, 0]
  size: [0, 0]
```

The engine API is `scene::pcg` (`scene/pcg/PcgGraph.h`: `evaluate`, `loadFromFile`; `scene/pcg/PcgTilemap.h`:
`generateTilemap`).
