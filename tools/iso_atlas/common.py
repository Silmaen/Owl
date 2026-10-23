"""
Constants, noise and the three-layer image buffer shared by the isometric atlas generator.
"""

from __future__ import annotations

import math

from PIL import Image

TILE_W = 64
"""Width in pixels of a tile at zoom 1."""
TILE_H = 48
"""Height in pixels of a tile cell at zoom 1: the 32 pixels diamond at the bottom, 16 pixels of relief above."""
Z_PX = 16.0
"""Screen pixels per world unit of height (2:1 dimetric, one slope step is half a unit)."""
STEP = 0.5
"""World height of one slope unit, the engine's ``terrain::heightStep``."""
Z_RANGE = 8.0
"""World height encoded by a depth value of 255."""

Vec3 = tuple[float, float, float]
RGB = tuple[float, float, float]


def normalize(v: Vec3) -> Vec3:
    """
    Scale a vector to unit length.

    :param v: The vector, not null.
    :return: The unit vector.
    """
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / length, v[1] / length, v[2] / length)


def dot(a: Vec3, b: Vec3) -> float:
    """
    Dot product of two vectors.

    :param a: First vector.
    :param b: Second vector.
    :return: The scalar product.
    """
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


LIGHT = normalize((-0.35, 0.35, 0.87))
"""Direction towards the sun: from the top, a little to the left of the screen."""


def light(normal: Vec3) -> float:
    """
    Baked lighting of a surface: a constant ambient plus a diffuse term.

    :param normal: Unit surface normal.
    :return: A brightness factor, 1.07 for a surface facing up.
    """
    return 0.55 + 0.6 * max(0.0, dot(normal, LIGHT))


def clamp_byte(value: float) -> int:
    """
    Round a value to a byte.

    :param value: Any number.
    :return: The value clamped to 0..255.
    """
    return max(0, min(255, int(value + 0.5)))


