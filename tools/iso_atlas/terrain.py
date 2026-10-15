"""
Terrain tiles: the flat and sloped diamonds, the overlays and the foundations of the isometric tileset.

The slope table mirrors ``scene::terrain`` of the engine: corner heights (back, right, front, left) relative to the
lowest corner, in steps of half a world unit.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from .common import RGB, STEP, TILE_H, TILE_W, Z_PX, Layers, Vec3, fbm, hash01, light, mix, normalize, scale_color

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


def _grass(base: RGB, flecks: tuple[RGB, ...] = ()) -> Material:
    def material(s: Sample) -> RGB:
        n = fbm(s.seed, s.wx * 7.0, s.wy * 7.0)
        color = scale_color(base, 0.84 + 0.32 * n)
        r = hash01(s.seed + 7, s.ix, s.iy)
        if r > 0.9:
            color = scale_color(color, 1.16)
        elif r < 0.06:
            color = scale_color(color, 0.82)
        if flecks and hash01(s.seed + 11, s.ix // 2, s.iy // 2) > 0.955:
            color = flecks[int(hash01(s.seed + 13, s.ix // 2, s.iy // 2) * len(flecks)) % len(flecks)]
        return color

    return material


def _sand(s: Sample) -> RGB:
    ripple = math.sin((s.wx * 1.0 + s.wy * 0.55) * 31.0 + fbm(s.seed, s.wx * 3.0, s.wy * 3.0) * 6.0)
    color = scale_color((214.0, 196.0, 128.0), 0.92 + 0.1 * fbm(s.seed + 3, s.wx * 9.0, s.wy * 9.0) + 0.04 * ripple)
    if hash01(s.seed + 5, s.ix, s.iy) > 0.94:
        color = scale_color(color, 0.86)
    return color


def _water(s: Sample) -> RGB:
    wave = math.sin((s.wx - s.wy) * 20.0 + fbm(s.seed, s.wx * 4.0, s.wy * 4.0) * 7.0)
    wave2 = math.sin((s.wx + s.wy) * 13.0 + fbm(s.seed + 9, s.wx * 3.0, s.wy * 3.0) * 5.0)
    color = scale_color((58.0, 112.0, 190.0), 0.94 + 0.07 * wave2)
    if wave > 0.93:
        color = mix(color, (206.0, 230.0, 250.0), 0.55)
    elif wave > 0.8:
        color = mix(color, (110.0, 160.0, 220.0), 0.5)
    return color


def _road(s: Sample) -> RGB:
    color = scale_color(
        (96.0, 96.0, 104.0), 0.88 + 0.2 * hash01(s.seed, s.ix, s.iy) + 0.1 * fbm(s.seed, s.wx * 6, s.wy * 6)
    )
    if s.wy < 0.07 or s.wy > 0.93:
        color = mix(color, (206.0, 206.0, 200.0), 0.75)
    elif abs(s.wy - 0.5) < 0.035 and math.floor(s.wx * 8.0) % 2 == 0:
        color = (236.0, 206.0, 70.0)
    return color


def _dirt(s: Sample) -> RGB:
    color = scale_color((138.0, 98.0, 62.0), 0.82 + 0.34 * fbm(s.seed, s.wx * 8.0, s.wy * 8.0))
    r = hash01(s.seed + 3, s.ix, s.iy)
    if r > 0.95:
        color = (168.0, 150.0, 130.0)
    elif r < 0.05:
        color = scale_color(color, 0.7)
    return color


def _pavement(s: Sample) -> RGB:
    gx, gy = s.wx * 4.0, s.wy * 4.0
    cx, cy = math.floor(gx), math.floor(gy)
    fx, fy = gx - cx, gy - cy
    if fx < 0.07 or fy < 0.07:
        return (120.0, 116.0, 110.0)
    tone = 0.9 + 0.2 * hash01(s.seed + 17, cx, cy)
    color = scale_color((168.0, 164.0, 156.0), tone * (0.96 + 0.08 * hash01(s.seed, s.ix, s.iy)))
    if fx > 0.9 or fy > 0.9:
        color = scale_color(color, 0.93)
    return color


def _rock(s: Sample) -> RGB:
    strata = math.sin((s.wx + s.wy * 1.7) * 23.0 + fbm(s.seed, s.wx * 5.0, s.wy * 5.0) * 9.0)
    color = scale_color((124.0, 118.0, 110.0), 0.78 + 0.4 * fbm(s.seed + 4, s.wx * 6.0, s.wy * 6.0) + 0.05 * strata)
    if abs(fbm(s.seed + 8, s.wx * 11.0, s.wy * 11.0) - 0.5) < 0.018:
        color = scale_color(color, 0.55)
    return color


def _snow(s: Sample) -> RGB:
    color = mix((246.0, 248.0, 252.0), (200.0, 216.0, 240.0), fbm(s.seed, s.wx * 5.0, s.wy * 5.0) * 0.9)
    if hash01(s.seed + 2, s.ix, s.iy) > 0.985:
        color = (255.0, 255.0, 255.0)
    return color


def _rail(s: Sample) -> RGB:
    color = scale_color((126.0, 120.0, 112.0), 0.8 + 0.4 * hash01(s.seed, s.ix, s.iy))
    if 0.2 < s.wy < 0.8 and (s.wx * 8.0) % 1.0 < 0.5:
        color = scale_color((96.0, 68.0, 46.0), 0.9 + 0.15 * hash01(s.seed + 1, s.ix, s.iy))
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


TERRAINS: list[Terrain] = [
    Terrain(0, "grass", _grass((88.0, 156.0, 64.0)), True),
    Terrain(1, "sand", _sand, True),
    Terrain(2, "water", _water, False),
    Terrain(3, "road", _road, True),
    Terrain(4, "dirt", _dirt, True),
    Terrain(5, "pavement", _pavement, False),
    Terrain(6, "dark grass", _grass((60.0, 120.0, 52.0)), True),
    Terrain(
        7,
        "flowers",
        _grass((92.0, 160.0, 70.0), ((236.0, 84.0, 84.0), (240.0, 220.0, 90.0), (250.0, 250.0, 250.0))),
        True,
    ),
    Terrain(8, "rock", _rock, True),
    Terrain(9, "snow", _snow, True),
    Terrain(10, "rail", _rail, True),
]
"""The terrains, grass first; ids 3 (road) and 10 (rail) are meant for the overlay layer."""

FOUNDATION_TOPS: list[tuple[int, Material]] = [
    (0, _grass((92.0, 150.0, 66.0))),
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


def draw_surface(
    layers: Layers, index_x: int, index_y: int, material: Material, profile: Profile, seed: int, scale: int
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
                color = material(Sample(min(1.0, max(0.0, wx)), min(1.0, max(0.0, wy)), x, y, seed))
                layers.put(ox + x, oy + y, scale_color(color, shade), normal, z)


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


def _wall_color(a: float, z: float, base: RGB, seed: int, ix: int, iy: int) -> RGB:
    """Dressed stone: courses of a quarter unit, blocks of half a unit, staggered."""
    row = math.floor(z / 0.25)
    col = a * 2.0 + (0.5 if row % 2 else 0.0)
    fx = col - math.floor(col)
    fz = z / 0.25 - row
    if fx < 0.07 or fz < 0.09:
        return scale_color(base, 0.7)
    tone = 0.9 + 0.2 * hash01(seed + 21, math.floor(col), row)
    return scale_color(base, tone * (0.95 + 0.1 * hash01(seed, ix, iy)))


def draw_foundation(
    layers: Layers, index_x: int, index_y: int, top_material: Material, profile: Profile, seed: int, scale: int
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
                color = top_material(Sample(min(1.0, max(0.0, wx)), min(1.0, max(0.0, wy)), x, y, seed))
                lip = min(1.0, max(0.0, min(wx, wy, 1 - wx, 1 - wy) / 0.06))
                layers.put(ox + x, oy + y, scale_color(color, 0.82 + 0.18 * lip), up, top_height)
            elif _inside_quad(point, left_wall):
                wx = (fx - 32.0) / 32.0 + 1.0
                z = (16.0 + 16.0 * (wx + 1.0) - fy) / 16.0
                color = _wall_color(wx, z, stone, seed, x, y)
                layers.put(ox + x, oy + y, scale_color(color, light(left_normal)), left_normal, z)
            elif _inside_quad(point, right_wall):
                wy = 1.0 - (fx - 32.0) / 32.0
                z = (16.0 + 16.0 * (1.0 + wy) - fy) / 16.0
                color = _wall_color(wy, z, stone, seed + 5, x, y)
                layers.put(ox + x, oy + y, scale_color(color, light(right_normal)), right_normal, z)
