#!/usr/bin/env python3
"""Generate the small isometric test set of the sample project (ISO1, ISO2).

Produces, next to this script, in ``../tilemaps`` and in ``../tilesets``:

* ``iso_tiles.png``     — 8x17 atlas of 64x48 tiles: the 8 flat diamond terrain tiles (grass, sand,
  water, road, dirt, pavement, dark grass, flowers) bottom-aligned, then the 18 sloped variants of the
  grass, sand, dirt, dark grass, flowers and road tiles, then the 18 grass foundations.
* ``iso_ground.owltileset`` — the slope table (terrain id and slope to tile) of that atlas.
* ``iso_tree.png``      — 64x96 upright tree, its foot at the bottom centre.
* ``iso_house.png``     — 64x80 one-tile house, its front corner at the bottom
  centre.
* ``iso_smoke.png``     — 48x48 soft smoke puff, drawn translucent.
* ``iso_test.owltilemap`` — 8x8 map (ground and road layers) with corner heights, in the chunked
  tilemap format 3.

The full procedural generator (normal and depth atlases, a bigger tile set) comes with ISO3 under
``tools/``. Deterministic; run with ``poetry run python``.
"""

from __future__ import annotations

import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).parent
TILEMAPS = OUT.parent / "tilemaps"
TILE_W = 64
TILE_H = 48
CHUNK = 32
EMPTY = 0xFFFF

TERRAIN = [
    ("grass", (88, 156, 64)),
    ("sand", (214, 196, 128)),
    ("water", (58, 112, 190)),
    ("road", (96, 96, 104)),
    ("dirt", (138, 98, 62)),
    ("pavement", (168, 164, 156)),
    ("dark_grass", (60, 120, 52)),
    ("flowers", (92, 160, 70)),
]


COLUMNS = 8
ROWS = 17
STEP_PX = 8
# Corner heights of the 19 slopes relative to the lowest corner (back, right, front, left): flat, 4 corners up,
# 4 corners down, 4 edges up, 2 saddles, 4 steep. Same order as ``scene::terrain`` in the engine.
SLOPES = [
    (0, 0, 0, 0),
    (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1),
    (0, 1, 1, 1), (1, 0, 1, 1), (1, 1, 0, 1), (1, 1, 1, 0),
    (1, 1, 0, 0), (0, 1, 1, 0), (0, 0, 1, 1), (1, 0, 0, 1),
    (1, 0, 1, 0), (0, 1, 0, 1),
    (2, 1, 0, 1), (1, 2, 1, 0), (0, 1, 2, 1), (1, 0, 1, 2),
]
SLOPED_TERRAINS = [0, 1, 4, 6, 7, 3]  # grass, sand, dirt, dark grass, flowers, road
FIRST_SLOPED = len(TERRAIN)
FIRST_FOUNDATION = FIRST_SLOPED + 18 * len(SLOPED_TERRAINS)
LIGHT = (-0.35, 0.35, 0.87)


def tile_index(terrain: int, slope: int) -> int:
    """Atlas index of a terrain on a slope (the flat tile is the terrain id itself)."""
    if slope == 0:
        return terrain
    return FIRST_SLOPED + SLOPED_TERRAINS.index(terrain) * 18 + slope - 1


def foundation_index(slope: int) -> int:
    """Atlas index of the grass foundation of a slope."""
    return FIRST_FOUNDATION + slope - 1


def _corners_px(profile: tuple[int, ...]) -> list[tuple[float, float]]:
    return [(32.0, 16.0 - STEP_PX * profile[0]), (64.0, 32.0 - STEP_PX * profile[1]),
            (32.0, 48.0 - STEP_PX * profile[2]), (0.0, 32.0 - STEP_PX * profile[3])]


def _triangles(profile: tuple[int, ...]) -> list[tuple[int, int, int]]:
    """The two triangles of a cell: the engine splits it along the diagonal whose corners are the closest."""
    if abs(profile[0] - profile[2]) <= abs(profile[1] - profile[3]):
        return [(0, 1, 2), (0, 2, 3)]
    return [(0, 1, 3), (1, 2, 3)]


def _in_triangle(point: tuple[float, float], a: tuple[float, float], b: tuple[float, float],
                 c: tuple[float, float]) -> bool:
    def sign(p: tuple[float, float], q: tuple[float, float], r: tuple[float, float]) -> float:
        return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1])

    d1, d2, d3 = sign(point, a, b), sign(point, b, c), sign(point, c, a)
    return not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0))


