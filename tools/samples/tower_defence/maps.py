"""
The maps of the Tower defence sample game, as data: the road (corners from the gate to the keep), the build pads, the
keep, the decor and the waves. ``generate_maps`` turns each one into a tilemap and a scene; a new map is a new entry of
``MAPS`` (or a scene duplicated and edited in Owl Nest).

A wave is a list of groups ``"kind count gap"`` separated by ``;``: ``count`` enemies of ``kind`` (``runner``,
``soldier``, ``armoured``) one every ``gap`` seconds, the groups one after the other.
"""

from __future__ import annotations

from dataclasses import dataclass, field

Cell = tuple[int, int]


@dataclass(frozen=True)
class MapData:
    """One map of the game."""

    number: int
    """Number of the map, from 1 (``map_01``)."""
    name: str
    """Name shown on the title and the HUD."""
    size: int
    """Side of the square grid, in cells."""
    road: list[Cell]
    """Corners of the road, from the gate cell (on the edge) to the cell beside the keep."""
    pads: list[Cell]
    """Build pads, each on a free cell beside the road."""
    keep: Cell
    """Back cell of the 2 x 2 keep."""
    waves: list[str]
    """The waves, in order."""
    start_gold: int = 220
    """Gold at the start of the map."""
    lives: int = 20
    """Lives at the start: one is lost per enemy reaching the keep."""
    seed: int = 1
    """Seed of the grass variants and of the decor."""
    trees: int = 34
    """Trees and rocks scattered on the free cells."""
    camera: tuple[float, float] = field(default=(12.0, 12.0))
    """Cell the camera looks at when the map starts."""


def line(a: Cell, b: Cell) -> list[Cell]:
    """The cells of a straight segment along an axis, both ends included."""
    (x0, y0), (x1, y1) = a, b
    if x0 != x1 and y0 != y1:
        raise ValueError(f"segment {a} -> {b} is not along an axis")
    step_x = (x1 > x0) - (x1 < x0)
    step_y = (y1 > y0) - (y1 < y0)
    count = max(abs(x1 - x0), abs(y1 - y0))
    return [(x0 + step_x * i, y0 + step_y * i) for i in range(count + 1)]


def road_cells(data: MapData) -> list[Cell]:
    """Every cell of the road, in walking order, without repeats."""
    cells: list[Cell] = []
    for a, b in zip(data.road, data.road[1:], strict=False):
        for cell in line(a, b):
            if not cells or cells[-1] != cell:
                cells.append(cell)
    return cells


MAPS = [
    MapData(
        number=1,
        name="Green Ford",
        size=24,
        road=[(0, 20), (5, 20), (5, 14), (11, 14), (11, 19), (17, 19), (17, 9), (10, 9), (10, 4), (19, 4)],
        pads=[
            (4, 18),
            (6, 17),
            (7, 15),
            (9, 12),
            (12, 16),
            (10, 17),
            (13, 20),
            (16, 17),
            (18, 15),
            (15, 11),
            (18, 10),
            (12, 10),
            (11, 7),
            (9, 6),
            (13, 5),
            (16, 3),
        ],
        keep=(20, 3),
        waves=[
            "runner 6 1.0",
            "runner 8 0.8; soldier 2 1.5",
            "soldier 6 1.2",
            "runner 10 0.6; soldier 4 1.2",
            "armoured 3 2.0; runner 6 0.7",
            "soldier 8 1.0; armoured 2 2.0",
            "runner 14 0.5; armoured 4 1.8",
            "soldier 10 0.9; armoured 6 1.6; runner 10 0.5",
        ],
    ),
]
"""Every map, in playing order."""
