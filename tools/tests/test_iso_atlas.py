"""
Tests of the isometric atlas generator: determinism, a stable pixel hash, atlas layout and slice coverage.
"""

from __future__ import annotations

import hashlib
import json
import math
from collections.abc import Callable
from pathlib import Path

import pytest
from PIL import Image, ImageStat

from tools.iso_atlas import atlas, town_map
from tools.iso_atlas import terrain as terrain_module
from tools.iso_atlas.common import TILE_H, TILE_W, Layers
from tools.iso_atlas.objects import (
    BUILDINGS,
    PROPS,
    SLICE_HEIGHT,
    VEHICLE_HEADINGS,
    VEHICLE_MODELS,
    VEHICLES,
    render_object,
    slice_object,
)
from tools.iso_atlas.terrain import SLOPE_COUNT, SLOPES

SEED = 1


def _px(image: Image.Image, xy: tuple[int, int]) -> tuple[int, ...]:
    value = image.getpixel(xy)
    assert isinstance(value, tuple)
    return value


def _opaque(image: Image.Image) -> int:
    return int(ImageStat.Stat(image.getchannel("A").point(lambda v: 1 if v else 0)).sum[0])


def _digest(layers: Layers) -> str:
    sha = hashlib.sha256()
    for image in (layers.albedo, layers.normal, layers.depth):
        sha.update(image.tobytes())
    return sha.hexdigest()


@pytest.fixture(scope="module")
def terrain() -> Layers:
    return atlas.build_terrain(SEED, 1)


def test_terrain_hash_is_stable(terrain: Layers) -> None:
    assert _digest(terrain) == "18b9d851cf936444b4c4c22a9076ac67701cc0b871626af11fe9b09396d403ea"


def test_same_seed_same_pixels(terrain: Layers) -> None:
    assert _digest(atlas.build_terrain(SEED, 1)) == _digest(terrain)


def test_other_seed_other_pixels(terrain: Layers) -> None:
    assert _digest(atlas.build_terrain(SEED + 1, 1)) != _digest(terrain)


def test_atlas_size_and_layers(terrain: Layers) -> None:
    assert terrain.albedo.size == (atlas.COLUMNS * TILE_W, atlas.ROWS * TILE_H)
    assert terrain.normal.size == terrain.albedo.size == terrain.depth.size
    assert atlas.TILE_COUNT <= atlas.COLUMNS * atlas.ROWS


