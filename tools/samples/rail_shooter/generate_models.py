#!/usr/bin/env python3
"""
Write the 3D models of the Rail shooter sample game into ``samples/rail_shooter/models/`` as binary glTF.

- Ships and asteroids: OBJ models of the Kenney Space Kit (CC0, https://opengameart.org/content/space-kit), one
  primitive per material of their ``.mtl`` (flat colours), scaled to the size the game uses and turned so that their
  nose points along -Z, the forward axis of the engine. The pack is read from the folder given on the command line
  (the ``Models`` folder of the unpacked archive); nothing of it is copied but these conversions.
- Bolts: the player's laser (a long cyan spindle) and the enemy shot (an orange gem), emissive, made here.

Run from the repository root:
``./docker_run.sh poetry run python -m tools.samples.rail_shooter.generate_models <Space Kit>/Models``.
"""

from __future__ import annotations

import json
import math
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3] / "samples" / "rail_shooter" / "models"
"""Folder of the written models."""

Vec3 = tuple[float, float, float]
Colour = tuple[float, float, float]


@dataclass
class Primitive:
    """Triangles of one material: flat lists of positions and normals, three vertices per triangle."""

    colour: Colour
    emissive: Colour = (0.0, 0.0, 0.0)
    positions: list[Vec3] = field(default_factory=list)
    normals: list[Vec3] = field(default_factory=list)


@dataclass
class Model:
    """A model to write: its primitives, in the order of their materials."""

    name: str
    primitives: list[Primitive]


@dataclass
class Conversion:
    """A Space Kit model converted: its source, its size along X once written, its tints by material."""

    source: str
    target: str
    width: float
    tints: dict[str, Colour] = field(default_factory=dict)
    turn: bool = True


CONVERSIONS = [
    Conversion(
        "spaceCraft1",
        "player",
        2.2,
        {"metal": (0.2, 0.45, 0.85), "glass": (0.4, 0.75, 0.95), "_defaultMat": (0.72, 0.75, 0.8)},
    ),
    Conversion(
        "spaceCraft3",
        "fighter",
        2.0,
        {"metal": (0.8, 0.22, 0.2), "pipe": (0.35, 0.35, 0.4), "_defaultMat": (0.5, 0.5, 0.55)},
    ),
    Conversion(
        "spaceCraft4",
        "interceptor",
        2.4,
        {"metal": (0.85, 0.55, 0.12), "pipe": (0.35, 0.35, 0.4), "_defaultMat": (0.45, 0.45, 0.5)},
    ),
    Conversion(
        "spaceCraft6",
        "boss",
        22.0,
        {"metal": (0.55, 0.12, 0.2), "_defaultMat": (0.42, 0.42, 0.48), "pipe": (0.2, 0.2, 0.24)},
    ),
    Conversion("meteorFull", "asteroid_a", 2.0, {"rock": (0.6, 0.5, 0.43)}, turn=False),
    Conversion("meteorFullRound", "asteroid_b", 2.0, {"rock": (0.55, 0.52, 0.5)}, turn=False),
    Conversion(
        "meteorHalf", "asteroid_c", 2.0, {"rock": (0.62, 0.55, 0.48), "_defaultMat": (0.45, 0.42, 0.4)}, turn=False
    ),
]
"""The models taken from the Space Kit; ``tints`` replace the colour of a material (sRGB)."""


def _linear(colour: Colour) -> Colour:
    """An sRGB colour in linear space, as glTF factors are."""
    r, g, b = (c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in colour)
    return r, g, b


def _read_mtl(path: Path) -> dict[str, Colour]:
    """The diffuse colour of each material of a ``.mtl`` file."""
    colours: dict[str, Colour] = {}
    name = ""
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "newmtl":
            name = parts[1]
        elif parts[0] == "Kd" and name:
            colours[name] = (float(parts[1]), float(parts[2]), float(parts[3]))
    return colours


def _index(token: str, count: int) -> int:
    """A zero-based index from an OBJ index (negative ones count from the end)."""
    value = int(token)
    return value - 1 if value > 0 else count + value


def read_obj(path: Path, conversion: Conversion) -> Model:
    """An OBJ model and its materials, fans of its polygons as triangles, scaled and turned as told."""
    colours = _read_mtl(path.with_suffix(".mtl"))
    positions: list[Vec3] = []
    normals: list[Vec3] = []
    faces: dict[str, list[list[tuple[int, int]]]] = {}
    material = "_defaultMat"
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            positions.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif parts[0] == "vn":
            normals.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif parts[0] == "usemtl":
            material = parts[1]
        elif parts[0] == "f":
            corners = []
            for token in parts[1:]:
                fields = token.split("/")
                normal = _index(fields[2], len(normals)) if len(fields) > 2 and fields[2] else -1
                corners.append((_index(fields[0], len(positions)), normal))
            faces.setdefault(material, []).append(corners)
    low = [min(p[k] for p in positions) for k in range(3)]
    high = [max(p[k] for p in positions) for k in range(3)]
    scale = conversion.width / (high[0] - low[0])
    centre = [(low[k] + high[k]) / 2 for k in range(3)]
    sign = -1.0 if conversion.turn else 1.0

    def place(point: Vec3) -> Vec3:
        x, y, z = ((point[k] - centre[k]) * scale for k in range(3))
        return sign * x, y, sign * z

    def turn(normal: Vec3) -> Vec3:
        return sign * normal[0], normal[1], sign * normal[2]

    primitives = []
    for name, polygons in faces.items():
        colour = conversion.tints.get(name, colours.get(name, (1.0, 1.0, 1.0)))
        primitive = Primitive(colour)
        for corners in polygons:
            for i in range(1, len(corners) - 1):
                triangle = [corners[0], corners[i], corners[i + 1]]
                points = [place(positions[c[0]]) for c in triangle]
                flat = _face_normal(points)
                for point, (_, normal) in zip(points, triangle, strict=True):
                    primitive.positions.append(point)
                    primitive.normals.append(turn(normals[normal]) if normal >= 0 else flat)
        primitives.append(primitive)
    return Model(conversion.target, primitives)


