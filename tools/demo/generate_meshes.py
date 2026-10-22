#!/usr/bin/env python3
"""Generate the procedural meshes of the Engine Demo tavern (``engine_demo/meshes/tavern/*.glb``: room shell, hearth,
mantel beam, fire grate, firewood, ember bed, flame cards, a candle, the cellar trapdoor, key, chest, barrel and
ladder, in metres, UVs in world units so the tiled Poly
Haven materials keep their scale) and the fire textures (``engine_demo/textures/fire/``)."""

import json
import math
import random
import struct
from pathlib import Path

from PIL import Image, ImageFilter

DEMO = Path(__file__).resolve().parents[2] / "engine_demo"
OUT = DEMO / "meshes" / "tavern"
TEXTURES = DEMO / "textures" / "fire"

Vec3 = tuple[float, float, float]
Vertex = tuple[Vec3, Vec3, tuple[float, float]]


class MeshBuilder:
    """Accumulates vertices and triangles (counter-clockwise front faces)."""

    def __init__(self) -> None:
        self.vertices: list[Vertex] = []
        self.indices: list[int] = []

    def add(self, position, normal, uv) -> int:
        """Add one vertex and return its index."""
        self.vertices.append((tuple(position), tuple(normal), tuple(uv)))
        return len(self.vertices) - 1

    def quad(self, a: int, b: int, c: int, d: int) -> None:
        """Two triangles of the quad a-b-c-d, given counter-clockwise."""
        self.indices += [a, b, c, a, c, d]

    def extend(self, other: "MeshBuilder") -> "MeshBuilder":
        """Append the vertices and triangles of another mesh."""
        base = len(self.vertices)
        self.vertices += other.vertices
        self.indices += [base + i for i in other.indices]
        return self


def normalize(v) -> Vec3:
    """Unit vector of v."""
    length = math.sqrt(sum(x * x for x in v)) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


def cross(a, b) -> Vec3:
    """Cross product a x b."""
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b) -> float:
    """Dot product."""
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def box(centre: Vec3, size: Vec3, tile: float = 1.0) -> MeshBuilder:
    """Axis-aligned box of ``size`` metres around ``centre``, UVs in world units (one repeat per ``tile`` metres)."""
    m = MeshBuilder()
    half = (size[0] / 2, size[1] / 2, size[2] / 2)
    faces = [  # normal, u axis, v axis
        ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
        ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
        ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ]
    for n, u, v in faces:
        hu = sum(abs(u[i]) * half[i] for i in range(3))
        hv = sum(abs(v[i]) * half[i] for i in range(3))
        corners = []
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = tuple(centre[i] + n[i] * half[i] + u[i] * su * hu + v[i] * sv * hv for i in range(3))
            corners.append(m.add(p, n, (dot(p, u) / tile, -dot(p, v) / tile)))
        m.quad(*corners)
    return m


def lattice(ix: int, iy: int, iz: int, seed: int) -> float:
    """Hashed value in [-1, 1] at an integer lattice point."""
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h * 1103515245 + 12345) & 0x7FFFFFFF
    h ^= h >> 13
    h = (h * 1274126177) & 0x7FFFFFFF
    return h / 0x3FFFFFFF - 1.0


def value_noise(p: Vec3, seed: int) -> float:
    """Smooth 3D value noise in about [-1, 1]."""
    base = [math.floor(c) for c in p]
    f = [c - b for c, b in zip(p, base, strict=True)]
    w = [t * t * (3 - 2 * t) for t in f]
    total = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                weight = (w[0] if dx else 1 - w[0]) * (w[1] if dy else 1 - w[1]) * (w[2] if dz else 1 - w[2])
                total += weight * lattice(base[0] + dx, base[1] + dy, base[2] + dz, seed)
    return total


def fbm(p: Vec3, seed: int, freq: float, octaves: int = 3) -> float:
    """Fractal sum of value noise, about [-1, 1]."""
    total, amplitude, norm = 0.0, 1.0, 0.0
    for octave in range(octaves):
        scale = freq * 2.0 ** octave
        total += amplitude * value_noise((p[0] * scale, p[1] * scale, p[2] * scale), seed + octave)
        norm += amplitude
        amplitude *= 0.5
    return total / norm