def test_every_tile_is_drawn(terrain: Layers) -> None:
    for index in range(atlas.TILE_COUNT):
        x, y = (index % atlas.COLUMNS) * TILE_W, (index // atlas.COLUMNS) * TILE_H
        box = (x, y, x + TILE_W, y + TILE_H)
        assert terrain.albedo.crop(box).getchannel("A").getbbox() is not None, index


def test_flat_tile_normal_points_up_and_depth_is_zero(terrain: Layers) -> None:
    pixel = (TILE_W // 2, TILE_H - 8)
    assert _px(terrain.normal, pixel)[:3] == (128, 128, 255)
    assert _px(terrain.depth, pixel)[0] == 0


def test_slope_tile_is_tilted_and_raised(terrain: Layers) -> None:
    index = atlas.tile_index(0, 1)
    x, y = (index % atlas.COLUMNS) * TILE_W, (index // atlas.COLUMNS) * TILE_H
    tile = terrain.normal.crop((x, y, x + TILE_W, y + TILE_H))
    assert _px(tile, (32, 20))[:3] != (128, 128, 255)
    assert terrain.depth.crop((x, y, x + TILE_W, y + TILE_H)).getchannel("R").getbbox() is not None


def test_tileset_tables_cover_every_slope() -> None:
    text = atlas.tileset_text("1x", "nam:textures/iso/tiles.png")
    assert "kind: Isometric" in text
    assert f"columns: {atlas.COLUMNS}" in text and f"rows: {atlas.ROWS}" in text
    tables = [line for line in text.splitlines() if line.strip().startswith("tiles: [")]
    assert len(tables) == len(atlas.SLOPED_TERRAINS)
    for line in tables:
        indices = [int(v) for v in line.split("[")[1].rstrip("]").split(",")]
        assert len(indices) == SLOPE_COUNT
        assert all(0 <= i < atlas.TILE_COUNT for i in indices)
    foundations = [line for line in text.splitlines() if "foundations:" in line]
    assert len(foundations) == len(atlas.FOUNDATION_TOPS)
    assert all(len(line.split("[")[1].split(",")) == SLOPE_COUNT for line in foundations)


def test_slope_count_matches_the_engine() -> None:
    assert SLOPE_COUNT == 19
    assert len(set(SLOPES)) == 19


def test_half_level_is_a_mip(terrain: Layers) -> None:
    assert terrain.half().albedo.size == (terrain.width // 2, terrain.height // 2)


def test_building_slices_cover_the_whole_image() -> None:
    spec = next(b for b in BUILDINGS if b.name == "townhouse")
    rendered = render_object(spec, SEED, 1)
    slices = slice_object(spec, rendered)
    assert sorted(slices) == [(0, 0), (1, 0)]
    covered = sum(_opaque(s.albedo) for s, _ in slices.values())
    whole = len(rendered.cells)
    assert 0.95 * whole <= covered <= whole
    assert all(s.albedo.size == (TILE_W, SLICE_HEIGHT) for s, _ in slices.values())
    assert all(zmax > 0 for _, zmax in slices.values())


def test_objects_fit_their_canvas() -> None:
    for spec in BUILDINGS + PROPS:
        rendered = render_object(spec, SEED, 1)
        assert rendered.layers.albedo.getchannel("A").getbbox() is not None, spec.name


def _vehicle_cell(sheet: Layers, model: int, heading: int) -> Image.Image:
    left, top = heading * TILE_W, model * atlas.VEHICLE_HEIGHT
    return sheet.albedo.crop((left, top, left + TILE_W, top + atlas.VEHICLE_HEIGHT))


def test_vehicle_sheet_draws_every_heading_of_every_model() -> None:
    sheet = atlas.build_vehicles(SEED)
    assert sheet.albedo.size == (VEHICLE_HEADINGS * TILE_W, len(VEHICLE_MODELS) * atlas.VEHICLE_HEIGHT)
    assert len(VEHICLES) == VEHICLE_HEADINGS * len(VEHICLE_MODELS)
    for model in range(len(VEHICLE_MODELS)):
        cells = [_vehicle_cell(sheet, model, heading) for heading in range(VEHICLE_HEADINGS)]
        for heading, cell in enumerate(cells):
            box = cell.getchannel("A").getbbox()
            assert box is not None, (model, heading)
            assert box[0] >= 4 and box[2] <= TILE_W - 4, "the vehicle stays over its tile"
            assert box[3] <= atlas.VEHICLE_HEIGHT - 4, "the vehicle is centred on its tile, above the front corner"
        assert len({cell.tobytes() for cell in cells}) == VEHICLE_HEADINGS, "every heading has its own picture"
    assert _digest(atlas.build_vehicles(SEED)) == _digest(sheet)


def test_vehicle_headings_follow_the_ground_axes_on_screen() -> None:
    # The truck has its cab (dark glass) at the front and a white cargo box behind: the cab sits further along the
    # heading than the cargo. +x runs to the lower right of the screen and +y to the lower left.
    sheet = atlas.build_vehicles(SEED)
    truck = next(i for i, model in enumerate(VEHICLE_MODELS) if model[4] is not None)

    def centroid(heading: int, wanted: Callable[[int, int, int], bool]) -> tuple[float, float]:
        cell = _vehicle_cell(sheet, truck, heading).convert("RGBA")
        points = [
            (x, y)
            for x in range(TILE_W)
            for y in range(atlas.VEHICLE_HEIGHT)
            if cell.getpixel((x, y))[3] > 0 and wanted(*cell.getpixel((x, y))[:3])
        ]
        assert points, heading
        return sum(p[0] for p in points) / len(points), sum(p[1] for p in points) / len(points)

    for heading in range(VEHICLE_HEADINGS):
        cargo = centroid(heading, lambda r, g, b: min(r, g, b) > 190)
        cab = centroid(heading, lambda r, g, b: b > r + 20 and r + g + b < 270)
        angle = math.radians(45.0 * heading)
        across = math.cos(angle) - math.sin(angle)
        # Only the screen column tells the front: the cargo box is taller, which confuses the rows.
        if abs(across) > 0.1:
            assert (cab[0] - cargo[0]) * across > 0.0, heading


def test_generate_writes_the_expected_files(tmp_path: Path) -> None:
    written = atlas.generate(tmp_path, SEED, ("1x",))
    names = {p.relative_to(tmp_path).as_posix() for p in written}
    assert "textures/iso/tiles.png" in names
    assert "textures/iso/tiles_normal.png" in names
    assert "textures/iso/tiles_depth.png" in names
    assert "tilesets/iso_ground.owltileset" in names
    assert "tilemaps/iso_test.owltilemap" in names
    assert "tilemaps/iso_town.owltilemap" in names
    assert "textures/iso/oak.png" in names
    assert "textures/iso/vehicles.png" in names
    sheet = json.loads((tmp_path / "textures/iso/buildings.json").read_text())
    assert set(sheet["buildings"]) == {b.name for b in BUILDINGS}
    assert len(sheet["buildings"]["warehouse"]["slices"]) == 4


def test_town_map_is_smooth_and_buildings_stand_on_flat_cells() -> None:
    heights = town_map.corner_heights()
    row = town_map.SIZE + 1
    for y in range(row):
        for x in range(row):
            for nx, ny in ((x + 1, y), (x, y + 1)):
                if nx < row and ny < row:
                    assert abs(heights[y * row + x] - heights[ny * row + nx]) <= 1
    used: set[tuple[int, int]] = set()
    for plot in town_map.PLOTS:
        for x, y in town_map.plot_cells(plot):
            assert 0 <= x < town_map.SIZE and 0 <= y < town_map.SIZE
            assert len(set(town_map.cell_corners(heights, x, y))) == 1, (plot, x, y)
            assert (x, y) not in used, (plot, x, y)
            used.add((x, y))
    roads = town_map.road_tiles()
    assert not used & set(roads)
    assert all(town_map.slope_of(heights, x, y) < 15 for x, y in roads)


def test_town_ring_is_closed_and_roads_follow_their_neighbours() -> None:
    ring = town_map.ring_cells()
    assert (
        len(ring) == len(set(ring)) == 2 * (town_map.RING[2] - town_map.RING[0] + town_map.RING[3] - town_map.RING[1])
    )
    for (x, y), (nx, ny) in zip(ring, ring[1:] + ring[:1], strict=True):
        assert abs(x - nx) + abs(y - ny) == 1
    roads = town_map.road_tiles()
    assert roads[(town_map.RING[0] + 2, town_map.RING[1])] == town_map.ROAD
    assert roads[(town_map.RING[0], town_map.RING[1] + 2)] == town_map.ROAD_Y


def test_every_road_mask_has_its_own_tile() -> None:
    assert sorted(terrain_module.ROAD_MASKS) == list(range(16))
    assert len(set(terrain_module.ROAD_MASKS.values())) == 16
    for mask, tile in terrain_module.ROAD_MASKS.items():
        assert tile < atlas.TILE_COUNT
        assert tile in atlas.SLOPED_TERRAINS, mask


def test_town_junctions_are_tiled_from_the_neighbours() -> None:
    roads = town_map.road_tiles()
    x0, y0, x1, y1 = town_map.RING
    masks = terrain_module.ROAD_MASKS
    assert roads[(x0, y0)] == masks[terrain_module.EAST | terrain_module.SOUTH]
    assert roads[(x1, y1)] == masks[terrain_module.NORTH | terrain_module.WEST]
    assert roads[(x0, 12)] == masks[terrain_module.NORTH | terrain_module.SOUTH | terrain_module.WEST]
    assert roads[(2, 12)] == masks[terrain_module.EAST]
    assert roads[(11, y1)] == masks[terrain_module.EAST | terrain_module.SOUTH | terrain_module.WEST]


def test_town_trees_stand_on_free_grass() -> None:
    blocked = set(town_map.road_tiles())
    for plot in town_map.PLOTS:
        blocked.update(town_map.plot_cells(plot))
    for _, x, y in town_map.TREES:
        assert 0 <= x < town_map.SHORE_COLUMN and 0 <= y < town_map.SIZE and int(y) != town_map.RAIL_ROW
        assert (int(x), int(y)) not in blocked, (x, y)


def test_town_map_file_is_deterministic() -> None:
    assert town_map.map_bytes(SEED) == town_map.map_bytes(SEED)
    assert town_map.map_bytes(SEED).startswith(b"Tilemap: iso_town\n")


def _tileset_connections(text: str) -> dict[int, list[str]]:
    """Connections written for each tile of a generated ``.owltileset``, by tile index."""
    found: dict[int, list[str]] = {}
    index = -1
    for line in text.splitlines():
        stripped = line.strip().removeprefix("- ")
        if stripped.startswith("index:"):
            index = int(stripped.split(":")[1])
        elif stripped.startswith("connections:"):
            found[index] = [side.strip() for side in stripped.split("[")[1].rstrip("]").split(",") if side.strip()]
    return found


def test_tileset_writes_the_connections_of_every_road_tile() -> None:
    text = atlas.tileset_text("1x", "nam:textures/iso/tiles.png")
    found = _tileset_connections(text)
    assert sorted(found) == sorted(terrain_module.ROAD_MASKS.values())
    names = dict(terrain_module.CONNECTION_NAMES)
    for mask, tile in terrain_module.ROAD_MASKS.items():
        assert sorted(found[tile]) == sorted(name for bit, name in names.items() if mask & bit), (mask, tile)


def test_straight_roads_and_junction_shapes_join_the_expected_sides() -> None:
    found = _tileset_connections(atlas.tileset_text("1x", "nam:textures/iso/tiles.png"))
    assert sorted(found[3]) == ["east", "west"]
    assert sorted(found[11]) == ["north", "south"]
    assert found[12] == []
    assert sorted(found[25]) == ["east", "north", "south", "west"]


def test_other_terrains_are_not_part_of_the_road_graph() -> None:
    found = _tileset_connections(atlas.tileset_text("1x", "nam:textures/iso/tiles.png"))
    for terrain in terrain_module.TERRAINS:
        assert (terrain.id in found) == (terrain.connections is not None), terrain.name
