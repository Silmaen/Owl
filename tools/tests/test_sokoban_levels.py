"""
Tests of the Sokoban levels and of their tilemap writer: every level is closed, has as many crates as goals and is
solved by its stored solution; a written tilemap holds the three layers of its level. The written files are not compared
with the generator: they are edited in Owl Nest afterwards.
"""

from __future__ import annotations

import struct

import pytest

from tools.samples.sokoban import generate_levels as gen
from tools.samples.sokoban import levels


@pytest.mark.parametrize("index", range(len(levels.LEVELS)))
def test_every_level_is_solved_by_its_solution(index: int) -> None:
    lvl = levels.level(index)
    assert lvl.boxes, lvl.name
    assert len(lvl.boxes) == len(lvl.goals), lvl.name
    assert lvl.width <= gen.CHUNK and lvl.height <= gen.CHUNK
    _, boxes = levels.replay(lvl, levels.SOLUTIONS[index])
    assert boxes == lvl.goals, lvl.name


def test_levels_get_harder() -> None:
    pushes = [sum(letter.isupper() for letter in moves) for moves in levels.SOLUTIONS]
    assert len(pushes) == len(levels.LEVELS) == 10
    assert pushes == sorted(pushes)


def test_replay_refuses_a_step_into_a_wall() -> None:
    with pytest.raises(ValueError, match="wall"):
        levels.replay(levels.level(0), "u" * 3)


def test_a_tilemap_holds_the_three_layers_of_its_level() -> None:
    lvl = levels.level(1)
    data = gen.tilemap("level_02", lvl)
    header, payload = data.split(b"\n...\n", 1)
    assert b"width: 8" in header and b"height: 6" in header
    assert payload[:8] == b"OWLTCHNK"
    assert struct.unpack("<I", payload[-4:])[0] == gen.fnv1a(payload[:-4])
    chunk, layer_count = struct.unpack("<II", payload[8:16])
    assert (chunk, layer_count) == (gen.CHUNK, len(gen.LAYERS))
    offset, layers = 16, []
    cells = chunk * chunk
    for _ in range(layer_count):
        (count,) = struct.unpack("<I", payload[offset : offset + 4])
        assert count == 1
        offset += 12
        layers.append(struct.unpack(f"<{cells}H", payload[offset : offset + 2 * cells]))
        offset += 5 * cells
    floor, goals, boxes = layers
    tiles = gen.TILES
    assert all(floor[y * chunk + x] in (tiles["floor"], tiles["floor_worn"]) for x, y in lvl.floor)
    assert sum(tile == tiles["goal"] for tile in goals) == len(lvl.goals)
    assert sum(tile in (tiles["box"], tiles["box_on_goal"]) for tile in boxes) == len(lvl.boxes)
    player = lvl.player
    assert boxes[player[1] * chunk + player[0]] == tiles["start"]
