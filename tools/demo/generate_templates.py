#!/usr/bin/env python3
"""Generate the binary assets of the project templates (``engine_assets/project_templates/``).

- ``mesh_3d`` and ``mixed``: ``meshes/crate.glb`` (a one-metre box standing on y = 0) and ``meshes/ground.glb`` (an
  80 m slab whose top is y = 0), built with the mesh helpers of ``generate_meshes.py``.
- ``isometric``: the flat row of the Engine Demo terrain atlas (``textures/iso_tiles*.png``, albedo, normal and depth),
  its tileset, a 12 x 12 island map and four object sprites copied from ``engine_demo/textures/iso/``.
- ``mixed``: the pine sprite drawn as a billboard among the meshes.

Every written file gets an ``.owlmeta`` with a GUID derived from its path, so a new run keeps the references.

Usage (from the repository root): ``./docker_run.sh poetry run python -m tools.demo.generate_templates``
"""

from __future__ import annotations

import hashlib
import random
import shutil
import struct
from pathlib import Path

from PIL import Image

from tools.demo.generate_meshes import box, write_glb
from tools.iso_atlas.common import TILE_H, TILE_W
from tools.iso_atlas.terrain import TERRAINS

ROOT = Path(__file__).resolve().parents[2]
TEMPLATES = ROOT / "engine_assets" / "project_templates"
DEMO_ISO = ROOT / "engine_demo" / "textures" / "iso"

CHUNK = 32
EMPTY = 0xFFFF
ISLAND = 12
GRASS, SAND, WATER, DIRT, DARK_GRASS, FLOWERS, ROCK = 0, 1, 2, 4, 6, 7, 8
FLAT_TILES = 16

IMPORTERS = {".png": "Texture", ".glb": "Mesh", ".owltileset": "Document", ".owltilemap": "Document"}


def guid(path: Path) -> str:
    """Stable 16-hex GUID of a template file, from its path inside the templates folder."""
    return hashlib.sha1(path.relative_to(TEMPLATES).as_posix().encode()).hexdigest()[:16]


def write_meta(path: Path) -> None:
    """Write the ``.owlmeta`` of a generated file."""
    importer = IMPORTERS[path.suffix]
    lines = ["FormatVersion: 1", f"Guid: {guid(path)}", f"Importer: {importer}"]
    if importer == "Texture":
        lines += ["Settings:", "  Nearest: true", "  Compression: None", "  Mips: false"]
    path.with_name(path.name + ".owlmeta").write_text("\n".join(lines) + "\n")


def write_meshes(template: Path) -> list[Path]:
    """The crate and the ground slab of a 3D template."""
    folder = template / "meshes"
    folder.mkdir(parents=True, exist_ok=True)
    crate = folder / "crate.glb"
    write_glb(crate, box((0.0, 0.5, 0.0), (1.0, 1.0, 1.0)), "crate", color=(0.78, 0.55, 0.32, 1.0), roughness=0.7)
    ground = folder / "ground.glb"
    write_glb(ground, box((0.0, -0.1, 0.0), (80.0, 0.2, 80.0), tile=2.0), "ground", color=(0.42, 0.55, 0.36, 1.0))
    return [crate, ground]


def tileset_text(texture: str) -> str:
    """Flat isometric tileset over the first atlas row: one tile per terrain, no slope."""
    lines = [
        "Tileset: iso_ground",
        "FormatVersion: 1",
        f"texture: {texture}",
        f"tileWidth: {TILE_W}",
        f"tileHeight: {TILE_H}",
        f"columns: {FLAT_TILES}",
        "rows: 1",
        "filterMode: Nearest",
        "kind: Isometric",
        "tiles:",
    ]
    for terrain in TERRAINS:
        if terrain.id < FLAT_TILES:
            lines += [f"  - index: {terrain.id}", f"    name: {terrain.name}"]
    return "\n".join(lines) + "\n"


