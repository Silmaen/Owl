#!/usr/bin/env python3
"""
Write the three floors of the Wolfenstein mini sample game into ``samples/wolfenstein/`` (deterministic).

Each floor is carved into a 32 x 32 grid of walls (``LEVELS``): rooms, corridors, doors, keys, secret walls, pick-ups
and props, one character per cell. From it the script writes the wall tilemap (``tilemaps/level_N.owltilemap``), the
minimap with its hidden wall layer and its empty fog layer (``tilemaps/level_N_minimap.owltilemap``) and the scene
(``scenes/level_N.owl``), entity by entity in the order Owl Nest saves them. The scenes stay editable in Owl Nest
afterwards (move a prop, add a treasure, retexture a door); running this script again overwrites those edits.

Every floor is checked before anything is written: each floor cell, pick-up, door and the exit can be reached from the
start (through the doors and the secret walls), a door stands between two walls, a secret wall has room to slide.

Run from the repository root: ``./docker_run.sh poetry run python tools/samples/wolfenstein/generate_levels.py``.
"""

from __future__ import annotations

import math
import random
import struct
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3] / "samples" / "wolfenstein"
"""The sample project."""
SIZE = 32
"""Cells per side of a floor."""
CHUNK = 32
"""Cells per side of a tilemap chunk."""
EMPTY = 0xFFFF
"""Empty cell of a tilemap layer."""
TILESET = "tilesets/wolf_walls.owltileset"
"""Wall tileset of the floors and of the doors."""

WALLS: dict[str, tuple[int, int]] = {
    "#": (0, 1),  # grey stone
    "B": (14, 15),  # blue stone
    "C": (8, 9),  # blue stone with a cell grid
    "W": (22, 23),  # wood panels
    "R": (32, 33),  # red brick
    "M": (50, 51),  # mossy grey stone
    "S": (52, 53),  # dark grey stone
    "e": (40, 40),  # elevator
    "X": (41, 41),  # elevator switch: the exit
}
"""Wall characters and their two alternating tiles in ``textures/wolf3d/walls_full.png``."""

DOORS: dict[str, tuple[str, int, str]] = {
    "D": ("Door", 98, ""),
    "G": ("GoldDoor", 104, "gold"),
    "Q": ("BlueDoor", 105, "blue"),
    "E": ("ElevatorDoor", 102, ""),
}
"""Door characters: tag, face tile and the key that opens it (none: the built-in `interact` action)."""
DOOR_SIDE = 100
"""Tile of the door jambs."""

PUSHWALLS: dict[str, tuple[int, int]] = {"^": (0, -1), "v": (0, 1), "<": (-1, 0), ">": (1, 0)}
"""Secret wall characters and the cell step they slide by (column, row)."""
PUSH_DISTANCE = 2
"""Cells a secret wall slides."""

ITEMS: dict[str, tuple[str, str]] = {
    "c": ("Cross", "treasure_cross"),
    "h": ("Chalice", "treasure_calice"),
    "t": ("Chest", "treasure"),
    "k": ("Crown", "treasure_crown"),
    "f": ("Food", "food"),
    "g": ("DogFood", "sausages"),
    "y": ("GoldKey", "key_yellow"),
    "b": ("BlueKey", "key_blue"),
}
"""Pick-up characters: tag (also the `item` of `pickup.lua`) and sprite."""

PROPS: dict[str, tuple[str, str, bool]] = {
    "a": ("Armor", "armor", True),
    "w": ("Barrel", "barrel_wood", True),
    "o": ("GreenBarrel", "green_barrel", True),
    "V": ("Vase", "blue_pot", True),
    "n": ("Bones", "bones_pile", False),
    "q": ("Cage", "cage_hang", False),
    "l": ("Lamp", "lamp_ceiling", False),
    "L": ("Chandelier", "lamp_ceiling_candle", False),
    "i": ("FloorLamp", "lamp_pilar", True),
    "p": ("Pillar", "pilar", True),
    "u": ("Puddle", "puddle", False),
    "s": ("Skeleton", "skeleton_floor", False),
    "j": ("Table", "table", True),
    "T": ("TableChairs", "table_chairs", True),
    "*": ("Plant", "tree_pot", True),
    "O": ("Well", "pit_water", True),
    "F": ("Stove", "fuirnace", True),
    "x": ("Spears", "spears", True),
    "K": ("Pans", "cooking_tools", False),
    "Z": ("Bed", "bed", True),
}
"""Prop characters: tag, sprite and whether the player bumps into it."""