def smooth_normals(m: MeshBuilder) -> MeshBuilder:
    """Recompute the vertex normals from the faces, averaged over the vertices sharing a position."""
    key = [tuple(round(c, 5) for c in v[0]) for v in m.vertices]
    sums: dict[tuple, list[float]] = {}
    for t in range(0, len(m.indices), 3):
        a, b, c = (m.vertices[m.indices[t + k]][0] for k in range(3))
        n = cross(tuple(b[i] - a[i] for i in range(3)), tuple(c[i] - a[i] for i in range(3)))
        for k in range(3):
            acc = sums.setdefault(key[m.indices[t + k]], [0.0, 0.0, 0.0])
            for i in range(3):
                acc[i] += n[i]
    m.vertices = [(v[0], normalize(sums.get(key[i], v[1])), v[2]) for i, v in enumerate(m.vertices)]
    return m


def shaped_box(centre: Vec3, size: Vec3, tile: float, cell: float, radius: float, amp: float, freq: float,
               seed: int, warp=None) -> MeshBuilder:
    """Box with rounded edges of ``radius``, faces subdivided in cells of about ``cell`` metres and displaced along the
    rounded normal by fractal noise of ``amp`` metres, then bent by ``warp`` (a function of the local position).

    Every displacement depends on the position only, so the faces stay joined; UVs are the world units of the flat
    box (one repeat per ``tile`` metres) and the normals are smoothed over the result."""
    m = MeshBuilder()
    half = (size[0] / 2, size[1] / 2, size[2] / 2)
    inner = tuple(max(0.0, h - radius) for h in half)
    faces = [
        ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
        ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
        ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ]

    def steps(h: float) -> list[float]:
        count = max(1, math.ceil(2 * h / cell))
        values = {round(-h + 2 * h * k / count, 6) for k in range(count + 1)}
        if radius < h:
            values |= {round(-h + radius, 6), round(h - radius, 6)}
        return sorted(values)

    def shape(q: list[float]) -> Vec3:
        clamped = [max(-inner[i], min(inner[i], q[i])) for i in range(3)]
        d = [q[i] - clamped[i] for i in range(3)]
        n = normalize(d)
        rounded = [clamped[i] + n[i] * radius for i in range(3)] if radius > 0 else list(q)
        world = (centre[0] + q[0], centre[1] + q[1], centre[2] + q[2])
        offset = amp * fbm(world, seed, freq)
        out = [rounded[i] + n[i] * offset for i in range(3)]
        if warp:
            bend = warp(q)
            out = [out[i] + bend[i] for i in range(3)]
        return (centre[0] + out[0], centre[1] + out[1], centre[2] + out[2])

    for n, u, v in faces:
        hu = sum(abs(u[i]) * half[i] for i in range(3))
        hv = sum(abs(v[i]) * half[i] for i in range(3))
        us, vs = steps(hu), steps(hv)
        first = len(m.vertices)
        for b in vs:
            for a in us:
                q = [n[i] * half[i] + u[i] * a + v[i] * b for i in range(3)]
                flat = (centre[0] + q[0], centre[1] + q[1], centre[2] + q[2])
                m.add(shape(q), n, (dot(flat, u) / tile, -dot(flat, v) / tile))
        stride = len(us)
        for j in range(len(vs) - 1):
            for i in range(stride - 1):
                a = first + j * stride + i
                m.quad(a, a + 1, a + stride + 1, a + stride)
    return smooth_normals(m)


