"""
A tiny orthographic ray caster for the buildings and props: convex solids and ellipsoids seen from the isometric view.

World axes: x to the right-down of the screen, y to the left-down, z up; one unit is a tile (64 px of diagonal width,
16 px of height). A point projects to ``sx = 32 * (x - y)`` and ``sy = 16 * (x + y) - 16 * z``.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from .common import RGB, Layers, Vec3, dot, light, normalize, scale_color

VIEW = (1.0, 1.0, 2.0)
"""Direction towards the camera: every point moving along it keeps its screen position."""


@dataclass(frozen=True)
class Hit:
    """
    What a material sees where a ray stops.
    """

    point: Vec3
    """World position."""
    normal: Vec3
    """Unit surface normal."""
    ix: int
    """Pixel column, for per-pixel noise."""
    iy: int
    """Pixel row, for per-pixel noise."""
    seed: int
    """Seed of the object."""


Shader = Callable[[Hit], RGB]


class Surface:
    """
    A material with relief: one function gives the colour and the height of the surface detail above the solid.

    The relief only bends the normal (and so the lighting); the depth and the silhouette stay those of the solid.
    """

    def __init__(self, sample: Callable[[Hit], tuple[RGB, float]]) -> None:
        """
        Store the sampling function.

        :param sample: Colour before lighting and relief height in world units (a few hundredths) of a hit.
        """
        self.sample = sample

    def __call__(self, hit: Hit) -> RGB:
        """
        Colour of a hit.

        :param hit: The hit.
        :return: The colour before lighting.
        """
        return self.sample(hit)[0]


def sample_of(shader: Shader, hit: Hit) -> tuple[RGB, float]:
    """
    Colour and relief of any material.

    :param shader: A plain shader or a ``Surface``.
    :param hit: The hit.
    :return: The colour and the relief height, 0 for a plain shader.
    """
    if isinstance(shader, Surface):
        return shader.sample(hit)
    return shader(hit), 0.0


class Solid:
    """
    A shape with a material.
    """

    def __init__(self, shader: Shader) -> None:
        """
        Store the material.

        :param shader: Colour of a hit, before lighting.
        """
        self.shader = shader

    def hit(self, origin: Vec3, direction: Vec3) -> tuple[float, Vec3] | None:
        """
        Intersect a ray.

        :param origin: Ray origin.
        :param direction: Ray direction.
        :return: The distance and unit normal of the first hit in front of the origin, or None.
        """
        raise NotImplementedError

    def top(self) -> float:
        """
        Highest point of the shape.

        :return: Its world height.
        """
        raise NotImplementedError


class Convex(Solid):
    """
    The intersection of half-spaces ``n . p <= d``.
    """

    def __init__(self, planes: list[tuple[Vec3, float]], top: float, shader: Shader) -> None:
        """
        Build the solid; the normals are normalised here.

        :param planes: Pairs of (outward normal, offset).
        :param top: Highest point of the solid.
        :param shader: Material.
        """
        super().__init__(shader)
        self._planes: list[tuple[Vec3, float]] = []
        for normal, offset in planes:
            length = math.sqrt(dot(normal, normal))
            self._planes.append((normalize(normal), offset / length))
        self._top = top

    def hit(self, origin: Vec3, direction: Vec3) -> tuple[float, Vec3] | None:
        """
        Slab test against every plane.

        :param origin: Ray origin.
        :param direction: Ray direction.
        :return: The entry distance and normal, or None.
        """
        t_in, t_out = 0.0, math.inf
        entry: Vec3 | None = None
        for normal, offset in self._planes:
            denominator = dot(normal, direction)
            numerator = offset - dot(normal, origin)
            if abs(denominator) < 1e-12:
                if numerator < 0:
                    return None
                continue
            t = numerator / denominator
            if denominator < 0:
                if t > t_in:
                    t_in, entry = t, normal
            else:
                t_out = min(t_out, t)
            if t_in > t_out:
                return None
        return (t_in, entry) if entry is not None else None

    def top(self) -> float:
        """
        Highest point.

        :return: The height given at construction.
        """
        return self._top


class Ellipsoid(Solid):
    """
    An axis-aligned ellipsoid, for foliage.
    """

    def __init__(self, center: Vec3, radii: Vec3, shader: Shader) -> None:
        """
        Build the solid.

        :param center: Centre.
        :param radii: Radius along each axis.
        :param shader: Material.
        """
        super().__init__(shader)
        self._center = center
        self._radii = radii

    def hit(self, origin: Vec3, direction: Vec3) -> tuple[float, Vec3] | None:
        """
        Solve the quadratic in the unit-sphere space.

        :param origin: Ray origin.
        :param direction: Ray direction.
        :return: The entry distance and normal, or None.
        """
        o = tuple((origin[i] - self._center[i]) / self._radii[i] for i in range(3))
        d = tuple(direction[i] / self._radii[i] for i in range(3))
        a = d[0] * d[0] + d[1] * d[1] + d[2] * d[2]
        b = 2.0 * (o[0] * d[0] + o[1] * d[1] + o[2] * d[2])
        c = o[0] * o[0] + o[1] * o[1] + o[2] * o[2] - 1.0
        discriminant = b * b - 4.0 * a * c
        if discriminant < 0:
            return None
        t = (-b - math.sqrt(discriminant)) / (2.0 * a)
        if t < 0:
            return None
        p = tuple(origin[i] + direction[i] * t for i in range(3))
        n = normalize(tuple((p[i] - self._center[i]) / self._radii[i] ** 2 for i in range(3)))  # type: ignore[arg-type]
        return t, n

    def top(self) -> float:
        """
        Highest point.

        :return: Centre height plus the vertical radius.
        """
        return self._center[2] + self._radii[2]


def box(lo: Vec3, hi: Vec3, shader: Shader) -> Convex:
    """
    An axis-aligned box.

    :param lo: Minimum corner.
    :param hi: Maximum corner.
    :param shader: Material.
    :return: The solid.
    """
    planes: list[tuple[Vec3, float]] = [
        ((1.0, 0.0, 0.0), hi[0]),
        ((-1.0, 0.0, 0.0), -lo[0]),
        ((0.0, 1.0, 0.0), hi[1]),
        ((0.0, -1.0, 0.0), -lo[1]),
        ((0.0, 0.0, 1.0), hi[2]),
        ((0.0, 0.0, -1.0), -lo[2]),
    ]
    return Convex(planes, hi[2], shader)


def gable(lo: Vec3, hi: Vec3, ridge: float, along_x: bool, shader: Shader) -> Convex:
    """
    A prism with a triangular section: a box up to ``lo[2]`` ... ``ridge`` whose top is a two-sided roof.

    :param lo: Minimum corner; ``lo[2]`` is the eaves height.
    :param hi: Maximum corner of the base (``hi[2]`` is ignored).
    :param ridge: Height of the ridge.
    :param along_x: Whether the ridge runs along x.
    :param shader: Material.
    :return: The solid.
    """
    planes: list[tuple[Vec3, float]] = [
        ((1.0, 0.0, 0.0), hi[0]),
        ((-1.0, 0.0, 0.0), -lo[0]),
        ((0.0, 1.0, 0.0), hi[1]),
        ((0.0, -1.0, 0.0), -lo[1]),
        ((0.0, 0.0, -1.0), -lo[2]),
    ]
    rise = ridge - lo[2]
    if along_x:
        half = (hi[1] - lo[1]) / 2.0
        k = rise / half
        planes.append(((0.0, -k, 1.0), lo[2] - k * lo[1]))
        planes.append(((0.0, k, 1.0), lo[2] + k * hi[1]))
    else:
        half = (hi[0] - lo[0]) / 2.0
        k = rise / half
        planes.append(((-k, 0.0, 1.0), lo[2] - k * lo[0]))
        planes.append(((k, 0.0, 1.0), lo[2] + k * hi[0]))
    return Convex(planes, ridge, shader)


def pyramid(lo: Vec3, hi: Vec3, apex: float, shader: Shader) -> Convex:
    """
    A four-sided pyramid on a rectangle.

    :param lo: Minimum corner of the base; ``lo[2]`` is the base height.
    :param hi: Maximum corner of the base.
    :param apex: Height of the tip.
    :param shader: Material.
    :return: The solid.
    """
    rise = apex - lo[2]
    kx = rise / ((hi[0] - lo[0]) / 2.0)
    ky = rise / ((hi[1] - lo[1]) / 2.0)
    planes: list[tuple[Vec3, float]] = [
        ((0.0, 0.0, -1.0), -lo[2]),
        ((-kx, 0.0, 1.0), lo[2] - kx * lo[0]),
        ((kx, 0.0, 1.0), lo[2] + kx * hi[0]),
        ((0.0, -ky, 1.0), lo[2] - ky * lo[1]),
        ((0.0, ky, 1.0), lo[2] + ky * hi[1]),
    ]
    return Convex(planes, apex, shader)


RELIEF_LIMIT = 0.9
"""Largest tilt of a relief normal."""


def bent_normal(surface: Surface, hit: Hit, scale: int) -> Vec3:
    """
    Normal of a surface bent by its relief, from the height half a pixel away along both tangents.

    :param surface: The material.
    :param hit: The hit.
    :param scale: Zoom factor.
    :return: The unit normal.
    """
    n = hit.normal
    t1 = (-n[1], n[0], 0.0)
    if abs(n[0]) + abs(n[1]) < 0.2:
        t1 = (1.0, 0.0, 0.0)
    t1 = normalize(t1)
    t2 = (n[1] * t1[2] - n[2] * t1[1], n[2] * t1[0] - n[0] * t1[2], n[0] * t1[1] - n[1] * t1[0])
    eps = 1.0 / (48.0 * scale)

    def height(t: Vec3, sign: float) -> float:
        p = (hit.point[0] + sign * eps * t[0], hit.point[1] + sign * eps * t[1], hit.point[2] + sign * eps * t[2])
        return surface.sample(Hit(p, n, hit.ix, hit.iy, hit.seed))[1]

    g1 = (height(t1, 1.0) - height(t1, -1.0)) / (2.0 * eps)
    g2 = (height(t2, 1.0) - height(t2, -1.0)) / (2.0 * eps)
    length = math.hypot(g1, g2)
    if length > RELIEF_LIMIT:
        g1, g2 = g1 * RELIEF_LIMIT / length, g2 * RELIEF_LIMIT / length
    return normalize((n[0] - g1 * t1[0] - g2 * t2[0], n[1] - g1 * t1[1] - g2 * t2[1], n[2] - g1 * t1[2] - g2 * t2[2]))


@dataclass
class Rendered:
    """
    An object rendered whole, with the footprint cell of every pixel.
    """

    layers: Layers
    """The three images."""
    cells: dict[tuple[int, int], tuple[int, int]]
    """Footprint cell (i, j) of each opaque pixel."""
    origin: tuple[float, float]
    """Screen position of the world origin (x1 pixels)."""
    scale: int
    """Zoom factor."""
    zmax: dict[tuple[int, int], float]
    """Highest point of each footprint cell."""


def render(solids: list[Solid], footprint: tuple[int, int], height_px: int, seed: int, scale: int) -> Rendered:
    """
    Render solids standing on a rectangular footprint.

    The image is ``32 * (w + d)`` pixels wide and ``height_px`` high at zoom 1, the front corner of the footprint at
    the bottom, so a 1x1 object is a tile cell with its relief above the diamond.

    :param solids: The shapes; the order breaks ties only.
    :param footprint: Width and depth in tiles.
    :param height_px: Image height at zoom 1.
    :param seed: Seed of the object.
    :param scale: Zoom factor.
    :return: The layers and the per-pixel footprint cell.
    """
    w, d = footprint
    width, height = 32 * (w + d), height_px
    ox, oy = 32.0 * d, height - 16.0 * (w + d)
    layers = Layers(width * scale, height * scale)
    owner: dict[tuple[int, int], int] = {}
    cells: dict[tuple[int, int], tuple[int, int]] = {}
    zmax: dict[tuple[int, int], float] = {}
    far = 14.0
    direction = (-VIEW[0], -VIEW[1], -VIEW[2])
    for y in range(height * scale):
        for x in range(width * scale):
            gx = ((x + 0.5) / scale - ox) / 32.0
            gy = ((y + 0.5) / scale - oy) / 16.0
            ground = ((gy + gx) / 2.0, (gy - gx) / 2.0)
            origin = (ground[0] + far * VIEW[0], ground[1] + far * VIEW[1], far * VIEW[2])
            best: tuple[float, Vec3, int] | None = None
            for index, solid in enumerate(solids):
                result = solid.hit(origin, direction)
                if result is not None and (best is None or result[0] < best[0]):
                    best = (result[0], result[1], index)
            if best is None:
                continue
            t, normal, index = best
            point = (origin[0] + direction[0] * t, origin[1] + direction[1] * t, origin[2] + direction[2] * t)
            hit = Hit(point, normal, x, y, seed)
            shader = solids[index].shader
            color = shader(hit)
            if isinstance(shader, Surface):
                normal = bent_normal(shader, hit, scale)
            shade = light(normal) * (0.82 + 0.18 * min(1.0, point[2] / 0.3))
            layers.put(x, y, scale_color(color, shade), normal, point[2])
            owner[(x, y)] = index
            cell = (min(w - 1, max(0, math.floor(point[0]))), min(d - 1, max(0, math.floor(point[1]))))
            cells[(x, y)] = cell
            zmax[cell] = max(zmax.get(cell, 0.0), point[2])
    for (x, y), index in owner.items():
        edge = any(
            owner.get((x + dx, y + dy), -1) != index for dx, dy in ((scale, 0), (-scale, 0), (0, scale), (0, -scale))
        )
        if edge:
            layers.darken(x, y, 0.84)
    return Rendered(layers, cells, (ox, oy), scale, zmax)