FLOOR_CHARS = {".", "@"} | set(DOORS) | set(PUSHWALLS) | set(ITEMS) | set(PROPS)
"""Characters of a walkable cell (a secret wall's cell is walkable once it slid)."""


@dataclass
class Level:
    """One floor: its grid, its start and its look."""

    number: int
    """Floor number (1 to 3)."""
    name: str
    """Name shown on the HUD and the tally."""
    wall: str
    """Wall character the grid starts filled with."""
    yaw: float
    """Start heading, radians (0 faces north, pi / 2 west)."""
    floor_color: tuple[float, float, float]
    """Floor colour of the raycaster."""
    ceiling_color: tuple[float, float, float]
    """Ceiling colour of the raycaster."""
    grid: list[list[str]] = field(default_factory=list)
    """Cells, ``grid[row][column]``."""

    def __post_init__(self) -> None:
        self.grid = [[self.wall] * SIZE for _ in range(SIZE)]

    def room(self, x0: int, y0: int, x1: int, y1: int) -> None:
        """Carve the floor of a rectangle of cells, corners included."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.grid[y][x] = "."

    def walls(self, char: str, x0: int, y0: int, x1: int, y1: int) -> None:
        """Retexture the walls of a rectangle of cells (its floor cells stay)."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if self.grid[y][x] in WALLS:
                    self.grid[y][x] = char

    def put(self, char: str, *cells: tuple[int, int]) -> None:
        """Place a door, a secret wall, a pick-up, a prop or the start on cells."""
        for x, y in cells:
            self.grid[y][x] = char

    def find(self, char: str) -> list[tuple[int, int]]:
        """Cells holding a character, row by row."""
        return [(x, y) for y in range(SIZE) for x in range(SIZE) if self.grid[y][x] == char]


def world(x: int, y: int) -> tuple[float, float]:
    """Centre of a cell in the world (the tilemap is centred on its entity, rows grow southward)."""
    return x - SIZE / 2 + 0.5, SIZE / 2 - 0.5 - y


def is_wall(level: Level, x: int, y: int) -> bool:
    """Whether a cell is a wall (outside the grid counts as one)."""
    return not (0 <= x < SIZE and 0 <= y < SIZE) or level.grid[y][x] in WALLS


def door_axis(level: Level, x: int, y: int) -> str:
    """Opening direction of a door: `North` in a west-east passage, `East` in a north-south one."""
    if (
        not is_wall(level, x - 1, y)
        and not is_wall(level, x + 1, y)
        and is_wall(level, x, y - 1)
        and is_wall(level, x, y + 1)
    ):
        return "North"
    if (
        not is_wall(level, x, y - 1)
        and not is_wall(level, x, y + 1)
        and is_wall(level, x - 1, y)
        and is_wall(level, x + 1, y)
    ):
        return "East"
    raise ValueError(f"floor {level.number}: door at {x},{y} is not between two walls")


def pushwall_tile(level: Level, x: int, y: int) -> int:
    """Tile of a secret wall: the most common wall around it, so that it does not stand out."""
    around = [
        level.grid[y + dy][x + dx]
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
        if 0 <= x + dx < SIZE and 0 <= y + dy < SIZE and level.grid[y + dy][x + dx] in WALLS
    ]
    char = max(sorted(set(around)), key=around.count) if around else level.wall
    return WALLS[char][(x + y) % 2]


