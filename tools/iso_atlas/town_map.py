"""
The compact 24x24 map of the isometric town (``iso_town_small``), in the chunked tilemap format 3.

It is the fixture of the town scenarios and of the render test, the full town being ``city_map``.

A ring road with two straight streets, a plaza, buildings on pavement plots, a hill, a raised embankment on the north
street, a shore and a railway; the same data places the buildings and trees of the scene.
"""

from __future__ import annotations

import random
import struct
from dataclasses import dataclass
from pathlib import Path

from .sample_map import CHUNK, DARK_GRASS, DIRT, FLOWERS, GRASS, PAVEMENT, RAIL, ROCK, SAND, WATER, _chunk, _fnv1a
from .terrain import EAST, NORTH, ROAD_MASKS, SLOPES, SOUTH, WEST

SIZE = 24
"""Map size in cells."""
ROAD = ROAD_MASKS[EAST | WEST]
"""Terrain id of the road along the X axis."""
ROAD_Y = ROAD_MASKS[NORTH | SOUTH]
"""Terrain id of the road along the Y axis."""
RING = (6, 8, 18, 16)
"""The ring road: first column, first row, last column, last row (cells)."""
SPURS = [(x, 12) for x in range(2, 6)] + [(11, y) for y in range(17, 20)]
"""Cells of the two short streets leaving the ring: one to the west, one to the south."""
RAIL_ROW = 21
"""Row of the railway."""
SHORE_COLUMN = 20
"""First column of sand; the water starts one column further."""


@dataclass(frozen=True)
class Plot:
    """
    One building of the town.
    """

    kind: str
    """Building name in ``buildings.json``."""
    cell: tuple[int, int]
    """Footprint cell with the smallest coordinates."""
    footprint: tuple[int, int]
    """Footprint width (X) and depth (Y) in cells."""


_FOOTPRINTS = {"cottage": (1, 1), "townhouse": (2, 1), "warehouse": (2, 2), "station": (3, 1), "tower": (1, 1)}
_PLOTS = [
    ("cottage", 7, 6),
    ("cottage", 8, 6),
    ("townhouse", 10, 6),
    ("tower", 13, 6),
    ("cottage", 15, 6),
    ("warehouse", 16, 4),
    ("cottage", 4, 9),
    ("cottage", 4, 11),
    ("townhouse", 3, 14),
    ("warehouse", 1, 16),
    ("cottage", 19, 9),
    ("townhouse", 19, 11),
    ("cottage", 19, 14),
    ("cottage", 7, 17),
    ("tower", 9, 17),
    ("cottage", 14, 17),
    ("cottage", 16, 17),
    ("townhouse", 8, 11),
    ("tower", 12, 11),
    ("cottage", 15, 11),
    ("cottage", 10, 14),
    ("station", 8, 19),
    ("warehouse", 14, 19),
]
PLOTS: list[Plot] = [Plot(kind, (x, y), _FOOTPRINTS[kind]) for kind, x, y in _PLOTS]
"""The buildings, in scene order."""

TREES: list[tuple[str, float, float]] = [
    ("oak", 1.5, 1.5),
    ("pine", 3.5, 1.5),
    ("pine", 1.5, 4.5),
    ("oak", 4.5, 3.5),
    ("pine", 8.5, 2.5),
    ("oak", 11.5, 2.5),
    ("pine", 14.5, 1.5),
    ("oak", 18.5, 2.5),
    ("pine", 2.5, 8.5),
    ("oak", 1.5, 11.5),
    ("pine", 0.5, 13.5),
    ("oak", 2.5, 19.5),
    ("pine", 5.5, 17.5),
    ("oak", 4.5, 22.5),
    ("pine", 12.5, 22.5),
    ("oak", 18.5, 22.5),
    ("pine", 17.5, 19.5),
    ("oak", 18.5, 18.5),
    ("pine", 19.5, 6.5),
    ("oak", 19.5, 4.5),
    ("pine", 14.5, 15.5),
]
"""Trees (kind, x, y) standing on grass."""


