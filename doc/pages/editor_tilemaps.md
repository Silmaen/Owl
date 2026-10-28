# Tilemaps and Tilesets {#page-editor-tilemaps}

[TOC]

A **tileset** (`.owltileset`) cuts a texture into tiles; a **tilemap** (`.owltilemap`) is a grid of layers painted
with the tiles of one tileset. A scene shows a tilemap through a Tilemap component; **Open in Tilemap Editor** in the
Scene Hierarchy menu opens its file.

## Tilemap editor

![The isometric tilemap of the Iso Town in the tilemap editor, with its layers and the Tile Palette](../images/nest/tilemap_editor.png)

- The left button paints the tile picked in the **Tile Palette** into the active layer, the right button erases.
- The Scene Hierarchy column holds the tileset, the grid (cell size, rows, columns) and the layers: add, rename,
  move up or down, show or hide, delete, and their parallax.
- Properties shows the clicked cell: its tile, its overlay and its flags (passable...).
- A stroke or a cell edit is one undo step. Resizing the grid keeps the cells: shrinking then growing back loses
  nothing.

### Isometric tilemaps

With an isometric tileset the canvas is an isometric preview (middle drag pans, wheel zooms) with slopes, foundations
and overlays, and a toolbar picks the tool:

| Tool        | Effect                                                                                                    |
|-------------|-----------------------------------------------------------------------------------------------------------|
| **Paint**   | Tile Palette brushes on the cell under the cursor, picked on the sloped surface                           |
| **Raise**   | Raise the nearest corner, or an area of 1 to 16 corners, by one step (right button: lower)                |
| **Lower**   | The opposite of Raise                                                                                     |
| **Level**   | Bring the area to the level height (Ctrl + click reads a corner)                                          |
| **Overlay** | Paint the overlay (tracks, roads) of the active layer; right button clears                                |
| **Node**    | Select corners (Shift adds or removes), drag up / down or **+** / **-** to move them one step per `zStep` |

Neighbouring corners follow, so every cell keeps one of the 19 slopes; a click, a drag or a key press is one undo
step. The cell properties show the slope and edit the four corner heights with the same rule. A flat tilemap has no
heights and no Node tool.

### Procedural generation

A tilemap can carry a **generator**: a [PCG graph](pcg_graphs.md), a seed, a layer and a region. The **Procedural**
section of the Scene Hierarchy column sets it, previews it on the canvas and writes its tiles into the layer.

## Tileset editor

- **Atlas**: the texture, the grid, the tile size, the filter and the **Kind**: `Orthogonal` square tiles or
  `Isometric` 2:1 diamonds. Each map (texture, normal map, roughness map, opacity map) is a row showing its
  thumbnail, file and size; drop an image on the thumbnail. A voxel tileset takes its normal and roughness maps here;
  the **Opacity Map** cuts any tileset out (its red channel multiplies the texture alpha). The filter is `Linear`,
  `Nearest` or `Pixel Art` (`filterMode: PixelArt`: sharp up close, mipmaps built per tile far away, see
  [Renderer](renderer.md)).
- Click a tile to edit it in Properties: its flags and, for a road network, its **Navigation** (**Road**, then the
  **Joined Sides**: four arrows as on screen; see [Pathfinding](pathfinding.md)).
- An isometric tileset adds **Slopes**: one folded row per terrain or overlay (with the name of its flat tile); open
  it for the tile of each of the 19 slopes (`-1` uses the flat tile) and, under **Foundations**, its foundations.
  **Delete Terrain** sits in the row and in its context menu.

### Composing the atlas

The atlas is built in the editor from source images; each gesture below is one undo step.

| Gesture                                                 | Effect                                                                                      |
|---------------------------------------------------------|---------------------------------------------------------------------------------------------|
| Drop an image from the Content Browser                  | The image, scaled to the cell, replaces the slot under the cursor                           |
| Drop a folder                                           | Its images sorted by name fill the slot, then the next empty ones; rows are added when full |
| Right-click: **Insert Image...** / **Insert Folder...** | The same, from a file dialog                                                                |
| Right-click: **Clear Image**, or **Delete**             | The pixels become transparent and the tile metadata is reset; the index stays               |
| **Select All** (Ctrl + A), Ctrl / Shift click           | Select several slots                                                                        |
| Drag the selection onto another slot                    | The pixels and the metadata move; an occupied slot swaps                                    |
| Grid **+** / **-** buttons                              | Resize the atlas; each cell keeps its column and row                                        |

Growing the atlas only adds rows, so the indices of the existing tiles, and the tilemaps that use them, do not move.
The atlas PNG is written when the tileset is saved; a tileset without a texture creates `<name>.png` beside its file
(save it first). While composed pixels are unsaved the texture and the tile size are locked.

Every edit is one undo step. A saved tileset or tilemap reloads in the scenes that use it, even while they play.