def check(level: Level) -> None:
    """Refuse a floor that cannot be finished at 100 %: see the module documentation."""
    starts = level.find("@")
    if len(starts) != 1 or len(level.find("X")) != 1:
        raise ValueError(f"floor {level.number}: one start and one exit switch expected")
    for x, y in level.find("X"):
        if not any(
            level.grid[y + dy][x + dx] == "."
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            if 0 <= x + dx < SIZE and 0 <= y + dy < SIZE
        ):
            raise ValueError(f"floor {level.number}: the exit switch at {x},{y} is not next to a floor cell")
    for y in range(SIZE):
        for x in range(SIZE):
            if (x in (0, SIZE - 1) or y in (0, SIZE - 1)) and level.grid[y][x] not in WALLS:
                raise ValueError(f"floor {level.number}: the border cell {x},{y} is not a wall")
            if level.grid[y][x] in DOORS:
                door_axis(level, x, y)
            if level.grid[y][x] in PUSHWALLS:
                dx, dy = PUSHWALLS[level.grid[y][x]]
                for step in range(1, PUSH_DISTANCE + 1):
                    if level.grid[y + dy * step][x + dx * step] != ".":
                        raise ValueError(f"floor {level.number}: secret wall at {x},{y} has no room to slide")
    # Every walkable cell is reached through doors and slid walls; solid props block, but nothing may sit behind one.
    blocked = {
        (x, y)
        for x, y in ((x, y) for y in range(SIZE) for x in range(SIZE))
        if level.grid[y][x] in PROPS and PROPS[level.grid[y][x]][2]
    }
    walkable = {(x, y) for y in range(SIZE) for x in range(SIZE) if level.grid[y][x] in FLOOR_CHARS}
    seen = {starts[0]}
    queue = deque(seen)
    while queue:
        x, y = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cell = (x + dx, y + dy)
            if cell in walkable and cell not in seen:
                seen.add(cell)
                if cell not in blocked:
                    queue.append(cell)
    if missing := sorted(walkable - seen):
        raise ValueError(f"floor {level.number}: cells out of reach {missing}")


def fnv1a(data: bytes) -> int:
    """Checksum of a tilemap payload."""
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def layer_bytes(cells: list[int]) -> bytes:
    """One tilemap layer of SIZE x SIZE cells (one chunk), or no chunk when it is empty."""
    if all(cell == EMPTY for cell in cells):
        return struct.pack("<I", 0)
    return (
        struct.pack("<I", 1)
        + struct.pack("<II", 0, 0)
        + struct.pack(f"<{CHUNK * CHUNK}H", *cells)
        + struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK))
        + bytes(CHUNK * CHUNK)
    )


def write_tilemap(path: Path, name: str, tileset: str, layers: list[tuple[str, bool, list[int]]]) -> None:
    """Write a `.owltilemap` (format 3): YAML header, then the chunked binary payload."""
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, len(layers))
    for _, _, cells in layers:
        body += layer_bytes(cells)
    body += b"\x00"  # no corner heights
    payload = body + struct.pack("<I", fnv1a(body))
    header = f"Tilemap: {name}\nFormatVersion: 3\ntilesetPath: {tileset}\nwidth: {SIZE}\nheight: {SIZE}\ncellSize: 1\n"
    header += "layers:\n"
    for layer, visible, _ in layers:
        header += f"  - name: {layer}\n" + ("" if visible else "    visible: false\n")
    header += f"payloadBytes: {len(payload)}\n"
    path.write_bytes(header.encode() + b"...\n" + payload)


def tilemaps(level: Level) -> None:
    """Write the wall tilemap and the minimap of a floor."""
    walls = [EMPTY] * (SIZE * SIZE)
    minimap = [1] * (SIZE * SIZE)
    for y in range(SIZE):
        for x in range(SIZE):
            if (char := level.grid[y][x]) in WALLS:
                walls[y * SIZE + x] = WALLS[char][(x + y) % 2]
                minimap[y * SIZE + x] = 0
    folder = ROOT / "tilemaps"
    folder.mkdir(parents=True, exist_ok=True)
    write_tilemap(
        folder / f"level_{level.number}.owltilemap", f"level_{level.number}", TILESET, [("walls", True, walls)]
    )
    write_tilemap(
        folder / f"level_{level.number}_minimap.owltilemap",
        f"level_{level.number}_minimap",
        "tilesets/minimap.owltileset",
        [("walls", False, minimap), ("seen", True, [EMPTY] * SIZE * SIZE)],
    )


def fmt(value: float) -> str:
    """A number as yaml-cpp writes it."""
    return str(int(value)) if float(value).is_integer() else repr(round(value, 7))


