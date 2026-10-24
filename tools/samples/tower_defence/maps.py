"""
The levels of the Tower defence sample game, as data: a layout grid and a height grid of the same size, the waves and
the values of the level. ``generate_maps`` turns each one into a scene of 3D tiles; a new level is a new entry of
``LEVELS`` (or a scene duplicated and edited in Owl Nest).

Layout cells (one character each, rows from the north, the top of the screen at the start):

- ``.`` grass, ``T`` a tree, ``r`` rocks, ``h`` a hillock;
- ``#`` road, ``/`` a road ramp (it climbs toward its higher road neighbour), ``=`` a bridge (road over the river);
- ``~`` river, ``G`` the gate the Horde comes out of (the first road cell), ``K`` the keep (its cells), ``P`` a pad.

Heights: one digit per cell, the number of steps of ``HEIGHT_STEP`` the ground stands above the plain. A ramp cell
takes the lower height of its two road neighbours; a pad on a higher step gives its tower more range.

A wave is a list of groups ``"kind count gap"`` separated by ``;``: ``count`` enemies of ``kind`` (``goblin``,
``grunt``, ``ogre``) one every ``gap`` seconds, the groups one after the other.
"""

from __future__ import annotations

from dataclasses import dataclass

Cell = tuple[int, int]

HEIGHT_STEP = 0.4
"""World height of one step of the height grid."""


@dataclass(frozen=True)
class LevelData:
    """One level of the campaign."""

    number: int
    """Number of the level, from 1 (``level_01``)."""
    name: str
    """Name shown on the world map, the HUD and the tally."""
    layout: tuple[str, ...]
    """Rows of layout characters, from the north."""
    heights: tuple[str, ...]
    """Rows of height digits, the same size as ``layout``."""
    waves: tuple[str, ...]
    """The waves, in order."""
    night_from: int = 0
    """First wave fought at dusk then night (0: every wave by day)."""
    start_gold: int = 240
    """Gold at the start of the level."""
    lives: int = 20
    """Lives at the start: one is lost per enemy reaching the keep."""
    start_hour: float = 9.5
    """Time of day of the first wave."""

    @property
    def width(self) -> int:
        """Columns of the grid (x, east)."""
        return len(self.layout[0])

    @property
    def depth(self) -> int:
        """Rows of the grid (z, south)."""
        return len(self.layout)

    def at(self, cell: Cell) -> str:
        """The layout character of a cell, blank outside the grid."""
        x, z = cell
        if 0 <= z < self.depth and 0 <= x < self.width:
            return self.layout[z][x]
        return " "

    def height(self, cell: Cell) -> int:
        """The height steps of a cell (0 outside the grid)."""
        x, z = cell
        if 0 <= z < self.depth and 0 <= x < self.width:
            return int(self.heights[z][x])
        return 0

    def cells(self, kinds: str) -> list[Cell]:
        """Every cell whose layout character is one of ``kinds``, row by row."""
        return [(x, z) for z in range(self.depth) for x in range(self.width) if self.layout[z][x] in kinds]


ROAD = "#/=G"
"""Layout characters the enemies walk on."""


def route(level: LevelData) -> list[Cell]:
    """The road from the gate to its last cell, beside the keep (each road cell has at most two road neighbours)."""
    gates = level.cells("G")
    if len(gates) != 1:
        raise ValueError(f"level {level.number}: one gate expected, {len(gates)} found")
    path = [gates[0]]
    previous: Cell | None = None
    while True:
        x, z = path[-1]
        nexts = [
            c
            for c in ((x, z - 1), (x + 1, z), (x, z + 1), (x - 1, z))
            if level.at(c) in ROAD and c != previous and c not in path
        ]
        if not nexts:
            return path
        if len(nexts) > 1:
            raise ValueError(f"level {level.number}: the road forks at {path[-1]}")
        previous = path[-1]
        path.append(nexts[0])


GREENMARCH_FORD = LevelData(
    number=1,
    name="Greenmarch Ford",
    layout=(
        "TT.T.T..T~..T..TT.",
        "T....rT..~r..Tr..T",
        ".G##.....~.....KKK",
        "T..#P...r~...P.KKK",
        "..P#..T..~.####KKK",
        "T..#.P.h.~.#.P.KKK",
        ".../.....~P#P..KKK",
        "T../P....~.#.....T",
        "..P#...P.~P#P.....",
        ".r.######=##...r.T",
        "....P..P.~...h....",
        ".Th...T..~....T...",
        "T....T...~T..T..T.",
        "..T....T.~.T......",
    ),
    heights=(
        "222222200000000000",
        "222222200000000000",
        "222222200000000000",
        "222222200000000000",
        "222222200000000000",
        "222222200000000000",
        "111111100000000000",
        "000000000000111111",
        "000000000000111111",
        "000000000000111111",
        "000000000000111111",
        "000000000000111111",
        "000000000000111111",
        "000000000000000000",
    ),
    waves=(
        "goblin 8 0.9",
        "goblin 6 0.8; grunt 3 1.6",
        "grunt 6 1.3",
        "goblin 12 0.6; grunt 2 1.5",
        "grunt 6 1.1; ogre 1 2",
        "goblin 10 0.5; grunt 6 1.0",
        "ogre 3 3.0; grunt 6 1.0",
        "goblin 14 0.45; grunt 8 0.9; ogre 3 2.5",
    ),
    night_from=8,
)

LEVELS = [GREENMARCH_FORD]
"""Every level, in the order of the campaign."""
