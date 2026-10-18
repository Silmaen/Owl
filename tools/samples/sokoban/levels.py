"""
The ten levels of the Sokoban sample game as text, their parser and a solution of each.

The text uses the usual Sokoban notation: ``#`` wall, space floor, ``.`` goal, ``$`` box, ``*`` box on a goal, ``@`` the
player, ``+`` the player on a goal. The floor is the area the player can walk to; what lies outside the walls stays
empty. ``generate_levels.py`` writes them as tilemaps once; the tilemaps are then edited in Owl Nest, so this text is
the first version of the levels, not their truth.
"""

from __future__ import annotations

from dataclasses import dataclass

Cell = tuple[int, int]

LEVELS: list[tuple[str, list[str]]] = [
    (
        "First push",
        [
            "#######",
            "#     #",
            "# @$ .#",
            "#     #",
            "#######",
        ],
    ),
    (
        "Two by two",
        [
            "########",
            "#   #  #",
            "# $   .#",
            "#@ #$ .#",
            "#      #",
            "########",
        ],
    ),
    (
        "Side door",
        [
            " #######",
            "## . $ #",
            "#@$    #",
            "# #   ##",
            "#  ##  #",
            "#.     #",
            "########",
        ],
    ),
    (
        "Back room",
        [
            "########",
            "#     .##",
            "## .##$ #",
            "#     $@#",
            "#  . $# #",
            "# ##  # #",
            "#    #  #",
            "#########",
        ],
    ),
    (
        "Cellar",
        [
            "#######",
            "#    .##",
            "#$ #   #",
            "#  #   #",
            "#    # ##",
            "#. $### #",
            "#     $+#",
            "#########",
        ],
    ),
    (
        "Four corners",
        [
            "##########",
            "#   ##   #",
            "#  $$ #  #",
            "#   $@   #",
            "## $    .#",
            " ##. # # #",
            " #    ..##",
            " ########",
        ],
    ),
    (
        "Pantry",
        [
            "##########",
            "#   .  # #",
            "# $   #  #",
            "##$#.## ##",
            "#@$ $    #",
            "##     . #",
            "#  . #   #",
            "##########",
        ],
    ),
    (
        "Columns",
        [
            "##########",
            "# .# #  .#",
            "#    .   #",
            "#     # .#",
            "# #$#    #",
            "#  $@$ $ #",
            "# #  # ###",
            "########",
        ],
    ),
    (
        "Loading bay",
        [
            " ##########",
            "##  # #   #",
            "#  .. #$. #",
            "# # # #   #",
            "#    # $$ #",
            "#   @$    #",
            "##   $# . #",
            "#.     #  #",
            "###########",
        ],
    ),
    (
        "Warehouse",
        [
            "    #######",
            " ####     #",
            "##        #",
            "# #.# # $ #",
            "#      #  #",
            "#  .. $#$##",
            "# # . $@* #",
            "#  # #    #",
            "###########",
        ],
    ),
]
"""Name and text of each level, in the order of play."""

