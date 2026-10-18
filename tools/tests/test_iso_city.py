"""
Tests of the 64x64 town of the Engine Demo: its terrain, its road graph, its buildings and its scene.
"""

from __future__ import annotations

import importlib.util
import wave
from collections import deque
from pathlib import Path

import yaml

from tools.iso_atlas import city_map, city_scene
from tools.iso_atlas.terrain import SLOPES

SEED = 1
CITY = city_map.build_city(SEED)
NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def _forbidden_moves() -> set[tuple[tuple[int, int], tuple[int, int]]]:
    """The moves (from cell, to cell) the one-way streets refuse."""
    refused = set()
    for first, last in city_map.ONE_WAYS:
        cells = city_map._line(first, last)
        for before, after in zip(cells, cells[1:], strict=False):
            refused.add((after, before))
    return refused


def _reach(start: tuple[int, int], reverse: bool) -> set[tuple[int, int]]:
    refused = _forbidden_moves()
    seen = {start}
    todo = deque([start])
    while todo:
        x, y = todo.popleft()
        for dx, dy in NEIGHBOURS:
            cell = (x + dx, y + dy)
            if cell not in CITY.roads or cell in seen:
                continue
            move = (cell, (x, y)) if reverse else ((x, y), cell)
            if move in refused:
                continue
            seen.add(cell)
            todo.append(cell)
    return seen


def test_city_terrain_is_smooth_and_every_cell_has_a_slope() -> None:
    row = city_map.SIZE + 1
    heights = CITY.heights
    assert len(heights) == row * row
    for y in range(row):
        for x in range(row):
            if x + 1 < row:
                assert abs(heights[y * row + x] - heights[y * row + x + 1]) <= 1
            if y + 1 < row:
                assert abs(heights[y * row + x] - heights[(y + 1) * row + x]) <= 1
    for y in range(city_map.SIZE):
        for x in range(city_map.SIZE):
            assert 0 <= city_map.slope_of(heights, x, y) < len(SLOPES)
    assert max(heights) >= 4, "the hills rise"


def test_city_roads_are_not_steep_and_water_is_flat() -> None:
    assert all(city_map.slope_of(CITY.heights, x, y) < 15 for x, y in CITY.roads)
    assert all(city_map.is_flat(CITY.heights, x, y) for x, y in CITY.water)


def test_city_road_graph_is_connected_both_ways_despite_the_one_way_streets() -> None:
    start = min(CITY.roads)
    assert _reach(start, reverse=False) == CITY.roads
    assert _reach(start, reverse=True) == CITY.roads


def test_city_has_dead_ends_junctions_bridges_and_a_level_crossing() -> None:
    masks = {cell: city_map.road_mask(CITY.roads, *cell) for cell in CITY.roads}
    dead_ends = [cell for cell, mask in masks.items() if mask in (1, 2, 4, 8)]
    assert len(dead_ends) >= 3
    assert sum(1 for mask in masks.values() if mask == 15) >= 6, "crossings"
    assert any(cell in CITY.water for cell in CITY.roads), "a bridge"
    assert any((x, city_map.RAIL_ROW) in CITY.roads for x in range(city_map.SIZE)), "a level crossing"
    assert any(tile == 10 for tile in CITY.overlay.values()), "the railway"


def test_city_exceptions_lie_on_roads() -> None:
    cells = [cell for cell, _ in city_map.navigation_overrides()]
    assert cells and all(cell in CITY.roads for cell in cells)
    costs = [int(str(entry["cost"])) for _, entry in city_map.navigation_overrides() if "cost" in entry]
    assert costs and min(costs) > 1
    assert any("noEnter" in entry for _, entry in city_map.navigation_overrides())


