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

CELL = 96
"""Side of a sprite cell, in pixels."""
PPU = 80.0
"""Pixels per world unit of the sprites."""
FOOT = (48, 82)
"""Pixel of the foot of a creature in its cell."""
DIRECTIONS = 8
"""Directions of a sheet (columns)."""
WALK, ATTACK, DEATH = 4, 2, 3
"""Frames of the walk cycle, of the blow at the keep and of the fall."""
FRAMES = WALK + ATTACK + DEATH
"""Rows of a sheet: the walk, then the attack, then the death."""
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


def _limb_angle(model: Model, bone: str, row: int) -> float:
    """Swing of a limb (radians, around the X axis at its joint) in a row of the sheet."""
    if row < WALK:
        swing = math.sin(row * 2.0 * math.pi / WALK)
        return {
            "leg_l": model.stride * swing,
            "leg_r": -model.stride * swing,
            "arm_l": -0.7 * model.stride * swing,
            "arm_r": 0.5 * model.stride * swing,
            "weapon": 0.5 * model.stride * swing,
        }.get(bone, 0.0)
    if row < WALK + ATTACK:
        raised = row == WALK
        return {"arm_r": -2.4 if raised else 0.6, "weapon": -2.4 if raised else 0.6, "arm_l": -0.4}.get(bone, 0.0)
    return {"arm_r": -1.2, "weapon": -1.2, "arm_l": -1.0, "leg_l": 0.3, "leg_r": -0.2}.get(bone, 0.0)


def _pose(model: Model, bone: str, point: Vec, row: int) -> Vec:
    """A point moved by its bone in a row of the sheet: the walk, the blow, or the fall backwards."""
    angle = _limb_angle(model, bone, row)
    if angle != 0.0:
        pivot = model.pivots.get("hip" if bone.startswith("leg") else "shoulder", (0.0, 0.0, 0.0))
        local = _rot_x((point[0] - pivot[0], point[1] - pivot[1], point[2] - pivot[2]), angle)
        point = (local[0] + pivot[0], local[1] + pivot[1], local[2] + pivot[2])
    if row < WALK:
        lift = model.bob * abs(math.cos(row * 2.0 * math.pi / WALK))
        if not bone.startswith("leg"):
            point = (point[0], point[1] + lift, point[2])
    elif row < WALK + ATTACK:
        lean = 0.12 if row == WALK + 1 else -0.08
        point = _rot_x(point, lean)
    else:
        fall = (row - WALK - ATTACK + 1) / DEATH
        point = _rot_x(point, -1.45 * fall)
        point = (point[0], point[1] + 0.04 * fall, point[2])
    return point


def _shade(colour: RGB, factor: float) -> tuple[int, int, int, int]:
    r, g, b = (max(0, min(255, int(c * factor))) for c in colour)
    return r, g, b, 255


def render(model: Model, direction: int, row: int) -> Image.Image:
    """One cell: the creature facing a direction of the sheet, in a row of its animation."""
    yaw = direction * 2.0 * math.pi / DIRECTIONS
    ce, se = math.cos(ELEVATION), math.sin(ELEVATION)
    light = _norm(LIGHT)
    depth: dict[tuple[int, int], float] = {}
    colour: dict[tuple[int, int], tuple[int, int, int, int]] = {}
    for part in model.parts:
        for point, normal in part.points:
            moved = _pose(model, part.bone, point, row)
            world = _rot_y(moved, yaw)
            n = _rot_y(normal, yaw)
            if row >= WALK + ATTACK:
                n = _rot_y(_rot_x(normal, -1.45 * (row - WALK - ATTACK + 1) / DEATH), yaw)
            sx = world[0]
            sy = world[1] * ce - world[2] * se
            d = world[2] * ce + world[1] * se
            nv = (n[0], n[1] * ce - n[2] * se, n[2] * ce + n[1] * se)
            if nv[2] < -0.35:
                continue
            k = nv[0] * light[0] + nv[1] * light[1] + nv[2] * light[2]
            shade = 1.25 if k > 0.72 else (1.05 if k > 0.35 else (0.86 if k > 0.0 else 0.64))
            if nv[2] < 0.25 and k < 0.2:
                shade += 0.12
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
    # A dark line one pixel outside the silhouette, and a softer one where two depths meet inside it.
    for (x, y), c in colour.items():
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            other = (x + dx, y + dy)
            if other not in colour and 0 <= other[0] < CELL and 0 <= other[1] < CELL:
                pixels[other[0], other[1]] = (24, 16, 10, 255)
        below = (x, y + 1)
        if below in depth and depth[below] - depth[(x, y)] > 0.08:
            pixels[x, y] = (int(c[0] * 0.55), int(c[1] * 0.55), int(c[2] * 0.55), 255)
    return image


