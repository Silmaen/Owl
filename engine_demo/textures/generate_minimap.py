#!/usr/bin/env python3
"""Build the minimap of the raycast level: ``minimap.png``, ``tilesets/minimap.owltileset`` and ``tilemaps/minimap.owltilemap``.

The tilemap is the wall layer of ``tilemaps/raycast_demo.owltilemap`` reduced to two tiles, a dark translucent wall and a light floor
so that a Renderer2D layer can draw it over the raycast view without colliders. It has two layers: ``walls`` (the whole level, hidden: the
truth ``raycast_level.lua`` casts its rays in) and ``seen`` (empty: the script copies a cell of ``walls`` into it once the player has
seen it, the fog of war). Run from anywhere:
``python3 engine_demo/textures/generate_minimap.py``.
"""
import struct
from pathlib import Path

import yaml
from PIL import Image

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
EMPTY = 0xFFFF
WALL, FLOOR = 0, 1


def fnv1a(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def read_walls(path: Path, size: int) -> set[tuple[int, int]]:
    raw = path.read_bytes()
    payload = raw[raw.index(b"\n...\n") + 5:]
    chunk, layers = struct.unpack("<II", payload[8:16])
    offset = 16
    walls: set[tuple[int, int]] = set()
    for _ in range(layers):
        (count,) = struct.unpack("<I", payload[offset:offset + 4])
        offset += 4
        for _ in range(count):
            cx, cy = struct.unpack("<II", payload[offset:offset + 8])
            offset += 8
            terrain = struct.unpack(f"<{chunk * chunk}H", payload[offset:offset + 2 * chunk * chunk])
            offset += 4 * chunk * chunk + chunk * chunk
            for k, tile in enumerate(terrain):
                if tile != EMPTY:
                    walls.add((cx * chunk + k % chunk, cy * chunk + k // chunk))
    return {cell for cell in walls if cell[0] < size and cell[1] < size}


def main() -> None:
    size = 64
    walls = read_walls(PROJECT / "tilemaps" / "raycast_demo.owltilemap", size)
    sheet = Image.new("RGBA", (16, 8), (0, 0, 0, 0))
    for x in range(8):
        for y in range(8):
            sheet.putpixel((x, y), (16, 16, 24, 150))
            sheet.putpixel((8 + x, y), (190, 190, 205, 235))
    sheet.save(HERE / "minimap.png")
    tileset = {"Tileset": "minimap", "FormatVersion": 1, "texture": "nam:textures/minimap.png", "tileWidth": 8,
               "tileHeight": 8, "columns": 2, "rows": 1, "filterMode": "Nearest",
               "tiles": [{"index": WALL, "name": "wall"}, {"index": FLOOR, "name": "floor"}]}
    (PROJECT / "tilesets" / "minimap.owltileset").write_text(yaml.safe_dump(tileset, sort_keys=False))
    chunk = 32
    body = b"OWLTCHNK" + struct.pack("<II", chunk, 2)
    chunks = []
    for cy in range(size // chunk):
        for cx in range(size // chunk):
            terrain = [WALL if (cx * chunk + k % chunk, cy * chunk + k // chunk) in walls else FLOOR
                       for k in range(chunk * chunk)]
            chunks.append(struct.pack("<II", cx, cy) + struct.pack(f"<{chunk * chunk}H", *terrain)
                          + struct.pack(f"<{chunk * chunk}H", *([EMPTY] * chunk * chunk)) + bytes(chunk * chunk))
    body += struct.pack("<I", len(chunks)) + b"".join(chunks)  # layer 0: walls
    body += struct.pack("<I", 0) + b"\x00"  # layer 1: seen, no chunk; then no heights
    payload = body + struct.pack("<I", fnv1a(body))
    header = (f"Tilemap: minimap\nFormatVersion: 3\ntilesetPath: tilesets/minimap.owltileset\nwidth: {size}\n"
              f"height: {size}\ncellSize: 1\nlayers:\n  - name: walls\n    visible: false\n  - name: seen\n"
              f"payloadBytes: {len(payload)}\n")
    (PROJECT / "tilemaps" / "minimap.owltilemap").write_bytes(header.encode() + b"...\n" + payload)

if __name__ == "__main__":
    main()
