"""
Tests of the Tower defence levels and of their scene writer: every level passes the generator's checks, its road is
one walk along the axes from the gate to the keep, its ramps climb, every pad is beside the road and every wave names
known enemies, each level after the first bringing a new one; the road tiles turn to their neighbours, the theme of a
level picks models that exist and the waypoints follow the road heights; the world map lists every level. The written
files are not compared with the generator: they are edited in Owl Nest afterwards.
"""

from __future__ import annotations

import re

import pytest

from tools.samples.tower_defence import creatures
from tools.samples.tower_defence import generate_maps as gen
from tools.samples.tower_defence import generate_menus as menus
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence import world_map_art as art

WAVE_GROUP = re.compile(r"^\s*([a-z_]+)\s+(\d+)\s+([\d.]+)\s*([ab]?)\s*$")


@pytest.mark.parametrize("level", mp.LEVELS, ids=lambda level: f"level_{level.number:02d}")
def test_every_level_is_valid(level: mp.LevelData) -> None:
    roads = gen.check(level)
    assert len(roads) == len(level.cells("G"))
    for road in roads:
        assert len(road) == len(set(road)), "the road crosses itself"
        for (x0, z0), (x1, z1) in zip(road, road[1:], strict=False):
            assert abs(x1 - x0) + abs(z1 - z0) == 1
        for a, b in zip(road, road[1:], strict=False):
            assert abs(gen.road_height(level, a) - gen.road_height(level, b)) <= mp.HEIGHT_STEP + 1e-6, (a, b)
    assert len(level.cells("P")) >= 10
    assert all(1 <= night <= len(level.waves) for night in level.nights)
    assert level.nights and level.nights[-1] == len(level.waves), "the last wave is fought at night"


@pytest.mark.parametrize("level", mp.LEVELS, ids=lambda level: f"level_{level.number:02d}")
def test_every_wave_names_known_enemies(level: mp.LevelData) -> None:
    assert len(level.waves) >= 1
    for wave in level.waves:
        for group in wave.split(";"):
            match = WAVE_GROUP.match(group)
            assert match is not None, group
            assert match.group(1) in creatures.CREATURES
            assert match.group(4) == "" or len(level.cells("G")) == 2, group
            assert int(match.group(2)) > 0


def test_road_tiles_turn_to_their_neighbours() -> None:
    assert gen.road_tile(gen.N | gen.S) == ("tile-straight", 0)
    assert gen.road_tile(gen.E | gen.W) == ("tile-straight", 1)
    name, quarters = gen.road_tile(gen.N | gen.E)
    assert name == "tile-corner-square"
    assert gen.turned(gen.E | gen.S, quarters) == gen.N | gen.E
    assert gen.turned(gen.S, 1) == gen.E


def test_the_scene_has_its_waypoints_pads_and_towers() -> None:
    level = mp.LEVELS[0]
    text = gen.level_scene(level)
    assert text.count("tag: Pad\n") == len(level.cells("P"))
    assert "tag: Waypoint 01\n" in text
    assert "scripts/game/defence.lua" in text
    prefabs = gen.prefabs()
    for kind in ("archer", "cannon", "frost"):
        for step in ("1", "2", "3"):
            assert f"tower_{kind}_{step}" in prefabs
    assert all(f"enemy_{kind}" in prefabs for kind in creatures.CREATURES)


def test_two_gates_make_two_roads_meeting_before_the_keep() -> None:
    level = mp.LEVELS[2]
    roads = mp.routes(level)
    assert len(roads) == 2
    assert roads[0][0] != roads[1][0]
    assert roads[0][-1] == roads[1][-1]
    assert "tag: Waypoint B 01" in gen.level_scene(level)


def test_the_horde_is_introduced_across_the_campaign() -> None:
    bringing = [level.number for level in mp.LEVELS if gen.new_enemies(level)]
    assert len(bringing) >= 6
    assert set(creatures.CREATURES) == {kind for level in mp.LEVELS for kind in gen.new_enemies(level)}
    assert [level.number for level in mp.LEVELS] == list(range(1, len(mp.LEVELS) + 1))


def test_the_level_modifiers_and_effects_exist_in_the_rules() -> None:
    game = gen.ROOT / "scripts" / "game"
    rules = "".join(path.read_text() for path in [game / "defence.lua", *sorted((game / "td").glob("*.lua"))])
    for level in mp.LEVELS:
        for key in level.modifiers:
            assert f"\n    {key} = {{" in rules, key
            assert key in menus.MODIFIER_NAMES
    for kind in creatures.CREATURES:
        assert f'"{kind} day ' in rules and f'"{kind} night ' in rules, kind
    for kind in ("archer", "cannon", "frost"):
        assert f'"{kind} day ' in rules and f'"{kind} night ' in rules, kind


@pytest.mark.parametrize("level", mp.LEVELS, ids=lambda level: f"level_{level.number:02d}")
def test_the_theme_picks_existing_tile_models(level: mp.LevelData) -> None:
    text = gen.level_scene(level)
    assert level.theme in gen.THEMES
    models = {line.split("mesh: ", 1)[1] for line in text.splitlines() if "mesh: " in line}
    missing = sorted(model for model in models if not (gen.ROOT / model).is_file())
    assert not missing
    if level.theme in ("snow", "dry", "ash"):
        assert any("/snow-" in model for model in models)


def test_the_world_map_lists_every_level_with_its_scene_and_road() -> None:
    text = menus.world_map()
    assert len(art.SITES) == len(mp.LEVELS) == len(menus.CAMPAIGN)
    for level in mp.LEVELS:
        assert f"scenes/level_{level.number:02d}.owl" in text
    for index in range(1, len(art.SITES) + 1):
        assert f"tag: PathDot {index}\n" in text
    assert text.count("tag: SiteFlag\n") == len(art.SITES)