SOLUTIONS: list[str] = [
    "RR",
    "rdrrUdllluuurrdRRurDlllulldRRRR",
    "ddrdrrruuuruLLrddddlllluuuRRRurDDDrdLLLL",
    "LLLdlluururrrDulllldddrruLulDruurrrdrdLLLddrUlddllluuurururrrddLdllUUluRRRlldddrruLdlUlldR",
    "LLLLUluuuulDDrddldRuuuuurrdddLdlUUUluRRRlllddDrddRRRR",
    "LLDlUluurrDullddrRdrruuLrddllluluurrDrDulullddrdrRRllluRRRRRurDllDDullddrRlluulluluRurDldRRurDldRurDD",
    "RdrrUUddlluRRuUlulldRDDrruuruLLrdddlluuRurDllluRRdrDDldRRRuLdlluluurrdDrdLdllUUUdrDruuululldRddrruuruLLrdddlluuRur"
    "DllluRR",
    "LUUrrdrdLLrrurrdLullullluRRdrddldlUUdrruulullldddRluurRRuRRurrddldllUddldlUrrurrruuulldRurDlddlldllUdlluurRRlddrr"
    "urrdLLLdlUUUruLdlUrrRRllldddrruUdrruUluR",
    "RRRdrruuullDurrdLuuulDrddDLLuRldLLrrrrUUdlldllddrUluRRurrdLLLdllluurrDrdLLruulldDrdLruUUUdlluurRddrdrrrrruuuulldDD"
    "rdLLLLdlUUU",
    "LLuuuurrrrddlUlullddddrrdrruLdlluluuuurrdrruLLLulDllddrrddrdrruUUUddddlluRlluurDllluuurrDullddrdrdRdrrUdllululluuu"
    "rrrrdrruLLLulDllddrdrruLLdlUrrrdddrruUUUlullDurrdrruLLullddDuuurrdLrrdddddlluuLLrrddrruuuulululDDDrddRluL",
]
"""A solution of each level, one letter per step (``l`` ``r`` ``u`` ``d``, upper case for a push), fewest pushes."""


@dataclass(frozen=True)
class Level:
    """A parsed level: its size and the cells of each kind."""

    name: str
    width: int
    height: int
    walls: frozenset[Cell]
    floor: frozenset[Cell]
    goals: frozenset[Cell]
    boxes: frozenset[Cell]
    player: Cell


def parse(name: str, rows: list[str]) -> Level:
    """Read a level text; the floor is what the player reaches without crossing a wall."""
    width = max(len(row) for row in rows)
    walls: set[Cell] = set()
    goals: set[Cell] = set()
    boxes: set[Cell] = set()
    players: list[Cell] = []
    for y, row in enumerate(rows):
        for x, char in enumerate(row):
            if char == "#":
                walls.add((x, y))
            if char in ".*+":
                goals.add((x, y))
            if char in "$*":
                boxes.add((x, y))
            if char in "@+":
                players.append((x, y))
    if len(players) != 1:
        raise ValueError(f"{name}: {len(players)} players")
    floor: set[Cell] = set()
    todo = [players[0]]
    while todo:
        cell = todo.pop()
        if cell in floor or cell in walls:
            continue
        x, y = cell
        if not (0 <= x < width and 0 <= y < len(rows)):
            raise ValueError(f"{name}: the floor leaks out at {cell}")
        floor.add(cell)
        todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return Level(
        name, width, len(rows), frozenset(walls), frozenset(floor), frozenset(goals), frozenset(boxes), players[0]
    )


def level(index: int) -> Level:
    """The parsed level of an index, from 0."""
    name, rows = LEVELS[index]
    return parse(name, rows)


STEPS: dict[str, Cell] = {"l": (-1, 0), "r": (1, 0), "u": (0, -1), "d": (0, 1)}
"""The step of each letter of a solution."""


def replay(lvl: Level, moves: str) -> tuple[Cell, frozenset[Cell]]:
    """Play a solution by the rules of ``sokoban.lua``: the player and the boxes at the end; a wrong letter raises."""
    player, boxes = lvl.player, set(lvl.boxes)
    for letter in moves:
        dx, dy = STEPS[letter.lower()]
        ahead = (player[0] + dx, player[1] + dy)
        if ahead not in lvl.floor:
            raise ValueError(f"{lvl.name}: '{letter}' walks into a wall")
        pushed = ahead in boxes
        if pushed != letter.isupper():
            raise ValueError(f"{lvl.name}: '{letter}' {'pushes' if pushed else 'pushes nothing'}")
        if pushed:
            beyond = (ahead[0] + dx, ahead[1] + dy)
            if beyond not in lvl.floor or beyond in boxes:
                raise ValueError(f"{lvl.name}: '{letter}' pushes a box into a wall or a box")
            boxes.remove(ahead)
            boxes.add(beyond)
        player = ahead
    return player, frozenset(boxes)
