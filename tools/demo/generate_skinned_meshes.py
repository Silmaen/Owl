#!/usr/bin/env python3
"""Generate the skinned mesh fixture of the mesh tests.

``test/unit/test_helper/assets/mesh_tests/skinned_arm.gltf``: a two-bone arm with one clip and one socket, the fixture
of the skinning tests. The characters of the Engine Demo tavern are Kenney's Mini Characters (CC0)."""

import base64
import json
import math
import struct
from pathlib import Path

from generate_meshes import MeshBuilder, normalize

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "test" / "unit" / "test_helper" / "assets" / "mesh_tests" / "skinned_arm.gltf"


class SkinnedBuilder:
    """Vertices with their joints and weights (two joints at most per vertex here)."""

    def __init__(self) -> None:
        self.vertices = []
        self.indices = []

    def add_part(self, mesh: MeshBuilder, weights) -> None:
        """Append a mesh, ``weights(position)`` giving the ``[(joint, weight), ...]`` of each vertex."""
        base = len(self.vertices)
        for position, normal, uv in mesh.vertices:
            pairs = (weights(position) + [(0, 0.0)] * 4)[:4]
            self.vertices.append((position, normal, uv, [j for j, _ in pairs], [w for _, w in pairs]))
        self.indices += [base + i for i in mesh.indices]


def limb(top, size, rings: int = 4) -> MeshBuilder:
    """A box hanging from ``top`` (``size`` = width, length, depth), its sides cut in ``rings`` slices along -Y."""
    w, length, d = size
    x0, x1 = top[0] - w / 2, top[0] + w / 2
    z0, z1 = top[2] - d / 2, top[2] + d / 2
    m = MeshBuilder()
    sides = [((0, 0, 1), lambda t, s: (x0 + s * w, z1)), ((1, 0, 0), lambda t, s: (x1, z1 - s * d)),
             ((0, 0, -1), lambda t, s: (x1 - s * w, z0)), ((-1, 0, 0), lambda t, s: (x0, z0 + s * d))]
    for normal, at in sides:
        start = len(m.vertices)
        for ring in range(rings + 1):
            y = top[1] - length * ring / rings
            for s in (0.0, 1.0):
                x, z = at(ring, s)
                m.add((x, y, z), normal, (s, ring / rings))
        for ring in range(rings):
            a = start + ring * 2
            m.quad(a + 2, a + 3, a + 1, a)
    y0, y1 = top[1] - length, top[1]
    for y, normal in ((y1, (0, 1, 0)), (y0, (0, -1, 0))):
        a = m.add((x0, y, z0), normal, (0, 0))
        b = m.add((x1, y, z0), normal, (1, 0))
        c = m.add((x1, y, z1), normal, (1, 1))
        e = m.add((x0, y, z1), normal, (0, 1))
        if normal[1] > 0:
            m.quad(a, e, c, b)
        else:
            m.quad(a, b, c, e)
    return m


def quat_axis(axis, degrees: float):
    """Quaternion (x, y, z, w) of a rotation about an axis."""
    half = math.radians(degrees) / 2
    x, y, z = normalize(axis)
    return (x * math.sin(half), y * math.sin(half), z * math.sin(half), math.cos(half))


class Document:
    """A glTF document filled with buffer views and accessors over one binary blob."""

    def __init__(self) -> None:
        self.blob = b""
        self.views = []
        self.accessors = []

    def accessor(self, data: bytes, count: int, component: int, kind: str, target=None, bounds=None) -> int:
        """Add a view of ``data`` and an accessor over it; return the accessor index."""
        view = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.views.append(view)
        self.blob += data + b"\0" * (-len(data) % 4)
        accessor = {"bufferView": len(self.views) - 1, "componentType": component, "count": count, "type": kind}
        if bounds:
            accessor["min"], accessor["max"] = bounds
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def floats(self, values, kind: str, width: int, bounds: bool = False, target=None) -> int:
        """Accessor of float tuples (or scalars with ``width`` 1)."""
        flat = [v for value in values for v in (value if width > 1 else (value,))]
        extra = None
        if bounds:
            extra = ([min(v[k] if width > 1 else v for v in values) for k in range(width)],
                     [max(v[k] if width > 1 else v for v in values) for k in range(width)])
        return self.accessor(struct.pack(f"<{len(flat)}f", *flat), len(values), 5126, kind, target, extra)


