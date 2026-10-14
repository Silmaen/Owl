#!/usr/bin/env python3
"""Generate the small isometric test set of the sample project (ISO1).

Produces, next to this script and in ``../tilemaps``:

* ``iso_tiles.png``     — 4x2 atlas of 64x32 diamond terrain tiles (grass, sand,
  water, road, dirt, pavement, dark grass, flowers).
* ``iso_tree.png``      — 64x96 upright tree, its foot at the bottom centre.
* ``iso_house.png``     — 64x80 one-tile house, its front corner at the bottom
  centre.
* ``iso_smoke.png``     — 48x48 soft smoke puff, drawn translucent.
* ``iso_test.owltilemap`` — 8x8 map (ground and road layers) in the chunked
  tilemap format 2.

The full procedural generator (slopes, foundations, normal and depth atlases)
comes with ISO3 under ``tools/``. Deterministic; run with ``poetry run python``.
"""

from __future__ import annotations

import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).parent
TILEMAPS = OUT.parent / "tilemaps"
TILE_W = 64
TILE_H = 32
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


def _inside(x: int, y: int) -> bool:
    """Whether pixel (x, y) of a 64x32 tile lies in its diamond."""
    return abs(x + 0.5 - TILE_W / 2) / (TILE_W / 2) + abs(y + 0.5 - TILE_H / 2) / (TILE_H / 2) <= 1.0


def _tile(atlas: Image.Image, index: int, name: str, base: tuple[int, int, int]) -> None:
    px = atlas.load()
    ox = (index % 4) * TILE_W
    oy = (index // 4) * TILE_H
    rng = random.Random(1000 + index)
    for y in range(TILE_H):
        for x in range(TILE_W):
            if not _inside(x, y):
                continue
            n = rng.randint(-10, 10)
            edge = not (_inside(x - 2, y) and _inside(x + 2, y) and _inside(x, y - 1) and _inside(x, y + 1))
            shade = -22 if edge else 0
            color = tuple(max(0, min(255, c + n + shade)) for c in base)
            if name == "road" and abs((x - TILE_W / 2) / 2 - (y - TILE_H / 2)) < 1.2 and (x // 6) % 2 == 0:
                color = (236, 206, 70)
            if name == "flowers" and rng.random() < 0.06:
                color = rng.choice([(236, 84, 84), (240, 220, 90), (250, 250, 250)])
            px[ox + x, oy + y] = (*color, 255)


def make_tiles() -> None:
    atlas = Image.new("RGBA", (4 * TILE_W, 2 * TILE_H), (0, 0, 0, 0))
    for index, (name, base) in enumerate(TERRAIN):
        _tile(atlas, index, name, base)
    atlas.save(OUT / "iso_tiles.png")


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


def _chunk(cells: dict[tuple[int, int], int]) -> bytes:
    terrain = [EMPTY] * (CHUNK * CHUNK)
    for (x, y), tile in cells.items():
        terrain[y * CHUNK + x] = tile
    return (struct.pack(f"<{CHUNK * CHUNK}H", *terrain) + struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK))
            + bytes(CHUNK * CHUNK))


def make_tilemap() -> None:
    size = 8
    ground: dict[tuple[int, int], int] = {}
    rng = random.Random(42)
    for y in range(size):
        for x in range(size):
            tile = 0 if rng.random() < 0.7 else rng.choice([6, 7])
            if x >= 6 and y <= 2:
                tile = 2
            elif (x == 5 and y <= 3) or (y == 3 and x >= 5):
                tile = 1
            ground[(x, y)] = tile
    roads = {(x, 4): 3 for x in range(size)}
    roads.update({(2, 4): 5, (3, 4): 5})
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for layer in (ground, roads):
        body += struct.pack("<I", 1) + struct.pack("<II", 0, 0) + _chunk(layer)
    payload = body + struct.pack("<I", _fnv1a(body))
    header = ("Tilemap: iso_test\nFormatVersion: 2\ntilesetPath: tilesets/iso_ground.owltileset\n"
              f"width: {size}\nheight: {size}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\n"
              f"payloadBytes: {len(payload)}\n")
    (TILEMAPS / "iso_test.owltilemap").write_bytes(header.encode() + b"...\n" + payload)


if __name__ == "__main__":
    make_tiles()
    make_tree()
    make_house()
    make_smoke()
    make_tilemap()