def test_city_buildings_stand_on_free_flat_cells() -> None:
    seen: set[tuple[int, int]] = set()
    assert len(CITY.plots) > 120
    for plot in CITY.plots:
        cells = city_map._footprint_cells(plot.kind, *plot.cell)
        for x, y in cells:
            assert 0 <= x < city_map.SIZE and 0 <= y < city_map.SIZE
            assert (x, y) not in CITY.roads | CITY.water
            assert (x, y) not in seen
            seen.add((x, y))
            assert city_map.is_flat(CITY.heights, x, y)
        assert len({city_map.cell_corners(CITY.heights, x, y)[0] for x, y in cells}) == 1


def test_city_trees_chimneys_and_lamps_stand_on_free_ground() -> None:
    blocked = set(CITY.roads) | CITY.water
    for plot in CITY.plots:
        blocked.update(city_map._footprint_cells(plot.kind, *plot.cell))
    assert len(CITY.trees) > 150
    for _, x, y in CITY.trees:
        assert (int(x), int(y)) not in blocked
    assert len(CITY.chimneys) >= 5
    assert len(CITY.lamps) >= 8
    for x, y in CITY.lamps:
        assert (int(x), int(y)) not in blocked


def test_city_map_file_is_deterministic_and_checksummed() -> None:
    data = city_map.map_bytes(SEED)
    assert data == city_map.map_bytes(SEED)
    assert data.startswith(b"Tilemap: iso_town\n")
    header, payload = data.split(b"...\n", 1)
    assert f"payloadBytes: {len(payload)}".encode() in header
    assert b"width: 64\nheight: 64" in header
    assert payload.startswith(b"OWLTCHNK")


def test_city_fleet_starts_on_distinct_straight_roads() -> None:
    fleet = city_scene.vehicles(CITY, SEED)
    assert len(fleet) == city_scene.FLEET_SIZE
    starts = [vehicle.start for vehicle in fleet]
    assert len(set(starts)) == len(starts)
    assert all(start in CITY.roads for start in starts)
    assert {vehicle.model for vehicle in fleet} == {0, 8}


def test_city_scene_is_valid_yaml_with_its_life_and_sounds() -> None:
    text = city_scene.scene_text(SEED)
    document = yaml.safe_load(text)
    entities = document["Entities"]
    ids = [entity["Entity"] for entity in entities]
    assert len(ids) == len(set(ids))
    tags = {entity["Tag"]["tag"] for entity in entities}
    assert {"MainCamera", "Ground", "Manager", "Traffic", "Life", "Ambience", "Vehicle12", "Bird_7", "Tint_5"} <= tags
    sources = [entity for entity in entities if "SoundSource" in entity]
    assert len(sources) >= city_scene.FLEET_SIZE + 3
    assert all(entity["SoundSource"]["soundAsset"].startswith("sounds/town/") for entity in sources)
    ground = next(entity for entity in entities if entity["Tag"]["tag"] == "Ground")
    assert len(ground["Tilemap"]["navigation"]["overrides"]) > 50
    assert text == city_scene.scene_text(SEED)


def test_town_sounds_are_mono_16_bit_and_the_loops_close(tmp_path: Path) -> None:
    path = Path(__file__).resolve().parents[2] / "tools" / "demo" / "generate_town_sounds.py"
    spec = importlib.util.spec_from_file_location("generate_town_sounds", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    setattr(module, "OUT", tmp_path)  # noqa: B010 (the module is loaded by path, its attribute is not typed)
    module.main()
    names = {file.name for file in tmp_path.glob("*.wav")}
    assert {"engine.wav", "ambience.wav", "waves.wav", "yard.wav", "horn.wav", "cash.wav", "bell.wav"} <= names
    for file in tmp_path.glob("*.wav"):
        with wave.open(str(file)) as sound:
            assert (sound.getnchannels(), sound.getsampwidth(), sound.getframerate()) == (1, 2, 22050)
            assert 0.05 < sound.getnframes() / sound.getframerate() < 12
    with wave.open(str(tmp_path / "engine.wav")) as engine:
        assert engine.getnframes() == 22050, "one second, so that the 55 Hz fundamental closes the loop"