def _face_normal(points: list[Vec3]) -> Vec3:
    """Unit normal of a triangle given counter-clockwise."""
    a, b, c = points
    u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    length = math.sqrt(sum(x * x for x in n)) or 1.0
    return n[0] / length, n[1] / length, n[2] / length


def spindle(length: float, radius: float, sides: int, colour: Colour, glow: Colour, name: str) -> Model:
    """A double cone along Z, ``length`` long: the bolts."""
    primitive = Primitive(colour, glow)
    tip_front: Vec3 = (0.0, 0.0, -length / 2)
    tip_back: Vec3 = (0.0, 0.0, length / 2)
    ring = [
        (radius * math.cos(2 * math.pi * k / sides), radius * math.sin(2 * math.pi * k / sides), 0.0)
        for k in range(sides)
    ]
    for k in range(sides):
        a, b = ring[k], ring[(k + 1) % sides]
        for triangle in ([tip_front, b, a], [tip_back, a, b]):
            normal = _face_normal(triangle)
            primitive.positions += triangle
            primitive.normals += [normal] * 3
    return Model(name, [primitive])


def write_glb(path: Path, model: Model) -> None:
    """Write a model as a binary glTF 2.0 file of one mesh, one primitive per material."""
    blob = b""
    views: list[dict[str, int]] = []
    accessors: list[dict[str, object]] = []
    primitives: list[dict[str, object]] = []
    materials: list[dict[str, object]] = []
    for index, primitive in enumerate(model.primitives):
        count = len(primitive.positions)
        chunks = [
            b"".join(struct.pack("<3f", *p) for p in primitive.positions),
            b"".join(struct.pack("<3f", *n) for n in primitive.normals),
            struct.pack("<" + "2f" * count, *([0.0, 0.0] * count)),
        ]
        first = len(accessors)
        low = [min(p[k] for p in primitive.positions) for k in range(3)]
        high = [max(p[k] for p in primitive.positions) for k in range(3)]
        for kind, chunk in zip(("VEC3", "VEC3", "VEC2"), chunks, strict=True):
            views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(chunk), "target": 34962})
            accessor: dict[str, object] = {
                "bufferView": len(views) - 1,
                "componentType": 5126,
                "count": count,
                "type": kind,
            }
            if len(accessors) == first:
                accessor["min"], accessor["max"] = low, high
            accessors.append(accessor)
            blob += chunk + b"\0" * (-len(chunk) % 4)
        primitives.append(
            {"attributes": {"POSITION": first, "NORMAL": first + 1, "TEXCOORD_0": first + 2}, "material": index}
        )
        material: dict[str, object] = {
            "name": f"{model.name}_{index}",
            "pbrMetallicRoughness": {
                "baseColorFactor": [*_linear(primitive.colour), 1.0],
                "metallicFactor": 0.1,
                "roughnessFactor": 0.6,
            },
        }
        if any(primitive.emissive):
            material["emissiveFactor"] = list(_linear(primitive.emissive))
        materials.append(material)
    doc = {
        "asset": {"version": "2.0", "generator": "Owl tools/samples/rail_shooter/generate_models.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": model.name}],
        "meshes": [{"name": model.name, "primitives": primitives}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(blob), 0x004E4942) + blob
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def main() -> None:
    """Convert the Space Kit models named on the command line's folder and write the bolts."""
    if len(sys.argv) != 2:
        sys.exit("usage: generate_models <Space Kit>/Models")
    pack = Path(sys.argv[1])
    ROOT.mkdir(parents=True, exist_ok=True)
    for conversion in CONVERSIONS:
        write_glb(ROOT / f"{conversion.target}.glb", read_obj(pack / f"{conversion.source}.obj", conversion))
    write_glb(ROOT / "bolt.glb", spindle(1.6, 0.09, 6, (0.6, 0.95, 1.0), (0.4, 0.9, 1.0), "bolt"))
    write_glb(ROOT / "enemy_bolt.glb", spindle(0.9, 0.22, 5, (1.0, 0.55, 0.2), (1.0, 0.45, 0.1), "enemy_bolt"))


if __name__ == "__main__":
    main()
