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
``grunt``, ``ogre``, ``wolf``, ``troll``, ``ogre_mage``, ``sapper``, ``berserker``, ``chieftain``) one every ``gap``
seconds, the groups one after the other, on the road ``a`` or ``b`` of a level with two gates (both in turn when it is
left out).

A level has a ``theme``: ``grass`` (the green tiles of the kit), ``marsh`` (the green tiles darkened),
``snow`` (its snow tiles), ``dry`` (the snow tiles tinted to sand) or ``ash`` (the snow tiles darkened to ash, dead
pines); its ``nights`` (the waves fought at
night) and its ``modifiers``.
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
    nights: tuple[int, ...] = ()
    """The waves fought at night (from 1); the sky turns to dusk or dawn between two waves."""
    modifiers: tuple[str, ...] = ()
    """Keys of the level modifiers of ``defence.lua`` (``fog``, ``blizzard``, ``blood_moon``, ``drought``, ``gale``,
    ``plunder``, ``long_night``)."""
    start_gold: int = 240
    """Gold at the start of the level."""
    lives: int = 20
    """Lives at the start: one is lost per enemy reaching the keep."""
    start_hour: float = 9.5
    """Time of day of the first wave."""
    theme: str = "grass"
    """Look of the tiles: ``grass``, ``marsh``, ``dry``, ``snow`` or ``ash``."""

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
    nights=(4, 8),
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
    nights=(3, 6, 9, 10),
    modifiers=("fog",),
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
    nights=(3, 4, 7, 8, 10),
    start_gold=300,
    start_hour=8.0,
)

FROSTMERE = LevelData(
    number=5,
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
    nights=(3, 6, 9, 10),
    modifiers=("blizzard",),
    start_gold=320,
    start_hour=14.5,
    theme="snow",
)

THE_BLACK_GATE = LevelData(
    number=10,
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
        "ogre_mage 2 2.2; berserker 6 0.9; grunt 8 0.8",
        "ogre 4 2.0; ogre_mage 2 2.0 b; wolf 12 0.45 a",
        "troll 12 0.7; sapper 8 0.7",
        "grunt 16 0.5; ogre 5 1.8",
        "ogre_mage 4 1.6; ogre 5 1.8; troll 10 0.7",
        "wolf 16 0.4; sapper 10 0.6; grunt 12 0.6",
        "chieftain 1 1 a; ogre_mage 3 2.0; ogre 6 1.6; troll 10 0.7",
    ),
    nights=(4, 8, 11, 12),
    modifiers=("blood_moon",),
    start_gold=380,
    start_hour=16.5,
    theme="ash",
)

MIREFEN_MARSH = LevelData(
    number=4,
    name="Mirefen Marsh",
    layout=(
        "......~......~......",
        "T.P...~.P....~T.....",
        "G#####=####..~.....T",
        "T...P.~.P.#..~......",
        "......~...#P.~T..T..",
        "......~..P#..~.P.KKK",
        "..T...~...###=###KKK",
        "......~...TPP~T..KKK",
        "......~.T####=###KKK",
        "......~..#rPT~.P.KKK",
        ".....r~.P#...~...KKK",
        "...P..~..#...~......",
        "G#####=###..T~......",
        "T....P~.P....~.r....",
        "....T.~...T..~......",
    ),
    heights=(
        "11111100000000000000",
        "11111100000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000001100000000000",
        "00000001100000000000",
        "00000001100000111111",
        "00000000000000111111",
        "00000000000000111111",
        "00000000000000111111",
    ),
    waves=(
        "goblin 10 0.7 a; goblin 8 0.7 b",
        "grunt 6 1.0; sapper 2 1.5 a",
        "wolf 8 0.6 b; goblin 8 0.5 a",
        "sapper 5 1.0; grunt 6 0.9",
        "troll 4 1.3 a; wolf 8 0.5 b",
        "ogre 2 2.4; sapper 6 0.8",
        "grunt 10 0.7; troll 4 1.1",
        "wolf 12 0.45; sapper 6 0.7",
        "ogre 3 2.0; troll 6 1.0; grunt 8 0.7",
        "sapper 10 0.6; ogre 4 1.8; wolf 12 0.45",
    ),
    nights=(3, 6, 9, 10),
    modifiers=("fog",),
    start_gold=300,
    start_hour=10.0,
    theme="marsh",
)

HIGHPEAK_WATCH = LevelData(
    number=6,
    name="Highpeak Watch",
    layout=(
        "T.......T..........T",
        ".G#######.....r....T",
        "...P..P.#TP.....r...",
        ".T..TPT.#.r..TT..r..",
        "......../..T........",
        ".......P#.P...P.TKKK",
        "......r.#T.r...T.KKK",
        "...T..../....####KKK",
        "......P.#.P.P#rP.KKK",
        "........#..P.#...KKK",
        ".......T######...KKK",
        ".......P...P......r.",
        "...........T.T......",
        ".T.T.....Tr.T.......",
        "..............T..T..",
    ),
    heights=(
        "22222222222000222222",
        "22222222222000222222",
        "22222222222000222222",
        "22222222222000222222",
        "11111111111000000000",
        "11111111111000000000",
        "11111111111000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "11111100000000000000",
        "11111100000000000000",
        "11111100000000000000",
        "11111100000000000000",
    ),
    waves=(
        "grunt 8 0.9",
        "berserker 2 1.5; goblin 10 0.5",
        "wolf 10 0.5",
        "berserker 4 1.2; grunt 6 0.8",
        "ogre_mage 1 2; troll 5 1.1",
        "berserker 6 1.0; wolf 8 0.5",
        "ogre 3 2.2; sapper 5 0.8",
        "berserker 8 0.8; grunt 10 0.6",
        "troll 8 0.9; ogre_mage 2 2",
        "ogre 4 2.0; berserker 8 0.7",
        "berserker 10 0.6; ogre_mage 3 1.8; troll 8 0.8",
    ),
    nights=(3, 4, 7, 8, 11),
    modifiers=("gale",),
    start_gold=340,
    start_hour=11.0,
    theme="snow",
)