def corner_heights() -> list[int]:
    """
    Corner grid of the town: a hill in the north corner and a one-step embankment under the north street.

    :return: The ``(SIZE + 1)^2`` corner heights, row by row, neighbours at most one step apart.
    """
    heights = [[0] * (SIZE + 1) for _ in range(SIZE + 1)]
    for y in range(SIZE + 1):
        for x in range(SIZE + 1):
            heights[y][x] = max(0, 3 - max(x, y) // 2)
    for y in (RING[1], RING[1] + 1):
        for x in range(10, 16):
            heights[y][x] = 1
    return [h for row in heights for h in row]


def cell_corners(heights: list[int], x: int, y: int) -> tuple[int, int, int, int]:
    """
    Corner heights of a cell: back, right, front, left.

    :param heights: The corner grid.
    :param x: Cell column.
    :param y: Cell row.
    :return: The four heights.
    """
    row = SIZE + 1
    return (
        heights[y * row + x],
        heights[y * row + x + 1],
        heights[(y + 1) * row + x + 1],
        heights[(y + 1) * row + x],
    )


def slope_of(heights: list[int], x: int, y: int) -> int:
    """
    Slope index of a cell.

    :param heights: The corner grid.
    :param x: Cell column.
    :param y: Cell row.
    :return: The index in ``SLOPES``.
    """
    corners = cell_corners(heights, x, y)
    lowest = min(corners)
    return SLOPES.index((corners[0] - lowest, corners[1] - lowest, corners[2] - lowest, corners[3] - lowest))


def ring_cells() -> list[tuple[int, int]]:
    """
    Cells of the ring road, clockwise on the screen from its first corner.

    :return: The cells.
    """
    x0, y0, x1, y1 = RING
    cells = [(x, y0) for x in range(x0, x1 + 1)]
    cells += [(x1, y) for y in range(y0 + 1, y1 + 1)]
    cells += [(x, y1) for x in range(x1 - 1, x0 - 1, -1)]
    cells += [(x0, y) for y in range(y1 - 1, y0, -1)]
    return cells


def road_mask(cells: set[tuple[int, int]], x: int, y: int) -> int:
    """
    Neighbour mask of a road cell.

    :param cells: Every road cell.
    :param x: Cell column.
    :param y: Cell row.
    :return: The OR of ``NORTH`` (-y), ``EAST`` (+x), ``SOUTH`` (+y) and ``WEST`` (-x) for the neighbours in ``cells``.
    """
    return (
        (NORTH if (x, y - 1) in cells else 0)
        | (EAST if (x + 1, y) in cells else 0)
        | (SOUTH if (x, y + 1) in cells else 0)
        | (WEST if (x - 1, y) in cells else 0)
    )


def road_tiles() -> dict[tuple[int, int], int]:
    """
    The road overlay: the ring and the two streets, each cell tiled from the mask of its road neighbours.

    :return: The overlay tile of each road cell.
    """
    cells = set(ring_cells()) | set(SPURS)
    return {(x, y): ROAD_MASKS[road_mask(cells, x, y)] for x, y in cells}


def plot_cells(plot: Plot) -> list[tuple[int, int]]:
    """
    Cells under a building.

    :param plot: The building.
    :return: Its footprint cells.
    """
    return [(plot.cell[0] + i, plot.cell[1] + j) for j in range(plot.footprint[1]) for i in range(plot.footprint[0])]


def map_bytes(seed: int) -> bytes:
    """
    Build the ``iso_town_small`` tilemap file.

    :param seed: Seed of the run, shuffling the grass variants.
    :return: The file content.
    """
    heights = corner_heights()
    rng = random.Random(77 + seed)
    ground: dict[tuple[int, int], int] = {}
    for y in range(SIZE):
        for x in range(SIZE):
            tile = GRASS if rng.random() < 0.72 else rng.choice([DARK_GRASS, FLOWERS])
            top = max(cell_corners(heights, x, y))
            if x > SHORE_COLUMN:
                tile = WATER
            elif x == SHORE_COLUMN:
                tile = SAND
            elif top >= 3:
                tile = ROCK
            elif top == 2 and slope_of(heights, x, y) != 0:
                tile = DIRT
            ground[(x, y)] = tile
    for x in range(SIZE):
        ground[(x, RAIL_ROW)] = GRASS
    for x in range(8, 17):
        for y in range(10, 15):
            ground[(x, y)] = PAVEMENT
    for plot in PLOTS:
        for cell in plot_cells(plot):
            ground[cell] = PAVEMENT
    overlay: dict[tuple[int, int], int] = road_tiles()
    overlay.update({(x, RAIL_ROW): RAIL for x in range(SHORE_COLUMN)})
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for terrain_cells, overlay_cells in ((ground, {}), ({}, overlay)):
        body += struct.pack("<I", 1) + struct.pack("<II", 0, 0) + _chunk(terrain_cells, overlay_cells, {})
    body += b"\x01" + bytes(heights)
    payload = body + struct.pack("<I", _fnv1a(body))
    header = (
        "Tilemap: iso_town_small\nFormatVersion: 3\ntilesetPath: tilesets/iso_ground.owltileset\n"
        f"width: {SIZE}\nheight: {SIZE}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\n"
        f"payloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def write_town_map(project: Path, seed: int) -> Path:
    """
    Write the compact town map.

    :param project: The project folder.
    :param seed: Seed of the run.
    :return: The written file.
    """
    path = project / "tilemaps" / "iso_town_small.owltilemap"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(map_bytes(seed))
    return path