def log(length: float, radius: float, seed: int, tile: float = 0.6, segments: int = 14, rings: int = 6) -> MeshBuilder:
    """Slightly irregular log lying along X, centred on the origin, with flat end caps."""
    rng = random.Random(seed)
    bumps = [1.0 + rng.uniform(-0.08, 0.08) for _ in range(segments)]
    m = MeshBuilder()
    circumference = 2 * math.pi * radius
    for r in range(rings + 1):
        x = -length / 2 + length * r / rings
        taper = 1.0 - 0.06 * abs(r / rings - 0.5)
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            rad = radius * taper * bumps[s % segments]
            n = (0.0, math.cos(a), math.sin(a))
            m.add((x, rad * n[1], rad * n[2]), n, (circumference * s / segments / tile, -x / tile))
    stride = segments + 1
    for r in range(rings):
        for s in range(segments):
            a = r * stride + s
            m.quad(a, a + 1, a + stride + 1, a + stride)
    for x, n in ((length / 2, (1.0, 0.0, 0.0)), (-length / 2, (-1.0, 0.0, 0.0))):
        taper = 0.97
        centre = m.add((x, 0, 0), n, (0.5, 0.5))
        ring = []
        for s in range(segments):
            a = 2 * math.pi * s / segments
            rad = radius * taper * bumps[s]
            ring.append(m.add((x, rad * math.cos(a), rad * math.sin(a)), n,
                              (0.5 + 0.5 * math.cos(a) * radius / tile, 0.5 + 0.5 * math.sin(a) * radius / tile)))
        for s in range(segments):
            a, b = ring[s], ring[(s + 1) % segments]
            m.indices += [centre, a, b] if n[0] > 0 else [centre, b, a]
    return m


def candle(radius: float = 0.022, height: float = 0.16, segments: int = 20) -> MeshBuilder:
    """Wax candle standing on the origin: a slightly melted top rim and a short dark wick."""
    m = MeshBuilder()
    rings = [(0.0, radius), (height - 0.01, radius), (height, radius * 0.85), (height - 0.004, radius * 0.6)]
    for y, r in rings:
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            lift = 0.0 if y < height - 0.01 else 0.7
            m.add((r * math.cos(a), y, -r * math.sin(a)), (math.cos(a), lift, -math.sin(a)), (s / segments, y / height))
    stride = segments + 1
    for k in range(len(rings) - 1):
        for s in range(segments):
            a = k * stride + s
            m.quad(a, a + 1, a + stride + 1, a + stride)
    top = m.add((0.0, height - 0.004, 0.0), (0.0, 1.0, 0.0), (0.5, 1.0))
    base = (len(rings) - 1) * stride
    for s in range(segments):
        m.indices += [top, base + s, base + s + 1]
    m.extend(box((0.0, height + 0.006, 0.0), (0.003, 0.014, 0.003)))  # wick
    return m


def transformed(mesh: MeshBuilder, yaw: float = 0.0, roll: float = 0.0, move: Vec3 = (0.0, 0.0, 0.0)) -> MeshBuilder:
    """The mesh rolled about X, turned about Y, then moved."""

    def rotate(p):
        y, z = p[1] * math.cos(roll) - p[2] * math.sin(roll), p[1] * math.sin(roll) + p[2] * math.cos(roll)
        x, z = p[0] * math.cos(yaw) + z * math.sin(yaw), -p[0] * math.sin(yaw) + z * math.cos(yaw)
        return (x, y, z)

    m = MeshBuilder()
    for p, n, uv in mesh.vertices:
        q = rotate(p)
        m.vertices.append(((q[0] + move[0], q[1] + move[1], q[2] + move[2]), rotate(n), uv))
    m.indices = list(mesh.indices)
    return m


def ember_bed(radius: float = 0.34, depth: float = 0.24, height: float = 0.05, grid: int = 24) -> MeshBuilder:
    """Low bumpy mound of embers and ash, an ellipse of ``radius`` by ``depth`` metres resting on y = 0."""
    rng = random.Random(11)
    noise = [[rng.uniform(-1, 1) for _ in range(grid + 1)] for _ in range(grid + 1)]
    m = MeshBuilder()
    heights = []
    for j in range(grid + 1):
        row = []
        for i in range(grid + 1):
            u, v = 2 * i / grid - 1, 2 * j / grid - 1
            d = min(1.0, math.sqrt(u * u + v * v))
            row.append(max(0.0, height * (1 - d * d) * (1 + 0.35 * noise[j][i])) if d < 1 else 0.0)
        heights.append(row)
    for j in range(grid + 1):
        for i in range(grid + 1):
            u, v = 2 * i / grid - 1, 2 * j / grid - 1
            dx = (heights[j][min(grid, i + 1)] - heights[j][max(0, i - 1)]) / (4 * radius / grid)
            dz = (heights[min(grid, j + 1)][i] - heights[max(0, j - 1)][i]) / (4 * depth / grid)
            m.add((u * radius, heights[j][i], v * depth), normalize((-dx, 1.0, -dz)), (i / grid, j / grid))
    stride = grid + 1
    for j in range(grid):
        for i in range(grid):
            a = j * stride + i
            m.quad(a, a + stride, a + stride + 1, a + 1)
    return m