def sheet(model: Model) -> Image.Image:
    """Every direction (columns) and animation frame (rows) of a creature."""
    image = Image.new("RGBA", (CELL * DIRECTIONS, CELL * FRAMES), (0, 0, 0, 0))
    for row in range(FRAMES):
        for direction in range(DIRECTIONS):
            image.paste(render(model, direction, row), (direction * CELL, row * CELL))
    return image


SKIN = (104, 158, 60)
SKIN_DARK = (74, 118, 44)
GRUNT_SKIN = (84, 132, 58)
LEATHER = (118, 76, 42)
LEATHER_DARK = (78, 50, 30)
CLOTH_RED = (164, 36, 30)
CLOTH_BLACK = (44, 38, 40)
METAL = (160, 162, 172)
METAL_DARK = (96, 98, 108)
GOLD = (220, 176, 70)
BONE = (236, 228, 200)
WOOD = (126, 84, 46)
EYE = (255, 214, 40)
RED_EYE = (240, 50, 30)
OGRE_SKIN = (214, 148, 96)
OGRE_DARK = (170, 110, 70)
BLUE_CLOTH = (60, 84, 150)


def goblin() -> Model:
    """A goblin, 0.55 tall: huge pointed ears, a long nose, red eyes, a leather vest and belt, a curved dagger."""
    m = Model(stride=0.75, bob=0.03)
    m.pivots = {"hip": (0.0, 0.17, 0.0), "shoulder": (0.0, 0.32, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.055, 0.16, 0.0), (side * 0.065, 0.05, 0.01), 0.032, SKIN_DARK, bone)
        m.ellipsoid((side * 0.068, 0.03, 0.035), (0.04, 0.028, 0.06), LEATHER_DARK, bone)
    m.ellipsoid((0.0, 0.18, 0.0), (0.085, 0.045, 0.07), LEATHER_DARK)
    m.ellipsoid((0.0, 0.25, 0.005), (0.085, 0.09, 0.07), LEATHER)
    m.ellipsoid((0.0, 0.25, 0.06), (0.03, 0.08, 0.02), SKIN)
    m.box((0.0, 0.195, 0.07), (0.022, 0.016, 0.006), GOLD)
    m.ellipsoid((0.0, 0.4, 0.02), (0.078, 0.07, 0.072), SKIN)
    m.ellipsoid((0.0, 0.385, 0.1), (0.022, 0.02, 0.045), SKIN_DARK)
    m.ellipsoid((0.0, 0.355, 0.075), (0.04, 0.012, 0.012), BONE)
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.12, 0.43, -0.01), (0.075, 0.018, 0.03), SKIN)
        m.ellipsoid((side * 0.115, 0.43, 0.0), (0.05, 0.01, 0.018), (200, 120, 110))
        m.ellipsoid((side * 0.032, 0.41, 0.078), (0.014, 0.012, 0.01), RED_EYE)
    m.ellipsoid((0.0, 0.455, -0.01), (0.05, 0.018, 0.05), LEATHER_DARK)
    m.capsule((0.09, 0.31, 0.0), (0.115, 0.2, 0.04), 0.022, SKIN, "arm_l")
    m.capsule((-0.09, 0.31, 0.0), (-0.115, 0.2, 0.04), 0.022, SKIN, "arm_r")
    m.capsule((-0.115, 0.2, 0.05), (-0.11, 0.33, 0.13), 0.012, METAL, "weapon")
    m.capsule((-0.115, 0.19, 0.03), (-0.115, 0.215, 0.07), 0.016, LEATHER_DARK, "weapon")
    return m