def vec(*values: float) -> str:
    """A flow sequence of numbers."""
    return "[" + ", ".join(fmt(v) for v in values) + "]"


class SceneWriter:
    """Entities of a scene, written in the component order of `SceneSerializer`."""

    def __init__(self, seed: int) -> None:
        self.random = random.Random(seed)
        self.lines: list[str] = []

    def uuid(self) -> int:
        """A new entity id."""
        return self.random.getrandbits(63)

    def entity(
        self,
        tag: str,
        components: list[str],
        uuid: int | None = None,
        x: float = 0.0,
        y: float = 0.0,
        z: float = 0.0,
        yaw: float = 0.0,
        scale: tuple[float, float] = (1.0, 1.0),
    ) -> int:
        """Append an entity; `components` are YAML blocks (key line first) in serialization order after Transform."""
        uid = self.uuid() if uuid is None else uuid
        self.lines += [
            f"  - Entity: {uid}",
            "    Tag:",
            f"      tag: {tag}",
            "    Transform:",
            f"      translation: {vec(x, y, z)}",
            f"      rotation: {vec(0, 0, yaw)}",
            f"      scale: {vec(scale[0], scale[1], 1)}",
        ]
        for block in components:
            self.lines += ["    " + line for line in block.strip("\n").split("\n")]
        return uid


def visible() -> str:
    """Visibility block."""
    return "Visibility:\n  gameVisible: true"


def child_of(parent: int) -> str:
    """Hierarchy block of a child."""
    return f"Hierarchy:\n  parentId: {parent}"


def group() -> str:
    """Hierarchy block of a root with children."""
    return "Hierarchy:\n  {}"


def tag_for(renderer: str) -> str:
    """RendererTag block."""
    return f"RendererTag:\n  Name: {renderer}"


def script(path: str, properties: list[tuple[str, str, str]] | None = None) -> str:
    """LuaScript block with its properties (name, type, value)."""
    block = f"LuaScript:\n  scriptPath: {path}"
    if properties:
        block += "\n  properties:"
        for name, kind, value in properties:
            if ": " in value:  # quoted as yaml-cpp writes such a string
                value = '"' + value.replace('"', '\\"') + '"'
            block += f"\n    - name: {name}\n      type: {kind}\n      value: {value}"
    return block


def sprite(texture: str) -> str:
    """SpriteRenderer block of a raycast billboard."""
    return (
        "SpriteRenderer:\n  color: [1, 1, 1, 1]\n  tilingFactor: 1\n"
        f"  texture: nam:textures/wolf3d/sprites/{texture}.png"
    )


def static_body(size: float) -> str:
    """PhysicBody block of a prop the player bumps into."""
    return (
        f"PhysicBody:\n  type: Static\n  fixedRotation: true\n  colliderSize: {vec(size, size, 1)}\n  density: 1\n"
        "  restitution: 0\n  friction: 0.5"
    )


