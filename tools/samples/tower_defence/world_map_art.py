#!/usr/bin/env python3
"""
The world map of the Tower defence campaign drawn as a hand-inked fantasy map (deterministic), at 80 pixels per world
unit so that a 2560 x 1440 screen shows it pixel for pixel:

- ``textures/ui/world_map.png``: the parchment plan, its paper grain and vignette, the western sea with its ripple
  lines and a ship, the inked coast, the rivers between their banks, the hatched mountains (snowy in the north, black
  in the east, a smoking volcano), the painted forests, the fields of the capital, the region and site names, the
  cartouche of the title and the compass rose;
- ``textures/ui/sites.png``: the sites, 4 frames (locked ruin, Horde camp, human keep, the capital);
- ``textures/ui/flags.png``: a banner waving on the pole of a site, 4 frames by row (red of the Horde, blue of the
  realm);
- ``textures/ui/path_dash.png``: a dash of the inked road traced between the sites;
- ``textures/ui/cloud.png`` and ``textures/ui/wave.png``: the clouds drifting over the map and the glints of the sea.

The places are shares of the width and height of the map from its top left (``SITES``, ``CAPITAL``); the road between
two of them is ``road_points``, shared with the world map scene.
"""

from __future__ import annotations

import math
import random
from collections.abc import Sequence
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from tools.samples.tower_defence.medieval_ui import GOLD, INK, WOOD_DARK, parchment

Point = tuple[float, float]
RGB = tuple[int, int, int]

PIXELS = (2560, 1440)
"""Size of the map image."""
WORLD = (32.0, 18.0)
"""World size of the map in the scene."""
PPU = PIXELS[0] / WORLD[0]
"""Pixels of the map image per world unit."""
SITES: tuple[Point, ...] = (
    (0.17, 0.55),
    (0.29, 0.33),
    (0.5, 0.5),
    (0.57, 0.64),
    (0.63, 0.3),
    (0.69, 0.12),
    (0.74, 0.52),
    (0.8, 0.27),
    (0.85, 0.6),
    (0.94, 0.4),
)
"""Places of the ten level sites, in shares of the map from its top left, from west to east."""
CAPITAL: Point = (0.07, 0.62)
"""Place of the human capital the road starts from."""
SITE_NAMES = (
    "Greenmarch Ford",
    "Thornwood Pass",
    "Stonebridge",
    "Mirefen Marsh",
    "Frostmere",
    "Highpeak Watch",
    "Sunscorch Flats",
    "Ashfall Ridge",
    "Ravenhold",
    "The Black Gate",
)
"""Names written under the sites, in the order of ``SITES``."""
NAME_DROP = 2.2
"""World distance from a site down to its name."""

COAST: tuple[Point, ...] = (
    (0.13, -0.05),
    (0.15, 0.05),
    (0.21, 0.1),
    (0.19, 0.17),
    (0.12, 0.2),
    (0.09, 0.27),
    (0.11, 0.34),
    (0.07, 0.42),
    (0.04, 0.5),
    (0.05, 0.58),
    (0.03, 0.66),
    (0.04, 0.74),
    (0.09, 0.8),
    (0.14, 0.86),
    (0.2, 0.93),
    (0.24, 1.05),
)
"""The western coast from north to south: the sea lies west of it."""
RIVER: tuple[Point, ...] = (
    (0.535, 0.12),
    (0.52, 0.22),
    (0.55, 0.32),
    (0.51, 0.44),
    (0.49, 0.58),
    (0.45, 0.7),
    (0.47, 0.82),
    (0.44, 1.04),
)
"""The great river, from the northern peaks through Stonebridge to the southern edge."""
BROOK: tuple[Point, ...] = ((0.34, 0.2), (0.29, 0.29), (0.25, 0.4), (0.19, 0.48), (0.13, 0.48), (0.06, 0.47))
"""The brook from the Thornwood to the sea, forded at Greenmarch."""
LAKE: Point = (0.6, 0.17)
"""Centre of the frozen lake of Frostmere."""
VOLCANO: Point = (0.955, 0.2)
"""Place of the volcano of the Black Lands."""

INK_RGBA = (*INK, 255)
SEA = (128, 166, 170)
MEADOW = (150, 170, 98)
FOREST = (64, 98, 52)
SNOW = (236, 238, 236)
ASH = (96, 78, 72)
WATER = (138, 176, 190)
MARSH = (96, 132, 104)
DUNE = (222, 186, 110)


def px(share: Point) -> Point:
    """The pixel of a share of the map."""
    return share[0] * PIXELS[0], share[1] * PIXELS[1]


def road_points(start: Point, end: Point, index: int, spacing: float = 0.5) -> list[tuple[Point, float]]:
    """Points about ``spacing`` world units apart along the gently bent road from one place to the next (shares of the
    map), with the heading of the road there (radians, counter-clockwise on screen); the ends are left out."""
    a, b = (start[0] * WORLD[0], start[1] * WORLD[1]), (end[0] * WORLD[0], end[1] * WORLD[1])
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    nx, ny = -(b[1] - a[1]), b[0] - a[0]
    length = math.hypot(nx, ny) or 1.0
    bend = 1.4 if index % 2 == 0 else -1.4
    cx, cy = mx + nx / length * bend, my + ny / length * bend
    count = max(3, int(math.hypot(b[0] - a[0], b[1] - a[1]) / spacing))
    out = []
    for k in range(2, count - 1):
        s = k / count
        x = (1 - s) ** 2 * a[0] + 2 * (1 - s) * s * cx + s * s * b[0]
        y = (1 - s) ** 2 * a[1] + 2 * (1 - s) * s * cy + s * s * b[1]
        dx = 2 * (1 - s) * (cx - a[0]) + 2 * s * (b[0] - cx)
        dy = 2 * (1 - s) * (cy - a[1]) + 2 * s * (b[1] - cy)
        out.append(((x / WORLD[0], y / WORLD[1]), math.atan2(-dy, dx)))
    return out


def all_roads() -> list[list[Point]]:
    """The points of every stretch of road, capital first, in shares."""
    places = (CAPITAL, *SITES)
    return [[p for p, _ in road_points(places[i], places[i + 1], i)] for i in range(len(SITES))]