def flame_cards(width: float = 0.42, height: float = 0.55, count: int = 3) -> MeshBuilder:
    """Vertical flame cards crossing at the origin, standing on y = 0 (the material draws both faces)."""
    m = MeshBuilder()
    for k in range(count):
        a = math.pi * k / count
        u = (math.cos(a) * width / 2, 0.0, -math.sin(a) * width / 2)
        n = (math.sin(a), 0.0, math.cos(a))
        mirror = k % 2 == 1
        u0, u1 = (1.0, 0.0) if mirror else (0.0, 1.0)
        m.quad(m.add((-u[0], 0, -u[2]), n, (u0, 1)), m.add((u[0], 0, u[2]), n, (u1, 1)),
               m.add((u[0], height, u[2]), n, (u1, 0)), m.add((-u[0], height, -u[2]), n, (u0, 0)))
    return m


def flame_texture(size: tuple[int, int] = (128, 256)) -> Image.Image:
    """Flame cluster: three tongues, white-yellow core to deep orange edges, alpha fading at the borders."""
    w, h = size
    rng = random.Random(5)
    img = Image.new("RGBA", size)
    px = img.load()
    tongues = [(0.5, 0.95, 0.30), (0.32, 0.65, 0.20), (0.68, 0.75, 0.22)]  # centre, height, half width
    wobble = [rng.uniform(0, math.tau) for _ in tongues]
    for y in range(h):
        t = 1.0 - y / (h - 1)  # 0 at the bottom, 1 at the top
        for x in range(w):
            s = x / (w - 1)
            density = 0.0
            for (cx, top, half), phase in zip(tongues, wobble, strict=True):
                if t > top:
                    continue
                k = t / top
                bend = 0.05 * math.sin(phase + 7 * k) * k
                width = half * (1 - k) ** 0.7 * (0.6 + 0.4 * math.sin(math.pi * min(1.0, k * 2.2 + 0.3)))
                if width <= 0:
                    continue
                d = abs(s - cx - bend) / width
                density = max(density, max(0.0, 1 - d * d) * (1 - k ** 3))
            if density <= 0:
                continue
            core = density ** 1.6
            r, g, b = 255, int(90 + 150 * core), int(20 + 150 * core ** 2.5)
            px[x, y] = (r, g, b, int(255 * min(1.0, density * 1.4)))
    return img.filter(ImageFilter.GaussianBlur(1.2))


def ember_texture(size: int = 256) -> Image.Image:
    """Glowing embers: orange cells cracked by dark ash, brighter towards the centre of the bed."""
    rng = random.Random(9)
    cells = [(rng.uniform(0, size), rng.uniform(0, size), rng.uniform(0.4, 1.0)) for _ in range(140)]
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            best, second, glow = 1e9, 1e9, 0.0
            for cx, cy, g in cells:
                d = (x - cx) ** 2 + (y - cy) ** 2
                if d < best:
                    best, second, glow = d, best, g
                elif d < second:
                    second = d
            edge = math.sqrt(second) - math.sqrt(best)
            radial = 1.0 - min(1.0, math.hypot(x - size / 2, y - size / 2) / (size / 2))
            heat = glow * min(1.0, edge / 6.0) * (0.25 + 0.75 * radial)
            px[x, y] = (int(255 * min(1.0, heat * 1.2)), int(255 * heat ** 2 * 0.55), int(255 * heat ** 4 * 0.15))
    return img.filter(ImageFilter.GaussianBlur(0.8))


