#!/usr/bin/env python3
"""Generate the procedural tile atlases for the Engine Demo.

Produces the 64 px-tile atlas:

* ``voxel_blocks.png``    — 4x4 block-face atlas for the voxel demo (grass,
  dirt, stone, sand, wood, ... ) with room for future block types.

The raycast atlases (``raycast_walls.png`` / the wolf3d sprites) are authored
separately and deliberately left untouched. Run with ``poetry run python``.
"""

from __future__ import annotations

import random
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "atlases"
TILE = 64


def _noise(px, ox: int, oy: int, base: tuple[int, int, int], var: int, seed: int,
           size: int = TILE) -> None:
    """Fill a tile with per-pixel jittered colour for a textured look."""
    rng = random.Random(seed)
    for y in range(size):
        for x in range(size):
            n = rng.randint(-var, var)
            px[ox + x, oy + y] = (
                max(0, min(255, base[0] + n)),
                max(0, min(255, base[1] + n)),
                max(0, min(255, base[2] + n)),
                255,
            )


def _tile_value_noise(px, ox: int, oy: int, base: tuple[int, int, int], amp: int, cells: int,
                      seed: int, size: int = TILE) -> None:
    """Fill a tile with seamless (wrapping) value noise.

    A ``cells``x``cells`` lattice of random values is laid over the tile and
    bilinearly interpolated; lattice indices wrap modulo ``cells`` so the left
    edge matches the right (and top matches bottom), making the tile periodic.
    """
    rng = random.Random(seed)
    lat = [[rng.uniform(-1.0, 1.0) for _ in range(cells)] for _ in range(cells)]
    step = size / cells
    for y in range(size):
        fy = y / step
        gy0 = int(fy) % cells
        gy1 = (gy0 + 1) % cells
        ty = fy - int(fy)
        for x in range(size):
            fx = x / step
            gx0 = int(fx) % cells
            gx1 = (gx0 + 1) % cells
            tx = fx - int(fx)
            top = lat[gy0][gx0] * (1 - tx) + lat[gy0][gx1] * tx
            bot = lat[gy1][gx0] * (1 - tx) + lat[gy1][gx1] * tx
            n = int((top * (1 - ty) + bot * ty) * amp)
            px[ox + x, oy + y] = (
                max(0, min(255, base[0] + n)),
                max(0, min(255, base[1] + n)),
                max(0, min(255, base[2] + n)),
                255,
            )


def _specks(draw: ImageDraw.ImageDraw, ox: int, oy: int, color: tuple[int, int, int],
            count: int, seed: int, size: int = TILE) -> None:
    """Scatter small 1-2 px specks over a tile (pebbles, grain, detail)."""
    rng = random.Random(seed)
    for _ in range(count):
        x = ox + rng.randint(0, size - 2)
        y = oy + rng.randint(0, size - 2)
        s = rng.randint(0, 1)
        draw.rectangle([x, y, x + s, y + s], fill=color + (255,))


# --- voxel block faces -----------------------------------------------------

