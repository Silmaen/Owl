"""
Terrain tiles: the flat and sloped diamonds, the overlays and the foundations of the isometric tileset.

The slope table mirrors ``scene::terrain`` of the engine: corner heights (back, right, front, left) relative to the
lowest corner, in steps of half a world unit.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from .common import (
    RGB,
    STEP,
    TILE_H,
    TILE_W,
    Z_PX,
    Layers,
    Vec3,
    bump,
    fbm,
    hash01,
    light,
    mix,
    normalize,
    scale_color,
    smoothstep,
)

SLOPES: list[tuple[int, int, int, int]] = [
    (0, 0, 0, 0),
    (1, 0, 0, 0),
    (0, 1, 0, 0),
    (0, 0, 1, 0),
    (0, 0, 0, 1),
    (0, 1, 1, 1),
    (1, 0, 1, 1),
    (1, 1, 0, 1),
    (1, 1, 1, 0),
    (1, 1, 0, 0),
    (0, 1, 1, 0),
    (0, 0, 1, 1),
    (1, 0, 0, 1),
    (1, 0, 1, 0),
    (0, 1, 0, 1),
    (2, 1, 0, 1),
    (1, 2, 1, 0),
    (0, 1, 2, 1),
    (1, 0, 1, 2),
]
"""The 19 slopes: flat, 4 corners up, 4 corners down, 4 edges up, 2 saddles, 4 steep. Same order as the engine."""
SLOPE_COUNT = len(SLOPES)
"""Number of slopes, flat included."""

Profile = tuple[int, int, int, int]
Point = tuple[float, float]


@dataclass(frozen=True)
class Sample:
    """
    What a terrain material sees at one pixel.
    """

    wx: float
    """World abscissa inside the cell, 0..1."""
    wy: float
    """World ordinate inside the cell, 0..1."""
    ix: int
    """Pixel column in the tile, for per-pixel noise."""
    iy: int
    """Pixel row in the tile, for per-pixel noise."""
    seed: int
    """Seed of the tile."""


Material = Callable[[Sample], RGB]
Relief = Callable[[Sample], float]
"""Height of a ground surface above its plane in world units (a few hundredths); it only bends the normal."""

SUN_DX, SUN_DY = -0.35, 0.35
"""Horizontal direction towards the sun in the world plane (``LIGHT``), where a bump casts its shadow away from."""


def _shadowed(
    seed: int, s: Sample, cells: int, presence: float, radius: float, reach: float, power: float = 1.0
) -> float:
    """Shadow cast on a point by the bumps towards the sun: how much higher the sun side is than the point."""
    here = bump(seed, s.wx, s.wy, cells, presence, radius, power)[0]
    toward = bump(seed, s.wx + SUN_DX * reach, s.wy + SUN_DY * reach, cells, presence, radius, power)[0]
    return max(0.0, toward - here)


def _flower(s: Sample, density: float) -> tuple[float, float]:
    """Flower of the cell under the point: height in 0..1 (a 0.2 radius dome) and its tag."""
    return bump(s.seed + 31, s.wx, s.wy, 7, density, 0.27, 0.5)


def _grass_relief(density: float) -> Relief:
    def relief(s: Sample) -> float:
        tuft = bump(s.seed + 21, s.wx, s.wy, 6, 0.5, 0.45, 1.0)[0]
        return 0.02 * tuft + 0.012 * fbm(s.seed, s.wx * 7.0, s.wy * 7.0) + 0.016 * _flower(s, density)[0]

    return relief


def _grass(base: RGB, flowers: tuple[RGB, ...] = (), density: float = 0.0) -> Material:
    def material(s: Sample) -> RGB:
        n = fbm(s.seed, s.wx * 7.0, s.wy * 7.0)
        color = scale_color(base, (0.84 + 0.32 * n) * (0.93 + 0.14 * fbm(s.seed + 90, s.wx * 1.6, s.wy * 1.6)))
        r = hash01(s.seed + 7, s.ix, s.iy)
        if r > 0.93:
            color = scale_color(color, 1.05)
        elif r < 0.05:
            color = scale_color(color, 0.95)
        tuft, tag = bump(s.seed + 21, s.wx, s.wy, 6, 0.5, 0.45, 1.0)
        if tuft > 0.0:
            blade = 0.5 + 0.5 * math.sin((s.wx * 1.0 + s.wy * 0.4) * 190.0 + tag * 20.0)
            color = scale_color(color, 0.96 + 0.1 * tuft + 0.03 * blade * tuft)
            color = mix(color, (170.0, 190.0, 80.0), 0.08 * tag * tuft)
        elif _shadowed(s.seed + 21, s, 6, 0.5, 0.45, 0.04, 1.0) > 0.0:
            color = scale_color(color, 0.9)
        if flowers and density > 0.0:
            height, tag = _flower(s, density)
            if height > 0.0:
                petal = flowers[int(tag * 97.0) % len(flowers)]
                if height > 0.78:
                    color = (244.0, 200.0, 56.0) if sum(petal) < 700.0 else (236.0, 176.0, 40.0)
                else:
                    color = scale_color(petal, 0.9 + 0.15 * height)
            elif _shadowed(s.seed + 31, s, 7, density, 0.27, 0.025, 0.5) > 0.0:
                color = scale_color(color, 0.78)
        return color

    return material


def _sand_relief(s: Sample) -> float:
    ripple = math.sin((s.wx * 1.0 + s.wy * 0.55) * 31.0 + fbm(s.seed, s.wx * 3.0, s.wy * 3.0) * 6.0)
    return 0.007 * ripple + 0.02 * bump(s.seed + 61, s.wx, s.wy, 10, 0.08, 0.3, 0.5)[0]


def _sand(s: Sample) -> RGB:
    ripple = math.sin((s.wx * 1.0 + s.wy * 0.55) * 31.0 + fbm(s.seed, s.wx * 3.0, s.wy * 3.0) * 6.0)
    color = scale_color((214.0, 196.0, 128.0), 0.92 + 0.1 * fbm(s.seed + 3, s.wx * 9.0, s.wy * 9.0) + 0.04 * ripple)
    if hash01(s.seed + 5, s.ix, s.iy) > 0.94:
        color = scale_color(color, 0.86)
    pebble = bump(s.seed + 61, s.wx, s.wy, 10, 0.08, 0.3, 0.5)[0]
    if pebble > 0.0:
        color = mix(color, (160.0, 146.0, 120.0), 0.7)
    elif _shadowed(s.seed + 61, s, 10, 0.08, 0.3, 0.025, 0.5) > 0.0:
        color = scale_color(color, 0.82)
    return color


def _rings(s: Sample) -> float:
    """Concentric ripples around a few drops: -1..1 close to a centre, 0 away from every centre."""
    gx, gy = s.wx * 3.0, s.wy * 3.0
    cx, cy = min(2, max(0, math.floor(gx))), min(2, max(0, math.floor(gy)))
    if hash01(s.seed + 71, cx, cy) > 0.75:
        return 0.0
    px, py = cx + 0.3 + 0.4 * hash01(s.seed + 72, cx, cy), cy + 0.3 + 0.4 * hash01(s.seed + 73, cx, cy)
    radius = math.hypot(gx - px, (gy - py) * 1.0)
    if radius >= 0.5:
        return 0.0
    return math.sin(radius * 46.0) * (1.0 - radius / 0.5) ** 2


def _waves(s: Sample) -> tuple[float, float]:
    wave = math.sin((s.wx - s.wy) * 20.0 + fbm(s.seed, s.wx * 4.0, s.wy * 4.0) * 7.0)
    wave2 = math.sin((s.wx + s.wy) * 13.0 + fbm(s.seed + 9, s.wx * 3.0, s.wy * 3.0) * 5.0)
    return wave, wave2


def _water_relief(s: Sample) -> float:
    wave, wave2 = _waves(s)
    return 0.007 * (0.6 * wave + 0.4 * wave2) + 0.006 * _rings(s)


def _water(s: Sample) -> RGB:
    wave, wave2 = _waves(s)
    ring = _rings(s)
    color = scale_color((58.0, 112.0, 190.0), 0.94 + 0.07 * wave2 + 0.04 * ring)
    if wave > 0.93:
        color = mix(color, (206.0, 230.0, 250.0), 0.55)
    elif wave > 0.8:
        color = mix(color, (110.0, 160.0, 220.0), 0.5)
    if ring > 0.55:
        color = mix(color, (196.0, 224.0, 248.0), 0.5 * (ring - 0.55) / 0.45)
    return color


NORTH, EAST, SOUTH, WEST = 1, 2, 4, 8
"""Bits of a road neighbour mask: -y, +x, +y and -x of the cell in the world."""
ROAD_X_MASK = EAST | WEST
"""Mask of the straight road along X (terrain id 3)."""
ROAD_Y_MASK = NORTH | SOUTH
"""Mask of the straight road along Y (terrain id 11)."""
ROAD_MASK_ORDER = (
    0,
    NORTH,
    EAST,
    SOUTH,
    WEST,
    NORTH | EAST,
    EAST | SOUTH,
    SOUTH | WEST,
    WEST | NORTH,
    NORTH | EAST | SOUTH,
    EAST | SOUTH | WEST,
    SOUTH | WEST | NORTH,
    WEST | NORTH | EAST,
    NORTH | EAST | SOUTH | WEST,
)
"""The masks beyond the straights, in id order: isolated, 4 dead ends, 4 corners, 4 T, cross."""
ROAD_MASK_NAMES: dict[int, str] = {
    0: "road alone",
    NORTH: "road end n",
    EAST: "road end e",
    SOUTH: "road end s",
    WEST: "road end w",
    NORTH | EAST: "road corner ne",
    EAST | SOUTH: "road corner es",
    SOUTH | WEST: "road corner sw",
    WEST | NORTH: "road corner wn",
    NORTH | EAST | SOUTH: "road t nes",
    EAST | SOUTH | WEST: "road t esw",
    SOUTH | WEST | NORTH: "road t swn",
    WEST | NORTH | EAST: "road t wne",
    NORTH | EAST | SOUTH | WEST: "road cross",
}
"""Editor name of each of those masks."""

CONNECTION_NAMES = ((NORTH, "north"), (EAST, "east"), (SOUTH, "south"), (WEST, "west"))
"""File name of each bit of a connections mask (the same sides as the engine's ``nav::Direction``)."""
FIRST_JUNCTION_ID = 12
"""Terrain id of the first road variant beyond the two straights."""
ROAD_MASKS: dict[int, int] = {
    ROAD_X_MASK: 3,
    ROAD_Y_MASK: 11,
    **{mask: FIRST_JUNCTION_ID + offset for offset, mask in enumerate(ROAD_MASK_ORDER)},
}
"""Overlay tile id of each of the 16 road neighbour masks."""


def _road_mask(mask: int) -> Material:
    """
    Material of a road cell joined to the neighbours of ``mask``.

    The asphalt fills the cell; a white edge line runs along every side that leads nowhere and the dashed centre line
    runs out to every side that is joined, so a straight road is the plain road of its axis.
    """

    def material(s: Sample) -> RGB:
        color = scale_color(
            (96.0, 96.0, 104.0), 0.88 + 0.2 * hash01(s.seed, s.ix, s.iy) + 0.1 * fbm(s.seed, s.wx * 6, s.wy * 6)
        )
        sides = (
            (s.wy < 0.07, NORTH),
            (s.wx > 0.93, EAST),
            (s.wy > 0.93, SOUTH),
            (s.wx < 0.07, WEST),
        )
        if any(inside and not mask & bit for inside, bit in sides):
            return mix(color, (206.0, 206.0, 200.0), 0.75)
        along_x = abs(s.wy - 0.5) < 0.035 and math.floor(s.wx * 8.0) % 2 == 0
        along_y = abs(s.wx - 0.5) < 0.035 and math.floor(s.wy * 8.0) % 2 == 0
        if (along_x and ((mask & EAST and s.wx >= 0.5) or (mask & WEST and s.wx <= 0.5))) or (
            along_y and ((mask & SOUTH and s.wy >= 0.5) or (mask & NORTH and s.wy <= 0.5))
        ):
            return (236.0, 206.0, 70.0)
        return color

    return material


_road = _road_mask(ROAD_X_MASK)


def _ruts(s: Sample) -> float:
    """Depth in 0..1 of two wheel ruts along x that come and go, with a raised crown between them."""
    sway = 0.03 * math.sin(s.wx * 9.0 + fbm(s.seed + 51, s.wx * 3.0, 0.5) * 6.0)
    d = min(abs(s.wy - 0.3 - sway), abs(s.wy - 0.7 - sway))
    return max(0.0, 1.0 - d / 0.075) * smoothstep(0.35, 0.55, fbm(s.seed + 52, s.wx * 3.0, s.wy * 2.0))


def _dirt_relief(s: Sample) -> float:
    return (
        0.014 * fbm(s.seed, s.wx * 8.0, s.wy * 8.0)
        + 0.028 * bump(s.seed + 41, s.wx, s.wy, 9, 0.3, 0.33, 0.5)[0]
        - 0.022 * _ruts(s)
    )


def _dirt(s: Sample) -> RGB:
    color = scale_color((138.0, 98.0, 62.0), 0.82 + 0.34 * fbm(s.seed, s.wx * 8.0, s.wy * 8.0))
    r = hash01(s.seed + 3, s.ix, s.iy)
    if r > 0.96:
        color = scale_color(color, 1.25)
    elif r < 0.05:
        color = scale_color(color, 0.7)
    rut = _ruts(s)
    if rut > 0.0:
        color = scale_color(color, 1.0 - 0.3 * rut)
    pebble, tag = bump(s.seed + 41, s.wx, s.wy, 9, 0.3, 0.33, 0.5)
    if pebble > 0.0:
        grey = scale_color((158.0, 148.0, 132.0), 0.8 + 0.4 * tag)
        color = scale_color(mix(color, grey, 0.85), 0.9 + 0.12 * pebble)
    elif _shadowed(s.seed + 41, s, 9, 0.3, 0.33, 0.03, 0.5) > 0.0:
        color = scale_color(color, 0.7)
    return color


def _cobble(s: Sample) -> tuple[float, float, float]:
    """Staggered cobbles: the stone's roundness in 0..1 (0 in the joints), its random tag and its row."""
    rows = 5.0
    row = math.floor(s.wy * rows)
    gx = s.wx * 4.0 + (0.5 if row % 2 else 0.0) + 0.17 * hash01(s.seed + 15, row, 0)
    col = math.floor(gx)
    fx, fy = gx - col, s.wy * rows - row
    edge = smoothstep(0.06, 0.34, min(fx, 1.0 - fx)) * smoothstep(0.08, 0.34, min(fy, 1.0 - fy))
    return edge, hash01(s.seed + 17, col, row), float(row)


def _pavement_relief(s: Sample) -> float:
    edge, tag, _ = _cobble(s)
    return (0.016 + 0.006 * tag) * math.sqrt(edge) * (edge > 0.02)


def _pavement(s: Sample) -> RGB:
    edge, tag, row = _cobble(s)
    if edge < 0.02:
        return scale_color((108.0, 104.0, 98.0), 0.85 + 0.3 * hash01(s.seed, s.ix, s.iy))
    warm = hash01(s.seed + 19, int(tag * 1000.0), int(row))
    color = mix((166.0, 164.0, 158.0), (176.0, 160.0, 140.0), warm)
    color = scale_color(color, (0.86 + 0.28 * tag) * (0.95 + 0.1 * hash01(s.seed, s.ix, s.iy)))
    if edge < 0.55:
        color = scale_color(color, 0.86 + 0.25 * edge)
    return color


def _rock_cracks(s: Sample) -> bool:
    return abs(fbm(s.seed + 8, s.wx * 11.0, s.wy * 11.0) - 0.5) < 0.018


def _rock_relief(s: Sample) -> float:
    strata = math.sin((s.wx + s.wy * 1.7) * 23.0 + fbm(s.seed, s.wx * 5.0, s.wy * 5.0) * 9.0)
    return 0.05 * fbm(s.seed + 4, s.wx * 6.0, s.wy * 6.0) + 0.008 * strata - 0.03 * _rock_cracks(s)


def _rock(s: Sample) -> RGB:
    strata = math.sin((s.wx + s.wy * 1.7) * 23.0 + fbm(s.seed, s.wx * 5.0, s.wy * 5.0) * 9.0)
    color = scale_color((124.0, 118.0, 110.0), 0.78 + 0.4 * fbm(s.seed + 4, s.wx * 6.0, s.wy * 6.0) + 0.05 * strata)
    if _rock_cracks(s):
        color = scale_color(color, 0.5)
    elif fbm(s.seed + 12, s.wx * 14.0, s.wy * 14.0) > 0.66:
        color = mix(color, (126.0, 132.0, 96.0), 0.35)
    return color


def _snow_relief(s: Sample) -> float:
    return 0.03 * fbm(s.seed, s.wx * 4.0, s.wy * 4.0) + 0.01 * fbm(s.seed + 5, s.wx * 12.0, s.wy * 12.0)


def _snow(s: Sample) -> RGB:
    color = mix((246.0, 248.0, 252.0), (200.0, 216.0, 240.0), fbm(s.seed, s.wx * 5.0, s.wy * 5.0) * 0.9)
    if hash01(s.seed + 2, s.ix, s.iy) > 0.985:
        color = (255.0, 255.0, 255.0)
    return color


def _rail_tie(s: Sample) -> bool:
    return 0.2 < s.wy < 0.8 and (s.wx * 8.0) % 1.0 < 0.5


def _rail_relief(s: Sample) -> float:
    ballast = 0.014 * bump(s.seed + 81, s.wx, s.wy, 14, 0.6, 0.38, 0.5)[0]
    tie = 0.014 if _rail_tie(s) else 0.0
    rail = 0.03 * max(0.0, 1.0 - min(abs(s.wy - 0.36), abs(s.wy - 0.64)) / 0.035)
    return ballast + tie + rail


def _rail(s: Sample) -> RGB:
    color = scale_color((126.0, 120.0, 112.0), 0.8 + 0.4 * hash01(s.seed, s.ix, s.iy))
    stone, tag = bump(s.seed + 81, s.wx, s.wy, 14, 0.6, 0.38, 0.5)
    if stone > 0.0:
        color = scale_color(color, 0.8 + 0.5 * tag)
    if _rail_tie(s):
        grain = 0.9 + 0.15 * hash01(s.seed + 1, s.ix, s.iy) + 0.08 * math.sin(s.wy * 90.0 + s.wx * 3.0)
        color = scale_color((96.0, 68.0, 46.0), grain)
        if (s.wx * 8.0) % 1.0 < 0.08:
            color = scale_color(color, 0.78)
    if abs(s.wy - 0.36) < 0.03 or abs(s.wy - 0.64) < 0.03:
        color = (178.0, 178.0, 186.0) if hash01(s.seed, s.ix, s.iy) > 0.2 else (140.0, 140.0, 148.0)
    return color


@dataclass(frozen=True)
class Terrain:
    """
    One terrain of the tileset: its id, name and look.
    """

    id: int
    """Terrain id in the map (also the atlas index of the flat tile)."""
    name: str
    """Name shown in the editor."""
    material: Material
    """Colour of a pixel."""
    sloped: bool
    """Whether the atlas holds its 18 sloped variants."""
    connections: int | None = None
    """Sides the tile joins on the road graph (``NORTH`` | ``EAST`` ...), ``None`` off the graph."""


_DAISIES: tuple[RGB, ...] = ((250.0, 250.0, 246.0), (246.0, 232.0, 120.0))
_MEADOW: tuple[RGB, ...] = (
    (236.0, 84.0, 84.0),
    (240.0, 220.0, 90.0),
    (250.0, 250.0, 250.0),
    (170.0, 120.0, 220.0),
)

TERRAINS: list[Terrain] = [
    Terrain(0, "grass", _grass((88.0, 156.0, 64.0), _DAISIES, 0.025), True),
    Terrain(1, "sand", _sand, True),
    Terrain(2, "water", _water, False),
    Terrain(3, "road", _road, True, ROAD_X_MASK),
    Terrain(4, "dirt", _dirt, True),
    Terrain(5, "pavement", _pavement, False),
    Terrain(6, "dark grass", _grass((60.0, 120.0, 52.0)), True),
    Terrain(
        7,
        "flowers",
        _grass((92.0, 160.0, 70.0), _MEADOW, 0.17),
        True,
    ),
    Terrain(8, "rock", _rock, True),
    Terrain(9, "snow", _snow, True),
    Terrain(10, "rail", _rail, True),
    Terrain(11, "road y", _road_mask(ROAD_Y_MASK), True, ROAD_Y_MASK),
    *(Terrain(ROAD_MASKS[mask], ROAD_MASK_NAMES[mask], _road_mask(mask), True, mask) for mask in ROAD_MASK_ORDER),
]
"""
The terrains, grass first; ids 3 (road along X), 11 (road along Y), 12 to 25 (the other road shapes) and 10 (rail) are
meant for the overlay layer. Road variants are sloped too, on every slope: the pattern is drawn in world coordinates,
so a junction on a ramp is as well defined as a straight piece.
"""

RELIEFS: dict[int, Relief] = {
    0: _grass_relief(0.025),
    1: _sand_relief,
    2: _water_relief,
    4: _dirt_relief,
    5: _pavement_relief,
    6: _grass_relief(0.0),
    7: _grass_relief(0.17),
    8: _rock_relief,
    9: _snow_relief,
    10: _rail_relief,
}
"""Relief of the terrains that have one, by terrain id; the others stay flat."""

FOUNDATION_TOPS: list[tuple[int, Material]] = [
    (0, _grass((92.0, 150.0, 66.0), _DAISIES, 0.025)),
    (3, _road),
]
"""Terrains that own a foundation set (their top surface), in atlas order."""


def corners_px(profile: Profile, scale: int) -> list[Point]:
    """
    Screen position of the four corners of a cell.

    :param profile: Corner heights in slope steps (back, right, front, left).
    :param scale: Zoom factor.
    :return: Four points in pixels of the scaled tile.
    """
    step = STEP * Z_PX
    return [
        (32.0 * scale, (16.0 - step * profile[0]) * scale),
        (64.0 * scale, (32.0 - step * profile[1]) * scale),
        (32.0 * scale, (48.0 - step * profile[2]) * scale),
        (0.0, (32.0 - step * profile[3]) * scale),
    ]


def triangles(profile: Profile) -> list[tuple[int, int, int]]:
    """
    The two triangles of a cell: the engine splits it along the diagonal whose corners are the closest.

    :param profile: Corner heights in slope steps.
    :return: Two corner index triples.
    """
    if abs(profile[0] - profile[2]) <= abs(profile[1] - profile[3]):
        return [(0, 1, 2), (0, 2, 3)]
    return [(0, 1, 3), (1, 2, 3)]


def _triangle_normal(profile: Profile, triangle: tuple[int, int, int]) -> Vec3:
    world = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    a, b, c = ((*world[i], profile[i] * STEP) for i in triangle)
    u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    if n[2] < 0:
        n = (-n[0], -n[1], -n[2])
    return normalize(n)


def terrain_shade(normal: Vec3) -> float:
    """
    Brightness of a ground surface: 1 when flat, brighter facing the sun, never darker than 0.6.

    :param normal: Unit normal.
    :return: The brightness factor.
    """
    return max(0.6, min(1.3, (normal[0] * -0.35 + normal[1] * 0.35 + normal[2] * 0.87) / 1.0013 / 0.8688))


def _world_xy(fx: float, fy: float, z: float) -> Point:
    u = (fx - 32.0) / 32.0
    v = (fy - 16.0 + Z_PX * z) / 16.0
    return ((u + v) / 2.0, (v - u) / 2.0)


RELIEF_STRENGTH = 1.0
"""How much a unit of relief gradient tilts the normal."""
RELIEF_LIMIT = 0.9
"""Largest tilt of a relief normal."""


def _bend(normal: Vec3, gx: float, gy: float, gz: float = 0.0) -> Vec3:
    """Tilt a normal against a relief gradient, never by more than ``RELIEF_LIMIT``."""
    length = math.hypot(gx, gy, gz)
    if length > RELIEF_LIMIT:
        gx, gy, gz = gx * RELIEF_LIMIT / length, gy * RELIEF_LIMIT / length, gz * RELIEF_LIMIT / length
    k = RELIEF_STRENGTH
    return normalize((normal[0] - k * gx, normal[1] - k * gy, normal[2] - k * gz))


def _relief_normal(normal: Vec3, relief: Relief, wx: float, wy: float, x: int, y: int, seed: int, scale: int) -> Vec3:
    """Normal of a ground surface bent by its relief, from the height a half pixel on each side."""
    d = 1.0 / (64.0 * scale)
    gx = (relief(Sample(wx + d, wy, x, y, seed)) - relief(Sample(wx - d, wy, x, y, seed))) / (2.0 * d)
    gy = (relief(Sample(wx, wy + d, x, y, seed)) - relief(Sample(wx, wy - d, x, y, seed))) / (2.0 * d)
    return _bend(normal, gx, gy)


def draw_surface(
    layers: Layers,
    index_x: int,
    index_y: int,
    material: Material,
    profile: Profile,
    seed: int,
    scale: int,
    relief: Relief | None = None,
) -> None:
    """
    Draw one sloped diamond into the three layers.

    :param layers: Destination atlas.
    :param index_x: Column of the tile in the atlas.
    :param index_y: Row of the tile in the atlas.
    :param material: Colour of the terrain.
    :param profile: Corner heights in slope steps.
    :param seed: Seed of the tile.
    :param scale: Zoom factor.
    :param relief: Height of the surface detail, bending the normal and the lighting; flat when None.
    """
    corners = corners_px(profile, scale)
    heights = [p * STEP for p in profile]
    ox, oy = index_x * TILE_W * scale, index_y * TILE_H * scale
    drawn: set[tuple[int, int]] = set()
    for triangle in triangles(profile):
        a, b, c = (corners[i] for i in triangle)
        za, zb, zc = (heights[i] for i in triangle)
        normal = _triangle_normal(profile, triangle)
        shade = terrain_shade(normal)
        det = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if det == 0:
            continue
        top = max(0, int(min(a[1], b[1], c[1])))
        bottom = min(TILE_H * scale, math.ceil(max(a[1], b[1], c[1])))
        for y in range(top, bottom):
            for x in range(TILE_W * scale):
                px, py = x + 0.5, y + 0.5
                w1 = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / det
                w2 = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / det
                w3 = 1.0 - w1 - w2
                if min(w1, w2, w3) < 0.0:
                    continue
                if (x, y) in drawn:
                    continue
                drawn.add((x, y))
                z = w1 * za + w2 * zb + w3 * zc
                wx, wy = _world_xy(px / scale, py / scale, z)
                wx, wy = min(1.0, max(0.0, wx)), min(1.0, max(0.0, wy))
                color = material(Sample(wx, wy, x, y, seed))
                pixel_normal, pixel_shade = normal, shade
                if relief is not None:
                    pixel_normal = _relief_normal(normal, relief, wx, wy, x, y, seed, scale)
                    pixel_shade = terrain_shade(pixel_normal)
                layers.put(ox + x, oy + y, scale_color(color, pixel_shade), pixel_normal, z)


def _inside_quad(point: Point, quad: list[Point]) -> bool:
    sign = 0
    for i in range(4):
        a, b = quad[i], quad[(i + 1) % 4]
        cross = (b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (point[0] - a[0])
        if cross != 0:
            if sign == 0:
                sign = 1 if cross > 0 else -1
            elif (cross > 0) != (sign > 0):
                return False
    return True


def _wall_cell(a: float, z: float) -> tuple[float, float, int, int, float]:
    """Dressed stone: courses of a quarter unit, blocks of half a unit, staggered: (fx, fz, column, row, roundness)."""
    row = math.floor(z / 0.25)
    col = a * 2.0 + (0.5 if row % 2 else 0.0)
    fx = col - math.floor(col)
    fz = z / 0.25 - row
    roundness = smoothstep(0.07, 0.25, fx) * smoothstep(0.09, 0.3, fz) * smoothstep(0.0, 0.1, 1.0 - fx)
    return fx, fz, math.floor(col), row, roundness


def _wall_color(a: float, z: float, base: RGB, seed: int, ix: int, iy: int) -> RGB:
    fx, fz, col, row, roundness = _wall_cell(a, z)
    if fx < 0.07 or fz < 0.09:
        return scale_color(base, 0.62 + 0.1 * hash01(seed + 23, ix, iy))
    tone = 0.9 + 0.2 * hash01(seed + 21, col, row)
    color = scale_color(base, tone * (0.95 + 0.1 * hash01(seed, ix, iy)))
    chip = fbm(seed + 25, a * 14.0, z * 14.0)
    if chip > 0.68:
        color = mix(color, (120.0, 130.0, 96.0), 0.25)
    return scale_color(color, 0.9 + 0.12 * roundness)


def _wall_relief(a: float, z: float, seed: int) -> float:
    _, _, col, row, roundness = _wall_cell(a, z)
    return (0.018 + 0.008 * hash01(seed + 21, col, row)) * roundness


def _wall_normal(normal: Vec3, axis: int, a: float, z: float, seed: int, scale: int) -> Vec3:
    """Bend a foundation wall's normal by its block relief; ``axis`` is the world axis ``a`` runs along."""
    d = 1.0 / (48.0 * scale)
    ga = (_wall_relief(a + d, z, seed) - _wall_relief(a - d, z, seed)) / (2.0 * d)
    gz = (_wall_relief(a, z + d, seed) - _wall_relief(a, z - d, seed)) / (2.0 * d)
    return _bend(normal, ga if axis == 0 else 0.0, ga if axis == 1 else 0.0, gz)


def draw_foundation(
    layers: Layers,
    index_x: int,
    index_y: int,
    top_material: Material,
    profile: Profile,
    seed: int,
    scale: int,
    top_relief: Relief | None = None,
) -> None:
    """
    Draw the level platform of a building on a slope: a flat top at the highest corner, stone walls to the ground.

    :param layers: Destination atlas.
    :param index_x: Column of the tile in the atlas.
    :param index_y: Row of the tile in the atlas.
    :param top_material: Colour of the platform's top.
    :param profile: Corner heights of the terrain under it, in slope steps.
    :param seed: Seed of the tile.
    :param scale: Zoom factor.
    :param top_relief: Relief of the platform's top, flat when None.
    """
    top = max(profile)
    flat = corners_px((top, top, top, top), scale)
    ground = corners_px(profile, scale)
    left_wall = [flat[3], flat[2], ground[2], ground[3]]
    right_wall = [flat[2], flat[1], ground[1], ground[2]]
    top_height = top * STEP
    up: Vec3 = (0.0, 0.0, 1.0)
    left_normal: Vec3 = (0.0, 1.0, 0.0)
    right_normal: Vec3 = (1.0, 0.0, 0.0)
    ox, oy = index_x * TILE_W * scale, index_y * TILE_H * scale
    stone: RGB = (150.0, 144.0, 132.0)
    for y in range(TILE_H * scale):
        for x in range(TILE_W * scale):
            point = (x + 0.5, y + 0.5)
            fx, fy = point[0] / scale, point[1] / scale
            if _inside_quad(point, flat):
                wx, wy = _world_xy(fx, fy, top_height)
                wx, wy = min(1.0, max(0.0, wx)), min(1.0, max(0.0, wy))
                color = top_material(Sample(wx, wy, x, y, seed))
                lip = min(1.0, max(0.0, min(wx, wy, 1 - wx, 1 - wy) / 0.06))
                top_normal, top_shade = up, 1.0
                if top_relief is not None:
                    top_normal = _relief_normal(up, top_relief, wx, wy, x, y, seed, scale)
                    top_shade = terrain_shade(top_normal)
                layers.put(ox + x, oy + y, scale_color(color, top_shade * (0.82 + 0.18 * lip)), top_normal, top_height)
            elif _inside_quad(point, left_wall):
                wx = (fx - 32.0) / 32.0 + 1.0
                z = (16.0 + 16.0 * (wx + 1.0) - fy) / 16.0
                color = _wall_color(wx, z, stone, seed, x, y)
                bent = _wall_normal(left_normal, 0, wx, z, seed, scale)
                layers.put(ox + x, oy + y, scale_color(color, light(bent)), bent, z)
            elif _inside_quad(point, right_wall):
                wy = 1.0 - (fx - 32.0) / 32.0
                z = (16.0 + 16.0 * (1.0 + wy) - fy) / 16.0
                color = _wall_color(wy, z, stone, seed + 5, x, y)
                bent = _wall_normal(right_normal, 1, wy, z, seed + 5, scale)
                layers.put(ox + x, oy + y, scale_color(color, light(bent)), bent, z)