def scene(level: Level) -> None:
    """Write the scene of a floor."""
    out = SceneWriter(1000 + level.number)
    fc, cc = level.floor_color, level.ceiling_color
    head = [
        "Scene: untitled",
        "FormatVersion: 5",
        "Hud: huds/play.owlhud",
        "EnabledRenderers:",
        "  - Name: world",
        "    Enabled: false",
        "  - Name: raycast_world",
        "    Enabled: true",
        "    Overrides:",
        f"      FloorColor: {vec(*fc, 1)}",
        f"      CeilingColor: {vec(*cc, 1)}",
        "  - Name: overlay",
        "    Enabled: true",
        "    Overrides:",
        "      Space: Screen",
        "Entities:",
    ]
    ((sx, sy),) = level.find("@")
    px, py = world(sx, sy)
    out.entity(
        "Player",
        [
            "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.7853982\n"
            "    perspectiveNear: 0.01\n    perspectiveFar: 1000\n    orthographicSize: 8\n"
            "    orthographicNear: -1\n    orthographicFar: 10\n  primary: true\n  fixedAspectRatio: false",
            "PhysicBody:\n  type: Dynamic\n  fixedRotation: true\n  colliderSize: [0.4, 0.4, 1]\n  density: 1\n"
            "  restitution: 0\n  friction: 0.2",
            "Player:\n  primary: true\n  linearImpulse: 0\n  jumpImpulse: 0\n  canJump: false",
            visible(),
            "SoundListener:\n  primary: true",
            script(
                "scripts/game/player.lua",
                [("speed", "float", "3"), ("turn_speed", "float", "1.8"), ("mouse_turn", "float", "0.006")],
            ),
        ],
        x=px,
        y=py,
        z=0.5,
        yaw=level.yaw,
    )
    out.entity(
        "Walls",
        [f"Tilemap:\n  tilemapPath: tilemaps/level_{level.number}.owltilemap", visible(), tag_for("raycast_world")],
    )
    out.entity(
        "LevelLogic",
        [
            visible(),
            script("scripts/game/level.lua", [("level", "int", str(level.number)), ("title", "string", level.name)]),
        ],
    )
    out.entity(
        "Ambience",
        [
            visible(),
            "SoundSource:\n  soundAsset: sounds/game/ambience.wav\n  category: Ambient\n"
            "  volume: 0.4\n  pitch: 1\n  loop: true\n  spatial: false\n  playOnStart: true\n"
            "  maxDistance: 50\n  rolloff: 1",
        ],
    )
    out.entity(
        "Minimap",
        [
            f"Tilemap:\n  tilemapPath: tilemaps/level_{level.number}_minimap.owltilemap",
            visible(),
            script("scripts/game/explore.lua"),
            tag_for("overlay"),
        ],
        x=96,
        y=96,
        scale=(5, 5),
    )
    out.entity(
        "MinimapPlayer",
        ["CircleRenderer:\n  color: [1, 0.85, 0.1, 1]\n  thickness: 1\n  fade: 0.05", visible(), tag_for("overlay")],
        x=96,
        y=96,
        z=0.1,
        scale=(6, 6),
    )
    out.entity(
        "MinimapFacing",
        ["CircleRenderer:\n  color: [1, 0.45, 0.1, 1]\n  thickness: 1\n  fade: 0.05", visible(), tag_for("overlay")],
        x=102,
        y=96,
        z=0.2,
        scale=(3, 3),
    )

    doors = out.entity("Doors", [visible(), group()])
    for y in range(SIZE):
        for x in range(SIZE):
            if (char := level.grid[y][x]) not in DOORS:
                continue
            tag, face, key = DOORS[char]
            wx, wy = world(x, y)
            components = [visible(), child_of(doors)]
            if key:
                components.append(script("scripts/game/locked_door.lua", [("key", "string", key)]))
            components.append(
                f"RaycastDoor:\n  tilesetPath: {TILESET}\n  faceTileIndex: {face}\n"
                f"  lateralTileIndex: {DOOR_SIDE}\n  openingDirection: {door_axis(level, x, y)}\n"
                "  slideSpeed: 2\n  holdTime: 4\n  closeSpeed: 2\n"
                f"  interactionAction: {'' if key else 'interact'}\n  interactionRange: 1.5".replace(
                    "interactionAction: \n", 'interactionAction: ""\n'
                )
            )
            out.entity(tag, components, x=wx, y=wy)

    secrets = out.entity("Secrets", [visible(), group()])
    for y in range(SIZE):
        for x in range(SIZE):
            if (char := level.grid[y][x]) not in PUSHWALLS:
                continue
            dx, dy = PUSHWALLS[char]
            wx, wy = world(x, y)
            out.entity(
                "Secret",
                [
                    visible(),
                    child_of(secrets),
                    script("scripts/game/secret.lua"),
                    f"RaycastPushWall:\n  tilesetPath: {TILESET}\n  tileIndex: {pushwall_tile(level, x, y)}\n"
                    f"  slideDirection: {vec(dx, -dy)}\n  slideDistance: {PUSH_DISTANCE}\n  slideSpeed: 1\n"
                    "  interactionAction: interact\n  interactionRange: 1.5",
                ],
                x=wx,
                y=wy,
            )

    items = out.entity("Items", [visible(), group()])
    for y in range(SIZE):
        for x in range(SIZE):
            if (char := level.grid[y][x]) not in ITEMS:
                continue
            tag, texture = ITEMS[char]
            wx, wy = world(x, y)
            out.entity(
                tag,
                [
                    sprite(texture),
                    "Trigger:\n  Type: LuaCallback\n  CallbackName: on_pickup",
                    visible(),
                    child_of(items),
                    script("scripts/game/pickup.lua", [("item", "string", tag)]),
                    tag_for("raycast_world"),
                ],
                x=wx,
                y=wy,
            )

    props = out.entity("Props", [visible(), group()])
    for y in range(SIZE):
        for x in range(SIZE):
            if (char := level.grid[y][x]) not in PROPS:
                continue
            tag, texture, solid = PROPS[char]
            wx, wy = world(x, y)
            components = [sprite(texture)] + ([static_body(0.7)] if solid else [])
            out.entity(tag, components + [visible(), child_of(props), tag_for("raycast_world")], x=wx, y=wy)

    ((ex, ey),) = level.find("X")
    wx, wy = world(ex, ey)
    out.entity(
        "ExitElevator",
        [
            "Trigger:\n  Type: Interaction\n  InteractionRange: 1.5\n  CallbackName: on_elevator_use",
            visible(),
            script("scripts/game/exit.lua"),
        ],
        x=wx,
        y=wy,
        scale=(3, 3),
    )
    folder = ROOT / "scenes"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"level_{level.number}.owl").write_text("\n".join(head + out.lines))