def grunt() -> Model:
    """An orc grunt, 0.8 tall: a black topknot, tusks, red armour under spiked iron pauldrons, a heavy axe."""
    m = Model(stride=0.55, bob=0.025)
    m.pivots = {"hip": (0.0, 0.25, 0.0), "shoulder": (0.0, 0.5, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.085, 0.24, 0.0), (side * 0.09, 0.07, 0.0), 0.052, LEATHER, bone)
        m.capsule((side * 0.09, 0.13, 0.0), (side * 0.09, 0.07, 0.0), 0.058, LEATHER_DARK, bone)
        m.ellipsoid((side * 0.092, 0.04, 0.035), (0.058, 0.04, 0.08), METAL_DARK, bone)
    m.ellipsoid((0.0, 0.27, 0.0), (0.14, 0.05, 0.11), LEATHER_DARK)
    m.box((0.0, 0.25, 0.1), (0.05, 0.06, 0.012), CLOTH_RED)
    m.ellipsoid((0.0, 0.4, 0.0), (0.16, 0.15, 0.115), CLOTH_RED)
    m.ellipsoid((0.0, 0.42, 0.06), (0.11, 0.1, 0.07), GRUNT_SKIN)
    m.ellipsoid((0.0, 0.31, 0.0), (0.15, 0.025, 0.115), LEATHER_DARK)
    m.box((0.0, 0.31, 0.115), (0.03, 0.022, 0.008), GOLD)
    for side in (1.0, -1.0):
        m.ellipsoid((side * 0.18, 0.5, 0.0), (0.085, 0.055, 0.085), METAL)
        for spike in (-0.04, 0.04):
            m.capsule((side * 0.2, 0.55, spike), (side * 0.24, 0.62, spike), 0.012, METAL_DARK, end_radius=0.003)
    m.ellipsoid((0.0, 0.61, 0.03), (0.088, 0.085, 0.085), GRUNT_SKIN)
    m.ellipsoid((0.0, 0.57, 0.08), (0.065, 0.04, 0.045), SKIN_DARK)
    m.ellipsoid((0.0, 0.69, -0.02), (0.05, 0.03, 0.05), CLOTH_BLACK)
    m.capsule((0.0, 0.7, -0.05), (0.0, 0.78, -0.08), 0.022, CLOTH_BLACK)
    for side in (1.0, -1.0):
        m.capsule((side * 0.035, 0.57, 0.115), (side * 0.045, 0.62, 0.13), 0.011, BONE)
        m.ellipsoid((side * 0.032, 0.635, 0.093), (0.015, 0.01, 0.01), RED_EYE)
    m.capsule((0.18, 0.48, 0.0), (0.2, 0.31, 0.05), 0.042, GRUNT_SKIN, "arm_l")
    m.capsule((0.2, 0.36, 0.03), (0.2, 0.31, 0.05), 0.048, LEATHER_DARK, "arm_l")
    m.capsule((-0.18, 0.48, 0.0), (-0.2, 0.31, 0.05), 0.042, GRUNT_SKIN, "arm_r")
    m.capsule((-0.2, 0.36, 0.03), (-0.2, 0.31, 0.05), 0.048, LEATHER_DARK, "arm_r")
    m.capsule((-0.2, 0.28, 0.05), (-0.2, 0.6, 0.13), 0.017, WOOD, "weapon")
    m.box((-0.2, 0.57, 0.19), (0.014, 0.085, 0.065), METAL, "weapon", tilt=0.3)
    m.box((-0.2, 0.57, 0.15), (0.016, 0.03, 0.02), METAL_DARK, "weapon", tilt=0.3)
    return m


