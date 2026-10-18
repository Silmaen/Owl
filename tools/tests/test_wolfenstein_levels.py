"""
Tests of the Wolfenstein mini floor generator: every floor passes its check (each cell within reach, so 100 % is
possible), and the check refuses a room that only a slid secret wall leads to (the wall stops in its doorway).
"""

from __future__ import annotations

import pytest

from tools.samples.wolfenstein import generate_levels as levels


@pytest.mark.parametrize("make", levels.LEVELS)
def test_every_floor_can_be_finished_at_one_hundred_percent(make: object) -> None:
    levels.check(make())  # type: ignore[operator]


def test_a_room_behind_the_stop_of_a_secret_wall_is_refused() -> None:
    floor = levels.Level(9, "Test", "#", 0.0, (0.4, 0.4, 0.4), (0.2, 0.2, 0.2))
    floor.room(2, 10, 6, 12)
    floor.put("@", (3, 11))
    floor.put("X", (2, 9))
    floor.put("^", (4, 9))
    floor.room(4, 7, 4, 8)  # the wall slides from (4, 9) to (4, 7)
    floor.room(5, 6, 6, 7)  # only (4, 7) leads into this room
    with pytest.raises(ValueError, match="out of reach"):
        levels.check(floor)
