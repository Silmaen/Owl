"""
The 64x64 map of the isometric town demo (``iso_town``), in the chunked tilemap format 3.

A street grid over five districts: hills with cottages in the north-west, a park with a pond in the north-east, a river
running down to the sea with bridges over it, a town centre around a paved plaza, and the industrial and harbour
districts in the south with the railway between them. The same data places the buildings, the trees, the chimneys, the
lamps and the vehicles of the scene, and lists the navigation exceptions (one-way streets, slow roads).
"""

from __future__ import annotations

import math
import random
import struct
from dataclasses import dataclass, field
from pathlib import Path

from .sample_map import CHUNK, DARK_GRASS, DIRT, FLOWERS, GRASS, PAVEMENT, RAIL, ROCK, SAND, WATER, _chunk, _fnv1a
from .terrain import EAST, NORTH, ROAD_MASKS, SLOPES, SOUTH, WEST

SIZE = 64
"""Map size in cells."""
RAIL_ROW = 47
"""Row of the railway, which ends at the harbour."""
SEA_COLUMN = 55
"""First column of open sea; the column before is the beach."""
STEP_Z = 0.5
"""World height of one corner step."""

HILLS = [(10.0, 14.0, 4.4, 10.0), (47.0, 5.0, 2.6, 5.0), (4.0, 60.0, 2.4, 5.0)]
"""Gaussian hills: centre x, centre y, peak (corner steps) and spread (cells)."""
RIVER = [(25, 0, 27, 16), (25, 14, 54, 16)]
"""Rectangles of water (first column, first row, last column, last row): the river and its bend to the sea."""
POND = (36.0, 6.0, 3.2)
"""Pond of the park: centre x, centre y and radius, in cells."""
PIER = (54, 37, 60, 40)
"""Paved pier into the sea (first column, first row, last column, last row)."""
PLAZA = (33, 21, 43, 31)
"""Paved plaza of the town centre."""
QUAY = (45, 33, 51, 43)
"""Paved quay of the harbour."""

ONE_WAYS = [
    # (first cell, last cell): the street is one way from the first cell to the last one.
    ((20, 32), (44, 32)),
    ((32, 20), (32, 32)),
    ((8, 56), (52, 56)),
]
"""One-way streets, each along one axis: east along the centre's south street, south along its west one and east along
the industrial street."""
SLOW_ROADS = [((44, 21), (44, 31), 3), ((8, 9), (8, 22), 2)]
"""Slow roads (first cell, last cell, cost): the cobbled harbour-side street and the steep road up the hill."""


def _line(a: tuple[int, int], b: tuple[int, int]) -> list[tuple[int, int]]:
    """Cells of a straight run along one axis, ends included."""
    (x0, y0), (x1, y1) = a, b
    assert x0 == x1 or y0 == y1
    if y0 == y1:
        return [(x, y0) for x in range(min(x0, x1), max(x0, x1) + 1)]
    return [(x0, y) for y in range(min(y0, y1), max(y0, y1) + 1)]


STREETS: list[tuple[tuple[int, int], tuple[int, int]]] = [
    # Avenues along Y.
    ((8, 8), (8, 56)),
    ((20, 8), (20, 56)),
    ((32, 8), (32, 56)),
    ((44, 6), (44, 56)),
    ((52, 18), (52, 56)),
    # Streets along X.
    ((8, 8), (32, 8)),
    ((8, 20), (52, 20)),
    ((8, 32), (52, 32)),
    ((8, 44), (52, 44)),
    ((8, 56), (52, 56)),
    # The hill: a road round the summit, ending in two dead ends, and a short street west of the avenue.
    ((8, 14), (14, 14)),
    ((20, 14), (16, 14)),
    ((8, 26), (3, 26)),
    # The pier of the harbour.
    ((52, 38), (58, 38)),
    # A street into the industrial yards and a spur from the plaza.
    ((20, 50), (26, 50)),
    ((38, 20), (38, 24)),
]
"""Straight streets (first cell, last cell)."""


def road_cells() -> set[tuple[int, int]]:
    """
    Every road cell of the town.

    :return: The cells.
    """
    cells: set[tuple[int, int]] = set()
    for a, b in STREETS:
        cells.update(_line(a, b))
    return cells


def _in_rect(x: float, y: float, rect: tuple[int, int, int, int]) -> bool:
    return rect[0] <= x <= rect[2] and rect[1] <= y <= rect[3]


def water_cells() -> set[tuple[int, int]]:
    """
    Cells of open water: the sea, the river and the pond (the pier and the bridges are drawn over them).

    :return: The cells.
    """
    cells: set[tuple[int, int]] = set()
    for y in range(SIZE):
        for x in range(SIZE):
            in_pond = math.hypot(x + 0.5 - POND[0], y + 0.5 - POND[1]) <= POND[2]
            if x >= SEA_COLUMN or in_pond or any(_in_rect(x, y, rect) for rect in RIVER):
                cells.add((x, y))
    return cells


