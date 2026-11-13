"""
The six rooms of the Sunken Shrine sample game as text maps, shared by the level and menu generators of
``tools/samples/sunken_shrine/``.

A room is 15 x 9 cells. ``#`` a wall, ``T`` a wall with a torch, ``.`` floor, ``,`` the worn floor of the dark room,
``N`` ``S`` ``E`` ``W`` the exits (a gap of the wall leading to another room), ``D`` a locked door in an exit,
``O`` the old man, ``C`` the chest of the key, ``A`` the sword on its stand, ``B`` a brazier, ``K`` the stone block,
``P`` the plate under it, ``m`` a slime, ``b`` a bat, ``s`` a spider, ``r`` a rat, ``Z`` the guardian, ``H`` the heart
it guards.
"""

from __future__ import annotations

from dataclasses import dataclass, field

WIDTH, HEIGHT = 15, 9
"""Size of a room in cells."""


@dataclass(frozen=True)
class Room:
    """A room: its name, its map, where its exits lead (direction: room number), the rule of its locked door, whether
    it is dark."""

    name: str
    rows: list[str]
    exits: dict[str, int]
    door_rule: str = ""
    dark: bool = False
    music: str = "sounds/music/shrine.wav"
    enemies: dict[str, int] = field(default_factory=dict)


ROOMS = [
    Room(
        "THE HALL OF ECHOES",
        [
            "#######N#######",
            "#T...........T#",
            "#.............#",
            "#.............#",
            "#......O......E",
            "#.............#",
            "#.............#",
            "#T...........T#",
            "###############",
        ],
        {"N": 2, "E": 3},
    ),
    Room(
        "THE SLIME PIT",
        [
            "###############",
            "#T.....C.....T#",
            "#.............#",
            "#..m.......m..#",
            "#.....###.....#",
            "#......m......#",
            "#.............#",
            "#T...........T#",
            "#######S#######",
        ],
        {"S": 1},
    ),
    Room(
        "THE ARMOURY",
        [
            "#######D#######",
            "#T...........T#",
            "#.............#",
            "#..b.......b..#",
            "W......A......#",
            "#.............#",
            "#..##.....##..#",
            "#T...........T#",
            "###############",
        ],
        {"W": 1, "N": 4},
        "key",
    ),
    Room(
        "THE DARK CRYPT",
        [
            "#######D#######",
            "#,,,,,,,,,,,,,#",
            "#,B,,,,,,,,,B,#",
            "#,,,,s,,,s,,,,#",
            "#,,,,,,,,,,,,,#",
            "#,,,,,,,,,,,,,#",
            "#,B,,,,,,,,,B,#",
            "#,,,,,,,,,,,,,#",
            "#######S#######",
        ],
        {"S": 3, "N": 5},
        "braziers",
        True,
    ),
    Room(
        "THE SINKING FLOOR",
        [
            "#######D#######",
            "#T...........T#",
            "#.........P.r.#",
            "#.............#",
            "#....K........#",
            "#............r#",
            "#.............#",
            "#T...........T#",
            "#######S#######",
        ],
        {"S": 4, "N": 6},
        "block",
    ),
    Room(
        "THE GUARDIAN'S DEN",
        [
            "###############",
            "#T.....H.....T#",
            "#.............#",
            "#..#.......#..#",
            "#......Z......#",
            "#..#.......#..#",
            "#.............#",
            "#T...........T#",
            "#######S#######",
        ],
        {"S": 5},
        "boss",
        music="sounds/music/guardian.wav",
    ),
]
"""The rooms, in the order of the quest."""

OPPOSITE = {"N": "S", "S": "N", "E": "W", "W": "E"}
"""The exit a room is entered by, from the exit of the room left."""


def cell_position(x: int, y: int, z: float = 0.0) -> tuple[float, float, float]:
    """World centre of a cell: the room is centred on the origin, y up."""
    return x - (WIDTH - 1) / 2.0, (HEIGHT - 1) / 2.0 - y, z