def level_1() -> Level:
    """The cellars: the tutorial floor, the gold key in the start room."""
    lv = Level(1, "Floor 1: the cellars", "#", -math.pi / 2, (0.44, 0.44, 0.42), (0.2, 0.2, 0.21))
    lv.room(1, 25, 7, 29)  # start room
    lv.put("@", (2, 27))
    lv.put("y", (1, 27))
    lv.put("c", (4, 27))
    lv.put("l", (4, 26))
    lv.put("f", (6, 29))
    lv.put("w", (7, 25), (1, 29))
    lv.put("G", (8, 27))
    lv.room(2, 22, 2, 23)  # secret behind the north wall of the start room
    lv.put("^", (2, 24))
    lv.room(3, 20, 6, 23)
    lv.put("t", (5, 21))
    lv.put("k", (6, 20))
    lv.put("n", (3, 20))
    lv.walls("M", 1, 19, 7, 24)
    lv.room(9, 27, 13, 27)  # corridor to the hall
    lv.room(14, 23, 22, 30)  # pillar hall
    lv.put("p", (16, 25), (20, 25), (16, 28), (20, 28))
    lv.put("h", (18, 26))
    lv.put("c", (15, 30))
    lv.put("f", (22, 23))
    lv.put("L", (18, 24), (18, 29))
    lv.put("u", (21, 30))
    lv.put("D", (18, 22))
    lv.room(18, 15, 18, 21)  # corridor north
    lv.put("D", (18, 14))
    lv.room(13, 5, 23, 13)  # mess hall
    lv.put("T", (15, 7), (21, 7), (15, 11), (21, 11))
    lv.put("g", (14, 5))
    lv.put("t", (22, 12))
    lv.put("c", (18, 9))
    lv.put("l", (18, 6), (18, 12))
    lv.put("K", (23, 5))
    lv.put("<", (12, 11))  # secret closet west of the mess hall
    lv.room(10, 11, 11, 11)
    lv.room(11, 12, 11, 13)
    lv.put("k", (11, 13))
    lv.room(11, 6, 12, 6)
    lv.put("D", (10, 6))
    lv.room(1, 1, 9, 9)  # dining room
    lv.walls("W", 0, 0, 10, 10)
    lv.put("T", (3, 3), (7, 3), (3, 7), (7, 7))
    lv.put("h", (5, 5))
    lv.put("f", (2, 9))
    lv.put("L", (5, 2), (5, 8))
    lv.put("a", (1, 1), (9, 1))
    lv.put("D", (24, 10))
    lv.room(25, 8, 30, 20)  # storage
    lv.walls("S", 24, 7, 31, 21)
    lv.put("w", (25, 8), (30, 8), (25, 12), (30, 13))
    lv.put("o", (26, 17), (30, 17))
    lv.put("c", (27, 15))
    lv.put("h", (29, 19))
    lv.put("f", (25, 20))
    lv.put("l", (27, 11), (27, 18))
    lv.put("v", (28, 21))  # secret room south of the storage
    lv.room(28, 22, 28, 23)
    lv.room(29, 22, 30, 26)
    lv.put("t", (30, 26))
    lv.put("s", (29, 25))
    lv.put("E", (27, 7))  # elevator
    lv.room(26, 3, 29, 6)
    lv.walls("e", 25, 2, 30, 7)
    lv.put("X", (27, 2))
    return lv