SUNSCORCH_FLATS = LevelData(
    number=7,
    name="Sunscorch Flats",
    layout=(
        "......r.r...........",
        ".r........r.........",
        "..P....P...r........",
        "G######.r......r....",
        "....P.#P..........r.",
        "rrr...#...rr.....KKK",
        "rr...P#.P.P..P...KKK",
        "......###########KKK",
        ".......r..r#...P.KKK",
        ".........P.#.P...KKK",
        "..........r#P....KKK",
        "...Pr......#....r..r",
        "G###########........",
        "......P..P.......r.r",
        "........r....r......",
    ),
    heights=(
        "00011111111000000000",
        "00011111111000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000001111000000000",
        "00000001111001111111",
        "00000000000001111111",
        "00000000000001111111",
        "00000000000001111111",
        "00000000000001111111",
    ),
    waves=(
        "goblin 12 0.5 a; wolf 6 0.7 b",
        "grunt 8 0.9 a; goblin 10 0.5 b",
        "troll 5 1.2",
        "wolf 12 0.45; sapper 4 1.0 a",
        "ogre 3 2.2 b; grunt 8 0.8 a",
        "berserker 6 1.0; troll 5 1.0",
        "ogre_mage 2 2.0 a; ogre 2 2.2 b; grunt 8 0.7",
        "wolf 16 0.4; sapper 6 0.7",
        "troll 10 0.8; berserker 6 0.9",
        "ogre 5 1.8; ogre_mage 3 1.8; wolf 14 0.4",
    ),
    nights=(5, 10),
    modifiers=("drought",),
    start_gold=330,
    start_hour=12.0,
    theme="dry",
)

ASHFALL_RIDGE = LevelData(
    number=8,
    name="Ashfall Ridge",
    layout=(
        ".........T..........",
        ".....T.r............",
        ".T..P...P...........",
        ".....#######........",
        "....r#..P..#........",
        "...T.#P...P#....TKKK",
        "...P.#T....#P....KKK",
        "G###/#.....#...P.KKK",
        "...........#..###KKK",
        "....P..T..P#..#T.KKK",
        "....T..T...#P.#P.KKK",
        "......T..../..#.....",
        "......T..P.####..T..",
        ".............P..T..T",
        "....r............T.r",
    ),
    heights=(
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000111111110000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
    ),
    waves=(
        "grunt 10 0.8",
        "goblin 14 0.45; wolf 6 0.6",
        "troll 6 1.0",
        "sapper 6 0.8; grunt 8 0.7",
        "ogre 3 2.0; berserker 4 1.0",
        "ogre_mage 2 2.0; troll 6 0.9",
        "wolf 16 0.4; sapper 6 0.7",
        "berserker 8 0.8; ogre 3 2.0",
        "troll 10 0.8; ogre_mage 3 1.8",
        "ogre 5 1.8; grunt 12 0.6",
        "berserker 10 0.6; ogre_mage 4 1.6; troll 10 0.7",
    ),
    nights=(2, 4, 6, 8, 10, 11),
    modifiers=("blood_moon",),
    start_gold=350,
    start_hour=15.0,
    theme="ash",
)

RAVENHOLD = LevelData(
    number=9,
    name="Ravenhold",
    layout=(
        ".G.T...T.P.........r",
        ".#.T...######.......",
        ".#....P#.P..#.......",
        ".#.P..T#....#T.T....",
        ".#######...P#.P....T",
        ".T...P.....T####rKKK",
        "............T.P#.KKK",
        ".........T..T..##KKK",
        ".T.............#.KKK",
        "....P..T...r####.KKK",
        ".######....P#.PTTKKK",
        ".#TP..#.....#.......",
        ".#..rP#.P.P.#....T..",
        ".#....#######.T.T...",
        ".G..T...............",
    ),
    heights=(
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000111100000000",
        "00111100111100000000",
        "00111100111100000000",
        "00111100111100000000",
        "00000000111100000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
        "00000000000000000000",
    ),
    waves=(
        "grunt 8 0.9 a; goblin 10 0.5 b",
        "wolf 10 0.5 b; sapper 4 1.0 a",
        "troll 6 1.0 a; berserker 3 1.2 b",
        "ogre 3 2.2; grunt 10 0.7",
        "ogre_mage 2 2.0 a; wolf 12 0.45 b",
        "sapper 8 0.7; berserker 6 0.9",
        "troll 10 0.8; ogre 3 2.0",
        "grunt 16 0.5; ogre_mage 3 1.8",
        "wolf 18 0.4; sapper 8 0.6",
        "ogre 5 1.8 a; troll 8 0.8 b",
        "berserker 12 0.6; ogre_mage 4 1.6",
        "ogre 6 1.6; troll 10 0.7; berserker 10 0.6",
    ),
    nights=(3, 4, 7, 8, 11, 12),
    modifiers=("plunder", "long_night"),
    start_gold=360,
    start_hour=14.0,
    theme="ash",
)

LEVELS = [
    GREENMARCH_FORD,
    THORNWOOD_PASS,
    STONEBRIDGE,
    MIREFEN_MARSH,
    FROSTMERE,
    HIGHPEAK_WATCH,
    SUNSCORCH_FLATS,
    ASHFALL_RIDGE,
    RAVENHOLD,
    THE_BLACK_GATE,
]
"""Every level, in the order of the campaign."""
