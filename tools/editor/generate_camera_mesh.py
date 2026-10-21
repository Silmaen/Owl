#!/usr/bin/env python3
"""Generate the Owl Nest camera gizmo mesh (``source/owlnest/assets/meshes/camera.glb``): a grey body with rounded
edges, two yellow film reels on top and a yellow tapered lens, Y up, the lens pointing toward -Z. The colours come from
one embedded 8x4 base-colour texture (grey left half, yellow right half)."""

import io
import json
import math
import struct
from pathlib import Path

from PIL import Image

OUT = Path(__file__).resolve().parents[2] / "source" / "owlnest" / "assets" / "meshes"

Vec = tuple[float, float, float]
GREY_UV = (0.25, 0.5)
YELLOW_UV = (0.75, 0.5)
RADIUS = 0.025
ARC_SEGMENTS = 4


class MeshBuilder:
    """Accumulates vertices and triangles (counter-clockwise front faces)."""

    def __init__(self) -> None:
        self.vertices: list[tuple[Vec, Vec, tuple[float, float]]] = []
        self.indices: list[int] = []
        self.parts: list[tuple[str, Vec, int, int]] = []  # name, centre, first index, index count
        self._welded: dict[tuple[int, ...], int] = {}

    def add(self, position: Vec, normal: Vec, uv: tuple[float, float], weld: bool = False) -> int:
        """Add one vertex (or reuse an identical one when welding) and return its index."""
        key = tuple(round(x * 1e5) for x in (*position, *normal))
        if weld and key in self._welded:
            return self._welded[key]
        self.vertices.append((position, normal, uv))
        self._welded[key] = len(self.vertices) - 1
        return len(self.vertices) - 1

    def quad(self, a: int, b: int, c: int, d: int) -> None:
        """Two triangles of the quad a-b-c-d, given counter-clockwise."""
        self.indices += [a, b, c, a, c, d]

    def begin(self, name: str, centre: Vec) -> None:
        """Start a named part whose triangles are checked against its centre."""
        self.parts.append((name, centre, len(self.indices), 0))

    def end(self) -> None:
        """Close the current part."""
        name, centre, first, _ = self.parts[-1]
        self.parts[-1] = (name, centre, first, len(self.indices) - first)

    def check_winding(self) -> None:
        """Assert every triangle faces outward and agrees with its vertex normals."""
        for name, centre, first, count in self.parts:
            for t in range(first, first + count, 3):
                a, b, c = (self.vertices[self.indices[t + k]] for k in range(3))
                n = cross(sub(b[0], a[0]), sub(c[0], a[0]))
                assert dot(n, n) > 1e-14, f"{name}: degenerate triangle at {t}"
                mid = (
                    sum(v[0][0] for v in (a, b, c)) / 3,
                    sum(v[0][1] for v in (a, b, c)) / 3,
                    sum(v[0][2] for v in (a, b, c)) / 3,
                )
                assert dot(n, sub(mid, centre)) > 0, f"{name}: triangle {t} wound inward"
                for v in (a, b, c):
                    assert dot(n, v[1]) > 0, f"{name}: triangle {t} disagrees with its normal"


def sub(a: Vec, b: Vec) -> Vec:
    """Vector a - b."""
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a: Vec, b: Vec) -> float:
    """Dot product."""
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec, b: Vec) -> Vec:
    """Cross product a x b."""
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normalize(v: Vec) -> Vec:
    """Unit vector of v."""
    length = math.sqrt(dot(v, v)) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


def rounded_box(m: MeshBuilder, half: Vec, radius: float, segments: int) -> None:
    """Closed box with rounded edges and corners: an inner box offset by radius along smooth directions."""
    m.begin("body", (0.0, 0.0, 0.0))
    inner = tuple(h - radius for h in half)
    # Profile along one face axis: (side, tangent of the direction angle) from -edge to +edge.
    tans = [math.tan(math.radians(45.0 * k / segments)) for k in range(segments + 1)]
    profile = [(-1, t) for t in reversed(tans)] + [(1, t) for t in tans]
    for axis in range(3):
        u_axis, v_axis = (axis + 1) % 3, (axis + 2) % 3  # cross(u, v) = +axis
        for sign in (1, -1):
            grid = []
            for su, tu in profile:
                row = []
                for sv, tv in profile:
                    d = [0.0, 0.0, 0.0]
                    d[axis], d[u_axis], d[v_axis] = float(sign), su * tu, sv * tv
                    n = normalize((d[0], d[1], d[2]))
                    p = [0.0, 0.0, 0.0]
                    p[axis], p[u_axis], p[v_axis] = sign * inner[axis], su * inner[u_axis], sv * inner[v_axis]
                    pos = (p[0] + radius * n[0], p[1] + radius * n[1], p[2] + radius * n[2])
                    row.append(m.add(pos, n, GREY_UV, weld=True))
                grid.append(row)
            last = len(profile) - 1
            for i in range(last):
                for j in range(last):
                    q0, q1, q2, q3 = grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]
                    if sign > 0:
                        m.quad(q0, q1, q2, q3)
                    else:
                        m.quad(q0, q3, q2, q1)
    m.end()


