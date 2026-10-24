#!/usr/bin/env python3
"""
The Horde of the Tower defence sample game drawn as pixel-art sprites in 8 directions, Warcraft 2 style.

Each creature is a small 3D model of ellipsoids and capsules, sampled once into surface points with their normals,
then splatted for every direction and walk frame through an orthographic camera looking down at ``ELEVATION`` (the
pitch of the game camera), shaded in three bands and outlined. A sheet holds ``DIRECTIONS`` columns (0 facing the
viewer, then turning clockwise as seen from above: 1 facing the lower right of the screen, 2 the right...) and
``FRAMES`` rows of the walk cycle; the frame of a column and a row is ``row * DIRECTIONS + column``.

Every cell is ``CELL`` pixels square at ``PPU`` pixels per world unit, the foot of the creature at ``FOOT`` (the
billboard of the game is placed so that this pixel lands on the foot).
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass, field

from PIL import Image

CELL = 64
"""Side of a sprite cell, in pixels."""
PPU = 46.0
"""Pixels per world unit of the sprites."""
FOOT = (32, 54)
"""Pixel of the foot of a creature in its cell."""
DIRECTIONS = 8
"""Directions of a sheet (columns)."""
FRAMES = 4
"""Walk frames of a sheet (rows)."""
ELEVATION = math.radians(42.0)
"""Angle of the view above the horizon, the pitch of the game camera."""
LIGHT = (-0.45, 0.75, 0.48)
"""Direction toward the light in the view frame (x right, y up, z toward the viewer)."""
SPACING = 0.55 / PPU
"""Distance between two surface samples, about half a pixel."""

Vec = tuple[float, float, float]
RGB = tuple[int, int, int]


def _norm(v: Vec) -> Vec:
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return v[0] / length, v[1] / length, v[2] / length


def _rot_x(v: Vec, angle: float) -> Vec:
    c, s = math.cos(angle), math.sin(angle)
    return v[0], v[1] * c - v[2] * s, v[1] * s + v[2] * c


def _rot_y(v: Vec, angle: float) -> Vec:
    c, s = math.cos(angle), math.sin(angle)
    return v[0] * c + v[2] * s, v[1], -v[0] * s + v[2] * c


@dataclass
class Part:
    """Surface points of one shape, the bone moving them and their colour."""

    points: list[tuple[Vec, Vec]]
    colour: RGB
    bone: str = "body"


@dataclass
class Model:
    """A creature: its parts and the pivots of its bones (hips, shoulders)."""

    parts: list[Part] = field(default_factory=list)
    pivots: dict[str, Vec] = field(default_factory=dict)
    stride: float = 0.55
    """Swing of the legs, in radians."""
    bob: float = 0.02
    """Rise of the body at the middle of a step, in world units."""

    def ellipsoid(self, centre: Vec, radii: Vec, colour: RGB, bone: str = "body") -> None:
        """An ellipsoid of three radii."""
        a, b, c = radii
        area = 4.0 * math.pi * (((a * b) ** 1.6 + (a * c) ** 1.6 + (b * c) ** 1.6) / 3.0) ** (1.0 / 1.6)
        count = max(24, int(area / (SPACING * SPACING)))
        golden = math.pi * (3.0 - math.sqrt(5.0))
        points = []
        for index in range(count):
            y = 1.0 - 2.0 * (index + 0.5) / count
            radius = math.sqrt(max(0.0, 1.0 - y * y))
            theta = golden * index
            unit = (math.cos(theta) * radius, y, math.sin(theta) * radius)
            point = (centre[0] + unit[0] * a, centre[1] + unit[1] * b, centre[2] + unit[2] * c)
            points.append((point, _norm((unit[0] / a, unit[1] / b, unit[2] / c))))
        self.parts.append(Part(points, colour, bone))

    def capsule(self, start: Vec, end: Vec, radius: float, colour: RGB, bone: str = "body", end_radius: float = -1.0):
        """A capsule from one point to another, its radius growing to ``end_radius`` (a club)."""
        end_radius = radius if end_radius < 0 else end_radius
        axis = (end[0] - start[0], end[1] - start[1], end[2] - start[2])
        length = math.sqrt(sum(v * v for v in axis)) or 1e-6
        w = _norm(axis)
        helper = (1.0, 0.0, 0.0) if abs(w[0]) < 0.9 else (0.0, 1.0, 0.0)
        u = _norm(
            (
                w[1] * helper[2] - w[2] * helper[1],
                w[2] * helper[0] - w[0] * helper[2],
                w[0] * helper[1] - w[1] * helper[0],
            )
        )
        v = (w[1] * u[2] - w[2] * u[1], w[2] * u[0] - w[0] * u[2], w[0] * u[1] - w[1] * u[0])
        points: list[tuple[Vec, Vec]] = []
        rings = max(2, int(length / SPACING))
        for ring in range(rings + 1):
            t = ring / rings
            r = radius + (end_radius - radius) * t
            around = max(8, int(2 * math.pi * r / SPACING))
            for k in range(around):
                angle = 2 * math.pi * k / around
                c, s = math.cos(angle), math.sin(angle)
                n = (u[0] * c + v[0] * s, u[1] * c + v[1] * s, u[2] * c + v[2] * s)
                p = (
                    start[0] + axis[0] * t + n[0] * r,
                    start[1] + axis[1] * t + n[1] * r,
                    start[2] + axis[2] * t + n[2] * r,
                )
                points.append((p, n))
        self.parts.append(Part(points, colour, bone))
        self.ellipsoid(start, (radius, radius, radius), colour, bone)
        self.ellipsoid(end, (end_radius, end_radius, end_radius), colour, bone)

    def box(self, centre: Vec, half: Vec, colour: RGB, bone: str = "body", tilt: float = 0.0) -> None:
        """A box of half extents, turned by ``tilt`` around its X axis (an axe blade)."""
        points = []
        for axis in range(3):
            for sign in (-1.0, 1.0):
                a1, a2 = [i for i in range(3) if i != axis]
                n1 = max(2, int(2 * half[a1] / SPACING))
                n2 = max(2, int(2 * half[a2] / SPACING))
                for i in range(n1 + 1):
                    for j in range(n2 + 1):
                        local = [0.0, 0.0, 0.0]
                        local[axis] = sign * half[axis]
                        local[a1] = -half[a1] + 2 * half[a1] * i / n1
                        local[a2] = -half[a2] + 2 * half[a2] * j / n2
                        normal = [0.0, 0.0, 0.0]
                        normal[axis] = sign
                        p = _rot_x((local[0], local[1], local[2]), tilt)
                        n = _rot_x((normal[0], normal[1], normal[2]), tilt)
                        points.append(((centre[0] + p[0], centre[1] + p[1], centre[2] + p[2]), n))
        self.parts.append(Part(points, colour, bone))


def _pose(model: Model, bone: str, point: Vec, phase: float) -> Vec:
    """A point moved by its bone at a phase of the walk cycle."""
    swing = math.sin(phase)
    angle = 0.0
    if bone == "leg_l":
        angle = model.stride * swing
    elif bone == "leg_r":
        angle = -model.stride * swing
    elif bone in ("arm_l",):
        angle = -0.7 * model.stride * swing
    elif bone in ("arm_r", "weapon"):
        angle = 0.5 * model.stride * swing
    lift = model.bob * abs(math.cos(phase))
    if angle != 0.0:
        pivot = model.pivots.get("hip" if bone.startswith("leg") else "shoulder", (0.0, 0.0, 0.0))
        local = (point[0] - pivot[0], point[1] - pivot[1], point[2] - pivot[2])
        local = _rot_x(local, angle)
        point = (local[0] + pivot[0], local[1] + pivot[1], local[2] + pivot[2])
    if not bone.startswith("leg"):
        point = (point[0], point[1] + lift, point[2])
    return point


def _shade(colour: RGB, factor: float) -> tuple[int, int, int, int]:
    r, g, b = (max(0, min(255, int(c * factor))) for c in colour)
    return r, g, b, 255


def render(model: Model, direction: int, frame: int) -> Image.Image:
    """One cell: the creature facing a direction of the sheet, at a frame of its walk."""
    yaw = direction * 2.0 * math.pi / DIRECTIONS
    phase = frame * 2.0 * math.pi / FRAMES
    ce, se = math.cos(ELEVATION), math.sin(ELEVATION)
    light = _norm(LIGHT)
    depth: dict[tuple[int, int], float] = {}
    colour: dict[tuple[int, int], tuple[int, int, int, int]] = {}
    for part in model.parts:
        for point, normal in part.points:
            moved = _pose(model, part.bone, point, phase)
            world = _rot_y(moved, yaw)
            n = _rot_y(normal, yaw)
            if part.bone.startswith(("leg", "arm", "weapon")):
                # Normals of a swung limb are close enough unturned at this size.
                pass
            sx = world[0]
            sy = world[1] * ce - world[2] * se
            d = world[2] * ce + world[1] * se
            nv = (n[0], n[1] * ce - n[2] * se, n[2] * ce + n[1] * se)
            if nv[2] < -0.35:
                continue
            k = nv[0] * light[0] + nv[1] * light[1] + nv[2] * light[2]
            shade = 1.18 if k > 0.6 else (1.0 if k > 0.12 else 0.72)
            px = int(round(FOOT[0] + sx * PPU))
            py = int(round(FOOT[1] - sy * PPU))
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                key = (px + dx, py + dy)
                if 0 <= key[0] < CELL and 0 <= key[1] < CELL and depth.get(key, -1e9) < d:
                    depth[key] = d
                    colour[key] = _shade(part.colour, shade)
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    pixels = image.load()
    assert pixels is not None
    for (x, y), c in colour.items():
        pixels[x, y] = c
    # A dark rim on the silhouette, one pixel inside it.
    for (x, y), c in colour.items():
        if any((x + dx, y + dy) not in colour for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            pixels[x, y] = (int(c[0] * 0.38), int(c[1] * 0.38), int(c[2] * 0.38), 255)
    return image


def sheet(model: Model) -> Image.Image:
    """Every direction (columns) and walk frame (rows) of a creature."""
    image = Image.new("RGBA", (CELL * DIRECTIONS, CELL * FRAMES), (0, 0, 0, 0))
    for frame in range(FRAMES):
        for direction in range(DIRECTIONS):
            image.paste(render(model, direction, frame), (direction * CELL, frame * CELL))
    return image


SKIN = (96, 150, 58)
SKIN_DARK = (70, 112, 44)
LEATHER = (110, 72, 42)
CLOTH_RED = (150, 34, 28)
METAL = (150, 152, 162)
BONE = (232, 224, 196)
WOOD = (120, 80, 44)
EYE = (250, 220, 60)
OGRE_SKIN = (196, 140, 96)


def goblin() -> Model:
    """A goblin, 0.5 tall: big ears, a long nose, a rag tunic and a dagger."""
    m = Model(stride=0.7, bob=0.025)
    m.pivots = {"hip": (0.0, 0.16, 0.0), "shoulder": (0.0, 0.3, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.055, 0.15, 0.0), (side * 0.06, 0.04, 0.0), 0.03, SKIN_DARK, bone)
        m.ellipsoid((side * 0.06, 0.025, 0.025), (0.035, 0.025, 0.05), LEATHER, bone)
    m.ellipsoid((0.0, 0.23, 0.0), (0.085, 0.1, 0.07), LEATHER)
    m.ellipsoid((0.0, 0.17, 0.0), (0.09, 0.035, 0.075), (80, 50, 30))
    m.ellipsoid((0.0, 0.37, 0.02), (0.075, 0.068, 0.07), SKIN)
    m.ellipsoid((0.0, 0.36, 0.09), (0.02, 0.018, 0.035), SKIN_DARK)
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.105, 0.39, 0.0), (0.065, 0.017, 0.028), SKIN)
        m.ellipsoid((side * 0.03, 0.385, 0.075), (0.013, 0.012, 0.01), EYE)
    m.capsule((0.09, 0.3, 0.0), (0.11, 0.19, 0.04), 0.022, SKIN, "arm_l")
    m.capsule((-0.09, 0.3, 0.0), (-0.11, 0.19, 0.04), 0.022, SKIN, "arm_r")
    m.capsule((-0.11, 0.19, 0.05), (-0.11, 0.3, 0.12), 0.012, METAL, "weapon")
    return m


def grunt() -> Model:
    """An orc grunt, 0.75 tall: broad shoulders under red armour and pads, tusks, an axe."""
    m = Model(stride=0.55, bob=0.02)
    m.pivots = {"hip": (0.0, 0.24, 0.0), "shoulder": (0.0, 0.48, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.08, 0.23, 0.0), (side * 0.085, 0.06, 0.0), 0.05, LEATHER, bone)
        m.ellipsoid((side * 0.085, 0.035, 0.03), (0.05, 0.035, 0.07), (60, 42, 30), bone)
    m.ellipsoid((0.0, 0.38, 0.0), (0.155, 0.15, 0.11), CLOTH_RED)
    m.ellipsoid((0.0, 0.26, 0.0), (0.13, 0.04, 0.1), (70, 46, 28))
    m.ellipsoid((0.0, 0.31, 0.105), (0.04, 0.035, 0.012), METAL)
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.17, 0.48, 0.0), (0.075, 0.05, 0.075), METAL)
    m.ellipsoid((0.0, 0.58, 0.03), (0.085, 0.08, 0.082), SKIN)
    m.ellipsoid((0.0, 0.545, 0.075), (0.06, 0.035, 0.04), SKIN_DARK)
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.035, 0.56, 0.11), (0.011, 0.025, 0.011), BONE)
        m.ellipsoid((side * 0.03, 0.605, 0.09), (0.014, 0.01, 0.01), (230, 60, 30))
    m.capsule((0.17, 0.46, 0.0), (0.19, 0.3, 0.05), 0.04, SKIN, "arm_l")
    m.capsule((-0.17, 0.46, 0.0), (-0.19, 0.3, 0.05), 0.04, SKIN, "arm_r")
    m.capsule((-0.19, 0.28, 0.05), (-0.19, 0.55, 0.12), 0.016, WOOD, "weapon")
    m.box((-0.19, 0.53, 0.17), (0.012, 0.07, 0.05), METAL, "weapon", tilt=0.3)
    return m


def ogre() -> Model:
    """An ogre, a full unit tall: a huge belly, a small head, a loincloth and a club."""
    m = Model(stride=0.4, bob=0.03)
    m.pivots = {"hip": (0.0, 0.3, 0.0), "shoulder": (0.0, 0.66, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.12, 0.3, 0.0), (side * 0.13, 0.07, 0.0), 0.075, OGRE_SKIN, bone)
        m.ellipsoid((side * 0.13, 0.04, 0.04), (0.07, 0.04, 0.09), (90, 60, 40), bone)
    m.ellipsoid((0.0, 0.48, 0.03), (0.24, 0.22, 0.2), OGRE_SKIN)
    m.ellipsoid((0.0, 0.31, 0.02), (0.2, 0.06, 0.17), (90, 110, 150))
    m.ellipsoid((0.0, 0.67, -0.02), (0.2, 0.08, 0.14), OGRE_SKIN)
    m.ellipsoid((0.0, 0.8, 0.05), (0.095, 0.09, 0.09), OGRE_SKIN)
    m.ellipsoid((0.0, 0.77, 0.12), (0.03, 0.025, 0.03), (170, 110, 80))
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.035, 0.82, 0.12), (0.016, 0.012, 0.01), EYE)
        m.ellipsoid((side * 0.05, 0.745, 0.12), (0.01, 0.022, 0.01), BONE)
    m.capsule((0.23, 0.64, 0.0), (0.27, 0.4, 0.06), 0.065, OGRE_SKIN, "arm_l")
    m.capsule((-0.23, 0.64, 0.0), (-0.27, 0.4, 0.06), 0.065, OGRE_SKIN, "arm_r")
    m.capsule((-0.27, 0.38, 0.06), (-0.28, 0.75, 0.2), 0.03, WOOD, "weapon", end_radius=0.07)
    return m


CREATURES: dict[str, Callable[[], Model]] = {"goblin": goblin, "grunt": grunt, "ogre": ogre}
"""The creature models, by kind."""
