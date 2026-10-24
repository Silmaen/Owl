"""
Tests of the Breakout level generator: the grids fit the field and every level holds both power-ups. The written scenes
are not compared with the generator: they are edited in Owl Nest afterwards.
"""

from __future__ import annotations

import pytest

from tools.samples.breakout import generate_levels as levels


@pytest.mark.parametrize("level", range(len(levels.LEVELS)))
def test_every_brick_sits_inside_the_field(level: int) -> None:
    bricks = levels.layout(level)
    assert bricks
    for brick in bricks:
        x, y = brick.position
        assert abs(x) + levels.BRICK_SIZE[0] / 2 <= levels.FIELD_HALF
        assert y > levels.PADDLE_Y + 4.0
        assert brick.hits in levels.POINTS


def test_every_level_drops_both_power_ups() -> None:
    for level in range(len(levels.LEVELS)):
        drops = {brick.drop for brick in levels.layout(level)}
        assert {"wide", "slow"} <= drops, f"level {level + 1}"
