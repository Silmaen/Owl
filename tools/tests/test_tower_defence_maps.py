"""
Tests of the Tower defence levels and of their scene writer: every level passes the generator's checks, its road is
one walk along the axes from the gate to the keep, its ramps climb, every pad is beside the road and every wave names
known enemies; the road tiles turn to their neighbours and the waypoints follow the road heights. The written files
are not compared with the generator: they are edited in Owl Nest afterwards.
"""

from __future__ import annotations

import re

import pytest

from tools.samples.tower_defence import creatures
from tools.samples.tower_defence import generate_maps as gen
from tools.samples.tower_defence import maps as mp

WAVE_GROUP = re.compile(r"^\s*([a-z]+)\s+(\d+)\s+([\d.]+)\s*([ab]?)\s*$")


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
    assert 0 <= level.night_from <= len(level.waves)


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