def hash01(seed: int, x: int, y: int) -> float:
    """
    Deterministic hash of a lattice point.

    :param seed: Stream seed.
    :param x: Lattice abscissa.
    :param y: Lattice ordinate.
    :return: A value in [0, 1).
    """
    h = (seed * 374761393 + x * 668265263 + y * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return h / 4294967296.0


def value_noise(seed: int, x: float, y: float) -> float:
    """
    Smooth value noise.

    :param seed: Stream seed.
    :param x: Abscissa in lattice units.
    :param y: Ordinate in lattice units.
    :return: A value in [0, 1).
    """
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    top = hash01(seed, ix, iy) * (1 - fx) + hash01(seed, ix + 1, iy) * fx
    bottom = hash01(seed, ix, iy + 1) * (1 - fx) + hash01(seed, ix + 1, iy + 1) * fx
    return top * (1 - fy) + bottom * fy


def fbm(seed: int, x: float, y: float) -> float:
    """
    Three octaves of value noise.

    :param seed: Stream seed.
    :param x: Abscissa in lattice units of the first octave.
    :param y: Ordinate in lattice units of the first octave.
    :return: A value in [0, 1).
    """
    return (
        value_noise(seed, x, y) * 0.55
        + value_noise(seed + 1, x * 2.1, y * 2.1) * 0.3
        + value_noise(seed + 2, x * 4.3, y * 4.3) * 0.15
    )


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    """
    Smooth Hermite interpolation between two edges.

    :param edge0: Value giving 0.
    :param edge1: Value giving 1.
    :param x: The input.
    :return: 0 below ``edge0``, 1 above ``edge1``, a smooth ramp between.
    """
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def bump(
    seed: int, x: float, y: float, cells: int, presence: float, radius: float, power: float = 1.0
) -> tuple[float, float]:
    """
    A scattering of round bumps: one optional bump per cell of a ``cells`` x ``cells`` grid over the unit square.

    A bump never leaves its cell, so the function needs only the cell under the point.

    :param seed: Stream seed.
    :param x: Abscissa in the unit square.
    :param y: Ordinate in the unit square.
    :param cells: Cells per side.
    :param presence: Probability that a cell holds a bump.
    :param radius: Bump radius in cell units, at most 0.5.
    :param power: Profile exponent: 0.5 is a dome (pebble), 1 a parabola, 2 a sharp tuft.
    :return: The bump height in 0..1 under the point (0 outside any bump) and a random tag of the bump in [0, 1).
    """
    gx, gy = x * cells, y * cells
    cx, cy = min(cells - 1, max(0, math.floor(gx))), min(cells - 1, max(0, math.floor(gy)))
    if hash01(seed, cx, cy) >= presence:
        return 0.0, 0.0
    px = cx + radius + (1.0 - 2.0 * radius) * hash01(seed + 1, cx, cy)
    py = cy + radius + (1.0 - 2.0 * radius) * hash01(seed + 2, cx, cy)
    d2 = ((gx - px) ** 2 + (gy - py) ** 2) / (radius * radius)
    if d2 >= 1.0:
        return 0.0, 0.0
    return (1.0 - d2) ** power, hash01(seed + 3, cx, cy)


def scale_color(color: RGB, factor: float) -> RGB:
    """
    Multiply a colour.

    :param color: The colour.
    :param factor: The brightness factor.
    :return: The scaled colour, not clamped.
    """
    return (color[0] * factor, color[1] * factor, color[2] * factor)


def mix(a: RGB, b: RGB, t: float) -> RGB:
    """
    Linear blend of two colours.

    :param a: Colour at ``t == 0``.
    :param b: Colour at ``t == 1``.
    :param t: Blend factor.
    :return: The blended colour.
    """
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


class Layers:
    """
    An albedo, a normal and a depth image drawn together.
    """

    def __init__(self, width: int, height: int) -> None:
        """
        Create three empty transparent images.

        :param width: Width in pixels.
        :param height: Height in pixels.
        """
        self.width = width
        self.height = height
        self.albedo = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self.normal = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self.depth = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self._a = self.albedo.load()
        self._n = self.normal.load()
        self._d = self.depth.load()

    def put(self, x: int, y: int, color: RGB, normal: Vec3, z: float) -> None:
        """
        Write one opaque pixel in the three layers.

        :param x: Column.
        :param y: Row.
        :param color: Albedo colour, 0..255 and clamped.
        :param normal: Unit world-space normal.
        :param z: Height above the sprite's ground in world units.
        """
        assert self._a is not None and self._n is not None and self._d is not None
        self._a[x, y] = (clamp_byte(color[0]), clamp_byte(color[1]), clamp_byte(color[2]), 255)
        self._n[x, y] = (
            clamp_byte((normal[0] * 0.5 + 0.5) * 255),
            clamp_byte((normal[1] * 0.5 + 0.5) * 255),
            clamp_byte((normal[2] * 0.5 + 0.5) * 255),
            255,
        )
        depth = clamp_byte(z / Z_RANGE * 255)
        self._d[x, y] = (depth, depth, depth, 255)

    def darken(self, x: int, y: int, factor: float) -> None:
        """
        Multiply the albedo of a pixel.

        :param x: Column.
        :param y: Row.
        :param factor: The brightness factor.
        """
        assert self._a is not None
        r, g, b, a = self._a[x, y]  # type: ignore[misc]  # RGBA pixels are 4-tuples
        self._a[x, y] = (clamp_byte(r * factor), clamp_byte(g * factor), clamp_byte(b * factor), a)

    def paste(self, other: Layers, x: int, y: int) -> None:
        """
        Copy another buffer into this one (no blending: transparent pixels are copied too).

        :param other: The source.
        :param x: Destination column of the source's left edge.
        :param y: Destination row of the source's top edge.
        """
        for mine, theirs in ((self.albedo, other.albedo), (self.normal, other.normal), (self.depth, other.depth)):
            mine.paste(theirs, (x, y))

    def half(self) -> Layers:
        """
        Build the next mip level, read-only afterwards.

        :return: A buffer of half the size, box filtered.
        """
        out = Layers(self.width // 2, self.height // 2)
        size = (out.width, out.height)
        out.albedo = self.albedo.resize(size, Image.Resampling.BOX)
        out.normal = self.normal.resize(size, Image.Resampling.BOX)
        out.depth = self.depth.resize(size, Image.Resampling.BOX)
        return out