def level_2() -> Level:
    """The prison: cells along a guarded block, the gold then the blue key."""
    lv = Level(2, "Floor 2: the prison", "B", 0.0, (0.36, 0.38, 0.42), (0.14, 0.15, 0.2))
    lv.room(12, 25, 19, 30)  # start hall
    lv.put("@", (15, 29))
    lv.put("f", (13, 26))
    lv.put("c", (18, 27))
    lv.put("l", (15, 27))
    lv.put("V", (12, 30), (19, 30))
    lv.put("D", (15, 24))
    lv.room(14, 14, 17, 23)  # cell block
    lv.put("l", (15, 16), (16, 21))
    for row in (16, 19, 22):
        lv.room(10, row - 1, 12, row)
        lv.room(19, row - 1, 21, row)
        lv.put("D", (13, row), (18, row))
    lv.walls("C", 13, 14, 13, 23)
    lv.walls("C", 18, 14, 18, 23)
    lv.put("s", (10, 15))
    lv.put("n", (21, 18))
    lv.put("h", (11, 18))
    lv.put("Z", (12, 21))
    lv.put("y", (20, 21))
    lv.put("q", (19, 15))
    lv.put("g", (10, 22))
    lv.put("G", (15, 13))
    lv.room(11, 6, 20, 12)  # guard room
    lv.walls("#", 10, 5, 21, 13)
    lv.put("T", (13, 8), (18, 8))
    lv.put("j", (13, 11))
    lv.put("b", (19, 7))
    lv.put("t", (12, 6))
    lv.put("c", (17, 11))
    lv.put("L", (15, 9))
    lv.put("a", (20, 12), (11, 12))
    lv.put("^", (14, 5))  # secret treasury north of the guard room
    lv.room(14, 3, 14, 4)
    lv.room(9, 2, 13, 3)
    lv.put("k", (9, 2))
    lv.put("h", (11, 3))
    lv.put("Q", (10, 9))
    lv.room(6, 9, 9, 9)
    lv.put("E", (5, 9))  # elevator
    lv.room(2, 8, 4, 10)
    lv.walls("e", 1, 7, 5, 11)
    lv.put("X", (1, 9))
    lv.put("D", (20, 27))  # storage east of the start hall
    lv.room(21, 27, 21, 27)
    lv.room(22, 23, 29, 30)
    lv.walls("S", 21, 22, 31, 31)
    lv.put("w", (22, 23), (29, 23), (29, 30), (24, 30))
    lv.put("o", (23, 26), (27, 26))
    lv.put("f", (28, 28))
    lv.put("c", (22, 29))
    lv.put("l", (25, 25))
    lv.put("^", (25, 22))  # secret store north of the storage
    lv.room(25, 20, 25, 21)
    lv.room(26, 18, 29, 21)
    lv.put("t", (29, 18))
    lv.put("k", (27, 19))
    lv.put("D", (11, 27))  # office west of the start hall
    lv.room(3, 24, 10, 30)
    lv.walls("W", 2, 23, 11, 31)
    lv.put("j", (5, 26))
    lv.put("*", (3, 24), (10, 24))
    lv.put("h", (8, 29))
    lv.put("g", (3, 30))
    lv.put("L", (6, 27))
    lv.put("^", (5, 23))  # secret cupboard north of the office
    lv.room(5, 21, 5, 22)
    lv.room(6, 20, 8, 22)
    lv.put("t", (8, 20))
    return lv