def skinned_document(skinned: SkinnedBuilder, bones, clips, sockets, name: str, color) -> tuple[dict, bytes]:
    """glTF of a skinned mesh: ``bones`` = [(name, parent, local translation)], ``clips`` = {name: [(bone, path,
    times, values)]}, ``sockets`` = [(name, bone, translation, rotation)]."""
    doc = Document()
    vertices = skinned.vertices
    position = doc.floats([v[0] for v in vertices], "VEC3", 3, bounds=True, target=34962)
    normal = doc.floats([normalize(v[1]) for v in vertices], "VEC3", 3, target=34962)
    uv = doc.floats([v[2] for v in vertices], "VEC2", 2, target=34962)
    joints = doc.accessor(b"".join(struct.pack("<4H", *v[3]) for v in vertices), len(vertices), 5123, "VEC4", 34962)
    weights = doc.floats([v[4] for v in vertices], "VEC4", 4, target=34962)
    indices = doc.accessor(struct.pack(f"<{len(skinned.indices)}I", *skinned.indices), len(skinned.indices), 5125,
                           "SCALAR", 34963)
    world = []
    for _, parent, local in bones:
        origin = world[parent] if parent >= 0 else (0.0, 0.0, 0.0)
        world.append(tuple(origin[k] + local[k] for k in range(3)))
    inverse_binds = []
    for w in world:
        inverse_binds += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -w[0], -w[1], -w[2], 1]
    inverse_bind = doc.accessor(struct.pack(f"<{len(inverse_binds)}f", *inverse_binds), len(bones), 5126, "MAT4")
    nodes = [{"name": name, "mesh": 0, "skin": 0}]
    first_bone = len(nodes)
    for bone_name, _, local in bones:
        nodes.append({"name": bone_name, "translation": list(local)})
    for index, (_, parent, _) in enumerate(bones):
        if parent >= 0:
            nodes[first_bone + parent].setdefault("children", []).append(first_bone + index)
    for socket_name, bone, translation, rotation in sockets:
        nodes.append({"name": f"socket_{socket_name}", "translation": list(translation), "rotation": list(rotation)})
        nodes[first_bone + bone].setdefault("children", []).append(len(nodes) - 1)
    roots = [first_bone + i for i, (_, parent, _) in enumerate(bones) if parent < 0]
    animations = []
    for clip_name, channels in clips.items():
        samplers, targets = [], []
        for bone, path, times, values in channels:
            width = 4 if path == "rotation" else 3
            samplers.append({"input": doc.floats(times, "SCALAR", 1, bounds=True),
                             "output": doc.floats(values, "VEC4" if width == 4 else "VEC3", width),
                             "interpolation": "LINEAR"})
            targets.append({"sampler": len(samplers) - 1, "target": {"node": first_bone + bone, "path": path}})
        animations.append({"name": clip_name, "samplers": samplers, "channels": targets})
    gltf = {
        "asset": {"version": "2.0", "generator": "Owl tools/demo/generate_skinned_meshes.py"},
        "scene": 0,
        "scenes": [{"nodes": [0] + roots}],
        "nodes": nodes,
        "skins": [{"joints": [first_bone + i for i in range(len(bones))], "inverseBindMatrices": inverse_bind,
                   "skeleton": roots[0]}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": position, "NORMAL": normal,
                                                                 "TEXCOORD_0": uv, "JOINTS_0": joints,
                                                                 "WEIGHTS_0": weights},
                                                  "indices": indices, "material": 0}]}],
        "materials": [{"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": 0.0,
                                                               "roughnessFactor": 0.7}}],
        "animations": animations,
        "accessors": doc.accessors,
        "bufferViews": doc.views,
        "buffers": [{"byteLength": len(doc.blob)}],
    }
    return gltf, doc.blob


def write_text(path: Path, gltf: dict, blob: bytes) -> None:
    """Write a ``.gltf`` with its buffer embedded as a data URI."""
    gltf["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(blob).decode()
    path.write_text(json.dumps(gltf, indent=1) + "\n")


def skinned_arm() -> tuple[dict, bytes]:
    """Two bones, ``root`` at the origin and ``lower`` 1 m up, a 2 m column; ``bend`` turns ``lower`` 90 degrees
    about Z in one second, ``bent`` holds it at 60 degrees; ``socket_tip`` sits 1 m above ``lower``."""
    bones = [("root", -1, (0.0, 0.0, 0.0)), ("lower", 0, (0.0, 1.0, 0.0))]
    skinned = SkinnedBuilder()
    skinned.add_part(limb((0.0, 1.0, 0.0), (0.2, 1.0, 0.2), 8), lambda p: [(0, 1.0)])
    skinned.add_part(limb((0.0, 2.0, 0.0), (0.2, 1.0, 0.2), 8), lambda p: [(1, 1.0)])
    bend = [(1, "rotation", [0.0, 1.0], [quat_axis((0, 0, 1), 0), quat_axis((0, 0, 1), 90)])]
    bent = [(1, "rotation", [0.0, 1.0], [quat_axis((0, 0, 1), 60), quat_axis((0, 0, 1), 60)])]
    return skinned_document(skinned, bones, {"bend": bend, "bent": bent}, [("tip", 1, (0.0, 1.0, 0.0), (0, 0, 0, 1))],
                            "Arm", (1.0, 1.0, 1.0, 1.0))


def main() -> None:
    """Write the test fixture."""
    write_text(FIXTURE, *skinned_arm())


if __name__ == "__main__":
    main()
