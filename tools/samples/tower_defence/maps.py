"""
The levels of the Tower defence sample game, as data: a layout grid and a height grid of the same size, the waves and
the values of the level. ``generate_maps`` turns each one into a scene of 3D tiles; a new level is a new entry of
``LEVELS`` (or a scene duplicated and edited in Owl Nest).

Layout cells (one character each, rows from the north, the top of the screen at the start):

- ``.`` grass, ``T`` a tree, ``r`` rocks, ``h`` a hillock;
- ``#`` road, ``/`` a road ramp (it climbs toward its higher road neighbour), ``=`` a bridge (road over the river);
- ``~`` river, ``G`` a gate the Horde comes out of (a first road cell; two gates make roads A and B), ``K`` the keep
  (its cells), ``P`` a pad.

Heights: one digit per cell, the number of steps of ``HEIGHT_STEP`` the ground stands above the plain. A ramp cell
takes the lower height of its two road neighbours; a pad on a higher step gives its tower more range.

A wave is a list of groups ``"kind count gap [road]"`` separated by ``;``: ``count`` enemies of ``kind`` (``goblin``,
``grunt``, ``ogre``, ``wolf``, ``troll``, ``ogre_mage``, ``sapper``, ``chieftain``) one every ``gap`` seconds, the
groups one after the other, on the road ``a`` or ``b`` of a level with two gates (both in turn when it is left out).

A level has a ``theme``: ``grass`` (the green tiles of the kit), ``snow`` (its snow tiles) or ``ash`` (the snow tiles
darkened to ash, dead pines).
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
    theme: str = "grass"
    """Look of the tiles: ``grass``, ``snow`` or ``ash``."""

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


def routes(level: LevelData) -> list[list[Cell]]:
    """The road of each gate (row by row from the north-west: route A, then B), the shortest walk over the road cells
    from the gate to a road cell beside the keep; two roads may merge on the way."""
    gates = level.cells("G")
    if not gates:
        raise ValueError(f"level {level.number}: no gate")
    keep = set(level.cells("K"))
    out = []
    for gate in gates:
        previous: dict[Cell, Cell | None] = {gate: None}
        queue = [gate]
        end: Cell | None = None
        while queue:
            cell = queue.pop(0)
            x, z = cell
            if cell != gate and any(n in keep for n in ((x, z - 1), (x + 1, z), (x, z + 1), (x - 1, z))):
                end = cell
                break
            for n in ((x, z - 1), (x + 1, z), (x, z + 1), (x - 1, z)):
                if level.at(n) in ROAD and n not in previous:
                    previous[n] = cell
                    queue.append(n)
        if end is None:
            raise ValueError(f"level {level.number}: no road from the gate {gate} to the keep")
        path: list[Cell] = []
        step: Cell | None = end
        while step is not None:
            path.append(step)
            step = previous[step]
        out.append(path[::-1])
    return out


def route(level: LevelData) -> list[Cell]:
    """The road of the first gate."""
    return routes(level)[0]


def road_cells(level: LevelData) -> set[Cell]:
    """Every cell of every road."""
    return {cell for path in routes(level) for cell in path}


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
        "ogre 3 2.6; grunt 8 0.9",
        "goblin 16 0.4; grunt 10 0.8; ogre 5 2.2",
    ),
    night_from=8,
    start_gold=220,
)

THORNWOOD_PASS = LevelData(
    number=2,
    name="Thornwood Pass",
    layout=(
        "....P...TT...T...T..",
        ".G#####......TTT.T..",
        "...P..#..TT.r...T...",
        ".....P#.P...T.....r.",
        "......#/###.....h..T",
        ".T.T....P.#P....TT..",
        ".T...T.T.P#.TT.T..TT",
        "...T.T.T..#.T.TT.TT.",
        ".TrTT...T.#PP.P.PKKK",
        ".T........###/#/#KKK",
        ".T.Th..T.P...P..#KKK",
        "..T.T..T.TTTTT.P.KKK",
        "....T.T....T..TT.KKK",
        "T.TTTTT..T.T......T.",
    ),
    heights=(
        "11111110000000000000",
        "11111110000000000000",
        "11111110000000000000",
        "11111110000000000000",
        "11111110000000000000",
        "11111110000000000000",
        "11111110000000000000",
        "00000000000000122222",
        "00000000000000122222",
        "00000000000000112222",
        "00000000000000122222",
        "00000000000000122222",
        "00000000000000122222",
        "00000000000000122222",
    ),
    waves=(
        "goblin 10 0.7",
        "wolf 6 0.9",
        "grunt 6 1.1; goblin 6 0.6",
        "wolf 10 0.6",
        "grunt 8 1.0; ogre 1 2",
        "goblin 14 0.45; wolf 6 0.7",
        "ogre 3 2.5; grunt 6 0.9",
        "wolf 14 0.5",
        "grunt 10 0.8; ogre 3 2.2",
        "wolf 12 0.5; ogre 4 2.0; grunt 10 0.7",
    ),
    night_from=8,
    start_gold=260,
    start_hour=13.0,
)

STONEBRIDGE = LevelData(
    number=3,
    name="Stonebridge",
    layout=(
        ".....TT...~~.....T..",
        "...P....h.~~.rT.....",
        "G####.....~~..TTT...",
        "..P.#.P...~~........",
        ".T..#.....~~T...TKKK",
        "..T.#...T.~~..P..KKK",
        "T.TP#..P..~~P..##KKK",
        "....######==####.KKK",
        ".r...####.~~.P..PKKK",
        "T.TT.#P..P~~..PTTKKK",
        "....P#.P..~~........",
        ".....#....~~...T..T.",
        "G#####..T.~~.....Th.",
        "...P..P...~~........",
        ".T......T.~~.T.....T",
    ),
    heights=(
        "00000000000000000000",
        "00000011100000000000",
        "00000011100000000000",
        "00000011100000000000",
        "00000011100000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000001111000",
        "00000000000001111000",
        "00000000000001111000",
        "00000000000001111000",
        "00000000000001111000",
        "00000000000001111000",
    ),
    waves=(
        "goblin 8 0.8 a; goblin 8 0.8 b",
        "grunt 5 1.2 a; wolf 5 0.9 b",
        "troll 4 1.6",
        "wolf 8 0.6 a; grunt 6 1.0 b",
        "troll 6 1.2; goblin 10 0.5",
        "ogre 2 2.5 a; ogre 2 2.5 b; grunt 8 0.9",
        "wolf 10 0.5 b; troll 6 1.0 a",
        "grunt 12 0.7; ogre 3 2.0",
        "troll 10 0.9; wolf 10 0.5",
        "ogre 6 1.6; troll 8 0.8; grunt 12 0.6",
    ),
    night_from=9,
    start_gold=300,
    start_hour=8.0,
)

FROSTMERE = LevelData(
    number=4,
    name="Frostmere",
    layout=(
        "TT.T..T..T.~~..TT.TT",
        ".G#####....~~.T....T",
        "...P..#..P.~~.......",
        ".T..P.#.r..~~..P....",
        "....../####==##..KKK",
        ".T...P....P~~.#P.KKK",
        "...........~~P#..KKK",
        "..T..P...P.~~####KKK",
        ".T.........~~#...KKK",
        "....P######==#...KKK",
        "...P.#..T..~~..T....",
        ".T...#.P...~~.....T.",
        "..P..#.....~~.......",
        ".G####..T..~~..T..T.",
        "T..T..T.TT.~~T..TT.T",
    ),
    heights=(
        "11111111110000111111",
        "11111111110000111111",
        "11111111110000111111",
        "11111111110000111111",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000011110000000000",
        "00000011110000000000",
        "00000011110000000000",
        "00000000000000000000",
        "00000000000000000000",
    ),
    waves=(
        "goblin 10 0.7 a; wolf 6 0.8 b",
        "grunt 6 1.1; sapper 3 1.5 b",
        "ogre_mage 1 2; grunt 8 0.9",
        "wolf 10 0.5 a; troll 4 1.2 b",
        "sapper 6 0.9; goblin 12 0.45",
        "ogre_mage 2 2.5; ogre 2 2.5; grunt 8 0.8",
        "troll 8 0.9 a; wolf 10 0.5 b",
        "ogre_mage 3 2; troll 6 1.0; sapper 6 0.8",
        "ogre 4 2.0; grunt 12 0.6; wolf 10 0.5",
        "ogre_mage 4 1.8; ogre 4 2.0; troll 8 0.8; sapper 8 0.7",
    ),
    night_from=7,
    start_gold=320,
    start_hour=14.5,
    theme="snow",
)

THE_BLACK_GATE = LevelData(
    number=5,
    name="The Black Gate",
    layout=(
        "rr.T..r...T..r..T.rr",
        ".G######...r.P...r..",
        "..P....#..P.####....",
        ".r..P..#.r..#..#....",
        "...r...######.P#.KKK",
        ".T....P..P..r..#.KKK",
        "..r.......T...P#.KKK",
        ".T........r..P.##KKK",
        "...T..T...P#####.KKK",
        ".r...P.....#..P..KKK",
        "..T...######..T.....",
        ".r..P.#.P..r...T..r.",
        "..P...#....T........",
        ".G#####..r....r..T..",
        "rr.T...r..T...rr..rr",
    ),
    heights=(
        "00000000000002222222",
        "00000000000002222222",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000111111000000000",
        "00000111111000000000",
        "00000111111000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000001111000000000",
        "00000001111000000000",
        "00000000000000000000",
        "00000000000000000000",
    ),
    waves=(
        "grunt 8 1.0 a; goblin 10 0.6 b",
        "wolf 10 0.5; sapper 4 1.2 a",
        "troll 6 1.1; ogre_mage 1 2",
        "ogre 3 2.4 a; grunt 10 0.8 b",
        "sapper 8 0.8; wolf 12 0.45",
        "ogre_mage 2 2.2; troll 8 0.9; grunt 8 0.8",
        "ogre 4 2.0; ogre_mage 2 2.0 b; wolf 12 0.45 a",
        "troll 12 0.7; sapper 8 0.7",
        "grunt 16 0.5; ogre 5 1.8",
        "ogre_mage 4 1.6; ogre 5 1.8; troll 10 0.7",
        "wolf 16 0.4; sapper 10 0.6; grunt 12 0.6",
        "chieftain 1 1 a; ogre_mage 3 2.0; ogre 6 1.6; troll 10 0.7",
    ),
    night_from=6,
    start_gold=360,
    start_hour=16.5,
    theme="ash",
)

LEVELS = [GREENMARCH_FORD, THORNWOOD_PASS, STONEBRIDGE, FROSTMERE, THE_BLACK_GATE]
"""Every level, in the order of the campaign."""
