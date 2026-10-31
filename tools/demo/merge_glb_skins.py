#!/usr/bin/env python3
"""Merge the skins of a ``.glb`` that share their joints into the first one (the engine reads one skin per file).

Kenney's Mini Characters bind their body and their head to two skins over the same seven joints with the same inverse
bind matrices: every skinned node is pointed at the first skin and the others are dropped, the binary chunk is kept
as it is. A file whose skins differ is left untouched (and reported). Standard library only.

Run with ``poetry run python tools/demo/merge_glb_skins.py engine_demo/meshes/characters/*.glb``.
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

GLB_MAGIC, JSON_CHUNK = 0x46546C67, 0x4E4F534A


def read_glb(path: Path) -> tuple[dict, bytes]:
    """The JSON document and the remaining chunks (the binary buffer) of a ``.glb``."""
    data = path.read_bytes()
    magic, _, _ = struct.unpack_from("<III", data, 0)
    length, kind = struct.unpack_from("<II", data, 12)
    if magic != GLB_MAGIC or kind != JSON_CHUNK:
        raise ValueError(f"{path} is not a binary glTF")
    return json.loads(data[20:20 + length]), data[20 + length:]


def write_glb(path: Path, document: dict, rest: bytes) -> None:
    """Write the JSON document (padded with spaces) followed by the untouched chunks."""
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    body = struct.pack("<II", len(text), JSON_CHUNK) + text + rest
    path.write_bytes(struct.pack("<III", GLB_MAGIC, 2, 12 + len(body)) + body)


def accessor_bytes(document: dict, rest: bytes, index: int) -> bytes:
    """The raw bytes of an accessor (tightly packed MAT4 floats here)."""
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    start = 8 + view.get("byteOffset", 0) + accessor.get("byteOffset", 0)  # past the BIN chunk header
    return rest[start:start + accessor["count"] * 64]


def merge(path: Path) -> bool:
    """Merge the skins of one file; true when it was rewritten."""
    document, rest = read_glb(path)
    skins = document.get("skins", [])
    if len(skins) < 2:
        return False
    first = skins[0]
    binds = accessor_bytes(document, rest, first["inverseBindMatrices"])
    for skin in skins[1:]:
        if skin["joints"] != first["joints"] or accessor_bytes(document, rest, skin["inverseBindMatrices"]) != binds:
            print(f"{path}: its skins differ, left as it is")
            return False
    for node in document["nodes"]:
        if "skin" in node:
            node["skin"] = 0
    document["skins"] = [first]
    write_glb(path, document, rest)
    return True


def main() -> None:
    for name in sys.argv[1:]:
        print(f"{name}: {'merged' if merge(Path(name)) else 'unchanged'}")


if __name__ == "__main__":
    main()
