"""
Atlas assembly: the terrain atlas, its tileset, the building sheet and the props, at every zoom level.

Zoom levels: ``1x`` is drawn natively at 64x48 pixels per tile, ``2x`` natively at 128x96, ``half`` is the mip
of ``1x`` (32x24). Every image has a ``_normal`` and a ``_depth`` companion on the same grid; the other levels carry
an ``@2x`` / ``@0.5x`` suffix.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from .city_map import write_city_map
from .city_scene import write_city_scene
from .common import TILE_H, TILE_W, Z_RANGE, Layers, Vec3
from .objects import (
    BUILDINGS,
    PROPS,
    SLICE_HEIGHT,
    VEHICLE_HEADINGS,
    VEHICLE_MODELS,
    VEHICLES,
    birds,
    render_object,
    slice_object,
    smoke,
)
from .sample_map import write_map
from .terrain import CONNECTION_NAMES, FOUNDATION_TOPS, SLOPE_COUNT, SLOPES, TERRAINS, draw_foundation, draw_surface
from .town_map import write_town_map

COLUMNS = 16
"""Tiles per row of the terrain atlas."""
LEVELS = ("1x", "2x", "half")
"""Zoom levels, in generation order."""
SUFFIX = {"1x": "", "2x": "@2x", "half": "@0.5x"}
"""File name suffix of each level."""
VEHICLE_HEIGHT = 64
"""Height in pixels of a vehicle cell."""
SHEET_COLUMNS = 4
"""Slices per row of the building sheet."""


def _tile_plan() -> tuple[list[int], int, int]:
    """Atlas layout: flat terrains, then 18 sloped tiles per sloped terrain, then 18 per foundation set."""
    sloped = [t.id for t in TERRAINS if t.sloped]
    first_sloped = len(TERRAINS)
    first_foundation = first_sloped + (SLOPE_COUNT - 1) * len(sloped)
    return sloped, first_sloped, first_foundation


SLOPED_TERRAINS, FIRST_SLOPED, FIRST_FOUNDATION = _tile_plan()
TILE_COUNT = FIRST_FOUNDATION + (SLOPE_COUNT - 1) * len(FOUNDATION_TOPS)
"""Number of tiles of the terrain atlas."""
ROWS = -(-TILE_COUNT // COLUMNS)
"""Rows of the terrain atlas."""


def tile_index(terrain: int, slope: int) -> int:
    """
    Atlas index of a terrain on a slope.

    :param terrain: Terrain id.
    :param slope: Slope index, 0 for flat.
    :return: The atlas index; the flat tile is the terrain id itself.
    """
    if slope == 0:
        return terrain
    return FIRST_SLOPED + SLOPED_TERRAINS.index(terrain) * (SLOPE_COUNT - 1) + slope - 1


def foundation_index(set_index: int, slope: int) -> int:
    """
    Atlas index of a foundation.

    :param set_index: Index in ``FOUNDATION_TOPS``.
    :param slope: Slope index, at least 1.
    :return: The atlas index.
    """
    return FIRST_FOUNDATION + set_index * (SLOPE_COUNT - 1) + slope - 1


def build_terrain(seed: int, scale: int) -> Layers:
    """
    Draw the terrain atlas.

    :param seed: Seed of the run.
    :param scale: Zoom factor, 1 or 2.
    :return: The three layers.
    """
    layers = Layers(COLUMNS * TILE_W * scale, ROWS * TILE_H * scale)

    def place(index: int) -> tuple[int, int]:
        return index % COLUMNS, index // COLUMNS

    for terrain in TERRAINS:
        x, y = place(terrain.id)
        draw_surface(layers, x, y, terrain.material, SLOPES[0], seed * 7919 + 1000 + terrain.id, scale)
    for terrain_id in SLOPED_TERRAINS:
        terrain = TERRAINS[terrain_id]
        for slope in range(1, SLOPE_COUNT):
            x, y = place(tile_index(terrain_id, slope))
            tile_seed = seed * 7919 + 5000 + terrain_id * 100 + slope
            draw_surface(layers, x, y, terrain.material, SLOPES[slope], tile_seed, scale)
    for set_index, (_, top) in enumerate(FOUNDATION_TOPS):
        for slope in range(1, SLOPE_COUNT):
            x, y = place(foundation_index(set_index, slope))
            tile_seed = seed * 7919 + 9000 + set_index * 100 + slope
            draw_foundation(layers, x, y, top, SLOPES[slope], tile_seed, scale)
    return layers


def tileset_text(level: str, texture: str) -> str:
    """
    Write the tileset: the slope table, the foundations and the tile names.

    :param level: Zoom level, giving the tile size.
    :param texture: Texture reference, as in ``nam:textures/iso/tiles.png``.
    :return: The ``.owltileset`` text.
    """
    factor = {"1x": 1.0, "2x": 2.0, "half": 0.5}[level]
    name = "iso_ground" + {"1x": "", "2x": "_2x", "half": "_half"}[level]
    lines = [
        f"Tileset: {name}",
        "FormatVersion: 1",
        f"texture: {texture}",
        f"tileWidth: {int(TILE_W * factor)}",
        f"tileHeight: {int(TILE_H * factor)}",
        f"columns: {COLUMNS}",
        f"rows: {ROWS}",
        "filterMode: Nearest",
        "kind: Isometric",
        "slopes:",
    ]
    owners = {top_id: i for i, (top_id, _) in enumerate(FOUNDATION_TOPS)}
    for terrain_id in SLOPED_TERRAINS:
        tiles = ", ".join(str(tile_index(terrain_id, slope)) for slope in range(SLOPE_COUNT))
        lines += [f"  - id: {terrain_id}", f"    tiles: [{tiles}]"]
        if terrain_id in owners:
            found = ", ".join(["-1"] + [str(foundation_index(owners[terrain_id], s)) for s in range(1, SLOPE_COUNT)])
            lines.append(f"    foundations: [{found}]")
    lines.append("tiles:")
    for terrain in TERRAINS:
        lines += [f"  - index: {terrain.id}", f"    name: {terrain.name}"]
        if terrain.connections is not None:
            sides = ", ".join(name for bit, name in CONNECTION_NAMES if terrain.connections & bit)
            lines.append(f"    connections: [{sides}]")
    return "\n".join(lines) + "\n"


@dataclass
class SheetEntry:
    """
    Where a building slice sits in the sheet.
    """

    building: str
    """Building name."""
    footprint: tuple[int, int]
    """Footprint in tiles."""
    cell: tuple[int, int]
    """Footprint tile of the slice."""
    zmax: float
    """Height of the slice's highest point in world units."""


