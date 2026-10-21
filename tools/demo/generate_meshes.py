#!/usr/bin/env python3
"""Generate the procedural meshes of the Engine Demo (``engine_demo/meshes/*.glb``): binary glTF files with normals,
UVs and, for the textured ones, an embedded PNG base-colour texture."""

import io
import json
import math
import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "meshes"

Vertex = tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float]]


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


def normalize(v):
    """Unit vector of v."""
    length = math.sqrt(sum(x * x for x in v)) or 1.0
    return tuple(x / length for x in v)


def cross(a, b):
    """Cross product a x b."""
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def box(half=(0.5, 0.5, 0.5), uv_scale=(1.0, 1.0)) -> MeshBuilder:
    """Axis-aligned box centred on the origin, one UV square per face."""
    m = MeshBuilder()
    hx, hy, hz = half
    faces = [  # normal, u axis, v axis
        ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
        ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
        ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ]
    for n, u, v in faces:
        centre = (n[0] * hx, n[1] * hy, n[2] * hz)
        hu = abs(u[0]) * hx + abs(u[1]) * hy + abs(u[2]) * hz
        hv = abs(v[0]) * hx + abs(v[1]) * hy + abs(v[2]) * hz
        corners = []
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = tuple(centre[i] + u[i] * su * hu + v[i] * sv * hv for i in range(3))
            uv = ((su + 1) / 2 * uv_scale[0], (1 - (sv + 1) / 2) * uv_scale[1])
            corners.append(m.add(p, n, uv))
        m.quad(*corners)
    return m


def pyramid(base=0.6, height=1.2) -> MeshBuilder:
    """Square pyramid standing on the origin, flat-shaded."""
    m = MeshBuilder()
    apex = (0.0, height, 0.0)
    ring = [(-base, 0, base), (base, 0, base), (base, 0, -base), (-base, 0, -base)]
    for i in range(4):
        a, b = ring[i], ring[(i + 1) % 4]
        n = normalize(cross(tuple(b[k] - a[k] for k in range(3)), tuple(apex[k] - a[k] for k in range(3))))
        m.indices += [m.add(a, n, (0, 1)), m.add(b, n, (1, 1)), m.add(apex, n, (0.5, 0))]
    down = [m.add(p, (0, -1, 0), ((p[0] / base + 1) / 2, (p[2] / base + 1) / 2)) for p in ring]
    m.quad(down[3], down[2], down[1], down[0])
    return m


def prism(sides=8, radius=0.35, height=2.0) -> MeshBuilder:
    """Regular prism (a pillar) standing on the origin, flat-shaded sides and caps."""
    m = MeshBuilder()
    for i in range(sides):
        a0, a1 = 2 * math.pi * i / sides, 2 * math.pi * (i + 1) / sides
        p0 = (radius * math.cos(a0), 0, -radius * math.sin(a0))
        p1 = (radius * math.cos(a1), 0, -radius * math.sin(a1))
        mid = (a0 + a1) / 2
        n = (math.cos(mid), 0, -math.sin(mid))
        u0, u1 = i / sides, (i + 1) / sides
        m.quad(m.add(p0, n, (u0, 1)), m.add(p1, n, (u1, 1)), m.add((p1[0], height, p1[2]), n, (u1, 0)),
               m.add((p0[0], height, p0[2]), n, (u0, 0)))
    for y, n in ((height, (0, 1, 0)), (0.0, (0, -1, 0))):
        centre = m.add((0, y, 0), n, (0.5, 0.5))
        ring = [m.add((radius * math.cos(2 * math.pi * i / sides), y, -radius * math.sin(2 * math.pi * i / sides)),
                      n, (0.5 + 0.5 * math.cos(2 * math.pi * i / sides), 0.5 - 0.5 * math.sin(2 * math.pi * i / sides)))
                for i in range(sides)]
        for i in range(sides):
            a, b = ring[i], ring[(i + 1) % sides]
            m.indices += [centre, a, b] if n[1] > 0 else [centre, b, a]
    return m


def sphere(radius=0.5, rings=16, segments=32) -> MeshBuilder:
    """UV sphere centred on the origin, smooth-shaded."""
    m = MeshBuilder()
    for r in range(rings + 1):
        phi = math.pi * r / rings
        for s in range(segments + 1):
            theta = 2 * math.pi * s / segments
            n = (math.sin(phi) * math.cos(theta), math.cos(phi), -math.sin(phi) * math.sin(theta))
            m.add(tuple(radius * x for x in n), n, (s / segments, r / rings))
    stride = segments + 1
    for r in range(rings):
        for s in range(segments):
            a = r * stride + s
            m.quad(a, a + stride, a + stride + 1, a + 1)
    return m