def frustum(
    m: MeshBuilder,
    name: str,
    basis: tuple[Vec, Vec, Vec],
    centre: Vec,
    length: float,
    r0: float,
    r1: float,
    sides: int = 24,
) -> None:
    """Closed truncated cone, smooth sides and flat caps; local z runs along basis[2] from r0 to r1."""
    e1, e2, e3 = basis
    m.begin(name, centre)

    def place(x: float, y: float, z: float) -> Vec:
        """Local coordinates to model space."""
        return tuple(centre[k] + e1[k] * x + e2[k] * y + e3[k] * z for k in range(3))  # type: ignore[return-value]

    def orient(x: float, y: float, z: float) -> Vec:
        """Local direction to model space."""
        return tuple(e1[k] * x + e2[k] * y + e3[k] * z for k in range(3))  # type: ignore[return-value]

    slope = (r0 - r1) / length
    w0, w1 = -length / 2, length / 2
    angles = [2 * math.pi * i / sides for i in range(sides + 1)]
    for a in angles:
        n = normalize(orient(math.cos(a), math.sin(a), slope))
        m.add(place(r0 * math.cos(a), r0 * math.sin(a), w0), n, YELLOW_UV)
        m.add(place(r1 * math.cos(a), r1 * math.sin(a), w1), n, YELLOW_UV)
    base = len(m.vertices) - 2 * (sides + 1)
    for i in range(sides):
        b = base + 2 * i
        m.quad(b, b + 2, b + 3, b + 1)
    for w, r, sign in ((w1, r1, 1.0), (w0, r0, -1.0)):
        n = orient(0, 0, sign)
        hub = m.add(place(0, 0, w), n, YELLOW_UV)
        ring = [m.add(place(r * math.cos(a), r * math.sin(a), w), n, YELLOW_UV) for a in angles]
        for i in range(sides):
            if sign > 0:
                m.indices += [hub, ring[i], ring[i + 1]]
            else:
                m.indices += [hub, ring[i + 1], ring[i]]
    m.end()


def camera() -> MeshBuilder:
    """The camera model."""
    m = MeshBuilder()
    rounded_box(m, (0.11, 0.09, 0.20), RADIUS, ARC_SEGMENTS)
    reel_basis = ((0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 0.0, 0.0))  # axis along +X
    for name, z in (("reel_back", 0.10), ("reel_front", -0.10)):
        frustum(m, name, reel_basis, (0.0, 0.09 + 0.085, z), 0.05, 0.085, 0.085)
    lens_basis = ((0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, -1.0))  # axis along -Z
    frustum(m, "lens", lens_basis, (0.0, 0.0, -0.20 - 0.06), 0.12, 0.06, 0.085)
    m.check_winding()
    return m


def texture() -> bytes:
    """8x4 base-colour image: grey left half, yellow right half."""
    img = Image.new("RGB", (8, 4), (140, 142, 150))
    img.paste((255, 199, 41), (4, 0, 8, 4))
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    return buffer.getvalue()


def write_glb(path: Path, mesh: MeshBuilder, name: str, tex: bytes) -> None:
    """Write one mesh as a binary glTF 2.0 file, its texture embedded."""
    positions = b"".join(struct.pack("<3f", *v[0]) for v in mesh.vertices)
    normals = b"".join(struct.pack("<3f", *v[1]) for v in mesh.vertices)
    uvs = b"".join(struct.pack("<2f", *v[2]) for v in mesh.vertices)
    indices = struct.pack(f"<{len(mesh.indices)}I", *mesh.indices)
    chunks = [positions, normals, uvs, indices, tex]
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
    material = {
        "name": name,
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
            "baseColorTexture": {"index": 0},
            "metallicFactor": 0.0,
            "roughnessFactor": 0.8,
        },
    }
    doc = {
        "asset": {"version": "2.0", "generator": "Owl tools/editor/generate_camera_mesh.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [
            {
                "name": name,
                "primitives": [
                    {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}
                ],
            }
        ],
        "materials": [material],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
        "textures": [{"source": 0, "sampler": 0}],
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


def main() -> None:
    """Write the camera mesh."""
    OUT.mkdir(parents=True, exist_ok=True)
    mesh = camera()
    write_glb(OUT / "camera.glb", mesh, "Camera", texture())
    print(f"Camera mesh written to {OUT}: {len(mesh.vertices)} vertices, {len(mesh.indices) // 3} triangles")


if __name__ == "__main__":
    main()