def _shade(profile: tuple[int, ...], triangle: tuple[int, int, int]) -> float:
    world = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    a, b, c = ((*world[i], profile[i] * 0.5) for i in triangle)
    u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    if n[2] < 0:
        n = (-n[0], -n[1], -n[2])
    length = sum(x * x for x in n) ** 0.5
    return max(0.6, min(1.3, sum(n[i] * LIGHT[i] for i in range(3)) / length / LIGHT[2]))


def _surface(profile: tuple[int, ...]) -> dict[tuple[int, int], float]:
    """Pixels of the sloped diamond of a 64x48 tile with their brightness."""
    corners = _corners_px(profile)
    pixels: dict[tuple[int, int], float] = {}
    for triangle in _triangles(profile):
        a, b, c = (corners[i] for i in triangle)
        shade = _shade(profile, triangle)
        for y in range(TILE_H):
            for x in range(TILE_W):
                if (x, y) not in pixels and _in_triangle((x + 0.5, y + 0.5), a, b, c):
                    pixels[(x, y)] = shade
    return pixels


def _draw_surface(atlas: Image.Image, index: int, name: str, base: tuple[int, int, int], profile: tuple[int, ...],
                  seed: int) -> None:
    px = atlas.load()
    ox = (index % COLUMNS) * TILE_W
    oy = (index // COLUMNS) * TILE_H
    rng = random.Random(seed)
    surface = _surface(profile)
    for (x, y), shade in sorted(surface.items()):
        n = rng.randint(-10, 10)
        edge = not all((x + dx, y + dy) in surface for dx, dy in ((-2, 0), (2, 0), (0, -1), (0, 1)))
        color = tuple(max(0, min(255, int(c * shade) + n + (-22 if edge else 0))) for c in base)
        if name == "road" and not any(profile) and abs((x - TILE_W / 2) / 2 - (y - 32)) < 1.2 and (x // 6) % 2 == 0:
            color = (236, 206, 70)
        if name == "flowers" and rng.random() < 0.06:
            color = rng.choice([(236, 84, 84), (240, 220, 90), (250, 250, 250)])
        px[ox + x, oy + y] = (*color, 255)


def _draw_foundation(atlas: Image.Image, index: int, profile: tuple[int, ...]) -> None:
    """Level platform of a building on a slope: a flat top at the highest corner, walls down to the ground."""
    tile = Image.new("RGBA", (TILE_W, TILE_H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    top = max(profile)
    back, right, front, left = _corners_px((top, top, top, top))
    ground = _corners_px(profile)
    draw.polygon([left, front, ground[2], ground[3]], fill=(150, 144, 132, 255))
    draw.polygon([front, right, ground[1], ground[2]], fill=(118, 112, 102, 255))
    draw.polygon([back, right, front, left], fill=(92, 150, 66, 255), outline=(70, 118, 52, 255))
    atlas.alpha_composite(tile, ((index % COLUMNS) * TILE_W, (index // COLUMNS) * TILE_H))


def make_tiles() -> None:
    atlas = Image.new("RGBA", (COLUMNS * TILE_W, ROWS * TILE_H), (0, 0, 0, 0))
    for index, (name, base) in enumerate(TERRAIN):
        _draw_surface(atlas, index, name, base, SLOPES[0], 1000 + index)
    for terrain in SLOPED_TERRAINS:
        name, base = TERRAIN[terrain]
        for slope in range(1, len(SLOPES)):
            _draw_surface(atlas, tile_index(terrain, slope), name, base, SLOPES[slope], 5000 + terrain * 100 + slope)
    for slope in range(1, len(SLOPES)):
        _draw_foundation(atlas, foundation_index(slope), SLOPES[slope])
    atlas.save(OUT / "iso_tiles.png")


def make_tileset() -> None:
    """The slope table: terrain id and slope to atlas tile, plus the grass foundations."""
    lines = ["Tileset: iso_ground", "FormatVersion: 1", "texture: nam:textures/iso_tiles.png", "tileWidth: 64",
             f"tileHeight: {TILE_H}", f"columns: {COLUMNS}", f"rows: {ROWS}", "filterMode: Nearest",
             "kind: Isometric", "slopes:"]
    for terrain in SLOPED_TERRAINS:
        tiles = ", ".join(str(tile_index(terrain, slope)) for slope in range(len(SLOPES)))
        lines += [f"  - id: {terrain}", f"    tiles: [{tiles}]"]
        if terrain == 0:
            foundations = ", ".join(["-1"] + [str(foundation_index(slope)) for slope in range(1, len(SLOPES))])
            lines.append(f"    foundations: [{foundations}]")
    lines.append("tiles:")
    for index, (name, _) in enumerate(TERRAIN):
        lines += [f"  - index: {index}", f"    name: {name.replace('_', ' ')}"]
    (OUT.parent / "tilesets" / "iso_ground.owltileset").write_text("\n".join(lines) + "\n")


def make_tree() -> None:
    image = Image.new("RGBA", (64, 96), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle([28, 60, 35, 95], fill=(110, 72, 40, 255))
    for cx, cy, r, color in [(32, 46, 22, (46, 112, 46)), (24, 36, 14, (58, 134, 54)), (40, 30, 13, (66, 146, 60)),
                             (32, 20, 12, (78, 160, 70))]:
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(*color, 255))
    image.save(OUT / "iso_tree.png")


def make_house() -> None:
    image = Image.new("RGBA", (64, 80), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    left = [(0, 63), (32, 79), (32, 47), (0, 31)]
    right = [(32, 79), (63, 63), (63, 31), (32, 47)]
    roof = [(0, 31), (32, 47), (63, 31), (32, 15)]
    draw.polygon(left, fill=(196, 160, 120, 255))
    draw.polygon(right, fill=(160, 124, 92, 255))
    draw.polygon(roof, fill=(172, 64, 52, 255))
    draw.polygon([(10, 52), (20, 57), (20, 45), (10, 40)], fill=(90, 140, 190, 255))
    draw.polygon([(40, 72), (48, 68), (48, 54), (40, 58)], fill=(92, 60, 36, 255))
    draw.rectangle([42, 8, 47, 26], fill=(120, 110, 104, 255))
    image.save(OUT / "iso_house.png")


def make_smoke() -> None:
    image = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    px = image.load()
    for y in range(48):
        for x in range(48):
            d = ((x - 23.5) ** 2 + (y - 23.5) ** 2) ** 0.5 / 24.0
            alpha = max(0.0, 1.0 - d) ** 1.5
            px[x, y] = (232, 232, 236, int(alpha * 255))
    image.save(OUT / "iso_smoke.png")


def _fnv1a(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def _chunk(terrain_cells: dict[tuple[int, int], int], overlay_cells: dict[tuple[int, int], int],
           flag_cells: dict[tuple[int, int], int]) -> bytes:
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


def _corner_heights(size: int) -> list[int]:
    """Corner grid of the test map: a hump under the house, a low ridge on the east edge and a ramp for the road."""
    heights = [[0] * (size + 1) for _ in range(size + 1)]
    raised = {(4, 2): 1, (4, 3): 1, (7, 4): 1, (8, 4): 1, (7, 5): 1, (8, 5): 1, (8, 3): 1,
              (8, 0): 1, (8, 1): 1, (8, 2): 1}
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


def _slope_of(heights: list[int], size: int, x: int, y: int) -> int:
    row = size + 1
    corners = (heights[y * row + x], heights[y * row + x + 1], heights[(y + 1) * row + x + 1],
               heights[(y + 1) * row + x])
    lowest = min(corners)
    return SLOPES.index(tuple(c - lowest for c in corners))


def make_tilemap() -> None:
    size = 8
    heights = _corner_heights(size)
    ground: dict[tuple[int, int], int] = {}
    rng = random.Random(42)
    for y in range(size):
        for x in range(size):
            tile = 0 if rng.random() < 0.7 else rng.choice([6, 7])
            if x >= 6 and y <= 2:
                tile = 2
            elif (x == 5 and y <= 3) or (y == 3 and x >= 5):
                tile = 1
            if tile in (2, 5) and _slope_of(heights, size, x, y) != 0:
                tile = 0
            ground[(x, y)] = tile
    ground[(3, 2)] = 0
    roads = {(x, 4): 3 for x in range(size)}
    roads.update({(2, 4): 5, (3, 4): 5})
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for terrain_cells, overlay_cells, flag_cells in (({**ground}, {}, {(3, 2): 1}), ({}, roads, {})):
        body += struct.pack("<I", 1) + struct.pack("<II", 0, 0) + _chunk(terrain_cells, overlay_cells, flag_cells)
    body += b"\x01" + bytes(heights)
    payload = body + struct.pack("<I", _fnv1a(body))
    header = ("Tilemap: iso_test\nFormatVersion: 3\ntilesetPath: tilesets/iso_ground.owltileset\n"
              f"width: {size}\nheight: {size}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\n"
              f"payloadBytes: {len(payload)}\n")
    (TILEMAPS / "iso_test.owltilemap").write_bytes(header.encode() + b"...\n" + payload)


if __name__ == "__main__":
    make_tiles()
    make_tileset()
    make_tree()
    make_house()
    make_smoke()
    make_tilemap()