def _smooth(points: Sequence[Point], steps: int = 12) -> list[Point]:
    """A Catmull-Rom curve through points (pixels)."""
    out: list[Point] = []
    padded = [points[0], *points, points[-1]]
    for i in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[i - 1], padded[i], padded[i + 1], padded[i + 2]
        for k in range(steps):
            t = k / steps
            t2, t3 = t * t, t * t * t
            out.append(
                (
                    0.5
                    * (
                        2 * p1[0]
                        + (-p0[0] + p2[0]) * t
                        + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2
                        + (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3
                    ),
                    0.5
                    * (
                        2 * p1[1]
                        + (-p0[1] + p2[1]) * t
                        + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2
                        + (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3
                    ),
                )
            )
    out.append(points[-1])
    return out


def _wobble(points: Sequence[Point], rng: random.Random, depth: int, amount: float) -> list[Point]:
    """A line made ragged by midpoint displacement (a hand-drawn coast)."""
    line = list(points)
    for _ in range(depth):
        out = [line[0]]
        for a, b in zip(line, line[1:], strict=False):
            length = math.hypot(b[0] - a[0], b[1] - a[1])
            nx, ny = -(b[1] - a[1]) / (length or 1), (b[0] - a[0]) / (length or 1)
            offset = rng.uniform(-amount, amount) * length
            out += [((a[0] + b[0]) / 2 + nx * offset, (a[1] + b[1]) / 2 + ny * offset), b]
        line = out
        amount *= 0.8
    return line


def _distance_to(point: Point, line: Sequence[Point]) -> float:
    """Distance from a point to a polyline (pixels)."""
    best = 1e9
    for a, b in zip(line, line[1:], strict=False):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0.0, min(1.0, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / ((dx * dx + dy * dy) or 1)))
        best = min(best, math.hypot(point[0] - a[0] - t * dx, point[1] - a[1] - t * dy))
    return best


def _wash(image: Image.Image, mask: Image.Image, colour: RGB, opacity: float, blur: float) -> None:
    """A watercolour wash of a colour through a mask, its edges soft."""
    soft = mask.filter(ImageFilter.GaussianBlur(blur)) if blur > 0 else mask
    alpha = soft.point(lambda v: int(v * opacity))
    image.paste(Image.new("RGBA", image.size, (*colour, 255)), (0, 0), alpha)


def _ink(image: Image.Image, mask: Image.Image, opacity: float = 1.0, colour: RGB = INK) -> None:
    """Ink through a mask."""
    _wash(image, mask, colour, opacity, 0)


def _ring(mask: Image.Image, grow: int) -> Image.Image:
    """The outline of a mask grown by about ``grow`` pixels (computed at a quarter of the size, then smoothed)."""
    small = mask.resize((mask.width // 4, mask.height // 4), Image.Resampling.BILINEAR)
    for _ in range(max(0, grow // 8)):
        small = small.filter(ImageFilter.MaxFilter(5))
    grown = small.resize(mask.size, Image.Resampling.BILINEAR).filter(ImageFilter.GaussianBlur(3))
    grown = grown.point(lambda v: 255 if v > 127 else 0)
    return grown.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.MaxFilter(3))


def _sea(image: Image.Image, rng: random.Random) -> Image.Image:
    """The western sea west of the coast, its ripple lines and wave marks; returns the land mask."""
    width, height = PIXELS
    coast = _wobble([px(p) for p in COAST], rng, 6, 0.22)
    land = Image.new("L", PIXELS, 0)
    ImageDraw.Draw(land).polygon([*coast, (width + 10, height + 10), (width + 10, -10)], fill=255)
    for centre, radius in (((0.035, 0.11), 34), ((0.07, 0.92), 26), ((0.12, 0.98), 18)):
        cx, cy = px(centre)
        blob = _wobble(
            [(cx + math.cos(a) * radius * 2, cy + math.sin(a) * radius) for a in [k * math.pi / 4 for k in range(9)]],
            rng,
            3,
            0.2,
        )
        ImageDraw.Draw(land).polygon(blob, fill=255)
    sea = ImageChops.invert(land)
    _wash(image, sea, SEA, 0.72, 6)
    for grow, opacity in ((10, 0.75), (24, 0.5), (44, 0.32), (70, 0.18)):
        ring = ImageChops.multiply(_ring(land, grow), sea)
        _ink(image, ring, opacity, (54, 84, 96))
    draw = ImageDraw.Draw(image)
    sea_px = sea.tobytes()
    for _ in range(90):
        x, y = rng.uniform(10, width * 0.24), rng.uniform(10, height - 10)
        if all(sea_px[int(y) * width + int(min(width - 1, x + dx))] > 200 for dx in (-30, 0, 30)):
            draw.arc((x - 12, y - 5, x + 12, y + 5), 200, 340, fill=(54, 84, 96, 200), width=2)
            draw.arc((x + 4, y - 3, x + 22, y + 5), 200, 330, fill=(54, 84, 96, 160), width=2)
    _ship(draw, px((0.055, 0.33)))
    coast_line = land.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.MaxFilter(5))
    _ink(image, coast_line, 0.95)
    shore = ImageChops.subtract(land, land.filter(ImageFilter.MinFilter(15)))
    _wash(image, shore, (110, 120, 80), 0.35, 6)
    return land


def _ship(draw: ImageDraw.ImageDraw, at: Point) -> None:
    """A small sailing ship."""
    x, y = at
    draw.polygon([(x - 34, y), (x + 34, y), (x + 24, y + 14), (x - 26, y + 14)], fill=(120, 76, 42), outline=INK_RGBA)
    draw.line([(x, y), (x, y - 58)], fill=INK_RGBA, width=3)
    draw.polygon([(x + 2, y - 54), (x + 30, y - 30), (x + 2, y - 8)], fill=(238, 228, 200), outline=INK_RGBA)
    draw.polygon([(x - 2, y - 50), (x - 26, y - 14), (x - 2, y - 10)], fill=(226, 214, 186), outline=INK_RGBA)
    draw.polygon([(x, y - 58), (x + 16, y - 54), (x, y - 50)], fill=(60, 84, 160), outline=INK_RGBA)
    for k in range(3):
        draw.arc((x - 50 + k * 26, y + 10, x - 30 + k * 26, y + 20), 200, 340, fill=(54, 84, 96, 220), width=2)


def _regions(image: Image.Image, land: Image.Image) -> None:
    """The washes of the regions: meadows in the west, snow in the north, ash in the east."""
    width, height = PIXELS
    meadow = land.copy()
    _wash(image, meadow, MEADOW, 0.3, 20)
    snow = Image.new("L", PIXELS, 0)
    ImageDraw.Draw(snow).polygon(
        [
            px(p)
            for p in (
                (0.47, -0.02),
                (0.47, 0.2),
                (0.56, 0.3),
                (0.66, 0.4),
                (0.75, 0.3),
                (0.78, 0.1),
                (0.8, -0.02),
            )
        ],
        fill=255,
    )
    _wash(image, snow, SNOW, 0.55, 40)
    ash = Image.linear_gradient("L").rotate(90, expand=True).resize(PIXELS)
    ash = ImageChops.invert(ash).point(lambda v: max(0, int((v - 190) * 3.9)))
    _wash(image, ImageChops.multiply(ash, land), ASH, 0.85, 30)
    wood = Image.new("L", PIXELS, 0)
    ImageDraw.Draw(wood).ellipse((width * 0.21, height * 0.14, width * 0.44, height * 0.5), fill=255)
    _wash(image, wood, FOREST, 0.28, 50)
    for centre, radii, colour, opacity in (
        ((0.57, 0.66), (0.08, 0.08), MARSH, 0.5),
        ((0.74, 0.55), (0.07, 0.09), DUNE, 0.55),
    ):
        mask = Image.new("L", PIXELS, 0)
        cx, cy = px(centre)
        rx, ry = radii[0] * width, radii[1] * height
        ImageDraw.Draw(mask).ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
        _wash(image, mask, colour, opacity, 40)


def _clear_of_sites(x: float, y: float) -> bool:
    """Whether a pixel is away from every site and the ribbon of its name."""
    for sx, sy in (px(p) for p in SITES):
        if math.hypot(x - sx, y - sy) < 120 or (abs(x - sx) < 150 and abs(y - sy - NAME_DROP * PPU) < 40):
            return False
    return True


def _marsh_and_dunes(image: Image.Image, rng: random.Random) -> None:
    """The pools and reeds of Mirefen, the dunes of Sunscorch."""
    draw = ImageDraw.Draw(image)
    for _ in range(26):
        x, y = px((0.57 + rng.uniform(-0.07, 0.07), 0.66 + rng.uniform(-0.07, 0.07)))
        if not _clear_of_sites(x, y):
            continue
        if rng.random() < 0.4:
            w = rng.uniform(16, 30)
            draw.ellipse((x - w, y - w * 0.45, x + w, y + w * 0.45), fill=(*WATER, 255), outline=(*INK, 200), width=2)
        else:
            for k in range(3):
                draw.line([(x + k * 5 - 5, y), (x + k * 6 - 7, y - 14 - k * 3)], fill=(60, 80, 40, 255), width=2)
    for _ in range(22):
        x, y = px((0.74 + rng.uniform(-0.06, 0.06), 0.55 + rng.uniform(-0.08, 0.08)))
        if not _clear_of_sites(x, y):
            continue
        w = rng.uniform(22, 40)
        draw.arc((x - w, y - w * 0.4, x + w, y + w * 0.4), 200, 340, fill=(150, 110, 60, 255), width=2)


def _river(image: Image.Image, points: Sequence[Point], start_width: float, end_width: float) -> list[Point]:
    """A river between inked banks, widening downstream; returns its curve (pixels)."""
    curve = _smooth([px(p) for p in points], 16)
    draw = ImageDraw.Draw(image)
    count = len(curve) - 1
    for k, (a, b) in enumerate(zip(curve, curve[1:], strict=False)):
        w = start_width + (end_width - start_width) * k / count
        draw.line([a, b], fill=INK_RGBA, width=int(w + 6))
        draw.ellipse((b[0] - (w + 6) / 2, b[1] - (w + 6) / 2, b[0] + (w + 6) / 2, b[1] + (w + 6) / 2), fill=INK_RGBA)
    for k, (a, b) in enumerate(zip(curve, curve[1:], strict=False)):
        w = start_width + (end_width - start_width) * k / count
        draw.line([a, b], fill=(*WATER, 255), width=int(w))
        draw.ellipse((b[0] - w / 2, b[1] - w / 2, b[0] + w / 2, b[1] + w / 2), fill=(*WATER, 255))
    for k in range(0, count - 8, 9):
        a, b = curve[k], curve[k + 4]
        draw.line([a, b], fill=(214, 230, 232, 200), width=2)
    return curve


def _lake(image: Image.Image, rng: random.Random) -> list[Point]:
    """The frozen lake of Frostmere, ice cracks on it."""
    cx, cy = px(LAKE)
    shape = _wobble(
        [(cx + math.cos(a) * 170, cy + math.sin(a) * 70) for a in [k * math.pi / 6 for k in range(13)]], rng, 4, 0.12
    )
    draw = ImageDraw.Draw(image)
    draw.polygon(shape, fill=(206, 226, 234, 255), outline=INK_RGBA, width=4)
    for _ in range(9):
        x, y = cx + rng.uniform(-120, 120), cy + rng.uniform(-40, 40)
        crack = [(x, y)]
        for _ in range(3):
            x, y = x + rng.uniform(-24, 24), y + rng.uniform(-10, 10)
            crack.append((x, y))
        draw.line(crack, fill=(120, 150, 170, 255), width=2)
    return shape


def _hatch(size: tuple[int, int], spacing: int, colour: RGB, width: int = 2) -> Image.Image:
    """Parallel diagonal ink lines over an image of a size."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for k in range(-size[1], size[0], spacing):
        draw.line([(k, 0), (k + size[1], size[1])], fill=(*colour, 255), width=width)
    return layer


def _mountain(image: Image.Image, at: Point, size: float, kind: str, rng: random.Random) -> None:
    """A hatched mountain: the lit west face, the east face hatched, an inked crest; snowy, black or a volcano."""
    x, y = at
    peak = (x + rng.uniform(-0.15, 0.15) * size, y - size * rng.uniform(1.05, 1.35))
    left, right = (x - size, y), (x + size, y)
    foot = (x + size * 0.15, y)
    face = {"snow": (176, 184, 196), "black": (88, 76, 74), "volcano": (98, 74, 66)}.get(kind, (190, 168, 128))
    shade = {"snow": (118, 128, 140), "black": (40, 30, 30), "volcano": (60, 34, 30)}.get(kind, (110, 88, 60))
    draw = ImageDraw.Draw(image)
    draw.polygon([left, peak, right], fill=(*face, 255))
    box = (int(x - size - 4), int(peak[1] - 4), int(x + size + 4), int(y + 4))
    w, h = box[2] - box[0], box[3] - box[1]
    mask = Image.new("L", (w, h), 0)
    local = [(p[0] - box[0], p[1] - box[1]) for p in (peak, right, foot)]
    ImageDraw.Draw(mask).polygon(local, fill=255)
    hatch = _hatch((w, h), 5 if size > 40 else 4, shade)
    image.paste(hatch, box[:2], ImageChops.multiply(mask, hatch.getchannel("A")))
    if kind in ("snow", "volcano"):
        cap = 0.32
        mid_l = (peak[0] + (left[0] - peak[0]) * cap, peak[1] + (left[1] - peak[1]) * cap)
        mid_r = (peak[0] + (right[0] - peak[0]) * cap, peak[1] + (right[1] - peak[1]) * cap)
        if kind == "volcano":
            draw.polygon([mid_l, (peak[0], peak[1] + size * 0.12), mid_r], fill=(200, 60, 30, 255))
            draw.line([mid_l, (peak[0] - size * 0.1, peak[1] + size * 0.5)], fill=(230, 110, 40, 255), width=3)
        else:
            notch = [
                (mid_l[0] + (mid_r[0] - mid_l[0]) * t, mid_l[1] + (0.12 if k % 2 else -0.02) * size)
                for k, t in enumerate((0.0, 0.25, 0.5, 0.75, 1.0))
            ]
            draw.polygon([peak, *reversed(notch)], fill=(*SNOW, 255))
    draw.line([left, peak, right], fill=INK_RGBA, width=3, joint="curve")
    draw.line(
        [peak, (peak[0] + (foot[0] - peak[0]) * 0.6, peak[1] + (foot[1] - peak[1]) * 0.6)], fill=INK_RGBA, width=2
    )


def _smoke(image: Image.Image, at: Point) -> None:
    """Smoke curling up from the volcano."""
    layer = Image.new("RGBA", PIXELS, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    x, y = at
    for k in range(5):
        r = 12 + k * 6
        cx, cy = x + math.sin(k * 0.9) * 16 + k * 6, y - 20 - k * 26
        draw.ellipse((cx - r, cy - r * 0.7, cx + r, cy + r * 0.7), fill=(70, 60, 60, 150 - k * 16), outline=INK_RGBA)
    image.alpha_composite(layer.filter(ImageFilter.GaussianBlur(1.2)))


def _tree(draw: ImageDraw.ImageDraw, x: float, y: float, r: float, kind: str) -> None:
    """A painted tree: a round canopy with its light side, a conifer in the snow, a dead tree in the ash."""
    if kind == "dead":
        draw.line([(x, y + r), (x, y - r)], fill=INK_RGBA, width=2)
        for side in (-1, 1):
            draw.line([(x, y - r * 0.2), (x + side * r * 0.7, y - r * 0.8)], fill=INK_RGBA, width=2)
            draw.line([(x, y + r * 0.3), (x + side * r * 0.5, y - r * 0.1)], fill=INK_RGBA, width=1)
        return
    if kind == "pine":
        draw.line([(x, y + r * 0.9), (x, y + r * 1.3)], fill=INK_RGBA, width=2)
        for k in range(3):
            top = y - r * 1.1 + k * r * 0.6
            half = r * (0.45 + 0.22 * k)
            draw.polygon(
                [(x, top), (x - half, top + r * 0.9), (x + half, top + r * 0.9)],
                fill=(58, 88, 70, 255),
                outline=INK_RGBA,
            )
            draw.polygon(
                [(x, top), (x - half * 0.6, top + r * 0.45), (x + half * 0.3, top + r * 0.4)], fill=(*SNOW, 255)
            )
        return
    draw.line([(x, y + r * 0.6), (x, y + r * 1.25)], fill=INK_RGBA, width=2)
    draw.ellipse((x - r, y - r, x + r, y + r), fill=(*FOREST, 255), outline=INK_RGBA, width=2)
    draw.ellipse((x - r * 0.75, y - r * 0.8, x + r * 0.2, y + r * 0.1), fill=(104, 136, 72, 255))
    draw.arc((x - r, y - r, x + r, y + r), 20, 160, fill=(30, 46, 26, 255), width=2)


def _keep_out(points: Sequence[Point]) -> list[Point]:
    """Pixels where nothing is drawn: the sites, the capital and the roads."""
    out = [px(p) for p in (CAPITAL, *SITES)]
    return out + [px(p) for p in points]


def _scatter(
    image: Image.Image,
    land: Image.Image,
    rivers: Sequence[Sequence[Point]],
    lake: Sequence[Point],
    rng: random.Random,
) -> None:
    """Mountains and forests, back to front."""
    width, height = PIXELS
    land_px = land.tobytes()
    roads = [p for road in all_roads() for p in road]
    blocked = _keep_out(roads)
    sites = [px(p) for p in (CAPITAL, *SITES)]
    items: list[tuple[float, str, Point, float]] = []

    def free(x: float, y: float, room: float) -> bool:
        if not (30 < x < width - 30 and 30 < y < height - 30) or land_px[int(y) * width + int(x)] < 128:
            return False
        if any(math.hypot(x - sx, y - sy) < 150 for sx, sy in sites):
            return False
        if any(abs(x - sx) < 150 and abs(y - sy - NAME_DROP * PPU) < 45 for sx, sy in sites[1:]):
            return False
        if any(math.hypot(x - bx, y - by) < room for bx, by in blocked):
            return False
        lx, ly = px(LAKE)
        if ((x - lx) / 200) ** 2 + ((y - ly) / 95) ** 2 < 1:
            return False
        return all(_distance_to((x, y), river) > room for river in rivers)

    for k in range(50):
        x, y = width * (0.48 + 0.34 * k / 50) + rng.uniform(-20, 20), height * rng.uniform(0.06, 0.2)
        if free(x, y, 36) and abs(x - px(LAKE)[0]) > 230:
            items.append((y, "snow", (x, y), rng.uniform(40, 70)))
    for k in range(40):
        x, y = width * rng.uniform(0.9, 1.0), height * (0.05 + 0.85 * k / 40)
        if free(x, y, 40):
            items.append((y, "black", (x, y), rng.uniform(34, 58)))
    for gate_x in (0.885, 0.995):
        x, y = px((gate_x, 0.44))
        items.append((y, "black", (x, y), 74))
    items.append((px(VOLCANO)[1], "volcano", px(VOLCANO), 84))
    areas: list[tuple[Point, Point, int]] = [
        ((0.325, 0.32), (0.12, 0.17), 900),
        ((0.12, 0.38), (0.05, 0.07), 80),
        ((0.36, 0.84), (0.06, 0.05), 70),
        ((0.78, 0.6), (0.04, 0.04), 40),
        ((0.62, 0.36), (0.1, 0.08), 160),
        ((0.82, 0.3), (0.06, 0.12), 90),
        ((0.5, 0.5), (0.5, 0.5), 500),
    ]
    for (gx, gy), (rx, ry), count in areas:
        for _ in range(count):
            a, r = rng.uniform(0, 2 * math.pi), math.sqrt(rng.random())
            sx, sy = gx + math.cos(a) * rx * r, gy + math.sin(a) * ry * r
            x, y = sx * width, sy * height
            kind = "dead" if sx > 0.77 else ("pine" if sy < 0.42 and sx > 0.5 else "tree")
            if count == 500 and rng.random() > (0.12 if kind == "tree" else 0.3):
                continue
            if free(x, y, 26):
                items.append((y, kind, (x, y), rng.uniform(10, 15) if kind != "dead" else rng.uniform(8, 12)))
    items.sort(key=lambda item: item[0])
    draw = ImageDraw.Draw(image)
    for _, kind, (x, y), size in items:
        if kind in ("snow", "black", "volcano"):
            _mountain(image, (x, y), size, kind, rng)
            draw = ImageDraw.Draw(image)
        else:
            _tree(draw, x, y, size, kind)
    _smoke(image, (px(VOLCANO)[0], px(VOLCANO)[1] - 100))


def _hills(image: Image.Image, rng: random.Random) -> None:
    """Rolling hills of the meadows: small inked humps with a hatched east side."""
    draw = ImageDraw.Draw(image)
    sites = [px(p) for p in (CAPITAL, *SITES)]
    for _ in range(26):
        x, y = px((rng.uniform(0.12, 0.72), rng.uniform(0.6, 0.95)))
        labels = [px(at) for _, at in REGIONS] + [px(COMPASS)]
        if any(abs(x - lx) < 260 and abs(y - ly) < 60 for lx, ly in labels) or any(
            abs(x - sx) < 170 and abs(y - sy - NAME_DROP * PPU) < 50 for sx, sy in sites
        ):
            continue
        if (
            any(math.hypot(x - sx, y - sy) < 170 for sx, sy in sites)
            or _distance_to((x, y), _smooth([px(p) for p in RIVER])) < 50
        ):
            continue
        w = rng.uniform(28, 46)
        draw.arc((x - w, y - w * 0.6, x + w, y + w * 0.6), 190, 350, fill=INK_RGBA, width=2)
        for k in range(3):
            hx = x + w * (0.25 + 0.2 * k)
            draw.line([(hx, y - w * 0.32 + k * 6), (hx + 6, y - 4)], fill=(*INK, 160), width=1)


def _tracks(image: Image.Image) -> None:
    """The old tracks between the sites in faint pencil, under the road the world map traces in red."""
    draw = ImageDraw.Draw(image)
    places = (CAPITAL, *SITES)
    for index in range(len(SITES)):
        for share, heading in road_points(places[index], places[index + 1], index, 0.3):
            x, y = px(share)
            dx, dy = math.cos(heading) * 6, -math.sin(heading) * 6
            draw.line([(x - dx, y - dy), (x + dx, y + dy)], fill=(*INK, 90), width=3)


def _fields(image: Image.Image, rng: random.Random) -> None:
    """The ploughed fields around the capital."""
    draw = ImageDraw.Draw(image)
    cx, cy = px(CAPITAL)
    for _ in range(9):
        x, y = cx + rng.uniform(40, 220), cy + rng.uniform(70, 230)
        if math.hypot(x - cx, y - cy) < 110:
            continue
        w, h = rng.uniform(50, 80), rng.uniform(28, 44)
        tone = rng.choice(((206, 180, 100), (170, 160, 90), (190, 150, 90)))
        quad = [(x, y), (x + w, y - 8), (x + w + 6, y + h), (x + 4, y + h + 6)]
        draw.polygon(quad, fill=(*tone, 255), outline=INK_RGBA)
        for k in range(1, 5):
            t = k / 5
            draw.line([(x + 2 + w * t, y - 8 * t + 2), (x + 6 + w * t, y + h + 4 - 6 * t)], fill=(120, 96, 52, 255))


def _spaced(
    draw: ImageDraw.ImageDraw,
    centre: Point,
    text: str,
    font: ImageFont.FreeTypeFont,
    fill: tuple[int, ...],
    spacing: float,
) -> None:
    """A text centred on a point with its letters spaced out."""
    widths = [draw.textlength(c, font=font) for c in text]
    total = sum(widths) + spacing * (len(text) - 1)
    x = centre[0] - total / 2
    for c, w in zip(text, widths, strict=False):
        draw.text((x, centre[1]), c, font=font, fill=fill, anchor="lm")
        x += w + spacing


def _labels(image: Image.Image, fonts: Path) -> None:
    """The region names in spaced faded ink, the site names on small ribbons."""
    draw = ImageDraw.Draw(image)
    region = ImageFont.truetype(str(fonts / "Cinzel.ttf"), 34)
    for text, at in (*REGIONS,):
        _spaced(draw, px(at), text, region, (*INK, 150), 10)
    sea = Image.new("RGBA", (520, 60), (0, 0, 0, 0))
    _spaced(ImageDraw.Draw(sea), (260, 30), "THE WESTERN SEA", region, (40, 66, 78, 190), 12)
    sea = sea.rotate(78, expand=True, resample=Image.Resampling.BICUBIC)
    image.alpha_composite(sea, (int(px((0.035, 0.55))[0] - sea.width / 2), int(px((0.035, 0.55))[1] - sea.height / 2)))
    name_font = ImageFont.truetype(str(fonts / "MedievalSharp.ttf"), 30)
    for share, name in zip(SITES, SITE_NAMES, strict=True):
        x, y = px(share)
        y += NAME_DROP * PPU
        width = draw.textlength(name, font=name_font) + 40
        _ribbon(draw, (x, y), width, 40)
        draw.text((x, y), name, font=name_font, fill=INK_RGBA, anchor="mm")


def _ribbon(draw: ImageDraw.ImageDraw, centre: Point, width: float, height: float) -> None:
    """A parchment ribbon with folded ends."""
    x, y = centre
    half_w, half_h = width / 2, height / 2
    for side in (-1, 1):
        end = x + side * half_w
        tail = [
            (end - side * 6, y - half_h + 8),
            (end + side * 26, y - half_h + 8),
            (end + side * 14, y + 4),
            (end + side * 26, y + half_h + 8),
            (end - side * 6, y + half_h + 8),
        ]
        draw.polygon(tail, fill=(196, 170, 120, 255), outline=INK_RGBA)
    draw.rectangle(
        (x - half_w, y - half_h, x + half_w, y + half_h), fill=(240, 226, 190, 255), outline=INK_RGBA, width=2
    )


def _cartouche(image: Image.Image, fonts: Path, title: str) -> None:
    """The cartouche of the title: a scroll with rolled ends, a gold rule and the title in capitals."""
    draw = ImageDraw.Draw(image)
    cx, cy = px((0.5, 0.075))
    w, h = 560, 112
    draw.rectangle((cx - w / 2 + 6, cy - h / 2 + 8, cx + w / 2 + 6, cy + h / 2 + 8), fill=(40, 26, 14, 90))
    draw.rectangle(
        (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2), fill=(244, 230, 196, 255), outline=INK_RGBA, width=3
    )
    draw.rectangle((cx - w / 2 + 12, cy - h / 2 + 12, cx + w / 2 - 12, cy + h / 2 - 12), outline=(*GOLD, 255), width=3)
    for side in (-1, 1):
        ex = cx + side * w / 2
        draw.ellipse(
            (ex - 22, cy - h / 2 - 10, ex + 22, cy + h / 2 + 10), fill=(214, 192, 150, 255), outline=INK_RGBA, width=3
        )
        draw.ellipse((ex - 8, cy - h / 2 - 2, ex + 8, cy + h / 2 + 2), fill=(170, 140, 96, 255), outline=INK_RGBA)
        for k in (-1, 1):
            draw.polygon(
                [
                    (cx + side * (w / 2 - 70), cy + k * 18),
                    (cx + side * (w / 2 - 46), cy),
                    (cx + side * (w / 2 - 70), cy - k * 0),
                ],
                fill=(*GOLD, 255),
            )
    font = ImageFont.truetype(str(fonts / "Cinzel.ttf"), 64)
    _spaced(draw, (cx, cy + 2), title.upper(), font, (120, 28, 18, 255), 8)


def _compass(image: Image.Image, at: Point, radius: float, fonts: Path) -> None:
    """A compass rose: rings, sixteen points dark and light, the north lettered."""
    draw = ImageDraw.Draw(image)
    x, y = at
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), outline=INK_RGBA, width=3)
    draw.ellipse(
        (x - radius * 0.86, y - radius * 0.86, x + radius * 0.86, y + radius * 0.86), outline=INK_RGBA, width=1
    )
    for k in range(64):
        a = k * math.pi / 32
        inner = radius * (0.86 if k % 4 else 0.8)
        draw.line(
            [
                (x + math.cos(a) * inner, y + math.sin(a) * inner),
                (x + math.cos(a) * radius * 0.98, y + math.sin(a) * radius * 0.98),
            ],
            fill=INK_RGBA,
            width=1,
        )
    for k in range(16):
        a = -math.pi / 2 + k * math.pi / 8
        length = radius * (1.25 if k % 4 == 0 else (0.75 if k % 2 == 0 else 0.5))
        width = math.pi / (8 if k % 4 == 0 else 12)
        tip = (x + math.cos(a) * length, y + math.sin(a) * length)
        side_l = (x + math.cos(a - width) * radius * 0.18, y + math.sin(a - width) * radius * 0.18)
        side_r = (x + math.cos(a + width) * radius * 0.18, y + math.sin(a + width) * radius * 0.18)
        dark = (120, 28, 18, 255) if k == 0 else INK_RGBA
        draw.polygon([(x, y), side_l, tip], fill=dark if k % 4 == 0 else (90, 70, 50, 255), outline=INK_RGBA)
        draw.polygon([(x, y), tip, side_r], fill=(244, 230, 196, 255), outline=INK_RGBA)
    draw.ellipse((x - 8, y - 8, x + 8, y + 8), fill=(*GOLD, 255), outline=INK_RGBA, width=2)
    font = ImageFont.truetype(str(fonts / "Cinzel.ttf"), 40)
    draw.text((x, y - radius * 1.25 - 26), "N", font=font, fill=(120, 28, 18, 255), anchor="mm")


def _finish(image: Image.Image) -> Image.Image:
    """Paper grain, the vignette and the inked border with its gold rule and corner pieces."""
    width, height = PIXELS
    grain = Image.effect_noise(PIXELS, 40).point(lambda v: 255 - int(abs(v - 128) * 0.35))
    image = ImageChops.multiply(image, Image.merge("RGBA", (grain, grain, grain, Image.new("L", PIXELS, 255))))
    vignette = Image.radial_gradient("L").resize(PIXELS, Image.Resampling.BICUBIC)
    vignette = vignette.point(lambda v: 255 - int(max(0, v - 120) * 0.9))
    image = ImageChops.multiply(image, Image.merge("RGBA", (vignette, vignette, vignette, Image.new("L", PIXELS, 255))))
    draw = ImageDraw.Draw(image)
    draw.rectangle((4, 4, width - 5, height - 5), outline=(*WOOD_DARK, 255), width=10)
    draw.rectangle((20, 20, width - 21, height - 21), outline=INK_RGBA, width=3)
    draw.rectangle((28, 28, width - 29, height - 29), outline=(*GOLD, 255), width=2)
    for cx, cy in ((24, 24), (width - 25, 24), (24, height - 25), (width - 25, height - 25)):
        draw.polygon([(cx, cy - 22), (cx + 22, cy), (cx, cy + 22), (cx - 22, cy)], fill=(*GOLD, 255), outline=INK_RGBA)
        draw.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=(120, 28, 18, 255))
    return image


def world_map(fonts: Path, title: str = "The Realm") -> Image.Image:
    """The whole map image."""
    rng = random.Random(9)
    image = parchment(PIXELS, 77)
    land = _sea(image, rng)
    _regions(image, land)
    lake = _lake(image, rng)
    river = _river(image, RIVER, 8, 22)
    brook = _river(image, BROOK, 5, 12)
    _fields(image, rng)
    _scatter(image, land, [river, brook], lake, rng)
    _hills(image, rng)
    _tracks(image)
    _marsh_and_dunes(image, rng)
    _compass(image, px(COMPASS), 70, fonts)
    _labels(image, fonts)
    _cartouche(image, fonts, title)
    return _finish(image)


CELL = 192
"""Side of a site cell, in pixels (2.4 world units on the map)."""
POLE_TOP = (172, 30)
"""Pixel of the top of the banner pole in a site cell."""


def _wall(draw: ImageDraw.ImageDraw, box: tuple[float, float, float, float], stone: RGB) -> None:
    """A stone wall with its crenels and joints."""
    x0, y0, x1, y1 = box
    draw.rectangle(box, fill=(*stone, 255), outline=INK_RGBA, width=3)
    for x in range(int(x0), int(x1) - 6, 16):
        draw.rectangle((x, y0 - 10, x + 9, y0), fill=(*stone, 255), outline=INK_RGBA, width=2)
    for row, y in enumerate(range(int(y0) + 10, int(y1) - 4, 11)):
        draw.line([(x0 + 3, y), (x1 - 3, y)], fill=(*INK, 110), width=1)
        for x in range(int(x0) + (8 if row % 2 else 16), int(x1) - 4, 16):
            draw.line([(x, y), (x, y + 11)], fill=(*INK, 110), width=1)


def site_sheet() -> Image.Image:
    """The sites, 4 frames: locked (a grey ruin), open (a Horde war camp), done (a human keep) and the capital."""
    sheet = Image.new("RGBA", (CELL * 4, CELL), (0, 0, 0, 0))
    for frame in range(4):
        image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw.ellipse((22, CELL - 46, CELL - 22, CELL - 12), fill=(40, 26, 14, 70))
        mid = CELL // 2
        if frame == 1:
            for k in range(-4, 5):
                x = mid + k * 15
                top = CELL - 72 - (k % 2) * 8
                draw.polygon(
                    [(x - 7, CELL - 30), (x - 7, top + 10), (x, top), (x + 7, top + 10), (x + 7, CELL - 30)],
                    fill=(120, 78, 44, 255),
                    outline=INK_RGBA,
                )
            draw.polygon(
                [(mid - 40, CELL - 76), (mid - 4, CELL - 124), (mid + 32, CELL - 76)],
                fill=(150, 104, 60, 255),
                outline=INK_RGBA,
                width=3,
            )
            draw.polygon([(mid - 16, CELL - 76), (mid - 4, CELL - 100), (mid + 8, CELL - 76)], fill=INK_RGBA)
            for side in (-1, 1):
                draw.arc(
                    (mid - 4 + side * 6 - 16, CELL - 140, mid - 4 + side * 6 + 16, CELL - 112),
                    90 if side < 0 else 0,
                    180 if side < 0 else 90,
                    fill=(236, 228, 200, 255),
                    width=5,
                )
            draw.ellipse((mid + 34, CELL - 52, mid + 58, CELL - 36), fill=(230, 120, 40, 255), outline=INK_RGBA)
        elif frame == 0:
            stone = (132, 128, 122)
            _wall(draw, (44, CELL - 84, 108, CELL - 30), stone)
            draw.polygon(
                [(108, CELL - 30), (108, CELL - 64), (124, CELL - 54), (134, CELL - 30)],
                fill=(*stone, 255),
                outline=INK_RGBA,
            )
            draw.line([(64, CELL - 84), (78, CELL - 56), (70, CELL - 36)], fill=INK_RGBA, width=3)
            for k in range(5):
                draw.ellipse(
                    (36 + k * 22, CELL - 34, 50 + k * 22, CELL - 24), fill=(110, 106, 100, 255), outline=INK_RGBA
                )
        else:
            stone = (214, 200, 170) if frame == 2 else (232, 220, 190)
            roof = (60, 84, 160) if frame == 2 else (150, 40, 32)
            big = frame == 3
            _wall(draw, (34, CELL - 78, CELL - 34, CELL - 28), stone)
            draw.rectangle(
                (mid - 14, CELL - 56, mid + 14, CELL - 28), fill=(70, 46, 28, 255), outline=INK_RGBA, width=2
            )
            for tx in (
                ((30, 58) if not big else (24, 54)),
                ((CELL - 58, CELL - 30) if not big else (CELL - 54, CELL - 24)),
            ):
                _wall(draw, (tx[0], CELL - 108, tx[1], CELL - 28), stone)
                draw.polygon(
                    [(tx[0] - 6, CELL - 112), ((tx[0] + tx[1]) / 2, CELL - 146), (tx[1] + 6, CELL - 112)],
                    fill=(*roof, 255),
                    outline=INK_RGBA,
                    width=2,
                )
            keep_top = CELL - (150 if big else 130)
            _wall(draw, (mid - 22, keep_top, mid + 22, CELL - 56), stone)
            draw.polygon(
                [(mid - 28, keep_top - 4), (mid, keep_top - 44), (mid + 28, keep_top - 4)],
                fill=(*roof, 255),
                outline=INK_RGBA,
                width=3,
            )
            draw.rectangle((mid - 5, keep_top + 16, mid + 5, keep_top + 30), fill=INK_RGBA)
            if big:
                draw.ellipse((mid - 8, keep_top - 58, mid + 8, keep_top - 42), fill=(*GOLD, 255), outline=INK_RGBA)
        if frame in (1, 2):
            draw.line([(POLE_TOP[0], POLE_TOP[1]), (POLE_TOP[0], CELL - 40)], fill=INK_RGBA, width=4)
            draw.ellipse(
                (POLE_TOP[0] - 5, POLE_TOP[1] - 6, POLE_TOP[0] + 5, POLE_TOP[1] + 4),
                fill=(*GOLD, 255),
                outline=INK_RGBA,
            )
        sheet.paste(image, (frame * CELL, 0))
    return sheet


FLAG_CELL = 64
"""Side of a banner frame, in pixels; the pole is its left edge."""


def flag_sheet() -> Image.Image:
    """The banner of a site waving, 4 frames by row: red with the Horde skull, blue with the gold crown."""
    sheet = Image.new("RGBA", (FLAG_CELL * 4, FLAG_CELL * 2), (0, 0, 0, 0))
    for row, colour in enumerate(((176, 32, 24), (52, 82, 168))):
        for frame in range(4):
            image = Image.new("RGBA", (FLAG_CELL, FLAG_CELL), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            phase = frame * math.pi / 2
            top = [(x, 8 + 4 * math.sin(x / 10 + phase) * x / 56) for x in range(2, 58, 4)]
            bottom = [(x, 36 + 4 * math.sin(x / 10 + phase) * x / 56) for x in range(54, 1, -4)]
            tail_y = 22 + 4 * math.sin(5.6 + phase)
            shape = [*top, (60, tail_y - 6), (50, tail_y), (60, tail_y + 6), *bottom]
            draw.polygon(shape, fill=(*colour, 255), outline=INK_RGBA)
            for k, (x, y) in enumerate(top[:-1]):
                if k % 2:
                    shade = (int(colour[0] * 0.7), int(colour[1] * 0.7), int(colour[2] * 0.7), 255)
                    draw.line([(x, y + 3), (x, y + 25)], fill=shade, width=3)
            cy = 22 + 2 * math.sin(2.4 + phase)
            if row == 0:
                draw.ellipse((18, cy - 9, 34, cy + 5), fill=(236, 228, 200, 255), outline=INK_RGBA)
                draw.rectangle((22, cy + 3, 30, cy + 9), fill=(236, 228, 200, 255), outline=INK_RGBA)
                draw.ellipse((21, cy - 4, 25, cy), fill=INK_RGBA)
                draw.ellipse((27, cy - 4, 31, cy), fill=INK_RGBA)
            else:
                draw.polygon(
                    [(17, cy + 6), (17, cy - 6), (21, cy), (26, cy - 8), (31, cy), (35, cy - 6), (35, cy + 6)],
                    fill=(*GOLD, 255),
                    outline=INK_RGBA,
                )
            sheet.paste(image, (frame * FLAG_CELL, row * FLAG_CELL))
    return sheet


def path_dash() -> Image.Image:
    """A dash of the inked road, along the width."""
    image = Image.new("RGBA", (32, 12), (0, 0, 0, 0))
    ImageDraw.Draw(image).rounded_rectangle((2, 3, 29, 8), radius=3, fill=(110, 30, 20, 255), outline=INK_RGBA)
    return image


def cloud() -> Image.Image:
    """A puffy cloud of old maps, white and inked, see-through."""
    width, height = 480, 220
    layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    puffs = ((120, 130, 70), (200, 100, 86), (290, 112, 78), (360, 140, 58), (210, 150, 70), (90, 160, 46))
    for x, y, r in puffs:
        draw.ellipse((x - r - 3, y - r * 0.8 - 3, x + r + 3, y + r * 0.8 + 3), fill=INK_RGBA)
    for x, y, r in puffs:
        draw.ellipse((x - r, y - r * 0.8, x + r, y + r * 0.8), fill=(248, 246, 240, 255))
    draw.rectangle((40, 160, 440, 220), fill=(0, 0, 0, 0))
    draw.line([(52, 160), (420, 160)], fill=INK_RGBA, width=3)
    for x, y, r in puffs[:4]:
        draw.arc((x - r * 0.6, y - r * 0.5, x + r * 0.5, y + r * 0.4), 200, 300, fill=(200, 200, 206, 255), width=3)
    alpha = layer.getchannel("A").point(lambda v: int(v * 0.82))
    layer.putalpha(alpha)
    return layer


def wave() -> Image.Image:
    """A glint of the sea: two inked wave crests."""
    image = Image.new("RGBA", (64, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.arc((2, 6, 34, 22), 200, 340, fill=(230, 244, 246, 255), width=3)
    draw.arc((28, 2, 62, 18), 200, 340, fill=(230, 244, 246, 255), width=3)
    return image


REGIONS: tuple[tuple[str, Point], ...] = (
    ("THE GREENMARCH", (0.27, 0.71)),
    ("THORNWOOD", (0.3, 0.115)),
    ("FROSTPEAKS", (0.76, 0.035)),
    ("THE BLACK LANDS", (0.7, 0.735)),
)
"""The region names and their places."""
COMPASS: Point = (0.42, 0.7)
"""Place of the compass rose."""

SEA_GLINTS: tuple[Point, ...] = (
    (0.04, 0.2),
    (0.09, 0.3),
    (0.03, 0.4),
    (0.025, 0.6),
    (0.06, 0.82),
    (0.12, 0.92),
    (0.06, 0.05),
)
"""Places of the sea glints."""


def write_all(folder: Path, fonts: Path) -> None:
    """Every image of the world map, into ``textures/ui/``."""
    world_map(fonts).save(folder / "world_map.png")
    site_sheet().save(folder / "sites.png")
    flag_sheet().save(folder / "flags.png")
    path_dash().save(folder / "path_dash.png")
    cloud().save(folder / "cloud.png")
    wave().save(folder / "wave.png")