def write_glb(path: Path, mesh: MeshBuilder, name: str, color=(1.0, 1.0, 1.0, 1.0)) -> None:
    """Write one mesh as a binary glTF 2.0 file of one primitive with a plain material."""
    count = len(mesh.vertices)
    chunks = [b"".join(struct.pack("<3f", *v[0]) for v in mesh.vertices),
              b"".join(struct.pack("<3f", *normalize(v[1])) for v in mesh.vertices),
              b"".join(struct.pack("<2f", *v[2]) for v in mesh.vertices),
              struct.pack(f"<{len(mesh.indices)}I", *mesh.indices)]
    lo = [min(v[0][k] for v in mesh.vertices) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.vertices) for k in range(3)]
    views, blob = [], b""
    for i, chunk in enumerate(chunks):
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(chunk),
                      "target": 34963 if i == 3 else 34962})
        blob += chunk + b"\0" * (-len(chunk) % 4)
    doc = {
        "asset": {"version": "2.0", "generator": "Owl tools/demo/generate_meshes.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                                  "indices": 3, "material": 0}]}],
        "materials": [{"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": 0.0,
                                                               "roughnessFactor": 0.8}}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": len(mesh.indices), "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(blob), 0x004E4942) + blob
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def hearth() -> MeshBuilder:
    """Stone hearth against a wall: origin at the floor in the middle of its foot, the wall face at z = 0, +Z out.

    Hearthstone 1.8 x 1.1 m, a firebox opening of 1.1 x 0.95 m, the jambs and back below the mantel (1.05 to
    1.25 m, a separate beam), the chimney breast from the mantel up to the 3.2 m ceiling. Every block has rounded,
    uneven edges and faces; the jambs carry a few protruding stones. The blocks overlap a little where they meet so
    the displaced faces leave no gap."""
    stone = {"tile": 1.0, "cell": 0.06, "radius": 0.03, "amp": 0.022, "freq": 5.0}
    m = MeshBuilder()
    m.extend(shaped_box((0.0, 0.05, 0.55), (1.8, 0.1, 1.1), seed=1, **{**stone, "cell": 0.08, "radius": 0.025, "amp": 0.01}))
    for index, side in enumerate((-1, 1)):
        m.extend(shaped_box((side * 0.725, 0.575, 0.3), (0.35, 1.03, 0.6), seed=2 + index, **stone))
    m.extend(shaped_box((0.0, 0.575, 0.05), (1.2, 0.95, 0.1), seed=4, **stone))
    m.extend(shaped_box((0.0, 2.2, 0.25), (1.8, 2.0, 0.5), seed=5, **{**stone, "cell": 0.09}))
    rng = random.Random(17)
    for side in (-1, 1):
        for _ in range(12):
            # Stones standing out of the front and inner faces of the jambs.
            size = (rng.uniform(0.12, 0.24), rng.uniform(0.07, 0.12), rng.uniform(0.07, 0.1))
            y = rng.uniform(0.2, 0.98)
            if rng.random() < 0.6:
                centre = (side * 0.725 + rng.uniform(-0.1, 0.1), y, 0.6 + rng.uniform(0.0, 0.02))
            else:
                size = (size[2], size[1], size[0])
                centre = (side * (0.55 - rng.uniform(0.0, 0.02)), y, rng.uniform(0.2, 0.5))
            m.extend(shaped_box(centre, size, tile=1.0, cell=0.045, radius=0.025, amp=0.012, freq=10.0,
                                seed=rng.randrange(1 << 16)))
    return m


def mantel_beam() -> MeshBuilder:
    """Rough-hewn mantel beam, 2.0 x 0.2 x 0.9 m, resting on y = 0 against the wall (z = 0 to 0.9): chamfered edges,
    a slight sag along its length and an uneven surface."""

    def sag(q):
        return (0.0, -0.015 * math.cos(math.pi * q[0] / 2.0) + 0.006 * math.sin(3.1 * q[0]),
                0.012 * math.sin(2.3 * q[0] + 0.7))

    return shaped_box((0.0, 0.1, 0.45), (2.0, 0.2, 0.9), tile=1.0, cell=0.05, radius=0.03, amp=0.007, freq=7.0,
                      seed=31, warp=sag)


def post() -> MeshBuilder:
    """Corner post, 0.25 x 3.2 x 0.25 m standing on the origin: chamfered, slightly twisted and bowed."""
    return shaped_box((0.0, 1.6, 0.0), (0.25, 3.2, 0.25), tile=1.0, cell=0.06, radius=0.025, amp=0.004, freq=8.0,
                      seed=41, warp=lambda q: (0.01 * math.sin(math.pi * q[1] / 3.2), 0.0,
                                               0.006 * math.sin(math.pi * q[1] / 1.6 + 0.4)))


def fire_grate() -> MeshBuilder:
    """Wrought-iron grate on short legs: two side rails and four bars at 0.1 m, centred on the origin."""
    m = MeshBuilder()
    for side in (-1, 1):
        m.extend(box((side * 0.26, 0.06, 0.0), (0.035, 0.12, 0.42)))
        m.extend(box((side * 0.26, 0.16, -0.17), (0.035, 0.12, 0.035)))  # fire dog uprights
    for k in range(4):
        m.extend(box((0.0, 0.105, -0.15 + 0.1 * k), (0.56, 0.022, 0.022)))
    return m


def firewood() -> MeshBuilder:
    """Three logs burning on the grate: two side by side, one across them, resting on y = 0 (top of the bars)."""
    m = MeshBuilder()
    m.extend(transformed(log(0.5, 0.06, 1), yaw=0.15, move=(0.0, 0.06, 0.08)))
    m.extend(transformed(log(0.48, 0.055, 2), yaw=-0.1, move=(0.02, 0.055, -0.07)))
    m.extend(transformed(log(0.46, 0.05, 3), yaw=1.2, roll=0.3, move=(-0.04, 0.15, 0.0)))
    return m


def log_stack() -> MeshBuilder:
    """Spare firewood stacked three, two, one against a wall, logs along Z, resting on y = 0."""
    m = MeshBuilder()
    radius = 0.065
    rows = [3, 2, 1]
    seed = 20
    for row, count in enumerate(rows):
        for k in range(count):
            x = (k - (count - 1) / 2) * 2 * radius * 1.02
            y = radius + row * radius * 1.75
            seed += 1
            m.extend(transformed(log(0.45, radius * random.Random(seed).uniform(0.88, 1.0), seed),
                                 yaw=math.pi / 2 + random.Random(seed).uniform(-0.05, 0.05), move=(x, y, 0.0)))
    return m


def upright(mesh: MeshBuilder) -> MeshBuilder:
    """The mesh turned a quarter about Z: what lay along X stands along Y."""
    m = MeshBuilder()
    for p, n, uv in mesh.vertices:
        m.vertices.append(((-p[1], p[0], p[2]), (-n[1], n[0], n[2]), uv))
    m.indices = list(mesh.indices)
    return m


def trapdoor() -> MeshBuilder:
    """Cellar trapdoor of four planks, 1 x 1 m, lying on the floor (y = 0 to 0.05)."""
    m = MeshBuilder()
    for k in range(4):
        m.extend(box((-0.375 + 0.25 * k, 0.025, 0.0), (0.24, 0.05, 1.0), tile=0.8))
    return m


def trapdoor_iron() -> MeshBuilder:
    """Iron straps and the ring handle of the trapdoor."""
    m = MeshBuilder()
    for z in (-0.32, 0.32):
        m.extend(box((0.0, 0.055, z), (1.0, 0.012, 0.07)))
    for k in range(8):
        a = 2 * math.pi * k / 8
        m.extend(box((0.3 + 0.06 * math.cos(a), 0.065, 0.06 * math.sin(a)), (0.03, 0.014, 0.03)))
    return m


def key() -> MeshBuilder:
    """Old iron key lying flat on y = 0: a ring bow, a shaft along X and two bit teeth, 0.16 m long."""
    m = MeshBuilder()
    for k in range(10):
        a = 2 * math.pi * k / 10
        m.extend(box((-0.05 + 0.025 * math.cos(a), 0.006, 0.025 * math.sin(a)), (0.018, 0.012, 0.018)))
    m.extend(box((0.03, 0.006, 0.0), (0.13, 0.01, 0.012)))
    m.extend(box((0.08, 0.006, 0.018), (0.012, 0.01, 0.026)))
    m.extend(box((0.06, 0.006, 0.016), (0.01, 0.01, 0.022)))
    return m


def chest() -> MeshBuilder:
    """Wooden treasure chest, 0.8 x 0.6 x 0.5 m standing on the origin: a box body and a rounded lid."""
    m = box((0.0, 0.22, 0.0), (0.8, 0.44, 0.5), tile=0.6)
    lid = transformed(log(0.8, 0.25, 7, segments=16, rings=2), move=(0.0, 0.44, 0.0))
    m.extend(MeshBuilder().extend(lid))
    return m


def chest_iron() -> MeshBuilder:
    """Iron bands and the lock plate of the chest."""
    m = MeshBuilder()
    for x in (-0.3, 0.3):
        m.extend(box((x, 0.22, 0.0), (0.05, 0.452, 0.512)))
        m.extend(transformed(log(0.052, 0.258, 9, segments=16, rings=1), move=(x, 0.44, 0.0)))
    m.extend(box((0.0, 0.4, 0.255), (0.1, 0.12, 0.02)))
    return m


def barrel(seed: int) -> MeshBuilder:
    """Standing barrel, 0.9 m tall and 0.3 m in radius, on y = 0."""
    return transformed(upright(log(0.9, 0.3, seed, tile=0.5, segments=18, rings=4)), move=(0.0, 0.45, 0.0))


def ladder(height: float = 3.9) -> MeshBuilder:
    """Wooden ladder standing on the origin: two rails along Y and a rung every 0.3 m."""
    m = MeshBuilder()
    for x in (-0.22, 0.22):
        m.extend(box((x, height / 2, 0.0), (0.06, height, 0.06), tile=0.6))
    rung = 0.3
    while rung < height - 0.1:
        m.extend(box((0.0, rung, 0.0), (0.44, 0.04, 0.04), tile=0.6))
        rung += 0.3
    return m


def main() -> None:
    """Write every procedural mesh and texture of the Engine Demo tavern."""
    OUT.mkdir(parents=True, exist_ok=True)
    TEXTURES.mkdir(parents=True, exist_ok=True)
    write_glb(OUT / "floor.glb", box((0.0, -0.05, 0.0), (9.0, 0.1, 9.0), tile=2.0), "Floor")
    write_glb(OUT / "wall.glb", box((0.0, 1.6, 0.0), (9.0, 3.2, 0.4), tile=2.0), "Wall")
    write_glb(OUT / "post.glb", post(), "Post")
    write_glb(OUT / "hearth.glb", hearth(), "Hearth")
    write_glb(OUT / "mantel_beam.glb", mantel_beam(), "MantelBeam")
    write_glb(OUT / "fire_grate.glb", fire_grate(), "FireGrate", color=(0.1, 0.1, 0.1, 1.0))
    write_glb(OUT / "firewood.glb", firewood(), "Firewood")
    write_glb(OUT / "log_stack.glb", log_stack(), "LogStack")
    write_glb(OUT / "embers.glb", ember_bed(), "Embers", color=(0.05, 0.04, 0.04, 1.0))
    write_glb(OUT / "flames.glb", flame_cards(), "Flames")
    write_glb(OUT / "candle.glb", candle(), "Candle", color=(0.93, 0.88, 0.78, 1.0))
    write_glb(OUT / "candle_flame.glb", flame_cards(0.024, 0.05, 2), "CandleFlame")
    write_glb(OUT / "trapdoor.glb", trapdoor(), "Trapdoor")
    write_glb(OUT / "trapdoor_iron.glb", trapdoor_iron(), "TrapdoorIron", color=(0.1, 0.1, 0.1, 1.0))
    write_glb(OUT / "key.glb", key(), "Key")
    write_glb(OUT / "chest.glb", chest(), "Chest")
    write_glb(OUT / "chest_iron.glb", chest_iron(), "ChestIron", color=(0.1, 0.1, 0.1, 1.0))
    write_glb(OUT / "barrel.glb", barrel(31), "Barrel")
    write_glb(OUT / "ladder.glb", ladder(), "Ladder")
    flame_texture().save(TEXTURES / "flame.png")
    ember_texture().save(TEXTURES / "embers.png")
    print(f"Meshes written to {OUT}, textures to {TEXTURES}")


if __name__ == "__main__":
    main()