def level_3() -> Level:
    """The headquarters: a courtyard between four wings, both keys, three secrets."""
    lv = Level(3, "Floor 3: the headquarters", "R", math.pi, (0.42, 0.36, 0.3), (0.22, 0.18, 0.16))
    lv.room(2, 2, 8, 6)  # start office
    lv.walls("W", 1, 1, 9, 7)
    lv.put("@", (4, 3))
    lv.put("f", (7, 5))
    lv.put("c", (2, 6))
    lv.put("j", (8, 2))
    lv.put("L", (5, 4))
    lv.put("D", (5, 7))
    lv.room(5, 8, 5, 14)
    lv.put("D", (5, 15))
    lv.room(2, 16, 9, 23)  # library
    lv.walls("W", 1, 15, 10, 24)
    lv.put("y", (3, 23))
    lv.put("T", (4, 18), (7, 18))
    lv.put("V", (2, 16), (9, 16))
    lv.put("h", (8, 22))
    lv.put("l", (5, 20))
    lv.put("D", (9, 4))
    lv.room(10, 4, 22, 4)  # north corridor
    lv.put("l", (13, 4), (20, 4))
    lv.room(23, 2, 29, 8)  # trophy hall
    lv.walls("#", 22, 1, 30, 9)
    lv.put("x", (23, 2), (29, 2))
    lv.put("t", (26, 3))
    lv.put("c", (24, 7))
    lv.put("L", (26, 5))
    lv.put("^", (18, 3))  # secret gallery north of the corridor
    lv.room(18, 1, 18, 2)
    lv.room(12, 1, 17, 2)
    lv.put("k", (12, 1))
    lv.put("h", (15, 2))
    lv.room(15, 5, 15, 9)
    lv.put("D", (15, 10))
    lv.room(11, 11, 20, 20)  # courtyard
    lv.walls("M", 10, 10, 21, 21)
    lv.put("*", (12, 12), (19, 12), (12, 19), (19, 19))
    lv.put("O", (15, 15))
    lv.put("g", (17, 13))
    lv.put("c", (13, 17))
    lv.put("D", (10, 18))
    lv.put("D", (21, 15))
    lv.put("G", (26, 9))
    lv.room(22, 10, 29, 20)  # armoury
    lv.walls("S", 22, 9, 30, 21)
    lv.put("b", (28, 19))
    lv.put("x", (22, 10), (29, 10))
    lv.put("a", (22, 20), (29, 14))
    lv.put("t", (25, 17))
    lv.put("f", (23, 13))
    lv.put("L", (26, 14))
    lv.put("v", (25, 21))  # secret vault south of the armoury
    lv.room(25, 22, 25, 23)
    lv.room(26, 22, 30, 24)
    lv.put("k", (30, 24))
    lv.put("h", (28, 22))
    lv.put("Q", (15, 21))
    lv.room(11, 22, 20, 29)  # great hall
    lv.put("p", (13, 24), (18, 24), (13, 27), (18, 27))
    lv.put("t", (15, 25))
    lv.put("h", (16, 28))
    lv.put("f", (11, 29))
    lv.put("L", (15, 23), (16, 26))
    lv.put("<", (10, 25))  # secret chapel west of the great hall
    lv.room(8, 25, 9, 25)
    lv.room(3, 26, 8, 29)
    lv.walls("#", 2, 24, 9, 30)
    lv.put("t", (3, 29))
    lv.put("c", (5, 27))
    lv.put("i", (3, 26))
    lv.put("E", (21, 27))  # elevator
    lv.room(22, 26, 24, 28)
    lv.walls("e", 21, 25, 25, 29)
    lv.put("X", (25, 27))
    return lv


LEVELS = [level_1, level_2, level_3]
"""The floors, in play order."""


def main() -> None:
    """Check then write every floor."""
    levels = [make() for make in LEVELS]
    for level in levels:
        check(level)
    for level in levels:
        tilemaps(level)
        scene(level)
        print(
            f"floor {level.number}: {len(level.find('@'))} start, "
            f"{sum(len(level.find(c)) for c in ITEMS)} items, {sum(len(level.find(c)) for c in PUSHWALLS)} secrets"
        )
        for row in level.grid:
            print("".join(row))


if __name__ == "__main__":
    main()