def _voxel_tile(atlas: Image.Image, draw: ImageDraw.ImageDraw, index: int, name: str) -> None:
    px = atlas.load()
    ox, oy = (index % 4) * TILE, (index // 4) * TILE
    seed = 1000 + index
    # Voxel faces are frac-tiled per block across greedy-merged quads, so the bases use seamless (wrapping) value
    # noise and any pattern has a period dividing 64 — otherwise every block boundary shows a discontinuity.
    if name == "grass_top":
        _tile_value_noise(px, ox, oy, (86, 142, 54), 18, 8, seed)
        _specks(draw, ox, oy, (66, 116, 42), 70, seed)
        _specks(draw, ox, oy, (110, 168, 70), 40, seed + 1)
    elif name == "grass_side":
        _tile_value_noise(px, ox, oy, (124, 90, 56), 14, 8, seed)  # dirt body
        draw.rectangle([ox, oy, ox + TILE - 1, oy + 13], fill=(86, 142, 54, 255))  # grass cap (1-block-tall face)
        _specks(draw, ox, oy + 8, (66, 116, 42), 50, seed + 2)
        _specks(draw, ox, oy, (150, 112, 72), 40, seed + 3)
    elif name == "dirt":
        _tile_value_noise(px, ox, oy, (124, 90, 56), 18, 8, seed)
        _specks(draw, ox, oy, (96, 68, 40), 80, seed)
        _specks(draw, ox, oy, (150, 112, 72), 40, seed + 1)
    elif name == "stone":
        _tile_value_noise(px, ox, oy, (128, 128, 132), 16, 8, seed)
        _specks(draw, ox, oy, (104, 104, 110), 60, seed)
    elif name == "cobblestone":
        _tile_value_noise(px, ox, oy, (96, 96, 100), 10, 4, seed)
        cell = 16  # 16 px cells (divides 64) so the cobble grid tiles seamlessly
        for cy in range(0, TILE, cell):
            for cx in range(0, TILE, cell):
                r = random.Random(seed + cx * 13 + cy * 7)
                g = 132 + r.randint(-18, 18)
                draw.rounded_rectangle([ox + cx + 2, oy + cy + 2, ox + cx + cell - 3, oy + cy + cell - 3],
                                       radius=3, fill=(g, g, g + 4, 255), outline=(70, 70, 74, 255))
    elif name == "sand":
        _tile_value_noise(px, ox, oy, (214, 196, 140), 12, 8, seed)
        _specks(draw, ox, oy, (196, 176, 120), 70, seed)
    elif name == "log_top":
        _tile_value_noise(px, ox, oy, (150, 112, 70), 10, 6, seed)
        cx, cy = ox + TILE // 2, oy + TILE // 2
        for r in range(26, 0, -5):
            draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(112, 80, 48, 255), width=2)
    elif name == "log_side":
        _tile_value_noise(px, ox, oy, (108, 78, 48), 12, 8, seed)
        for lx in range(0, TILE, 16):  # vertical bark grooves, period 16 -> tiles horizontally
            draw.line([(ox + lx, oy), (ox + lx, oy + TILE)], fill=(84, 58, 34, 255), width=2)
    elif name == "planks":
        _tile_value_noise(px, ox, oy, (164, 120, 70), 10, 8, seed)
        for ly in range(0, TILE, 16):  # plank seams, period 16 -> tiles vertically
            draw.line([(ox, oy + ly), (ox + TILE, oy + ly)], fill=(120, 84, 48, 255), width=2)
    elif name == "leaves":
        _tile_value_noise(px, ox, oy, (58, 122, 50), 22, 8, seed)
        _specks(draw, ox, oy, (40, 96, 36), 90, seed)
        _specks(draw, ox, oy, (84, 150, 66), 50, seed + 1)
    elif name == "snow":
        _tile_value_noise(px, ox, oy, (236, 240, 248), 8, 8, seed)
        _specks(draw, ox, oy, (210, 220, 238), 40, seed)
    elif name == "gravel":
        _tile_value_noise(px, ox, oy, (118, 114, 110), 18, 8, seed)
        _specks(draw, ox, oy, (92, 88, 84), 70, seed)
        _specks(draw, ox, oy, (150, 146, 142), 40, seed + 1)
    elif name == "brick":
        _tile_value_noise(px, ox, oy, (150, 60, 46), 8, 4, seed)
        for i, ly in enumerate(range(0, TILE, 16)):  # rows period 16
            draw.line([(ox, oy + ly), (ox + TILE, oy + ly)], fill=(210, 200, 190, 255), width=2)
            off = 0 if i % 2 == 0 else 16  # running bond, period 32 -> tiles
            for bx in range(off, TILE + off, 32):
                draw.line([(ox + (bx % TILE), oy + ly), (ox + (bx % TILE), oy + ly + 16)],
                          fill=(210, 200, 190, 255), width=2)
    elif name == "water":
        _tile_value_noise(px, ox, oy, (54, 104, 196), 10, 6, seed)
        for ly in range(0, TILE, 16):  # gentle ripples, period 16
            draw.line([(ox, oy + ly), (ox + TILE, oy + ly)], fill=(96, 150, 230, 200), width=2)
    elif name == "ice":
        _tile_value_noise(px, ox, oy, (176, 208, 236), 8, 6, seed)
        _specks(draw, ox, oy, (150, 178, 214), 30, seed)
    elif name == "glass":
        _tile_value_noise(px, ox, oy, (198, 222, 230), 6, 4, seed)
        draw.rectangle([ox + 2, oy + 2, ox + TILE - 3, oy + TILE - 3], outline=(150, 180, 196, 255), width=2)
        draw.line([(ox + 10, oy + 10), (ox + 28, oy + 28)], fill=(235, 245, 250, 255), width=2)
    else:
        _noise(px, ox, oy, (200, 0, 200), 0, seed)  # missing-texture magenta
    # Translucent block faces: drop the alpha so the voxel transparent pass blends them over what is behind.
    translucent = {"water": 150, "ice": 205, "glass": 120}
    if name in translucent:
        alpha = translucent[name]
        for y in range(TILE):
            for x in range(TILE):
                r, g, b, _ = px[ox + x, oy + y]
                px[ox + x, oy + y] = (r, g, b, alpha)


VOXEL_TILES = [
    "grass_top", "grass_side", "dirt", "stone",
    "cobblestone", "sand", "log_top", "log_side",
    "planks", "leaves", "snow", "gravel",
    "brick", "water", "ice", "glass",
]


def make_voxel_blocks() -> None:
    atlas = Image.new("RGBA", (4 * TILE, 4 * TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    for i, name in enumerate(VOXEL_TILES):
        _voxel_tile(atlas, draw, i, name)
    atlas.save(OUT / "voxel_blocks.png")


if __name__ == "__main__":
    make_voxel_blocks()
    print("Generated atlases in", OUT)
