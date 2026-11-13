#!/usr/bin/env python3
"""
Write the home-made 3D models of the Bridge builder sample game into ``samples/bridge_builder/models/`` as binary
glTF.

- The parts, one unit long along X (their entities are scaled to the length of the part): ``road.glb`` (an asphalt
  deck on planks, 2.4 deep), ``wood.glb`` and ``steel.glb`` (a pair of beams in front of and behind the deck);
  ``node.glb`` (a bolt across the bridge), ``anchor.glb`` (a red anchor bolt), ``grid_line.glb`` (a line of the
  build grid);
- ``car.glb``, ``van.glb``, ``truck.glb``: the vehicles, their chassis centred on the origin, nose along +X;
- the valley: ``cliff.glb`` (a block of rock with grass on top), ``mountain.glb``, ``tree.glb``, ``water.glb``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.bridge_builder.generate_models``.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from tools.samples import mesh_kit as mk

ROOT = Path(__file__).resolve().parents[3] / "samples" / "bridge_builder" / "models"
"""Folder of the written models."""


def road() -> mk.Model:
    """The deck: asphalt over a wooden frame, its top at y = 0.15."""
    asphalt = mk.Primitive((0.24, 0.24, 0.26))
    mk.box(asphalt, (0.0, 0.08, 0.0), (1.0, 0.14, 2.4))
    frame = mk.Primitive((0.55, 0.38, 0.2))
    mk.box(frame, (0.0, -0.06, 0.0), (1.0, 0.16, 2.5))
    paint = mk.Primitive((0.95, 0.85, 0.3), (0.1, 0.08, 0.0))
    mk.box(paint, (0.0, 0.155, 0.0), (0.5, 0.01, 0.1))
    return mk.Model("road", [asphalt, frame, paint])


def beams(name: str, colour: tuple[float, float, float], thickness: float) -> mk.Model:
    """Two beams of a support, in front of and behind the deck."""
    primitive = mk.Primitive(colour)
    for z in (-1.15, 1.15):
        mk.box(primitive, (0.0, 0.0, z), (1.0, thickness, thickness * 0.8))
    return mk.Model(name, [primitive])


def bolt(name: str, colour: tuple[float, float, float], radius: float) -> mk.Model:
    """A bolt across the bridge, from front to back."""
    primitive = mk.Primitive(colour)
    mk.cylinder(primitive, (0.0, 0.0, -1.35), (0.0, 0.0, 1.35), radius, 10)
    return mk.Model(name, [primitive])


def vehicle(name: str, body: tuple[float, float, float], length: float, height: float, cabin: float) -> mk.Model:
    """A vehicle: a chassis ``length`` long, a cabin at the front (``cabin`` of the length), four wheels."""
    paint = mk.Primitive(body)
    mk.box(paint, (0.0, 0.0, 0.0), (length, height * 0.5, 1.5))
    mk.box(paint, (length * (0.5 - cabin / 2), height * 0.5, 0.0), (length * cabin, height * 0.5, 1.4))
    glass = mk.Primitive((0.2, 0.3, 0.4), (0.02, 0.04, 0.06))
    mk.box(glass, (length * (0.5 - cabin / 2) + 0.02, height * 0.55, 0.0), (length * cabin * 0.9, height * 0.3, 1.42))
    tyres = mk.Primitive((0.08, 0.08, 0.09))
    for x in (-length * 0.33, length * 0.33):
        for z in (-0.72, 0.72):
            mk.cylinder(tyres, (x, -height * 0.25, z - 0.12), (x, -height * 0.25, z + 0.12), 0.35, 12)
    return mk.Model(name, [paint, glass, tyres])


def cliff() -> mk.Model:
    """A block of rock 1 x 1 x 1 (scaled by its entity), grass on top."""
    rock = mk.Primitive((0.45, 0.4, 0.36))
    mk.box(rock, (0.0, -0.02, 0.0), (1.0, 0.96, 1.0))
    grass = mk.Primitive((0.35, 0.55, 0.25))
    mk.box(grass, (0.0, 0.48, 0.0), (1.0, 0.04, 1.0))
    return mk.Model("cliff", [rock, grass])


def mountain() -> mk.Model:
    """A mountain 1 high and about 1 across, snow on top."""
    rng = random.Random(4)
    rock = mk.Primitive((0.42, 0.45, 0.5))
    snow = mk.Primitive((0.92, 0.94, 0.97))
    sides = 9
    ring = [
        (
            math.cos(2 * math.pi * k / sides) * rng.uniform(0.4, 0.55),
            0.0,
            math.sin(2 * math.pi * k / sides) * rng.uniform(0.4, 0.55),
        )
        for k in range(sides)
    ]
    shoulder = [(x * 0.35, 0.68, z * 0.35) for x, _, z in ring]
    peak = (0.03, 1.0, -0.02)
    for k in range(sides):
        a, b = ring[k], ring[(k + 1) % sides]
        c, d = shoulder[(k + 1) % sides], shoulder[k]
        mk.add_triangle(rock, a, d, c)
        mk.add_triangle(rock, a, c, b)
        mk.add_triangle(snow, d, peak, c)
    return mk.Model("mountain", [rock, snow])


def tree() -> mk.Model:
    """A fir 1 high."""
    trunk = mk.Primitive((0.4, 0.27, 0.15))
    mk.cylinder(trunk, (0.0, 0.0, 0.0), (0.0, 0.25, 0.0), 0.05, 6)
    leaves = mk.Primitive((0.18, 0.38, 0.2))
    for base, top, radius in ((0.2, 0.65, 0.28), (0.45, 0.85, 0.22), (0.65, 1.0, 0.15)):
        mk.cone(leaves, (0.0, base, 0.0), (0.0, top, 0.0), radius, 8)
    return mk.Model("tree", [trunk, leaves])


def water() -> mk.Model:
    """A flat river 1 x 1, faintly glowing so that it reads blue in the shade of the gorge."""
    primitive = mk.Primitive((0.12, 0.3, 0.45), (0.01, 0.03, 0.05))
    a, b, c, d = (-0.5, 0.0, -0.5), (0.5, 0.0, -0.5), (0.5, 0.0, 0.5), (-0.5, 0.0, 0.5)
    mk.add_triangle(primitive, a, d, c)
    mk.add_triangle(primitive, a, c, b)
    return mk.Model("water", [primitive])


def cursor() -> mk.Model:
    """The build cursor: a glowing ring 0.5 across in the plane of the bridge, in front of it."""
    primitive = mk.Primitive((1.0, 0.8, 0.3), (1.0, 0.6, 0.15))
    sides = 24
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        outer = [(0.32 * math.cos(a), 0.32 * math.sin(a), 2.0) for a in (a0, a1)]
        inner = [(0.22 * math.cos(a), 0.22 * math.sin(a), 2.0) for a in (a0, a1)]
        normal = ((0.0, 0.0, 1.0),) * 3
        mk.add_triangle(primitive, inner[0], outer[0], outer[1], normal)
        mk.add_triangle(primitive, inner[0], outer[1], inner[1], normal)
    return mk.Model("cursor", [primitive])


def grid_line() -> mk.Model:
    """A line of the build grid, one unit long along X, thin and faintly glowing, in front of the parts."""
    primitive = mk.Primitive((0.45, 0.52, 0.62), (0.04, 0.05, 0.07))
    mk.box(primitive, (0.0, 0.0, 1.45), (1.0, 0.025, 0.01))
    return mk.Model("grid_line", [primitive])


def main() -> None:
    """Write every model."""
    ROOT.mkdir(parents=True, exist_ok=True)
    models = [
        road(),
        beams("wood", (0.62, 0.43, 0.24), 0.22),
        beams("steel", (0.45, 0.52, 0.62), 0.18),
        bolt("node", (0.75, 0.75, 0.78), 0.12),
        bolt("anchor", (0.8, 0.2, 0.15), 0.2),
        vehicle("car", (0.2, 0.45, 0.85), 2.2, 0.9, 0.45),
        vehicle("van", (0.95, 0.75, 0.2), 2.8, 1.3, 0.3),
        vehicle("truck", (0.85, 0.25, 0.2), 3.6, 1.6, 0.25),
        cliff(),
        mountain(),
        tree(),
        water(),
        cursor(),
        grid_line(),
    ]
    for model in models:
        mk.write_glb(ROOT / f"{model.name}.glb", model)


if __name__ == "__main__":
    main()