def corner_heights() -> list[int]:
    """
    Corner grid of the town: three hills, flat water, plaza, quay and pier, neighbours at most one step apart.

    :return: The ``(SIZE + 1)^2`` corner heights, row by row.
    """
    row = SIZE + 1
    heights = [0] * (row * row)
    for y in range(row):
        for x in range(row):
            total = sum(p * math.exp(-((x - cx) ** 2 + (y - cy) ** 2) / (2.0 * s * s)) for cx, cy, p, s in HILLS)
            heights[y * row + x] = round(total)
    flat: set[tuple[int, int]] = set()
    for cx, cy in water_cells() | {(x, y) for rect in (PLAZA, QUAY, PIER) for x, y in _rect_cells(rect)}:
        flat.update({(cx, cy), (cx + 1, cy), (cx, cy + 1), (cx + 1, cy + 1)})
    for x, y in flat:
        heights[y * row + x] = 0
    changed = True
    while changed:
        changed = False
        for y in range(row):
            for x in range(row):
                for nx, ny in ((x + 1, y), (x, y + 1)):
                    if nx >= row or ny >= row:
                        continue
                    a, b = heights[y * row + x], heights[ny * row + nx]
                    if abs(a - b) > 1:
                        # Lower the higher corner: the flat ones (water, plaza, quay) are the lowest, never raised.
                        if a > b:
                            heights[y * row + x] = b + 1
                        else:
                            heights[ny * row + nx] = a + 1
                        changed = True
    return heights


def _rect_cells(rect: tuple[int, int, int, int]) -> list[tuple[int, int]]:
    return [(x, y) for y in range(rect[1], rect[3] + 1) for x in range(rect[0], rect[2] + 1)]


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
    return SLOPES.index(tuple(c - lowest for c in corners))  # type: ignore[arg-type]


def is_flat(heights: list[int], x: int, y: int) -> bool:
    """
    Whether the four corners of a cell are at the same height.

    :param heights: The corner grid.
    :param x: Cell column.
    :param y: Cell row.
    :return: True when flat.
    """
    return len(set(cell_corners(heights, x, y))) == 1


