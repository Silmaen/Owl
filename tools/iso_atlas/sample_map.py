"""
The 8x8 test map of the isometric sample scene, in the chunked tilemap format 3.
"""

from __future__ import annotations

import random
import struct
from pathlib import Path

from .terrain import SLOPES

CHUNK = 32
EMPTY = 0xFFFF
GRASS, SAND, WATER, ROAD, DIRT, PAVEMENT, DARK_GRASS, FLOWERS, ROCK, SNOW, RAIL = range(11)


def _fnv1a(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def _chunk(
    terrain_cells: dict[tuple[int, int], int],
    overlay_cells: dict[tuple[int, int], int],
    flag_cells: dict[tuple[int, int], int],
) -> bytes:
    terrain = [EMPTY] * (CHUNK * CHUNK)
    overlay = [EMPTY] * (CHUNK * CHUNK)
    flags = bytearray(CHUNK * CHUNK)
    for (x, y), tile in terrain_cells.items():
        terrain[y * CHUNK + x] = tile
    for (x, y), tile in overlay_cells.items():
        overlay[y * CHUNK + x] = tile
    for (x, y), bits in flag_cells.items():
        flags[y * CHUNK + x] = bits
    return struct.pack(f"<{CHUNK * CHUNK}H", *terrain) + struct.pack(f"<{CHUNK * CHUNK}H", *overlay) + bytes(flags)


def corner_heights(size: int) -> list[int]:
    """
    Corner grid of the test map: a hump under the house, a low ridge on the east edge and a ramp for the road.

    :param size: Map size in cells.
    :return: The ``(size + 1)^2`` corner heights, row by row, neighbours at most one step apart.
    """
    heights = [[0] * (size + 1) for _ in range(size + 1)]
    raised = {
        (4, 2): 1,
        (4, 3): 1,
        (7, 4): 1,
        (8, 4): 1,
        (7, 5): 1,
        (8, 5): 1,
        (8, 3): 1,
        (8, 0): 1,
        (8, 1): 1,
        (8, 2): 1,
    }
    for (x, y), height in raised.items():
        heights[y][x] = height
    changed = True
    while changed:
        changed = False
        for y in range(size + 1):
            for x in range(size + 1):
                for nx, ny in ((x + 1, y), (x, y + 1)):
                    if nx > size or ny > size:
                        continue
                    low, high = sorted([(heights[y][x], (x, y)), (heights[ny][nx], (nx, ny))])
                    if high[0] - low[0] > 1:
                        heights[low[1][1]][low[1][0]] = high[0] - 1
                        changed = True
    return [h for row in heights for h in row]


def slope_of(heights: list[int], size: int, x: int, y: int) -> int:
    """
    Slope index of a cell.

    :param heights: The corner grid.
    :param size: Map size in cells.
    :param x: Cell column.
    :param y: Cell row.
    :return: The index in ``SLOPES``.
    """
    row = size + 1
    corners = (
        heights[y * row + x],
        heights[y * row + x + 1],
        heights[(y + 1) * row + x + 1],
        heights[(y + 1) * row + x],
    )
    lowest = min(corners)
    return SLOPES.index((corners[0] - lowest, corners[1] - lowest, corners[2] - lowest, corners[3] - lowest))


def map_bytes(seed: int) -> bytes:
    """
    Build the ``iso_test`` tilemap file.

    :param seed: Seed of the run, shuffling the grass variants.
    :return: The file content.
    """
    size = 8
    heights = corner_heights(size)
    ground: dict[tuple[int, int], int] = {}
    rng = random.Random(42 + seed)
    for y in range(size):
        for x in range(size):
            tile = GRASS if rng.random() < 0.7 else rng.choice([DARK_GRASS, FLOWERS])
            if x >= 6 and y <= 2:
                tile = WATER
            elif (x == 5 and y <= 3) or (y == 3 and x >= 5):
                tile = SAND
            elif x == 7 and y >= 4:
                tile = ROCK
            if tile in (WATER, PAVEMENT) and slope_of(heights, size, x, y) != 0:
                tile = GRASS
            ground[(x, y)] = tile
    ground[(3, 2)] = GRASS
    overlay: dict[tuple[int, int], int] = {(x, 4): ROAD for x in range(size)}
    overlay.update({(2, 4): PAVEMENT, (3, 4): PAVEMENT})
    overlay.update({(x, 7): RAIL for x in range(size)})
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for terrain_cells, overlay_cells, flag_cells in (({**ground}, {}, {(3, 2): 1}), ({}, overlay, {})):
        body += struct.pack("<I", 1) + struct.pack("<II", 0, 0) + _chunk(terrain_cells, overlay_cells, flag_cells)
    body += b"\x01" + bytes(heights)
    payload = body + struct.pack("<I", _fnv1a(body))
    header = (
        "Tilemap: iso_test\nFormatVersion: 3\ntilesetPath: tilesets/iso_ground.owltileset\n"
        f"width: {size}\nheight: {size}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\n"
        f"payloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def write_map(project: Path, seed: int) -> Path:
    """
    Write the test map.

    :param project: The project folder.
    :param seed: Seed of the run.
    :return: The written file.
    """
    path = project / "tilemaps" / "iso_test.owltilemap"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(map_bytes(seed))
    return path
