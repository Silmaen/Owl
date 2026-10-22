#!/usr/bin/env python3
"""Generate the glTF fixture of the mesh loader tests (``test/unit/test_helper/assets/mesh_tests/two_materials.glb``):
two triangle primitives, each with its own PBR material, a normal map and a metallic-roughness image."""

import json
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "test" / "unit" / "test_helper" / "assets" / "mesh_tests"


def png_bytes(width: int, height: int, rows: list[list[tuple[int, int, int]]]) -> bytes:
    """PNG encoding (RGB, 8 bits) of the given rows, top row first."""

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    raw = b"".join(b"\0" + b"".join(bytes(pixel) for pixel in row) for row in rows)
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


def write_two_materials(path: Path) -> None:
    """Write two triangles: the first with a plain material, the second with every optional layer."""
    positions = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (2, 0, 0), (3, 0, 0), (2, 1, 0)]
    position_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    index_bytes = struct.pack("<6H", 0, 1, 2, 0, 1, 2)
    normal_map = png_bytes(2, 2, [[(255, 0, 0), (0, 255, 0)], [(0, 0, 255), (255, 255, 255)]])
    metallic_roughness = png_bytes(4, 2, [[(10 * (x + 1), 20, 30) for x in range(4)] for _ in range(2)])
    chunks = [position_bytes, index_bytes, normal_map, metallic_roughness]
    views, blob = [], b""
    for i, data in enumerate(chunks):
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if i == 0:
            view["target"] = 34962
        elif i == 1:
            view["target"] = 34963
        views.append(view)
        blob += data + b"\0" * (-len(data) % 4)
    materials = [
        {
            "name": "Red",
            "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.0, 0.0, 1.0], "metallicFactor": 0.25,
                                     "roughnessFactor": 0.75},
            "emissiveFactor": [0.1, 0.2, 0.3],
            "doubleSided": True,
            "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": 2.0}},
        },
        {
            "name": "Green",
            "pbrMetallicRoughness": {"baseColorFactor": [0.0, 1.0, 0.0, 0.5], "metallicFactor": 1.0,
                                     "roughnessFactor": 0.1, "metallicRoughnessTexture": {"index": 1}},
            "normalTexture": {"index": 0, "scale": 0.5},
            "occlusionTexture": {"index": 1, "strength": 0.7},
            "alphaMode": "MASK",
            "alphaCutoff": 0.4,
            "extensions": {
                "KHR_materials_clearcoat": {"clearcoatFactor": 0.6, "clearcoatRoughnessFactor": 0.2},
                "KHR_materials_sheen": {"sheenColorFactor": [0.1, 0.2, 0.3], "sheenRoughnessFactor": 0.4},
                "KHR_materials_anisotropy": {"anisotropyStrength": 0.3, "anisotropyRotation": 1.5},
            },
        },
    ]
    doc = {
        "asset": {"version": "2.0", "generator": "Owl tools/demo/generate_material_fixtures.py"},
        "extensionsUsed": ["KHR_materials_emissive_strength", "KHR_materials_clearcoat", "KHR_materials_sheen",
                           "KHR_materials_anisotropy"],
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [
            {"attributes": {"POSITION": 0}, "indices": 1, "material": 0},
            {"attributes": {"POSITION": 2}, "indices": 3, "material": 1},
        ]}],
        "materials": materials,
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0, 0, 0], "max": [1, 1, 0]},
            {"bufferView": 1, "byteOffset": 0, "componentType": 5123, "count": 3, "type": "SCALAR"},
            {"bufferView": 0, "byteOffset": 36, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [2, 0, 0], "max": [3, 1, 0]},
            {"bufferView": 1, "byteOffset": 6, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
        "images": [{"name": "normal_map", "bufferView": 2, "mimeType": "image/png"},
                   {"name": "metallic_roughness", "bufferView": 3, "mimeType": "image/png"}],
        "textures": [{"source": 0}, {"source": 1}],
    }
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(blob), 0x004E4942) + blob
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def main() -> None:
    """Write the fixtures."""
    OUT.mkdir(parents=True, exist_ok=True)
    write_two_materials(OUT / "two_materials.glb")
    print(f"Fixtures written to {OUT}")


if __name__ == "__main__":
    main()
