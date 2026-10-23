"""
Tests of the Tower defence maps and of their tilemap writer: every map passes the generator's checks, its road is one
connected walk along the axes whose tiles connect to their neighbours, every pad is off the road and every wave names
known enemies. The written files are not compared with the generator: they are edited in Owl Nest afterwards.
"""

from __future__ import annotations

import re

import pytest

from tools.samples.tower_defence import generate_maps as gen
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence.generate_assets import ENEMIES

WAVE_GROUP = re.compile(r"^\s*([a-z]+)\s+(\d+)\s+([\d.]+)\s*$")


@pytest.mark.parametrize("data", mp.MAPS, ids=lambda data: f"map_{data.number:02d}")
def test_every_map_is_valid(data: mp.MapData) -> None:
    gen.check(data)
    road = mp.road_cells(data)
    assert len(road) == len(set(road)), "the road crosses itself"
    for (x0, y0), (x1, y1) in zip(road, road[1:], strict=False):
        assert abs(x1 - x0) + abs(y1 - y0) == 1
    assert road[0][0] in (0, data.size - 1) or road[0][1] in (0, data.size - 1), "the gate is not on an edge"
    assert len(data.pads) >= 10


@pytest.mark.parametrize("data", mp.MAPS, ids=lambda data: f"map_{data.number:02d}")
def test_every_wave_names_known_enemies(data: mp.MapData) -> None:
    assert len(data.waves) >= 1
    for wave in data.waves:
        for group in wave.split(";"):
            match = WAVE_GROUP.match(group)
            assert match is not None, group
            assert match.group(1) in ENEMIES
            assert int(match.group(2)) > 0


def test_the_written_tilemap_holds_the_road_and_the_pads() -> None:
    data = mp.MAPS[0]
    blob = gen.tilemap(data)
    header, payload = blob.split(b"...\n", 1)
    assert b"width: 24" in header and b"- name: roads" in header
    assert payload.startswith(b"OWLTCHNK")
    _, overlay = gen.layers(data)
    assert {cell for cell, tile in overlay.items() if tile == gen.PAVEMENT} == set(data.pads)
    assert len(overlay) == len(mp.road_cells(data)) + len(data.pads)