def torus(major=0.45, minor=0.15, rings=32, sides=16) -> MeshBuilder:
    """Torus lying in the XZ plane, smooth-shaded."""
    m = MeshBuilder()
    for r in range(rings + 1):
        u = 2 * math.pi * r / rings
        for s in range(sides + 1):
            v = 2 * math.pi * s / sides
            n = (math.cos(v) * math.cos(u), math.sin(v), -math.cos(v) * math.sin(u))
            centre = (major * math.cos(u), 0, -major * math.sin(u))
            m.add(tuple(centre[i] + minor * n[i] for i in range(3)), n, (r / rings, s / sides))
    stride = sides + 1
    for r in range(rings):
        for s in range(sides):
            a = r * stride + s
            m.quad(a, a + stride, a + stride + 1, a + 1)
    return m


def crate_texture() -> bytes:
    """Wooden crate face: planks, a frame and a cross brace."""
    rng = random.Random(7)
    img = Image.new("RGB", (128, 128), (150, 104, 60))
    draw = ImageDraw.Draw(img)
    for y in range(0, 128, 16):
        shade = rng.randint(-14, 14)
        draw.rectangle((0, y, 127, y + 15), fill=(150 + shade, 104 + shade, 60 + shade // 2))
        draw.line((0, y, 127, y), fill=(96, 64, 34))
    for _ in range(90):
        x, y = rng.randint(0, 127), rng.randint(0, 127)
        draw.line((x, y, min(127, x + rng.randint(4, 14)), y), fill=(128, 86, 48))
    draw.rectangle((0, 0, 127, 127), outline=(84, 54, 28), width=12)
    draw.line((10, 10, 117, 117), fill=(110, 72, 38), width=12)
    draw.rectangle((0, 0, 127, 127), outline=(60, 38, 20), width=2)
    return png_bytes(img)


def checker_texture() -> bytes:
    """Stone floor: soft two-tone checker with joints."""
    img = Image.new("RGB", (128, 128))
    draw = ImageDraw.Draw(img)
    for j in range(4):
        for i in range(4):
            tone = 120 if (i + j) % 2 == 0 else 98
            draw.rectangle((i * 32, j * 32, i * 32 + 31, j * 32 + 31), fill=(tone, tone + 4, tone + 10),
                           outline=(70, 72, 78))
    return png_bytes(img)


def png_bytes(img: Image.Image) -> bytes:
    """PNG encoding of an image."""
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    return buffer.getvalue()


def write_glb(path: Path, mesh: MeshBuilder, name: str, color=(1.0, 1.0, 1.0, 1.0), texture: bytes | None = None):
    """Write one mesh as a binary glTF 2.0 file, its texture embedded."""
    positions = b"".join(struct.pack("<3f", *v[0]) for v in mesh.vertices)
    normals = b"".join(struct.pack("<3f", *v[1]) for v in mesh.vertices)
    uvs = b"".join(struct.pack("<2f", *v[2]) for v in mesh.vertices)
    indices = struct.pack(f"<{len(mesh.indices)}I", *mesh.indices)
    chunks = [positions, normals, uvs, indices] + ([texture] if texture else [])
    views, blob = [], b""
    for i, chunk in enumerate(chunks):
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(chunk)}
        if i < 3:
            view["target"] = 34962
        elif i == 3:
            view["target"] = 34963
        views.append(view)
        blob += chunk + b"\0" * (-len(chunk) % 4)
    count = len(mesh.vertices)
    lo = [min(v[0][k] for v in mesh.vertices) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.vertices) for k in range(3)]
    material = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": 0.0,
                                                       "roughnessFactor": 0.8}}
    doc = {
        "asset": {"version": "2.0", "generator": "Owl tools/demo/generate_meshes.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                                  "indices": 3, "material": 0}]}],
        "materials": [material],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": len(mesh.indices), "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    if texture:
        doc["images"] = [{"bufferView": 4, "mimeType": "image/png"}]
        doc["samplers"] = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]
        doc["textures"] = [{"source": 0, "sampler": 0}]
        material["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(blob), 0x004E4942) + blob
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def main() -> None:
    """Write every mesh of the Engine Demo."""
    OUT.mkdir(parents=True, exist_ok=True)
    write_glb(OUT / "crate.glb", box(), "Crate", texture=crate_texture())
    write_glb(OUT / "floor.glb", box((4.0, 0.05, 4.0), (4.0, 4.0)), "Floor", texture=checker_texture())
    write_glb(OUT / "pillar.glb", prism(), "Pillar", color=(0.82, 0.80, 0.74, 1.0))
    write_glb(OUT / "pyramid.glb", pyramid(), "Pyramid", color=(0.90, 0.68, 0.25, 1.0))
    write_glb(OUT / "sphere.glb", sphere(), "Sphere", color=(0.30, 0.55, 0.90, 1.0))
    write_glb(OUT / "torus.glb", torus(), "Torus", color=(0.85, 0.30, 0.35, 1.0))
    print(f"Meshes written to {OUT}")


if __name__ == "__main__":
    main()
