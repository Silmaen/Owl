#!/usr/bin/env python3
"""
The scenery under the menus of the Tower defence sample game, in the pixel art of the Horde (``creatures.py``): small
3D models of ellipsoids, capsules and boxes splatted through the same camera and shading.

- ``textures/menu/keep.png``: the human keep, a curtain wall between four round towers under blue roofs, the hall
  behind it and its banners; ``textures/menu/keep_fallen.png``: the same keep burnt and broken (the game over screen);
- ``textures/menu/tower.png``: an archer tower of stone with its wooden hoard and red roof;
- ``textures/menu/ground.png``: the meadow and its dirt road, tiling along the width.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples.tower_defence.creatures import Model, render

STONE = (178, 172, 158)
STONE_DARK = (128, 122, 112)
ROOF_BLUE = (62, 86, 164)
ROOF_RED = (158, 44, 34)
WOOD = (120, 80, 44)
DARK = (40, 30, 26)
GOLD = (220, 176, 70)
BANNER = (52, 82, 170)
ASH = (70, 62, 58)


def _cone(m: Model, base: tuple[float, float, float], radius: float, height: float, colour: tuple[int, int, int]):
    """A pointed roof: discs shrinking to a tip."""
    steps = 10
    for k in range(steps):
        t = k / steps
        r = radius * (1.0 - t) + 0.01
        m.ellipsoid((base[0], base[1] + height * t, base[2]), (r, height / steps * 0.8, r), colour)


def _crenels(m: Model, centre: tuple[float, float, float], half: tuple[float, float], colour: tuple[int, int, int]):
    """Merlons along the four sides of a wall top."""
    x, y, z = centre
    hx, hz = half
    step = 0.09
    count_x, count_z = max(2, int(2 * hx / step)), max(2, int(2 * hz / step))
    for k in range(0, count_x + 1, 2):
        for side in (-1.0, 1.0):
            m.box((x - hx + k * step, y, z + side * hz), (0.025, 0.035, 0.025), colour)
    for k in range(0, count_z + 1, 2):
        for side in (-1.0, 1.0):
            m.box((x + side * hx, y, z - hz + k * step), (0.025, 0.035, 0.025), colour)


def keep(fallen: bool = False) -> Model:
    """The human keep, about 1.6 wide and 1.7 high in creature units (``fallen``: burnt, roofless and broken)."""
    m = Model(stride=0.0, bob=0.0)
    stone = ASH if fallen else STONE
    dark = DARK if fallen else STONE_DARK
    m.box((0.0, 0.28, -0.25), (0.42, 0.28, 0.32), stone)
    m.box((0.0, 0.72, -0.25), (0.3, 0.2, 0.26), stone)
    if not fallen:
        _crenels(m, (0.0, 0.95, -0.25), (0.3, 0.26), stone)
        _cone(m, (0.0, 0.95, -0.25), 0.34, 0.45, ROOF_BLUE)
        m.capsule((0.0, 1.38, -0.25), (0.0, 1.62, -0.25), 0.008, WOOD)
        m.box((0.07, 1.57, -0.25), (0.07, 0.04, 0.005), BANNER)
    m.box((0.0, 0.18, 0.3), (0.55, 0.18, 0.04), stone)
    if not fallen:
        _crenels(m, (0.0, 0.38, 0.3), (0.55, 0.04), stone)
    m.box((0.0, 0.12, 0.345), (0.11, 0.12, 0.006), DARK)
    m.ellipsoid((0.0, 0.24, 0.345), (0.11, 0.05, 0.006), DARK)
    for x in (-0.6, 0.6):
        height = 0.32 if fallen and x > 0 else 0.5
        m.capsule((x, 0.0, 0.3), (x, height, 0.3), 0.14, stone)
        m.ellipsoid((x, height, 0.3), (0.16, 0.03, 0.16), dark)
        if not fallen:
            _cone(m, (x, height + 0.02, 0.3), 0.18, 0.32, ROOF_BLUE)
            m.capsule((x, height + 0.32, 0.3), (x, height + 0.5, 0.3), 0.006, WOOD)
            m.box((x + 0.05, height + 0.46, 0.3), (0.05, 0.03, 0.004), BANNER)
        for k in range(3):
            m.box((x, 0.12 + k * 0.12, 0.44), (0.015, 0.03, 0.004), DARK)
    if fallen:
        for k in range(9):
            rx = -0.5 + k * 0.12
            m.ellipsoid((rx, 0.03, 0.45 + 0.04 * (k % 2)), (0.06, 0.035, 0.05), dark)
    return m


def archer_tower() -> Model:
    """An archer tower, about 0.5 wide and 1.4 high: a round stone shaft, a wooden hoard, a red roof and a pennant."""
    m = Model(stride=0.0, bob=0.0)
    m.capsule((0.0, 0.0, 0.0), (0.0, 0.8, 0.0), 0.17, STONE)
    m.ellipsoid((0.0, 0.03, 0.0), (0.2, 0.04, 0.2), STONE_DARK)
    for k in range(3):
        m.box((0.0, 0.25 + k * 0.2, 0.17), (0.012, 0.035, 0.004), DARK)
    m.ellipsoid((0.0, 0.86, 0.0), (0.24, 0.08, 0.24), WOOD)
    for k in range(8):
        a = k * math.pi / 4
        m.capsule(
            (math.cos(a) * 0.22, 0.86, math.sin(a) * 0.22), (math.cos(a) * 0.22, 0.98, math.sin(a) * 0.22), 0.015, WOOD
        )
    _cone(m, (0.0, 0.98, 0.0), 0.27, 0.34, ROOF_RED)
    m.capsule((0.0, 1.3, 0.0), (0.0, 1.45, 0.0), 0.006, WOOD)
    m.box((0.05, 1.42, 0.0), (0.05, 0.025, 0.004), GOLD)
    return m


def sprite(model: Model, size: tuple[int, int], foot: tuple[int, int]) -> Image.Image:
    """A model drawn facing the viewer on a canvas of a size, its foot on a pixel."""
    return render(model, 0, 0, size, foot)


def ground(size: tuple[int, int] = (240, 112), seed: int = 5) -> Image.Image:
    """The meadow strip and its road, in pixel art, tiling along the width: grass with tufts and flowers, the road of
    packed earth with its ruts and stones, darker grass in front."""
    width, height = size
    rng = random.Random(seed)
    image = Image.new("RGBA", size, (58, 104, 50, 255))
    draw = ImageDraw.Draw(image)
    road_top, road_bottom = 26, 50
    draw.rectangle((0, 0, width, road_top), fill=(66, 116, 54, 255))
    draw.rectangle((0, road_top, width, road_bottom), fill=(134, 104, 66, 255))
    draw.line([(0, road_top), (width, road_top)], fill=(92, 72, 44, 255), width=2)
    draw.line([(0, road_bottom), (width, road_bottom)], fill=(76, 58, 36, 255), width=2)
    for y in (road_top + 8, road_bottom - 8):
        draw.line([(0, y), (width, y)], fill=(112, 86, 54, 255), width=2)
    draw.rectangle((0, road_bottom + 1, width, height), fill=(50, 92, 44, 255))
    for _ in range(46):
        x, y = rng.randrange(width), rng.randrange(road_top + 3, road_bottom - 3)
        tone = rng.choice(((150, 140, 124), (118, 110, 98), (164, 128, 86)))
        draw.rectangle((x, y, x + rng.choice((1, 2)), y + 1), fill=(*tone, 255))
    for _ in range(160):
        x, y = (
            rng.randrange(width),
            rng.choice((rng.randrange(0, road_top - 2), rng.randrange(road_bottom + 3, height))),
        )
        shade = (84, 140, 64) if y < road_top else (68, 120, 56)
        draw.line([(x, y), (x + rng.choice((-1, 0, 1)), y - rng.randrange(2, 5))], fill=(*shade, 255), width=1)
    for _ in range(14):
        x, y = (
            rng.randrange(width),
            rng.choice((rng.randrange(2, road_top - 4), rng.randrange(road_bottom + 6, height - 2))),
        )
        draw.point((x, y), fill=rng.choice(((240, 220, 120, 255), (236, 236, 236, 255), (200, 90, 120, 255))))
    # Columns of a tile repeat seamlessly: the first column copies the last.
    image.paste(image.crop((width - 1, 0, width, height)), (0, 0))
    return image


def write_all(target: Path) -> None:
    """Every image of the menu scenery into ``textures/menu/``."""
    target.mkdir(parents=True, exist_ok=True)
    sprite(keep(), (240, 200), (120, 160)).save(target / "keep.png")
    sprite(keep(fallen=True), (240, 200), (120, 160)).save(target / "keep_fallen.png")
    sprite(archer_tower(), (96, 150), (48, 136)).save(target / "tower.png")
    ground().save(target / "ground.png")
