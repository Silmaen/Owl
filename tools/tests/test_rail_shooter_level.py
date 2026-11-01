"""
Tests of the Rail shooter level generator: the rail sampler, the waves in the order of their distance with the boss
last, and the field kept clear of the intro stretch. The written scene is not compared with the generator: it is
edited in Owl Nest afterwards.
"""

from __future__ import annotations

import math

from tools.samples import scene_yaml as y
from tools.samples.rail_shooter import generate_level as level


def test_the_rail_runs_through_its_points_and_its_frame_is_orthonormal() -> None:
    rail = level.Rail(level.RAIL_POINTS)
    start, _ = rail.sample(0.0)
    end, _ = rail.sample(rail.length)
    assert math.dist(start, level.RAIL_POINTS[0]) < 1e-6
    assert math.dist(end, level.RAIL_POINTS[-1]) < 1e-6
    assert rail.length > level.BOSS_DISTANCE + 600.0
    for distance in (0.0, 500.0, 2000.0, rail.length):
        point, tangent = rail.sample(distance)
        assert abs(math.hypot(*tangent) - 1.0) < 1e-3
        across = rail.place(distance, 1.0, 0.0)
        up = rail.place(distance, 0.0, 1.0)
        right = [across[c] - point[c] for c in range(3)]
        upward = [up[c] - point[c] for c in range(3)]
        assert abs(math.hypot(*right) - 1.0) < 1e-6
        assert abs(math.hypot(*upward) - 1.0) < 1e-6
        assert abs(sum(right[c] * tangent[c] for c in range(3))) < 1e-6
        assert abs(sum(upward[c] * tangent[c] for c in range(3))) < 1e-6


def test_the_waves_come_in_order_and_end_with_the_boss() -> None:
    distances = [wave.distance for wave in level.WAVES]
    assert distances == sorted(distances)
    assert [wave.kind for wave in level.WAVES].count("boss") == 1
    assert level.WAVES[-1].kind == "boss"
    assert level.WAVES[-1].distance == level.BOSS_DISTANCE
    assert all(wave.count > 0 for wave in level.WAVES)


def test_the_rocks_leave_the_intro_clear_and_stay_near_the_rail() -> None:
    rail = level.Rail(level.RAIL_POINTS)
    entities = y.Entities(0)
    level.asteroid_field(entities, rail)
    text = entities.text()
    first_belt = level.BELTS[0][0]
    assert first_belt >= 150.0
    assert "tag: Asteroid" in text
    assert "tag: FarRock" in text
    for start, end, density in level.BELTS:
        assert start < end
        assert 0.0 < density < 0.2