def build_buildings(seed: int, scale: int) -> tuple[Layers, list[SheetEntry]]:
    """
    Render the buildings and pack their per-tile slices into one sheet.

    :param seed: Seed of the run.
    :param scale: Zoom factor, 1 or 2.
    :return: The sheet and its entries in sheet order.
    """
    entries: list[SheetEntry] = []
    cells: list[Layers] = []
    for spec in BUILDINGS:
        rendered = render_object(spec, seed, scale)
        for cell, (layers, zmax) in sorted(slice_object(spec, rendered).items(), key=lambda kv: (kv[0][1], kv[0][0])):
            entries.append(SheetEntry(spec.name, spec.footprint, cell, zmax))
            cells.append(layers)
    rows = -(-len(cells) // SHEET_COLUMNS)
    sheet = Layers(SHEET_COLUMNS * TILE_W * scale, rows * SLICE_HEIGHT * scale)
    for index, layers in enumerate(cells):
        sheet.paste(layers, (index % SHEET_COLUMNS) * TILE_W * scale, (index // SHEET_COLUMNS) * SLICE_HEIGHT * scale)
    return sheet, entries


def build_vehicles(seed: int) -> Layers:
    """
    Render the vehicles into a spritesheet of 64x64 cells, at zoom 1.

    One row per vehicle model, one column per heading (every 45 degrees from +x towards +y). The cell is centred on the
    vehicle, its bottom centre on the front corner of the tile the vehicle stands on, where the engine puts the entity.

    :param seed: Seed of the run.
    :return: The sheet.
    """
    sheet = Layers(VEHICLE_HEADINGS * TILE_W, len(VEHICLE_MODELS) * VEHICLE_HEIGHT)
    for index, spec in enumerate(VEHICLES):
        sheet.paste(
            render_object(spec, seed, 1).layers,
            (index % VEHICLE_HEADINGS) * TILE_W,
            (index // VEHICLE_HEADINGS) * VEHICLE_HEIGHT,
        )
    return sheet


def buildings_json(entries: list[SheetEntry], level: str) -> str:
    """
    Describe the slices of the building sheet.

    Each slice is a sprite cell whose bottom centre is the front corner of its footprint tile, with the 3D box the
    slice occupies (tile units): the footprint tile and the height of its highest point.

    :param entries: The sheet entries.
    :param level: Zoom level, scaling the pixel rectangles.
    :return: The JSON text.
    """
    factor = {"1x": 1.0, "2x": 2.0, "half": 0.5}[level]
    buildings: dict[str, dict[str, object]] = {}
    for index, entry in enumerate(entries):
        info = buildings.setdefault(entry.building, {"footprint": list(entry.footprint), "slices": []})
        slice_box: dict[str, list[float]] = {
            "min": [float(entry.cell[0]), float(entry.cell[1]), 0.0],
            "max": [float(entry.cell[0] + 1), float(entry.cell[1] + 1), round(entry.zmax, 3)],
        }
        rect = [
            int((index % SHEET_COLUMNS) * TILE_W * factor),
            int((index // SHEET_COLUMNS) * SLICE_HEIGHT * factor),
            int(TILE_W * factor),
            int(SLICE_HEIGHT * factor),
        ]
        slices = info["slices"]
        assert isinstance(slices, list)
        slices.append({"cell": list(entry.cell), "rect": rect, "anchor": [rect[2] // 2, rect[3]], "box": slice_box})
    return json.dumps({"zoom": level, "buildings": buildings}, indent=2) + "\n"


def _save(layers: Layers, folder: Path, stem: str, level: str) -> list[Path]:
    written = []
    for suffix, image in (("", layers.albedo), ("_normal", layers.normal), ("_depth", layers.depth)):
        path = folder / f"{stem}{suffix}{SUFFIX[level]}.png"
        image.save(path, optimize=True)
        written.append(path)
    return written


def generate(
    project: Path, seed: int = 1, levels: tuple[str, ...] = LEVELS, tests: Path | None = None
) -> list[Path]:
    """
    Generate every atlas, the tilesets, the sprites and the town map into a project folder, and the test maps.

    :param project: The project folder (``engine_demo``); textures go to ``textures/iso``, tilesets to ``tilesets``.
    :param seed: Seed of the run; the same seed always gives the same pixels.
    :param levels: The zoom levels to write.
    :param tests: The ``test/test_helper/assets`` folder of the repository, for the test-only maps (skipped when absent).
    :return: Every written file.
    """
    folder = project / "textures" / "iso"
    folder.mkdir(parents=True, exist_ok=True)
    (project / "tilesets").mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    terrain: dict[str, Layers] = {}
    sheets: dict[str, tuple[Layers, list[SheetEntry]]] = {}
    for level in ("1x", "2x"):
        if level in levels or (level == "1x" and "half" in levels):
            terrain[level] = build_terrain(seed, 1 if level == "1x" else 2)
            sheets[level] = build_buildings(seed, 1 if level == "1x" else 2)
    if "half" in levels:
        terrain["half"] = terrain["1x"].half()
        half_sheet = sheets["1x"][0].half()
        sheets["half"] = (half_sheet, sheets["1x"][1])
    for level in levels:
        written += _save(terrain[level], folder, "tiles", level)
        sheet, entries = sheets[level]
        written += _save(sheet, folder, "buildings", level)
        json_path = folder / f"buildings{SUFFIX[level]}.json"
        json_path.write_text(buildings_json(entries, level))
        written.append(json_path)
        tileset = (
            project / "tilesets" / ("iso_ground" + {"1x": "", "2x": "_2x", "half": "_half"}[level] + ".owltileset")
        )
        tileset.write_text(tileset_text(level, f"nam:textures/iso/tiles{SUFFIX[level]}.png"))
        written.append(tileset)
    for spec in BUILDINGS + PROPS:
        written += _save(render_object(spec, seed, 1).layers, folder, spec.name, "1x")
    written += _save(smoke(1, seed), folder, "smoke", "1x")
    birds_path = folder / "birds.png"
    birds().save(birds_path, optimize=True)
    written.append(birds_path)
    written += _save(build_vehicles(seed), folder, "vehicles", "1x")
    if tests is not None:
        written.append(write_map(tests / "render_tests", seed))
        written.append(write_town_map(tests / "render_tests", seed))
        written.append(write_town_map(tests / "scenarios", seed))
    written.append(write_city_map(project, seed))
    written.append(write_city_scene(project, seed))
    return written


def depth_of(value: int) -> float:
    """
    Decode a depth value.

    :param value: Byte of the depth atlas.
    :return: The world height it stands for.
    """
    return value / 255.0 * Z_RANGE


def decode_normal(r: int, g: int, b: int) -> Vec3:
    """
    Decode a normal-atlas pixel.

    :param r: Red byte.
    :param g: Green byte.
    :param b: Blue byte.
    :return: The world-space normal, not renormalised.
    """
    return (r / 255.0 * 2.0 - 1.0, g / 255.0 * 2.0 - 1.0, b / 255.0 * 2.0 - 1.0)