def surface_z(heights: list[int], x: float, y: float) -> float:
    """
    World height of the ground at the centre of the cell holding a position.

    :param heights: The corner grid.
    :param x: World X.
    :param y: World Y.
    :return: The mean corner height of the cell, in world units.
    """
    cx, cy = min(SIZE - 1, max(0, int(x))), min(SIZE - 1, max(0, int(y)))
    return sum(cell_corners(heights, cx, cy)) / 4.0 * STEP_Z


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
    The road overlay, each cell tiled from the mask of its road neighbours.

    :return: The overlay tile of each road cell.
    """
    cells = road_cells()
    return {(x, y): ROAD_MASKS[road_mask(cells, x, y)] for x, y in cells}


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
    district: str
    """District the building stands in."""


FOOTPRINTS = {"cottage": (1, 1), "townhouse": (2, 1), "warehouse": (2, 2), "station": (3, 1), "tower": (1, 1)}
"""Footprint of each building."""
STATION = ("station", 13, 45)
"""The railway station, between the street and the rail."""


def district(x: float, y: float) -> str:
    """
    District of a position.

    :param x: Column.
    :param y: Row.
    :return: ``park``, ``residential``, ``centre``, ``industrial``, ``harbour`` or ``outskirts``.
    """
    if x >= 28 and y <= 13:
        return "park"
    if x >= 36 and y >= 33:
        return "harbour"
    if y >= 36 and x < 36:
        return "industrial"
    if 28 <= x < 54 and 17 <= y <= 32:
        return "centre"
    if x < 25 and y < 36:
        return "residential"
    return "outskirts"


_MIX = {
    "residential": (0.4, (("cottage", 0.7), ("townhouse", 0.3))),
    "centre": (0.5, (("tower", 0.34), ("townhouse", 0.46), ("cottage", 0.2))),
    "industrial": (0.4, (("warehouse", 0.72), ("cottage", 0.28))),
    "harbour": (0.4, (("warehouse", 0.7), ("townhouse", 0.2), ("tower", 0.1))),
    "outskirts": (0.3, (("cottage", 0.8), ("townhouse", 0.2))),
}
"""Density of each district and the weights of its building kinds."""


def _footprint_cells(kind: str, x: int, y: int) -> list[tuple[int, int]]:
    w, d = FOOTPRINTS[kind]
    return [(x + i, y + j) for j in range(d) for i in range(w)]


@dataclass
class City:
    """
    Everything the generator decides about the town.
    """

    heights: list[int]
    """Corner grid."""
    roads: set[tuple[int, int]]
    """Road cells."""
    water: set[tuple[int, int]]
    """Open water cells."""
    plots: list[Plot] = field(default_factory=list)
    """Buildings, in scene order."""
    trees: list[tuple[str, float, float]] = field(default_factory=list)
    """Trees (kind, x, y) standing on grass."""
    chimneys: list[tuple[float, float, float]] = field(default_factory=list)
    """Chimneys: world x, y of the cell centre and z of the foot (on the roof of a warehouse)."""
    lamps: list[tuple[float, float]] = field(default_factory=list)
    """Street lamps, at the centre of their cell."""
    ground: dict[tuple[int, int], int] = field(default_factory=dict)
    """Terrain tile of every cell."""
    overlay: dict[tuple[int, int], int] = field(default_factory=dict)
    """Overlay tile (road, rail) of the cells that have one."""


def build_city(seed: int = 1) -> City:
    """
    Lay the town out: ground, roads, railway, buildings, trees, chimneys and lamps.

    :param seed: Seed of the run, shuffling the grass variants and the lots.
    :return: The town.
    """
    rng = random.Random(77 + seed)
    heights = corner_heights()
    roads = road_cells()
    water = water_cells()
    city = City(heights, roads, water)
    rail = {(x, RAIL_ROW) for x in range(0, SEA_COLUMN - 1) if (x, RAIL_ROW) not in roads}
    paved = {c for rect in (PLAZA, QUAY, PIER) for c in _rect_cells(rect)}

    for y in range(SIZE):
        for x in range(SIZE):
            top = max(cell_corners(heights, x, y))
            tile = GRASS if rng.random() < 0.72 else rng.choice([DARK_GRASS, FLOWERS])
            if district(x, y) == "park" and rng.random() < 0.3:
                tile = DARK_GRASS
            if (x, y) in water:
                tile = WATER
            elif top >= 4:
                tile = ROCK
            elif top >= 2 and slope_of(heights, x, y) != 0:
                tile = DIRT
            city.ground[(x, y)] = tile
    # Sand along every shore, then the beach, the pavements.
    for y in range(SIZE):
        for x in range(SIZE):
            if city.ground[(x, y)] == WATER:
                continue
            near_water = any((x + dx, y + dy) in water for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if near_water and is_flat(heights, x, y):
                city.ground[(x, y)] = SAND
    for cell in paved:
        city.ground[cell] = PAVEMENT
    city.overlay.update(road_tiles())
    city.overlay.update({cell: RAIL for cell in rail})

    occupied: set[tuple[int, int]] = set(roads) | set(rail) | water
    occupied |= {c for c in city.ground if city.ground[c] in (SAND, WATER)}
    _place_buildings(city, rng, occupied)
    _place_trees(city, rng, occupied)
    _place_lamps(city, rng, occupied)
    return city


def _adjacent_to_road(roads: set[tuple[int, int]], cells: list[tuple[int, int]]) -> bool:
    return any((x + dx, y + dy) in roads for x, y in cells for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))


def _place_buildings(city: City, rng: random.Random, occupied: set[tuple[int, int]]) -> None:
    def free(cells: list[tuple[int, int]]) -> bool:
        if any(not (0 <= x < SIZE and 0 <= y < SIZE) or (x, y) in occupied for x, y in cells):
            return False
        if not all(is_flat(city.heights, x, y) for x, y in cells):
            return False
        return len({cell_corners(city.heights, x, y)[0] for x, y in cells}) == 1

    def put(kind: str, x: int, y: int) -> None:
        cells = _footprint_cells(kind, x, y)
        city.plots.append(Plot(kind, (x, y), FOOTPRINTS[kind], district(x, y)))
        occupied.update(cells)
        for cell in cells:
            city.ground[cell] = PAVEMENT

    kind, sx, sy = STATION
    if free(_footprint_cells(kind, sx, sy)):
        put(kind, sx, sy)
    for y in range(SIZE):
        for x in range(SIZE):
            zone = district(x, y)
            if zone not in _MIX or y == RAIL_ROW:
                continue
            density, weights = _MIX[zone]
            if rng.random() > density:
                continue
            roll, kind = rng.random(), weights[-1][0]
            for name, weight in weights:
                if roll < weight:
                    kind = name
                    break
                roll -= weight
            cells = _footprint_cells(kind, x, y)
            if free(cells) and _adjacent_to_road(city.roads, cells):
                put(kind, x, y)
    # Chimneys on the roofs of the industrial warehouses.
    warehouses = [p for p in city.plots if p.kind == "warehouse" and p.district == "industrial"]
    for plot in warehouses[:: max(1, len(warehouses) // 7)][:7]:
        level = cell_corners(city.heights, *plot.cell)[0] * STEP_Z
        city.chimneys.append((plot.cell[0] + 1.0, plot.cell[1] + 1.0, level + 2.1))


def _place_trees(city: City, rng: random.Random, occupied: set[tuple[int, int]]) -> None:
    density = {"park": 0.5, "residential": 0.2, "centre": 0.06, "industrial": 0.12, "harbour": 0.05, "outskirts": 0.3}
    for y in range(SIZE):
        for x in range(SIZE):
            if (x, y) in occupied or city.ground[(x, y)] not in (GRASS, DARK_GRASS, FLOWERS):
                continue
            if rng.random() > density[district(x, y)]:
                continue
            if (
                any((x + dx, y + dy) in city.roads for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                and rng.random() < 0.7
            ):
                continue
            kind = "pine" if rng.random() < 0.45 or max(cell_corners(city.heights, x, y)) >= 2 else "oak"
            city.trees.append((kind, x + 0.5, y + 0.5))
            occupied.add((x, y))


def _place_lamps(city: City, rng: random.Random, occupied: set[tuple[int, int]]) -> None:
    candidates = [
        (x, y)
        for (x, y) in sorted(city.roads)
        for dx, dy in ((0, 1), (1, 0))
        if district(x, y) in ("centre", "harbour")
        and (x + dx, y + dy) not in occupied
        and (x + dx, y + dy) not in city.roads
        and city.ground[(x + dx, y + dy)] != WATER
        and is_flat(city.heights, x + dx, y + dy)
    ]
    rng.shuffle(candidates)
    for x, y in candidates:
        cell = (x + (1 if (x + 1, y) not in city.roads else 0), y + (1 if (x, y + 1) not in city.roads else 0))
        if cell in occupied or any(abs(cell[0] - lx) + abs(cell[1] - ly) < 5 for lx, ly in map(_cell_of, city.lamps)):
            continue
        city.lamps.append((cell[0] + 0.5, cell[1] + 0.5))
        occupied.add(cell)
        if len(city.lamps) >= 14:
            break


def _cell_of(position: tuple[float, float]) -> tuple[int, int]:
    return int(position[0]), int(position[1])


def navigation_overrides() -> list[tuple[tuple[int, int], dict[str, object]]]:
    """
    The per-cell exceptions of the road graph: the one-way streets and the slow roads.

    :return: Each cell with its ``noEnter`` / ``noExit`` directions and its ``cost``.
    """
    merged: dict[tuple[int, int], dict[str, object]] = {}
    for first, last in ONE_WAYS:
        cells = _line(first, last)
        dx, dy = last[0] - first[0], last[1] - first[1]
        forward = "east" if dx > 0 else "south" if dy > 0 else "west" if dx < 0 else "north"
        back = {"east": "west", "west": "east", "south": "north", "north": "south"}[forward]
        for index, cell in enumerate(cells):
            entry = merged.setdefault(cell, {})
            if index > 0:
                entry["noExit"] = [back]
            if index < len(cells) - 1:
                entry["noEnter"] = [forward]
    for first, last, cost in SLOW_ROADS:
        for cell in _line(first, last):
            merged.setdefault(cell, {})["cost"] = cost
    return sorted(merged.items(), key=lambda item: (item[0][1], item[0][0]))


def _payload(city: City) -> bytes:
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for ground_layer in (True, False):
        source = city.ground if ground_layer else city.overlay
        chunks: dict[tuple[int, int], dict[tuple[int, int], int]] = {}
        for (x, y), tile in source.items():
            chunks.setdefault((x // CHUNK, y // CHUNK), {})[(x % CHUNK, y % CHUNK)] = tile
        body += struct.pack("<I", len(chunks))
        for (cx, cy), cells in sorted(chunks.items()):
            body += struct.pack("<II", cx, cy) + (_chunk(cells, {}, {}) if ground_layer else _chunk({}, cells, {}))
    body += b"\x01" + bytes(city.heights)
    return body + struct.pack("<I", _fnv1a(body))


def map_bytes(seed: int) -> bytes:
    """
    Build the ``iso_town`` tilemap file.

    :param seed: Seed of the run, shuffling the grass variants and the lots.
    :return: The file content.
    """
    payload = _payload(build_city(seed))
    header = (
        "Tilemap: iso_town\nFormatVersion: 3\ntilesetPath: tilesets/iso_ground.owltileset\n"
        f"width: {SIZE}\nheight: {SIZE}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\n"
        f"payloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def write_city_map(project: Path, seed: int) -> Path:
    """
    Write the town map.

    :param project: The project folder.
    :param seed: Seed of the run.
    :return: The written file.
    """
    path = project / "tilemaps" / "iso_town.owltilemap"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(map_bytes(seed))
    return path
