"""
The levels of the Bridge builder sample game: the gap of each one, its anchors, its budget and its vehicles, shared
by the level and menu generators of ``tools/samples/bridge_builder/``.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Level:
    """A level: the gap between the cliffs (the left top at x = 0, y = 0), the height of the right top, the anchors,
    the rock pillars (x, top), the budget, the budget for three stars and the vehicles that cross it."""

    name: str
    width: int
    right_top: int
    anchors: list[tuple[int, int]]
    budget: int
    star_budget: int
    vehicles: list[str]
    pillars: list[tuple[int, int]] = field(default_factory=list)
    towers: bool = False


LEVELS = [
    Level("FIRST CROSSING", 8, 0, [(0, 0), (8, 0), (0, -2), (8, -2)], 2600, 1900, ["car"]),
    Level("MILL CREEK", 10, 0, [(0, 0), (10, 0), (0, -2), (10, -2), (5, -4)], 3000, 2300, ["car", "van"], [(5, -4)]),
    Level("LONG REACH", 14, 0, [(0, 0), (14, 0), (0, -3), (14, -3)], 4800, 3700, ["van", "car"]),
    Level(
        "DEEP GORGE", 16, 0, [(0, 0), (16, 0), (0, -4), (16, -4), (0, 4), (16, 4)], 5800, 4600, ["truck"], towers=True
    ),
    Level("THE STEP", 14, -3, [(0, 0), (0, -3), (14, -3), (14, -6)], 5000, 3900, ["car", "van", "truck"]),
    Level(
        "GREAT RIVER",
        20,
        0,
        [(0, 0), (20, 0), (0, -3), (20, -3), (10, -5)],
        7600,
        6000,
        ["truck", "van", "truck"],
        [(10, -5)],
    ),
]
"""The six levels, of rising span and load."""

VEHICLES = {
    "car": (2.2, 0.9, 40.0),
    "van": (2.8, 1.3, 50.0),
    "truck": (3.6, 1.6, 70.0),
}
"""Length, height and density of each vehicle."""