def ogre() -> Model:
    """A two-headed ogre, 1.05 tall: a huge belly, tusks, iron bracers, a blue loincloth and a spiked club."""
    m = Model(stride=0.4, bob=0.035)
    m.pivots = {"hip": (0.0, 0.3, 0.0), "shoulder": (0.0, 0.68, 0.0)}
    for side, bone in ((1.0, "leg_l"), (-1.0, "leg_r")):
        m.capsule((side * 0.12, 0.31, 0.0), (side * 0.13, 0.08, 0.0), 0.08, OGRE_SKIN, bone)
        m.ellipsoid((side * 0.13, 0.045, 0.045), (0.075, 0.045, 0.1), OGRE_DARK, bone)
    m.ellipsoid((0.0, 0.48, 0.04), (0.25, 0.23, 0.21), OGRE_SKIN)
    m.ellipsoid((0.0, 0.44, 0.15), (0.17, 0.14, 0.1), (226, 166, 118))
    m.ellipsoid((0.0, 0.31, 0.02), (0.21, 0.065, 0.18), BLUE_CLOTH)
    m.box((0.0, 0.24, 0.15), (0.09, 0.07, 0.012), BLUE_CLOTH)
    m.ellipsoid((0.0, 0.31, 0.19), (0.04, 0.03, 0.01), GOLD)
    m.ellipsoid((0.0, 0.69, -0.02), (0.22, 0.09, 0.15), OGRE_SKIN)
    for side in (1.0, -1.0):
        head = (side * 0.11, 0.82, 0.05)
        m.ellipsoid(head, (0.085, 0.085, 0.085), OGRE_SKIN)
        m.ellipsoid((head[0], 0.79, 0.12), (0.03, 0.025, 0.03), OGRE_DARK)
        m.ellipsoid((head[0], 0.875, 0.04), (0.06, 0.02, 0.06), (70, 46, 30))
        for eye in (-0.03, 0.03):
            m.ellipsoid((head[0] + eye, 0.84, 0.125), (0.013, 0.011, 0.01), EYE)
        for tusk in (-0.03, 0.03):
            m.capsule((head[0] + tusk, 0.765, 0.12), (head[0] + tusk, 0.8, 0.135), 0.01, BONE)
    m.capsule((0.24, 0.66, 0.0), (0.28, 0.42, 0.06), 0.066, OGRE_SKIN, "arm_l")
    m.capsule((0.27, 0.48, 0.04), (0.28, 0.42, 0.06), 0.074, METAL_DARK, "arm_l")
    m.capsule((-0.24, 0.66, 0.0), (-0.28, 0.42, 0.06), 0.066, OGRE_SKIN, "arm_r")
    m.capsule((-0.27, 0.48, 0.04), (-0.28, 0.42, 0.06), 0.074, METAL_DARK, "arm_r")
    m.capsule((-0.28, 0.38, 0.06), (-0.29, 0.8, 0.22), 0.03, WOOD, "weapon", end_radius=0.075)
    for k in range(5):
        a = k * 2.0 * math.pi / 5
        base = (-0.29 + 0.06 * math.cos(a), 0.74, 0.2 + 0.06 * math.sin(a))
        tip = (-0.29 + 0.11 * math.cos(a), 0.76, 0.2 + 0.11 * math.sin(a))
        m.capsule(base, tip, 0.012, METAL, "weapon", end_radius=0.003)
    return m


CREATURES: dict[str, Callable[[], Model]] = {"goblin": goblin, "grunt": grunt, "ogre": ogre}
"""The creature models, by kind."""