def _fnv1a(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def island_cells() -> dict[tuple[int, int], int]:
    """Ground of the island: grass in the middle, a sandy beach, water around, a dirt path and a rocky corner."""
    rng = random.Random(7)
    cells: dict[tuple[int, int], int] = {}
    centre = (ISLAND - 1) / 2
    for y in range(ISLAND):
        for x in range(ISLAND):
            distance = max(abs(x - centre), abs(y - centre))
            if distance > 5:
                tile = WATER
            elif distance > 4:
                tile = SAND
            elif x == 6 and y >= 3:
                tile = DIRT
            elif x >= 8 and y <= 3:
                tile = ROCK
            else:
                tile = GRASS if rng.random() < 0.7 else rng.choice([DARK_GRASS, FLOWERS])
            cells[(x, y)] = tile
    return cells


def tilemap_bytes(tileset: str) -> bytes:
    """The ``island`` tilemap file, chunked format 3, one ground layer, no corner heights."""
    terrain = [EMPTY] * (CHUNK * CHUNK)
    for (x, y), tile in island_cells().items():
        terrain[y * CHUNK + x] = tile
    chunk = struct.pack(f"<{CHUNK * CHUNK}H", *terrain) + struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK))
    chunk += bytes(CHUNK * CHUNK)
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 1) + struct.pack("<I", 1) + struct.pack("<II", 0, 0) + chunk
    body += b"\x00"
    payload = body + struct.pack("<I", _fnv1a(body))
    header = (
        f"Tilemap: island\nFormatVersion: 3\ntilesetPath: {tileset}\nwidth: {ISLAND}\nheight: {ISLAND}\n"
        f"cellSize: 1\nlayers:\n  - name: ground\npayloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def write_isometric(template: Path) -> list[Path]:
    """Atlas row, tileset, island map and object sprites of the isometric template."""
    textures = template / "textures"
    textures.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for suffix in ("", "_normal", "_depth"):
        atlas = Image.open(DEMO_ISO / f"tiles{suffix}.png")
        target = textures / f"iso_tiles{suffix}.png"
        atlas.crop((0, 0, FLAT_TILES * TILE_W, TILE_H)).save(target, optimize=True)
        written.append(target)
    for sprite in ("oak", "pine", "lamp", "marker"):
        for suffix in ("", "_normal", "_depth"):
            target = textures / f"{sprite}{suffix}.png"
            shutil.copyfile(DEMO_ISO / f"{sprite}{suffix}.png", target)
            written.append(target)
    atlas_path = textures / "iso_tiles.png"
    tileset = template / "tilesets" / "iso_ground.owltileset"
    tileset.parent.mkdir(parents=True, exist_ok=True)
    tileset.write_text(tileset_text(f"gid:{guid(atlas_path)}:textures/iso_tiles.png"))
    written.append(tileset)
    tilemap = template / "tilemaps" / "island.owltilemap"
    tilemap.parent.mkdir(parents=True, exist_ok=True)
    tilemap.write_bytes(tilemap_bytes(f"gid:{guid(tileset)}:tilesets/iso_ground.owltileset"))
    written.append(tilemap)
    return written


def write_mixed_sprites(template: Path) -> list[Path]:
    """The pine billboard of the mixed template."""
    textures = template / "textures"
    textures.mkdir(parents=True, exist_ok=True)
    target = textures / "pine.png"
    shutil.copyfile(DEMO_ISO / "pine.png", target)
    return [target]


def main() -> int:
    """Write every generated template asset and its ``.owlmeta``."""
    written = write_meshes(TEMPLATES / "mesh_3d")
    written += write_meshes(TEMPLATES / "mixed")
    written += write_mixed_sprites(TEMPLATES / "mixed")
    written += write_isometric(TEMPLATES / "isometric")
    for path in written:
        write_meta(path)
        print(path.relative_to(ROOT), guid(path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
