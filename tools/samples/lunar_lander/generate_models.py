#!/usr/bin/env python3
"""
Write the home-made 3D models of the Lunar lander sample game into ``samples/lunar_lander/models/`` as binary glTF.

- ``moon.glb``: the moon, a sphere of ``layout.MOON_RADIUS`` with craters, its dark seas and bright highlands, the
  ground flat around each pad of ``layout.PADS`` (the same mesh is the collider of the moon);
- ``pad.glb``: a landing pad, an octagonal platform with a ring of lamps;
- ``lander_hull.glb``, ``lander_leg.glb``: the descent stage in gold foil, the ascent stage, the engine bell, and one
  leg (strut, brace, foot) turned towards the hull by its entity;
- ``flame.glb``, ``bead.glb``, ``impact.glb``: the engine plume, a bead of the predicted path and the ring marking
  the predicted impact.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.lunar_lander.generate_models``.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from tools.samples import mesh_kit as mk
from tools.samples.lunar_lander import layout

ROOT = Path(__file__).resolve().parents[3] / "samples" / "lunar_lander" / "models"
"""Folder of the written models."""

Vec3 = tuple[float, float, float]


def _craters(seed: int, count: int) -> list[tuple[Vec3, float, float]]:
    """Crater centres (unit vectors), angular radii and depths, kept away from the pads."""
    rng = random.Random(seed)
    out: list[tuple[Vec3, float, float]] = []
    while len(out) < count:
        z = rng.uniform(-1, 1)
        angle = rng.uniform(0, 2 * math.pi)
        r = math.sqrt(1 - z * z)
        centre = (r * math.cos(angle), z, r * math.sin(angle))
        size = rng.choice([0.03, 0.05, 0.07, 0.1, 0.16]) * rng.uniform(0.7, 1.3)
        # Daedalus pad lies inside a large crater: no other crater over a pad.
        if any(_angle(centre, pad.up) < size + 0.16 for pad in layout.PADS):
            continue
        out.append((centre, size, size * layout.MOON_RADIUS * rng.uniform(0.1, 0.16)))
    daedalus = layout.PADS[4].up
    out.append((daedalus, 0.3, 9.0))
    return out


def _angle(a: Vec3, b: Vec3) -> float:
    return math.acos(max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1] + a[2] * b[2])))


CRATERS = _craters(11, 70)
"""The craters of the moon."""


def height(d: Vec3) -> float:
    """Height of the ground above the mean radius in a direction."""
    h = 0.0
    for centre, size, depth in CRATERS:
        t = _angle(d, centre) / size
        if t < 1.6:
            bowl = -depth * (1 - t * t) if t < 1 else 0.0
            rim = 0.35 * depth * math.exp(-(((t - 1) / 0.22) ** 2))
            h += bowl + rim
    h += 1.4 * math.sin(d[0] * 9.1 + 1.3) * math.sin(d[1] * 7.3 + 0.4) * math.sin(d[2] * 8.7 + 2.1)
    h += 0.6 * math.sin(d[0] * 23.0) * math.sin(d[2] * 19.0 + d[1] * 5.0)
    for pad in layout.PADS:
        distance = _angle(d, pad.up) * layout.MOON_RADIUS
        if distance < pad.flat * 2.0:
            blend = max(0.0, min(1.0, (distance - pad.flat) / pad.flat))
            blend = blend * blend * (3 - 2 * blend)
            h = pad_height(pad) * (1 - blend) + h * blend
    return h


def _maria(d: list[float]) -> float:
    """Low-frequency field of the dark seas: the large plains of old lava, above 0.35."""
    return (
        math.sin(d[0] * 2.3 + 0.7) * math.sin(d[1] * 1.9 + 1.1) * 0.6
        + math.sin(d[2] * 3.1 + d[0] * 1.3) * 0.35
        + math.sin(d[1] * 4.7 + d[2] * 2.9) * 0.15
    )


def pad_height(pad: layout.Pad) -> float:
    """Height of the flattened ground under a pad: the mean ground, or the floor of the Daedalus crater."""
    return -7.0 if pad.tag == "Pad5" else 0.0


def moon() -> mk.Model:
    """The moon: dark seas below the mean ground, bright highlands above."""
    whole = mk.Primitive((0.6, 0.6, 0.6))
    mk.relief_sphere(whole, (0.0, 0.0, 0.0), lambda d: layout.MOON_RADIUS + height(d), 84, 168)
    seas = mk.Primitive((0.27, 0.27, 0.29))
    lands = mk.Primitive((0.46, 0.45, 0.43))
    for k in range(0, len(whole.positions), 3):
        triangle = whole.positions[k : k + 3]
        centre = [sum(p[c] for p in triangle) / 3 / layout.MOON_RADIUS for c in range(3)]
        target = seas if _maria(centre) > 0.35 else lands
        target.positions += triangle
        target.normals += whole.normals[k : k + 3]
    return mk.Model("moon", [lands, seas])


def pad() -> mk.Model:
    """An octagonal platform 8 across and 0.4 thick, its top at y = 0.2, with eight amber lamps."""
    deck = mk.Primitive((0.72, 0.72, 0.75))
    mk.cylinder(deck, (0.0, -0.2, 0.0), (0.0, 0.2, 0.0), 4.2, 8)
    marks = mk.Primitive((0.95, 0.95, 0.95), (0.3, 0.3, 0.3))
    for k in range(4):
        angle = k * math.pi / 2
        mk.box(
            marks,
            (math.cos(angle) * 1.6, 0.21, math.sin(angle) * 1.6),
            (0.4 + 2.0 * abs(math.sin(angle)), 0.02, 0.4 + 2.0 * abs(math.cos(angle))),
        )
    lamps = mk.Primitive((1.0, 0.75, 0.3), (1.0, 0.6, 0.15))
    for k in range(8):
        angle = (k + 0.5) * math.pi / 4
        mk.sphere(lamps, (math.cos(angle) * 3.9, 0.3, math.sin(angle) * 3.9), 0.16, 4, 6)
    return mk.Model("pad", [deck, marks, lamps])


def hull() -> mk.Model:
    """The hull of the lander, its centre at the origin: the descent stage, the ascent stage, the engine bell."""
    foil = mk.Primitive((0.85, 0.65, 0.2))
    mk.cylinder(foil, (0.0, -0.55, 0.0), (0.0, 0.1, 0.0), 0.95, 8)
    cabin = mk.Primitive((0.82, 0.83, 0.86))
    mk.cylinder(cabin, (0.0, 0.1, 0.0), (0.0, 0.62, 0.0), 0.72, 6, 0.6)
    mk.box(cabin, (0.0, 0.66, 0.0), (0.3, 0.12, 0.3))
    glass = mk.Primitive((0.1, 0.15, 0.22), (0.05, 0.15, 0.3))
    mk.box(glass, (0.0, 0.42, -0.62), (0.5, 0.2, 0.08))
    metal = mk.Primitive((0.25, 0.25, 0.28))
    mk.cylinder(metal, (0.0, -0.55, 0.0), (0.0, -0.95, 0.0), 0.22, 10, 0.42, caps=False)
    for k in range(4):
        angle = k * math.pi / 2 + math.pi / 4
        mk.box(metal, (math.cos(angle) * 0.78, 0.48, math.sin(angle) * 0.78), (0.14, 0.14, 0.14))
    return mk.Model("lander_hull", [foil, cabin, glass, metal])


def leg() -> mk.Model:
    """A leg, its centre at the origin: the strut from the hull down to the foot pad, a brace towards the hull (-X)."""
    strut = mk.Primitive((0.7, 0.7, 0.72))
    mk.cylinder(strut, (0.0, 0.5, 0.0), (0.0, -0.42, 0.0), 0.07, 8)
    mk.cylinder(strut, (0.0, -0.05, 0.0), (-0.55, 0.45, 0.0), 0.05, 6)
    foot = mk.Primitive((0.55, 0.45, 0.2))
    mk.cylinder(foot, (0.0, -0.5, 0.0), (0.0, -0.42, 0.0), 0.24, 10, 0.18)
    return mk.Model("lander_leg", [strut, foot])


def flame() -> mk.Model:
    """The engine plume: a cone pointing down from the origin, 1 long, glowing."""
    plume = mk.Primitive((1.0, 0.75, 0.45), (1.0, 0.55, 0.2))
    mk.cone(plume, (0.0, 0.0, 0.0), (0.0, -1.0, 0.0), 0.32, 10)
    core = mk.Primitive((1.0, 0.95, 0.85), (1.0, 0.9, 0.7))
    mk.cone(core, (0.0, -0.02, 0.0), (0.0, -0.5, 0.0), 0.16, 8)
    return mk.Model("flame", [plume, core])


def bead() -> mk.Model:
    """A bead of the predicted path."""
    primitive = mk.Primitive((0.5, 0.9, 1.0), (0.3, 0.8, 1.0))
    mk.sphere(primitive, (0.0, 0.0, 0.0), 0.18, 4, 6)
    return mk.Model("bead", [primitive])


def impact() -> mk.Model:
    """A flat glowing ring of radius 1.4 in the XZ plane: where the path meets the ground."""
    primitive = mk.Primitive((1.0, 0.4, 0.3), (1.0, 0.3, 0.2))
    sides = 28
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        outer0, outer1 = (1.4 * math.cos(a0), 0.05, 1.4 * math.sin(a0)), (1.4 * math.cos(a1), 0.05, 1.4 * math.sin(a1))
        inner0, inner1 = (1.1 * math.cos(a0), 0.05, 1.1 * math.sin(a0)), (1.1 * math.cos(a1), 0.05, 1.1 * math.sin(a1))
        mk.add_triangle(primitive, inner0, outer1, outer0, ((0.0, 1.0, 0.0),) * 3)
        mk.add_triangle(primitive, inner0, inner1, outer1, ((0.0, 1.0, 0.0),) * 3)
    return mk.Model("impact", [primitive])


def main() -> None:
    """Write every model."""
    ROOT.mkdir(parents=True, exist_ok=True)
    for model in (moon(), pad(), hull(), leg(), flame(), bead(), impact()):
        mk.write_glb(ROOT / f"{model.name}.glb", model)


if __name__ == "__main__":
    main()
