#!/usr/bin/env python3
"""Generate the skinned meshes of the Engine Demo and of the mesh tests.

``engine_demo/meshes/characters/mannequin.glb``: a 1.8 m mannequin of 13 bones with the clips ``idle``, ``walk`` and
``wave`` and a ``socket_hand_r`` node under its right hand; ``engine_demo/meshes/characters/lantern.glb``: the
lantern it holds; ``test/unit/test_helper/assets/mesh_tests/skinned_arm.gltf``: a two-bone arm with one clip and one
socket, the fixture of the skinning tests."""

import base64
import json
import math
import struct
from pathlib import Path

from generate_meshes import MeshBuilder, box, normalize, write_glb

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "engine_demo" / "meshes" / "characters"
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


def blend_near_top(bone: int, parent: int, top_y: float, length: float):
    """Weights of a limb: its bone, blended with the parent over the top fifth of the limb."""
    def weights(p):
        t = min(max((top_y - p[1]) / (0.2 * length), 0.0), 1.0)
        if parent < 0 or t >= 1.0:
            return [(bone, 1.0)]
        return [(bone, 0.5 + 0.5 * t), (parent, 0.5 - 0.5 * t)]
    return weights


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


def write_binary(path: Path, gltf: dict, blob: bytes) -> None:
    """Write a ``.glb``."""
    text = json.dumps(gltf, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(blob), 0x004E4942) + blob
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def write_text(path: Path, gltf: dict, blob: bytes) -> None:
    """Write a ``.gltf`` with its buffer embedded as a data URI."""
    gltf["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(blob).decode()
    path.write_text(json.dumps(gltf, indent=1) + "\n")


MANNEQUIN_BONES = [
    ("hips", -1, (0.0, 0.95, 0.0)),
    ("spine", 0, (0.0, 0.12, 0.0)),
    ("head", 1, (0.0, 0.5, 0.0)),
    ("upperarm_l", 1, (0.24, 0.42, 0.0)),
    ("forearm_l", 3, (0.0, -0.3, 0.0)),
    ("hand_l", 4, (0.0, -0.27, 0.0)),
    ("upperarm_r", 1, (-0.24, 0.42, 0.0)),
    ("forearm_r", 6, (0.0, -0.3, 0.0)),
    ("hand_r", 7, (0.0, -0.27, 0.0)),
    ("thigh_l", 0, (0.1, -0.05, 0.0)),
    ("shin_l", 9, (0.0, -0.44, 0.0)),
    ("thigh_r", 0, (-0.1, -0.05, 0.0)),
    ("shin_r", 11, (0.0, -0.44, 0.0)),
]


def mannequin() -> tuple[dict, bytes]:
    """The mannequin, its clips and its hand socket."""
    world = []
    for _, parent, local in MANNEQUIN_BONES:
        origin = world[parent] if parent >= 0 else (0.0, 0.0, 0.0)
        world.append(tuple(origin[k] + local[k] for k in range(3)))
    skinned = SkinnedBuilder()
    parts = [  # bone, top, (width, length, depth)
        (0, (0.0, 1.07, 0.0), (0.32, 0.2, 0.2)),
        (1, (0.0, 1.5, 0.0), (0.4, 0.44, 0.22)),
        (2, (0.0, 1.8, 0.0), (0.2, 0.24, 0.22)),
        (3, world[3], (0.1, 0.3, 0.1)), (4, world[4], (0.09, 0.27, 0.09)), (5, world[5], (0.08, 0.12, 0.05)),
        (6, world[6], (0.1, 0.3, 0.1)), (7, world[7], (0.09, 0.27, 0.09)), (8, world[8], (0.08, 0.12, 0.05)),
        (9, world[9], (0.13, 0.44, 0.14)), (10, world[10], (0.11, 0.44, 0.12)),
        (11, world[11], (0.13, 0.44, 0.14)), (12, world[12], (0.11, 0.44, 0.12)),
    ]
    for bone, top, size in parts:
        parent = MANNEQUIN_BONES[bone][1]
        skinned.add_part(limb(top, size), blend_near_top(bone, parent if bone > 2 else -1, top[1], size[1]))
    skinned.add_part(box((0.0, 0.9, 0.11), (0.06, 0.06, 0.06)), lambda p: [(0, 1.0)])

    def swing(bone, axis, amplitude, period, phase=0.0, offset=0.0, keys=9):
        times = [period * k / (keys - 1) for k in range(keys)]
        values = [quat_axis(axis, offset + amplitude * math.sin(2 * math.pi * (t / period) + phase)) for t in times]
        return (bone, "rotation", times, values)

    rest = (0.0, 0.95, 0.0)
    walk = [swing(9, (1, 0, 0), 30, 1.0), swing(11, (1, 0, 0), 30, 1.0, math.pi),
            swing(10, (1, 0, 0), -20, 1.0, -math.pi / 2, -20), swing(12, (1, 0, 0), -20, 1.0, math.pi / 2, -20),
            swing(3, (1, 0, 0), 25, 1.0, math.pi), swing(6, (1, 0, 0), 12, 1.0, 0.0),
            swing(7, (1, 0, 0), -10, 1.0, 0.0, -35),
            (0, "translation", [0.0, 0.25, 0.5, 0.75, 1.0],
             [rest, (0.0, 0.92, 0.0), rest, (0.0, 0.92, 0.0), rest])]
    idle = [swing(1, (1, 0, 0), 2.5, 2.0), swing(3, (0, 0, 1), 3, 2.0), swing(6, (0, 0, 1), -3, 2.0),
            swing(7, (1, 0, 0), 2, 2.0, 0.0, -35)]
    wave = [swing(3, (0, 0, 1), 0, 1.5, 0.0, 150), swing(4, (0, 0, 1), 25, 0.75, 0.0, 10, keys=9),
            swing(7, (1, 0, 0), 2, 1.5, 0.0, -35)]
    sockets = [("hand_r", 8, (0.0, -0.1, 0.03), quat_axis((1, 0, 0), 35))]
    return skinned_document(skinned, MANNEQUIN_BONES, {"idle": idle, "walk": walk, "wave": wave}, sockets,
                            "Mannequin", (0.72, 0.62, 0.5, 1.0))


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


def lantern() -> MeshBuilder:
    """A small lantern hanging from its ring at the origin, 0.25 m tall."""
    m = box((0.0, -0.02, 0.0), (0.03, 0.04, 0.03))
    m.extend(box((0.0, -0.06, 0.0), (0.12, 0.02, 0.12)))
    m.extend(box((0.0, -0.14, 0.0), (0.09, 0.14, 0.09)))
    m.extend(box((0.0, -0.22, 0.0), (0.13, 0.03, 0.13)))
    return m


def main() -> None:
    """Write the demo characters and the test fixture."""
    OUT.mkdir(parents=True, exist_ok=True)
    write_binary(OUT / "mannequin.glb", *mannequin())
    write_glb(OUT / "lantern.glb", lantern(), "Lantern", (0.85, 0.62, 0.25, 1.0))
    write_text(FIXTURE, *skinned_arm())


if __name__ == "__main__":
    main()
