"""
Solids for the home-made models of the sample games (boxes, cylinders, cones, spheres, height-field spheres), added
to the primitives of ``tools.samples.rail_shooter.generate_models`` and written by its binary glTF writer.
"""

from __future__ import annotations

import math
from collections.abc import Callable

from tools.samples.rail_shooter.generate_models import Model, Primitive, write_glb

__all__ = ["Model", "Primitive", "write_glb", "box", "cylinder", "cone", "sphere", "add_triangle", "relief_sphere"]

Vec3 = tuple[float, float, float]


def _sub(a: Vec3, b: Vec3) -> Vec3:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def _cross(a: Vec3, b: Vec3) -> Vec3:
    return a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]


def _unit(a: Vec3) -> Vec3:
    length = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) or 1.0
    return a[0] / length, a[1] / length, a[2] / length


def add_triangle(
    primitive: Primitive, a: Vec3, b: Vec3, c: Vec3, normals: tuple[Vec3, Vec3, Vec3] | None = None
) -> None:
    """A triangle given counter-clockwise seen from outside, flat shaded unless its normals are given."""
    flat = _unit(_cross(_sub(b, a), _sub(c, a)))
    primitive.positions += [a, b, c]
    primitive.normals += list(normals) if normals else [flat, flat, flat]


def _quad(primitive: Primitive, a: Vec3, b: Vec3, c: Vec3, d: Vec3) -> None:
    add_triangle(primitive, a, b, c)
    add_triangle(primitive, a, c, d)


def box(primitive: Primitive, centre: Vec3, size: Vec3) -> None:
    """An axis-aligned box."""
    hx, hy, hz = (s / 2 for s in size)
    cx, cy, cz = centre
    p = [(cx + sx * hx, cy + sy * hy, cz + sz * hz) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    # index = (sx>0)*4 + (sy>0)*2 + (sz>0)
    _quad(primitive, p[0], p[1], p[3], p[2])  # -x
    _quad(primitive, p[4], p[6], p[7], p[5])  # +x
    _quad(primitive, p[0], p[4], p[5], p[1])  # -y
    _quad(primitive, p[2], p[3], p[7], p[6])  # +y
    _quad(primitive, p[0], p[2], p[6], p[4])  # -z
    _quad(primitive, p[1], p[5], p[7], p[3])  # +z


def cylinder(
    primitive: Primitive,
    base: Vec3,
    top: Vec3,
    radius: float,
    sides: int = 12,
    top_radius: float | None = None,
    caps: bool = True,
) -> None:
    """A cylinder (a cone frustum with ``top_radius``) from ``base`` to ``top``, smooth sides."""
    axis = _unit(_sub(top, base))
    helper: Vec3 = (1.0, 0.0, 0.0) if abs(axis[0]) < 0.9 else (0.0, 0.0, 1.0)
    u = _unit(_cross(axis, helper))
    v = _cross(axis, u)
    upper = radius if top_radius is None else top_radius

    def ring(centre: Vec3, r: float, k: int) -> Vec3:
        angle = 2 * math.pi * k / sides
        return tuple(centre[i] + r * (math.cos(angle) * u[i] + math.sin(angle) * v[i]) for i in range(3))  # type: ignore[return-value]

    def normal(k: int) -> Vec3:
        angle = 2 * math.pi * k / sides
        return tuple(math.cos(angle) * u[i] + math.sin(angle) * v[i] for i in range(3))  # type: ignore[return-value]

    for k in range(sides):
        a, b = ring(base, radius, k), ring(base, radius, k + 1)
        c, d = ring(top, upper, k + 1), ring(top, upper, k)
        na, nb = normal(k), normal(k + 1)
        add_triangle(primitive, a, b, c, (na, nb, nb))
        add_triangle(primitive, a, c, d, (na, nb, na))
        if caps:
            add_triangle(primitive, base, b, a)
            add_triangle(primitive, top, d, c)


def cone(primitive: Primitive, base: Vec3, tip: Vec3, radius: float, sides: int = 12) -> None:
    """A cone from a disc at ``base`` to ``tip``."""
    cylinder(primitive, base, tip, radius, sides, 0.001, caps=True)


def sphere(primitive: Primitive, centre: Vec3, radius: float, rings: int = 8, segments: int = 12) -> None:
    """A smooth UV sphere."""
    relief_sphere(primitive, centre, lambda _d: radius, rings, segments)


def relief_sphere(
    primitive: Primitive, centre: Vec3, height: Callable[[Vec3], float], rings: int, segments: int
) -> None:
    """A sphere whose radius in each direction is ``height(direction)``, smooth shaded."""

    def direction(i: int, j: int) -> Vec3:
        theta = math.pi * i / rings
        phi = 2 * math.pi * j / segments
        return math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi)

    points: dict[tuple[int, int], Vec3] = {}
    for i in range(rings + 1):
        for j in range(segments + 1):
            d = direction(i, j % segments)
            r = height(d)
            points[(i, j)] = (centre[0] + d[0] * r, centre[1] + d[1] * r, centre[2] + d[2] * r)

    def smooth(i: int, j: int) -> Vec3:
        here = points[(i, j)]
        left = points[(i, (j - 1) % segments)]
        right = points[(i, (j + 1) % segments)]
        up = points[(max(i - 1, 0), j)]
        down = points[(min(i + 1, rings), j)]
        n = _cross(_sub(right, left), _sub(up, down))
        if i in (0, rings) or sum(x * x for x in n) < 1e-12:
            return _unit(_sub(here, centre))
        out = _unit(n)
        return out if sum(out[k] * (here[k] - centre[k]) for k in range(3)) > 0 else (-out[0], -out[1], -out[2])

    for i in range(rings):
        for j in range(segments):
            corners = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            pa, pb, pc, pd = (points[k] for k in corners)
            na, nb, nc, nd = (smooth(*k) for k in corners)
            if i > 0:
                add_triangle(primitive, pa, pd, pb, (na, nd, nb))
            if i < rings - 1:
                add_triangle(primitive, pb, pd, pc, (nb, nd, nc))
