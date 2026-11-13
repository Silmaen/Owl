#!/usr/bin/env python3
"""
Write the project files, the tileset, the six room tilemaps and scenes, the behaviour trees, the dialogue, the play
HUD and the game graphs of the Sunken Shrine sample game into ``samples/sunken_shrine/``, from the maps of
``layout.py``.

- ``tilesets/dungeon.owltileset`` (the walls collide) and ``tilemaps/room_N.owltilemap``;
- ``scenes/room_1.owl`` to ``room_6.owl``: the room tilemap (a static body per wall cell), the hero, the exits
  (sensors), the locked doors, the old man, the chest, the sword, the braziers, the block and its plate, the creatures
  (``SteeringAgent`` in the plane, ``BehaviourTreeAgent``), the guardian and its heart, the 2D lights; the scene script
  ``scripts/game/room.owlvs`` with the rule of the room's door;
- ``scripts/game/``: ``room.owlvs`` (the quest, the doors, the dialogue, the hearts, the end), ``player.owlvs`` (walk,
  sword, use, hurt), ``creature.owlvs``, ``guardian.owlvs``, ``exit.owlvs``, ``heart.owlvs``; the trees
  ``creature.owlbt``, ``bat.owlbt`` and ``guardian.owlbt``; ``shrine.lua`` for the block and the names;
- ``dialogues/old_man.owldlg``; ``huds/play.owlhud``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sunken_shrine.generate_level``;
then ``OwlNest --upgrade-assets samples/sunken_shrine`` gives the new files their ``.owlmeta``.
"""

from __future__ import annotations

import struct
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.asteroid_field.generate_level import TreeWriter
from tools.samples.sokoban.generate_levels import CHUNK, EMPTY, fnv1a
from tools.samples.sunken_shrine import layout as lay
from tools.samples.vs_graph import Graph, Node, Pin

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sunken_shrine"
"""The sample project."""
SHEET = "textures/tiles/dungeon.png"
"""The Kenney Tiny Dungeon sheet."""
COLUMNS, ROWS = 12, 11
"""Tiles of the sheet."""
TILE = {
    "floor": 48,
    "floor_b": 49,
    "dark": 12,
    "dark_b": 24,
    "wall": 40,
    "torch": 29,
    "door": 46,
    "hero": 85,
    "old_man": 84,
    "chest": 89,
    "chest_open": 91,
    "sword": 104,
    "slime": 108,
    "bat": 120,
    "spider": 122,
    "rat": 123,
    "guardian": 110,
}
"""Tiles of the sheet the game uses."""
CREATURES = {
    "m": ("Slime", "slime", 2, 1.8),
    "b": ("Bat", "bat", 1, 3.0),
    "s": ("Spider", "spider", 2, 2.4),
    "r": ("Rat", "rat", 1, 2.6),
}
"""Creatures of the maps: tag, tile, hits and speed."""
TEXT = (1, 0.93, 0.7, 1)
"""Colour of the HUD texts."""
ACCENT = (1, 0.78, 0.25, 1)
"""Colour of the highlights."""
SOFT = (0.85, 0.8, 0.72, 1)
"""Colour of the secondary texts."""
FONT = "OpenSans-ExtraBold"
"""Font of the titles and of the buttons."""
MONO = "JetBrainsMono-Regular"
"""Font of the counters."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
CONTROLS = (
    "WALK          W A S D, arrows, left stick\n"
    "SWORD         Space, left click, A\n"
    "USE / TALK    E, Enter, right click, X\n"
    "PAUSE         Escape, P, Start"
)
"""The controls, in the How to play window of the title and in the pause window."""


# --------------------------------------------------------------------------------------------- tileset, tilemaps


def tileset() -> str:
    """The tileset of the sheet: the walls and the torches collide."""
    lines = [
        "Tileset: dungeon",
        "FormatVersion: 1",
        f"texture: nam:{SHEET}",
        "tileWidth: 16",
        "tileHeight: 16",
        f"columns: {COLUMNS}",
        f"rows: {ROWS}",
        "filterMode: Nearest",
        "tiles:",
    ]
    for name, index in sorted(TILE.items(), key=lambda item: item[1]):
        if name in ("floor", "floor_b", "dark", "dark_b", "wall", "torch"):
            lines.append(f"  - index: {index}")
            if name in ("wall", "torch"):
                lines.append("    collidable: true")
            lines.append(f"    name: {name}")
    return "\n".join(lines) + "\n"


def cells(room: lay.Room) -> dict[tuple[int, int], int]:
    """The tile of each cell of a room: walls and torches, the floor everywhere else."""
    out: dict[tuple[int, int], int] = {}
    for y_, row in enumerate(room.rows):
        for x, c in enumerate(row):
            if c == "#":
                out[(x, y_)] = TILE["wall"]
            elif c == "T":
                out[(x, y_)] = TILE["torch"]
            elif room.dark:
                out[(x, y_)] = TILE["dark_b"] if (x * 7 + y_ * 3) % 6 == 0 else TILE["dark"]
            else:
                out[(x, y_)] = TILE["floor_b"] if (x * 5 + y_ * 11) % 7 == 0 else TILE["floor"]
    return out


def tilemap(name: str, room: lay.Room) -> bytes:
    """A room as a ``.owltilemap`` of one layer, in the binary layout Owl Nest saves."""
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 1)
    terrain = [EMPTY] * CHUNK * CHUNK
    for (x, y_), tile in cells(room).items():
        terrain[y_ * CHUNK + x] = tile
    body += struct.pack("<III", 1, 0, 0) + struct.pack(f"<{CHUNK * CHUNK}H", *terrain)
    body += struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK)) + bytes(CHUNK * CHUNK)
    body += b"\x00"
    payload = body + struct.pack("<I", fnv1a(body))
    header = (
        f"Tilemap: {name}\nFormatVersion: 3\ntilesetPath: tilesets/dungeon.owltileset\nwidth: {lay.WIDTH}\n"
        f"height: {lay.HEIGHT}\ncellSize: 1\nlayers:\n  - name: room\npayloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


# ------------------------------------------------------------------------------------------------------ components


def frame(tile: str, colour: y.Colour = (1, 1, 1, 1)) -> str:
    """A sprite of the dungeon sheet."""
    return y.frames(SHEET, COLUMNS, ROWS, TILE[tile], colour)


def body(kind: str, shape: str, size: tuple[float, float], layer: int, mask: int = 255, sensor: bool = False) -> str:
    """A 2D ``PhysicBody`` without gravity."""
    out = (
        f"PhysicBody:\n  type: {kind}\n  shape: {shape}\n  fixedRotation: true\n"
        f"  colliderSize: {y.vec(size[0], size[1], 1)}\n"
        "  density: 1\n  restitution: 0\n  friction: 0\n  gravityScale: 0\n"
    )
    if sensor:
        out += "  sensor: true\n"
    if layer or mask != 255:
        out += f"  layer: {layer}\n  mask: {mask}\n"
    return out


def light(
    colour: tuple[float, float, float], intensity: float, radius: float, kind: str = "Point", enabled: bool = True
) -> str:
    """A 2D light."""
    return (
        f"Light2D: {{type: {kind}, enabled: {'true' if enabled else 'false'}, color: {y.vec(*colour)},"
        f" intensity: {y.num(intensity)}, radius: {y.num(radius)}, falloff: 2, height: 1, castShadows: false}}\n"
    )


def steering(speed: float) -> str:
    """A ``SteeringAgent`` on the plane of the room."""
    names = ["seek", "flee", "arrive", "pursue", "evade", "wander", "separation", "alignment", "cohesion"]
    names += ["avoidance", "formation"]
    weights = {"wander": 1, "separation": 1, "avoidance": 1}
    weight_text = ", ".join(f"{n}: {y.num(weights.get(n, 0))}" for n in names)
    return (
        f"SteeringAgent:\n  enabled: true\n  plane: Plane2D\n  maxSpeed: {y.num(speed)}\n  maxAcceleration: 14\n"
        f"  radius: 0.4\n  turn: false\n  target: 0\n  targetPoint: [0, 0, 0]\n  weights: {{{weight_text}}}\n"
        "  slowRadius: 0.5\n  panicDistance: 0\n  prediction: 0.5\n  wander: {radius: 1, distance: 1.5, jitter: 3}\n"
        "  flock: ''\n  neighbourRadius: 2\n  separationDistance: 0.8\n  lookAhead: 1\n  leader: 0\n"
        "  slot: [0, 0, -2]\n"
    )


def tree(path: str) -> str:
    """A ``BehaviourTreeAgent`` running a tree of the project."""
    return f"BehaviourTreeAgent:\n  enabled: true\n  tree: {path}\n  overrides: []\n"


EXIT_CELLS = {"N": (7, 0), "S": (7, 8), "E": (14, 4), "W": (0, 4)}
"""The gap of each exit."""
INSIDE = {"N": (7, 1), "S": (7, 7), "E": (13, 4), "W": (1, 4)}
"""Where a hero entering by an exit stands."""
OUTWARD = {"N": (0, 1), "S": (0, -1), "E": (1, 0), "W": (-1, 0)}
"""The world direction out of the room through each exit."""


def room_scene(number: int, room: lay.Room) -> str:
    """One room."""
    entities = y.Entities(7700000000000000000 + number * 10000)
    entities.add("RoomCamera", [y.camera(11.0)], position=(0, -0.6, 5))
    entities.add("Background", [y.background((0.08, 0.05, 0.07, 1))], position=(0, 0, -1), scale=(40, 26, 1))
    entities.add("Room", [f"Tilemap:\n  tilemapPath: tilemaps/room_{number}.owltilemap\n"])
    ambient = 0.14 if room.dark else 0.72
    entities.add("Ambient", [light((0.85, 0.8, 1.0), ambient, 5, "Ambient")])
    braziers = 0
    blocks: list[tuple[int, int]] = []
    for y_, row in enumerate(room.rows):
        for x, c in enumerate(row):
            position = lay.cell_position(x, y_, 0.05)
            if c == "T":
                entities.add("Torch", [light((1.0, 0.65, 0.3), 1.4, 4.5)], position=position)
            elif c in EXIT_CELLS and EXIT_CELLS[c] == (x, y_) or c == "D":
                direction = next(d for d, cell in EXIT_CELLS.items() if cell == (x, y_))
                target = room.exits[direction]
                script = y.script(
                    "scripts/game/exit.owlvs",
                    [
                        ("target_scene", "string", f"scenes/room_{target}.owl"),
                        ("entry", "string", f"Spawn{lay.OPPOSITE[direction]}"),
                    ],
                )
                if c == "D":
                    # Behind a door the sensor is a strip along the outer edge: the hero against the shut door is not
                    # in it.
                    dx, dy = OUTWARD[direction]
                    sensor = body("Static", "Box", (1 - 0.7 * abs(dx), 1 - 0.7 * abs(dy)), 3, 255, True)
                    spot = (position[0] + 0.35 * dx, position[1] + 0.35 * dy, position[2])
                    entities.add(f"Exit{direction}", [sensor, script], position=spot)
                else:
                    entities.add(
                        f"Exit{direction}", [body("Static", "Box", (1, 1), 3, 255, True), script], position=position
                    )
                if c == "D":
                    entities.add(
                        "Door",
                        [frame("door"), body("Static", "Box", (1, 1), 0)],
                        position=lay.cell_position(x, y_, 0.06),
                    )
            elif c == "O":
                entities.add("OldMan", [frame("old_man"), body("Static", "Box", (0.8, 0.8), 0)], position=position)
            elif c == "C":
                entities.add("Chest", [frame("chest"), body("Static", "Box", (0.9, 0.8), 0)], position=position)
            elif c == "A":
                entities.add("SwordStand", [frame("sword"), body("Static", "Box", (0.6, 0.8), 0)], position=position)
            elif c == "B":
                braziers += 1
                components = [frame("torch", (0.35, 0.35, 0.4, 1)), body("Static", "Box", (0.8, 0.8), 0)]
                tag = f"Brazier{braziers}"
                root = entities.add(tag, components, position=position)
                entities.add(f"{tag}Light", [light((1.0, 0.6, 0.25), 1.8, 5.0, enabled=False)], parent=root)
            elif c == "K":
                blocks.append((x, y_))
                entities.add(
                    "Block",
                    [frame("wall", (0.85, 0.85, 0.95, 1)), body("Kinematic", "Box", (0.96, 0.96), 0)],
                    position=position,
                )
            elif c == "P":
                entities.add("Plate", [y.sprite("textures/fx/plate.png")], position=lay.cell_position(x, y_, 0.02))
            elif c in CREATURES:
                tag, tile, hits, speed = CREATURES[c]
                room.enemies[tag] = room.enemies.get(tag, 0) + 1
                tree_path = "scripts/game/bat.owlbt" if tag == "Bat" else "scripts/game/creature.owlbt"
                entities.add(
                    f"{tag}{room.enemies[tag]}",
                    [
                        frame(tile),
                        body("Kinematic", "Sphere", (0.7, 0.7), 2),
                        steering(speed),
                        tree(tree_path),
                        y.script("scripts/game/creature.owlvs", [("hits", "float", float(hits))]),
                    ],
                    position=position,
                )
            elif c == "Z":
                entities.add(
                    "Guardian",
                    [
                        frame("guardian"),
                        body("Kinematic", "Sphere", (0.8, 0.8), 2),
                        steering(2.0),
                        tree("scripts/game/guardian.owlbt"),
                        y.script("scripts/game/guardian.owlvs"),
                    ],
                    position=position,
                    scale=(2, 2, 1),
                )
            elif c == "H":
                icon = (
                    "AnimatedSpriteRenderer:\n  color: [1, 1, 1, 1]\n  texture: nam:textures/hud/icons.png\n"
                    "  columns: 4\n  rows: 1\n  firstFrame: 0\n  lastFrame: 0\n  frameDuration: 0.1\n  loop: false\n"
                )
                entities.add(
                    "Heart",
                    [
                        icon,
                        body("Static", "Sphere", (0.8, 0.8), 3, 255, True),
                        y.script("scripts/game/heart.owlvs"),
                        light((1, 0.3, 0.35), 1.5, 3.0),
                    ],
                    position=position,
                    scale=(1.4, 1.4, 1),
                    visible=False,
                )
    for direction, (x, y_) in INSIDE.items():
        entities.add(f"Spawn{direction}", [], position=lay.cell_position(x, y_, 0.1))
    start = lay.cell_position(7, 6, 0.1)
    entities.add("SpawnStart", [], position=start)
    player_script = y.script("scripts/game/player.owlvs")
    player = entities.add(
        "Player",
        [y.frames("textures/fx/hero.png", 4, 1, 0), body("Dynamic", "Sphere", (0.72, 0.72), 1), player_script],
        position=start,
    )
    entities.add(
        "PlayerLight", [light((1.0, 0.85, 0.6), 1.0 if room.dark else 0.5, 3.2 if room.dark else 2.5)], parent=player
    )
    entities.add("Slash", [y.sprite("textures/fx/slash.png")], position=(0.8, 0, 0.1), visible=False)
    entities.add("Blade", [frame("sword")], position=(0.6, 0, 0.25), visible=False)
    entities.add("RoomMusic", [y.music(room.music, 0.45)])
    following = "scenes/victory.owl"
    properties = [
        ("room", "int", number),
        ("room_name", "string", room.name),
        ("this_scene", "string", f"scenes/room_{number}.owl"),
        ("door_rule", "string", room.door_rule or "none"),
        ("braziers", "int", max(braziers, 1)),
        ("end_scene", "string", following),
    ]
    script = y.script("scripts/game/room.owlvs", properties).replace("LuaScript:", "SceneScript:")
    header = (
        script
        + "EnabledRenderers:\n  - {Name: world, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
        + "Physics:\n  solverSubSteps: 1\n  interpolate: true\n  workerCount: 0\n  gravity: [0, 0, 0]\n"
    )
    return y.scene(f"room_{number}", entities, "huds/play.owlhud", header)


# ------------------------------------------------------------------------------------------------- trees, dialogue


def creature_tree(bat: bool) -> str:
    """A creature: back off when hit, chase the hero when close (a bat swoops, then rests), else wander."""
    t = TreeWriter(3970000000000000000 + (1000 if bat else 0))
    t.key("player", "Entity", "0", "The hero, written by creature.owlvs.")
    t.key("hurt", "Bool", "false", "Set when it is hit: it backs off.")
    root = t.node("root", None, 0)
    choose = t.node(
        "composite.selector", root, 1, "The first branch that holds wins, checked at every tick.", reactive="true"
    )
    flee = t.node("composite.sequence", choose, 2, "Hit: back off for a moment.")
    t.node("condition.compare", flee, 3, key="hurt", op="'=='", value="true")
    t.node("action.steer", flee, 3, behaviour="flee", key="player", weight=1)
    t.node("action.wait", flee, 3, seconds=0.8 if not bat else 1.4)
    t.node("action.set", flee, 3, key="hurt", value="false")
    chase = t.node("composite.sequence", choose, 2, "Close: go for the hero.")
    t.node("condition.distance", chase, 3, key="player", op="closer than", distance=6 if bat else 4, space="plane")
    if bat:
        cooldown = t.node("decorator.cooldown", chase, 3, "A swoop, then a rest.", seconds=2.5)
        swoop = t.node("composite.sequence", cooldown, 4)
        t.node("action.steer", swoop, 5, behaviour="pursue", key="player", weight=1)
        t.node("action.wait", swoop, 5, seconds=1.2)
    else:
        t.node("action.steer", chase, 3, behaviour="pursue", key="player", weight=1)
        t.node("action.wait", chase, 3, seconds=0.3)
    roam = t.node("composite.sequence", choose, 2, "Alone: wander.")
    t.node("action.steer", roam, 3, behaviour="wander", weight=1)
    t.node("action.wait", roam, 3, seconds=1, spread=1)
    return t.text()


def guardian_tree() -> str:
    """The guardian: back off when hit, else chase the hero; its speed grows with its phase (guardian.owlvs)."""
    t = TreeWriter(3972000000000000000)
    t.key("player", "Entity", "0", "The hero, written by guardian.owlvs.")
    t.key("hurt", "Bool", "false", "Set when it is hit: it backs off.")
    root = t.node("root", None, 0)
    choose = t.node("composite.selector", root, 1, "Checked at every tick.", reactive="true")
    flee = t.node("composite.sequence", choose, 2, "Hit: back off.")
    t.node("condition.compare", flee, 3, key="hurt", op="'=='", value="true")
    t.node("action.steer", flee, 3, behaviour="flee", key="player", weight=1)
    t.node("action.wait", flee, 3, seconds=1.0)
    t.node("action.set", flee, 3, key="hurt", value="false")
    chase = t.node("composite.sequence", choose, 2, "Chase the hero.")
    t.node("action.steer", chase, 3, behaviour="pursue", key="player", weight=1)
    t.node("action.wait", chase, 3, seconds=0.4)
    return t.text()


DIALOGUE = [
    "Ah, a visitor! Few come down to the Sunken Shrine any more.",
    "Its guardian sleeps behind three doors. A key, fire and stone open them.",
    "The key lies in the pit to the north. In the armoury to the east, a sword waits for a hand.",
    "Go, and may the light of the braziers guide you.",
]
"""What the old man says."""


def dialogue() -> str:
    """The old man's lines, one after the other."""
    base = 7790000000000000000
    lines = [
        'Dialogue: ""',
        "FormatVersion: 1",
        "nodes:",
        f"  - id: {base}",
        "    kind: Start",
        "    position: [0, 120]",
        f"    next: {base + 1}",
    ]
    for index, text in enumerate(DIALOGUE, start=1):
        lines += [
            f"  - id: {base + index}",
            "    kind: Line",
            f"    position: [{index * 260}, 120]",
            "    speaker: {text: Old man, key: dialogue.old_man.name}",
            f"    text: {{text: {y.quote(text)}, key: dialogue.old_man.line_{index}}}",
            f"    next: {base + index + 1}",
        ]
    lines += [
        f"  - id: {base + len(DIALOGUE) + 1}",
        "    kind: End",
        f"    position: [{(len(DIALOGUE) + 1) * 260}, 120]",
    ]
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------------------------------------------------------- HUD


def _label(value: str, size: int, colour: y.Colour = TEXT, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.14, 0.07, 0.1, 1), outline_width=2)


LOOK = mk.Look(
    prefix="shrine",
    title_font="MedievalSharp",
    mono_font=MONO,
    text=TEXT,
    soft=SOFT,
    accent=ACCENT,
    danger=(1, 0.35, 0.35, 1),
    button=(0.18, 0.12, 0.16),
    button_hover=(0.3, 0.2, 0.26, 1),
    button_pressed=(0.38, 0.26, 0.32, 1),
    window_body=(0.1, 0.07, 0.09, 0.96),
    window_title=(0.3, 0.18, 0.22, 1),
    outline=(0.14, 0.07, 0.1, 1),
    transition="fade",
    sweep=(0.05, 0.02, 0.04, 1),
    header="",
    backdrop=lambda _e, _n: None,
    title_art="textures/ui/title.png",
    frame="textures/ui/frame.png",
    toggles=[("shrine_gentle", "GENTLE CREATURES"), ("shrine_bright", "BRIGHTER DARK ROOM")],
    first_scene="scenes/room_1.owl",
    save_slot=1,
    # A storybook: the title large over the hall, the choices in a framed box at the foot of the screen.
    layout=mk.Layout(
        title=((0.5, 0), (1.28, 0.32), (0, 0.04)),
        best=((0.5, 0), (0.9, 0.05), (0, 0.39)),
        buttons=((0.5, 1), (0.62, 0.46), (0, -0.05)),
        button_width=0.86,
        button_height=0.14,
        button_spacing=4,
        label_size=30,
        box=((0.12, 0.08, 0.06, 0.92), (1, 0.78, 0.25, 0.9)),
        box_padding=8,
    ),
)
"""The look of the game, the backdrop set by ``generate_menus``."""


def icon(frame_index: int) -> str:
    """A ``UiImage`` of a frame of the HUD icons."""
    return (
        "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/hud/icons.png\n  columns: 4\n  rows: 1\n"
        f"  frame: {frame_index}\n"
    )


def hud() -> str:
    """Hearts, key and sword at the top left, the name of the room at the top right, a message line, the dialogue box
    and the pause window (the only place of a room showing the controls)."""
    entities = y.Entities(7799000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    row = entities.add(
        "Hearts",
        [y.rect((0, 0), (0.6, 0.06), (0.03, 0.025), SCREEN, SCREEN), y.layout("Row", 4, cross="Start")],
        parent=root,
    )
    for index in reversed(range(1, 6)):
        entities.add(f"Heart{index}", [y.child_rect((0.09, 1)), icon(0)], parent=row)
    items = entities.add(
        "Items",
        [y.rect((0, 0), (0.3, 0.06), (0.03, 0.095), SCREEN, SCREEN), y.layout("Row", 6, cross="Start")],
        parent=root,
    )
    entities.add("SwordIcon", [y.child_rect((0.2, 1)), icon(3)], parent=items, visible=False)
    entities.add("KeyIcon", [y.child_rect((0.2, 1)), icon(2)], parent=items, visible=False)
    entities.add(
        "RoomText",
        [y.rect((1, 0), (0.6, 0.06), (-0.03, 0.025), SCREEN, SCREEN), _label("", 24, SOFT, "Right")],
        parent=root,
    )
    entities.add(
        "Message",
        [y.rect((0.5, 0), (1.2, 0.07), (0, 0.22), SCREEN, SCREEN), _label("", 30, ACCENT)],
        parent=root,
        visible=False,
    )
    box = entities.add(
        "DialogueBox",
        [
            y.rect((0.5, 1), (1.2, 0.2), (0, -0.04), SCREEN, SCREEN),
            y.panel((0.08, 0.05, 0.07, 0.92), (1, 0.78, 0.25, 0.8), 2),
            y.layout("Column", 2, 6, "Start"),
        ],
        parent=root,
        visible=False,
    )
    entities.add("DialogueText", [y.child_rect((0.94, 0.5)), y.text("", 22, "Left", TEXT, wrap=True)], parent=box)
    entities.add("DialogueSpeaker", [y.child_rect((0.94, 0.22)), _label("", 22, ACCENT, "Left")], parent=box)
    pause = mk.window(LOOK, entities, root, "PauseWindow", "PAUSED", (1.0, 0.8))
    for index, (tag, text, callback) in reversed(
        list(
            enumerate(
                [("ResumeButton", "RESUME", "on_resume_clicked"), ("QuitButton", "QUIT TO TITLE", "on_quit_clicked")]
            )
        )
    ):
        mk.button(LOOK, entities, pause, tag, text, callback, (0.6, 0.1), index == 0)
    controls = y.text(CONTROLS, 20, "Left", ACCENT, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("PauseControls", [y.child_rect((0.94, 0.45)), controls], parent=pause)
    return y.hud_file("play", entities)


# ---------------------------------------------------------------------------------------------------------- graphs

MODULE = "scripts.game.shrine"
"""The Lua module of the block and the names."""


def _helpers(g: Graph) -> tuple:
    def find(tag: object) -> Pin:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    def widget(tag: str) -> Pin:
        return g.call("ui.find", name=tag)["entity_id"]

    def pressed(action: str) -> Pin:
        return g.call("input.is_action_just_pressed", action=action)["pressed"]

    def state(key: str, default: object) -> Pin:
        return g.call("gamestate.get", key=key, default=default)["value"]

    return find, widget, pressed, state


def room_graph() -> str:
    """A room: where the hero comes in, what the room remembers, the door of the room, the old man, the hearts, the
    fall and the end of the quest."""
    g = Graph(6700000000000000000)
    g.var("room", "int", 1)
    g.var("room_name", "string", "THE HALL OF ECHOES")
    g.var("this_scene", "string", "scenes/room_1.owl")
    g.var("door_rule", "string", "none")
    g.var("braziers", "int", 4, low=1)
    g.var("end_scene", "string", "scenes/victory.owl")
    g.var("max_hearts", "int", 5, low=1)
    for name, kind, default in (
        ("conversation", "int", 0),
        ("talking", "bool", False),
        ("paused", "bool", False),
        ("leaving", "bool", False),
        ("down_time", "float", -1.0),
        ("message_time", "float", 0.0),
        ("door_open", "bool", False),
        ("heart_shown", "bool", False),
    ):
        g.var(name, kind, default, exposed=False)
    find, widget, pressed, state = _helpers(g)

    def call(function: str, **values: object) -> Node:
        return g.node("func.call", function=function, **values)

    def room_key(prefix: str) -> Pin:
        return g.op("text.concat", prefix, g.get("room"))

    # ---------------------------------------------------------------- on_create
    create = g.event("on_create")
    hearts_set = g.branch(g.op("compare.less_equal", state("shrine_hearts", 0), 0))
    g.chain(create, hearts_set)
    filled = g.chain(hearts_set["True"], g.call("gamestate.set", key="shrine_hearts", value=g.get("max_hearts")))
    entry = state("shrine_entry", "SpawnStart")
    spot = g.node("math.break_vector3", Value=g.call("transform.get_position", entity_id=find(entry))["xyz"])
    place = g.join(
        [hearts_set["False"], filled],
        g.call(
            "physics.set_transform",
            entity_id=find("Player"),
            xy=g.node("math.make_vector2", X=spot["X"], Y=spot["Y"])["Result"],
            rotation_or_z=0,
        ),
    )
    g.chain(
        place,
        g.call(
            "transform.set_position",
            entity_id=find("Player"),
            xyz=g.call("transform.get_position", entity_id=find(state("shrine_entry", "SpawnStart")))["xyz"],
        ),
        g.call("gamestate.set", key="shrine_room", value=g.get("room")),
        g.call("gamestate.set", key="shrine_in_room", value=0),
        g.call("gamestate.set", key="shrine_state", value="play"),
        g.call("gamestate.set", key="shrine_use", value="none"),
        g.call("gamestate.set", key="shrine_paused", value=False),
        g.call("ui.set_text", entity_id=widget("RoomText"), text=g.get("room_name")),
        call("say", text=g.get("room_name")),
        call("restore"),
        g.call("gamestate.set", key="shrine_retry", value=g.get("this_scene")),
        g.call("save.save_game", slot=1),
    )

    # restore(): what the room remembers (the chest, the sword, the door, the braziers, the block, the guardian).
    g.function("restore")
    entry_node = g.entry("restore")
    chest = g.branch(state("shrine_chest", False))
    g.chain(entry_node, chest)
    opened = g.chain(
        chest["True"], g.call("isometric.set_sprite_frame", entity_id=find("Chest"), frame=TILE["chest_open"])
    )
    sword = g.join(
        [chest["False"], opened],
        g.branch(g.op("logic.and", state("shrine_sword", False), g.op("compare.not_equal", find("SwordStand"), 0))),
    )
    taken = g.chain(sword["True"], g.call("scene.destroy_entity", entity_id=find("SwordStand")))
    door = g.join(
        [sword["False"], taken], g.branch(g.call("gamestate.get", key=room_key("shrine_open_"), default=False)["value"])
    )
    unlocked = g.chain(door["True"], g.set("door_open", True), g.call("scene.destroy_entity", entity_id=find("Door")))
    loop = g.join([door["False"], unlocked], g.node("flow.for", First=1, Last=g.get("braziers")))
    lit_key = g.op("text.concat", "shrine_lit_", loop["Index"])
    lit = g.branch(g.call("gamestate.get", key=lit_key, default=False)["value"])
    g.chain(loop["Body"], lit)
    g.chain(lit["True"], call("light_brazier", index=loop["Index"], quiet=True))
    block = g.branch(g.op("logic.and", state("shrine_solved", False), g.op("compare.not_equal", find("Block"), 0)))
    g.chain(loop["Completed"], block)
    plate = g.node("math.break_vector3", Value=g.call("transform.get_position", entity_id=find("Plate"))["xyz"])
    moved = g.chain(
        block["True"],
        g.call(
            "physics.set_transform",
            entity_id=find("Block"),
            xy=g.node("math.make_vector2", X=plate["X"], Y=plate["Y"])["Result"],
            rotation_or_z=0,
        ),
        g.call(
            "transform.set_position",
            entity_id=find("Block"),
            xyz=g.call("transform.get_position", entity_id=find("Plate"))["xyz"],
        ),
    )
    guardian = g.join(
        [block["False"], moved],
        g.branch(g.op("logic.and", state("shrine_guardian", False), g.op("compare.not_equal", find("Guardian"), 0))),
    )
    slain = g.chain(guardian["True"], g.call("scene.destroy_entity", entity_id=find("Guardian")))
    ambient = find("Ambient")
    dim = g.op("compare.less", g.call("entity.get_light_intensity", entity_id=ambient)["intensity"], 0.3)
    bright = g.join(
        [guardian["False"], slain],
        g.branch(g.op("logic.and", dim, g.call("settings.get", key="shrine_bright", default=False)["value"])),
    )
    brighter = g.chain(bright["True"], g.call("entity.set_light_intensity", entity_id=find("Ambient"), intensity=0.45))
    g.join([bright["False"], brighter], g.node("func.return", function="restore"))

    # ---------------------------------------------------------------- on_update
    update = g.event("on_update")
    toggle = call("set_paused", paused=g.op("logic.not", g.get("paused")))
    pause_branch = g.branch(
        g.op("logic.and", pressed("pause"), g.op("compare.equal", state("shrine_state", "play"), "play"))
    )
    g.chain(update, pause_branch, toggle)
    running = g.join([pause_branch["False"], toggle], g.branch(g.get("paused")))
    nodes = [
        g.set("message_time", g.op("math.subtract", g.get("message_time"), update["dt"])),
        g.call("gamestate.set", key="shrine_time", value=g.op("math.add", state("shrine_time", 0), update["dt"])),
        g.call("gamestate.set", key="shrine_in_room", value=g.op("math.add", state("shrine_in_room", 0), update["dt"])),
        g.call("ui.set_visible", entity_id=widget("KeyIcon"), visible=state("shrine_key", False)),
        g.call("ui.set_visible", entity_id=widget("SwordIcon"), visible=state("shrine_sword", False)),
    ]
    g.chain(running["False"], *nodes)
    hearts = g.node("flow.for", First=1, Last=g.get("max_hearts"))
    g.chain(nodes[-1], hearts)
    full = g.op("compare.less_equal", hearts["Index"], state("shrine_hearts", 0))
    heart_frame = g.node("logic.select", Condition=full, **{"True": 0, "False": 1})["Result"]
    g.chain(
        hearts["Body"],
        g.call(
            "ui.set_image_frame",
            entity_id=g.call("ui.find", name=g.op("text.concat", "Heart", hearts["Index"]))["entity_id"],
            frame=heart_frame,
        ),
    )
    steps = g.sequence()
    g.chain(hearts["Completed"], steps)
    expired = g.branch(
        g.op("logic.and", g.op("compare.less", g.get("message_time"), 0), g.op("logic.not", g.get("talking")))
    )
    g.chain(steps["Then 1"], expired, g.call("ui.set_visible", entity_id=widget("Message"), visible=False))
    g.chain(steps["Then 2"], call("use"))
    g.chain(steps["Then 3"], call("rules", dt=update["dt"]))

    # use(): what the hero used (the old man, the chest, the sword, a brazier, the block).
    g.function("use")
    entry_node = g.entry("use")
    used = state("shrine_use", "none")
    something = g.branch(g.op("compare.not_equal", used, "none"))
    g.chain(entry_node, something)
    used_name = g.call("gamestate.get", key="shrine_use", default="none")["value"]
    used_kind = g.lua(MODULE, "kind", pure=True, name=used_name)["kind"]
    # The kind is kept as `shrine_used` before the key is cleared.
    copy = g.call("gamestate.set", key="shrine_used", value=used_kind)
    clear = g.call("gamestate.set", key="shrine_use", value="none")
    g.chain(something["True"], copy, clear)
    what = state("shrine_used", "none")
    talk = g.branch(g.op("compare.equal", what, "OldMan"))
    g.chain(clear, talk)
    talked = g.chain(talk["True"], call("talk"))
    chest_branch = g.branch(
        g.op(
            "logic.and",
            g.op("compare.equal", state("shrine_used", "none"), "Chest"),
            g.op("logic.not", state("shrine_chest", False)),
        )
    )
    g.chain(talk["False"], chest_branch)
    opened = g.chain(
        chest_branch["True"],
        g.call("isometric.set_sprite_frame", entity_id=find("Chest"), frame=TILE["chest_open"]),
        g.call("gamestate.set", key="shrine_chest", value=True),
        g.call("gamestate.set", key="shrine_key", value=True),
        g.call("sound.play", asset_path="sounds/game/chest.wav"),
        g.call("ui.show_subtitle", text="[the chest opens]", seconds=2),
        call("say", text="YOU FOUND A KEY"),
    )
    sword_branch = g.branch(g.op("compare.equal", state("shrine_used", "none"), "SwordStand"))
    g.chain(chest_branch["False"], sword_branch)
    took = g.chain(
        sword_branch["True"],
        g.call("gamestate.set", key="shrine_sword", value=True),
        g.call("scene.destroy_entity", entity_id=find("SwordStand")),
        g.call("sound.play", asset_path="sounds/game/pickup.wav"),
        call("say", text="YOU TOOK THE SWORD"),
    )
    brazier_branch = g.branch(g.op("compare.equal", state("shrine_used", "none"), "Brazier"))
    g.chain(sword_branch["False"], brazier_branch)
    number = g.lua(
        MODULE, "number", pure=True, name=g.call("gamestate.get", key="shrine_use_name", default="Brazier0")["value"]
    )["number"]
    burnt = g.chain(brazier_branch["True"], call("light_brazier", index=number, quiet=False))
    block_branch = g.branch(g.op("compare.equal", state("shrine_used", "none"), "Block"))
    g.chain(brazier_branch["False"], block_branch)
    face = g.node("math.make_vector2", X=state("shrine_face_x", 0), Y=state("shrine_face_y", -1))["Result"]
    push = g.lua(MODULE, "push", block=find("Block"), direction=face)
    g.chain(block_branch["True"], push)
    pushed = g.branch(push["moved"])
    g.chain(push, pushed)
    rumbled = g.chain(
        pushed["True"],
        g.call("sound.play", asset_path="sounds/game/push.wav"),
        g.call("gamestate.set", key="shrine_pushes", value=g.op("math.add", state("shrine_pushes", 0), 1)),
    )
    on_plate = g.join(
        [pushed["False"], rumbled],
        g.branch(g.lua(MODULE, "same_cell", pure=True, a=find("Block"), b=find("Plate"))["same"]),
    )
    solved = g.chain(on_plate["True"], g.call("gamestate.set", key="shrine_solved", value=True))
    saved = g.join([opened, took, burnt, solved], g.call("save.save_game", slot=1))
    g.join(
        [something["False"], talked, saved, block_branch["False"], on_plate["False"]],
        g.node("func.return", function="use"),
    )

    # light_brazier(index, quiet).
    g.function("light_brazier", [("index", "integer"), ("quiet", "boolean")])
    entry_node = g.entry("light_brazier")
    tag = g.op("text.concat", "Brazier", entry_node["index"])
    nodes = [
        g.call("entity.set_tint", entity_id=g.call("scene.find_entity", name=tag)["entity_id"], rgba=(1, 1, 1, 1)),
        g.call(
            "entity.set_light_enabled",
            entity_id=g.call(
                "scene.find_entity",
                name=g.op("text.concat", g.op("text.concat", "Brazier", entry_node["index"]), "Light"),
            )["entity_id"],
            enabled=True,
        ),
        g.call("gamestate.set", key=g.op("text.concat", "shrine_lit_", entry_node["index"]), value=True),
    ]
    loud = g.branch(g.op("logic.not", entry_node["quiet"]))
    g.chain(entry_node, *nodes, loud)
    heard = g.chain(
        loud["True"],
        g.call("sound.play", asset_path="sounds/game/brazier.wav"),
        g.call("ui.show_subtitle", text="[fire]", seconds=1.5),
    )
    g.join([loud["False"], heard], g.node("func.return", function="light_brazier"))

    # talk(): the old man, a line at each use.
    g.function("talk")
    entry_node = g.entry("talk")
    going = g.branch(g.get("talking"))
    g.chain(entry_node, going)
    begun = g.chain(
        going["False"],
        g.set("conversation", g.call("dialogue.start", dialogue="dialogues/old_man.owldlg")["conversation"]),
        g.set("talking", True),
    )
    stepped = g.chain(going["True"], g.call("dialogue.next", conversation=g.get("conversation")))
    still = g.join(
        [begun, stepped], g.branch(g.call("dialogue.is_running", conversation=g.get("conversation"))["running"])
    )
    line = g.call("dialogue.get_line", conversation=g.get("conversation"))["line"]
    shown = g.chain(
        still["True"],
        g.call("ui.set_visible", entity_id=widget("DialogueBox"), visible=True),
        g.call(
            "ui.set_text", entity_id=widget("DialogueText"), text=g.node("table.get", Table=line, Key="text")["Result"]
        ),
        g.call(
            "ui.set_text",
            entity_id=widget("DialogueSpeaker"),
            text=g.node(
                "table.get",
                Table=g.call("dialogue.get_line", conversation=g.get("conversation"))["line"],
                Key="speaker",
            )["Result"],
        ),
        g.call("sound.play", asset_path="sounds/game/voice.wav"),
        g.call("gamestate.set", key="shrine_talking", value=True),
    )
    ended = g.chain(
        still["False"],
        g.set("talking", False),
        g.call("ui.set_visible", entity_id=widget("DialogueBox"), visible=False),
        g.call("gamestate.set", key="shrine_talking", value=False),
        g.call("gamestate.set", key="shrine_talked", value=True),
    )
    g.join([shown, ended], g.node("func.return", function="talk"))

    # rules(dt): the door of the room, the fall of the hero, the end of the quest.
    g.function("rules", [("dt", "number")])
    entry_node = g.entry("rules")
    rule = g.get("door_rule")
    key_rule = g.op(
        "logic.and",
        g.op("logic.and", g.op("compare.equal", rule, "key"), state("shrine_key", False)),
        g.op("compare.less", _near_door(g), 1.6),
    )
    fire_rule = g.op(
        "logic.and",
        g.op("compare.equal", g.get("door_rule"), "braziers"),
        g.op(
            "compare.greater_equal", g.lua(MODULE, "lit", pure=True, count=g.get("braziers"))["lit"], g.get("braziers")
        ),
    )
    block_rule = g.op("logic.and", g.op("compare.equal", g.get("door_rule"), "block"), state("shrine_solved", False))
    open_now = g.op(
        "logic.and",
        g.op("logic.not", g.get("door_open")),
        g.op("logic.or", key_rule, g.op("logic.or", fire_rule, block_rule)),
    )
    opening = g.branch(open_now)
    g.chain(entry_node, opening)
    consumed = g.node(
        "logic.select",
        Condition=g.op("compare.equal", g.get("door_rule"), "key"),
        **{"True": False, "False": state("shrine_key", False)},
    )["Result"]
    opened = g.chain(
        opening["True"],
        g.set("door_open", True),
        g.call("scene.destroy_entity", entity_id=find("Door")),
        g.call("gamestate.set", key=room_key("shrine_open_"), value=True),
        g.call("gamestate.set", key="shrine_key", value=consumed),
        g.call("sound.play", asset_path="sounds/game/door.wav"),
        g.call("ui.show_subtitle", text="[a door grinds open]", seconds=2),
        call("say", text="A DOOR OPENS"),
    )
    heart = g.join(
        [opening["False"], opened],
        g.branch(
            g.op(
                "logic.and",
                g.op("logic.not", g.get("heart_shown")),
                g.op("logic.and", g.op("compare.equal", g.get("door_rule"), "boss"), state("shrine_guardian", False)),
            )
        ),
    )
    revealed = g.chain(
        heart["True"],
        g.set("heart_shown", True),
        g.call("ui.set_visible", entity_id=find("Heart"), visible=True),
        call("say", text="THE GUARDIAN FALLS"),
    )
    fallen = g.join(
        [heart["False"], revealed],
        g.branch(
            g.op(
                "logic.and",
                g.op("compare.less_equal", state("shrine_hearts", 1), 0),
                g.op("compare.less", g.get("down_time"), 0),
            )
        ),
    )
    downed = g.chain(
        fallen["True"],
        g.set("down_time", 0),
        g.call("gamestate.set", key="shrine_state", value="down"),
        g.call("gamestate.set", key="shrine_deaths", value=g.op("math.add", state("shrine_deaths", 0), 1)),
        g.call("sound.play", asset_path="sounds/ui/game_over.wav"),
        call("say", text="YOU FALL..."),
    )
    rising = g.join(
        [fallen["False"], downed],
        g.branch(
            g.op("logic.and", g.op("compare.greater_equal", g.get("down_time"), 0), g.op("logic.not", g.get("leaving")))
        ),
    )
    clock = g.set("down_time", g.op("math.add", g.get("down_time"), entry_node["dt"]))
    g.chain(rising["True"], clock)
    again = g.branch(g.op("compare.greater", g.get("down_time"), 2.0))
    g.chain(clock, again)
    reloaded = g.chain(
        again["True"],
        g.set("leaving", True),
        g.call("gamestate.set", key="shrine_hearts", value=g.get("max_hearts")),
        g.call("gamestate.set", key="shrine_line_1", value=g.fmt("YOU FELL IN %s", g.get("room_name"))),
        g.call("gamestate.set", key="shrine_line_2", value=g.fmt("CREATURES        %d", state("shrine_kills", 0))),
        g.call("gamestate.set", key="shrine_line_3", value=g.fmt("FALLS            %d", state("shrine_deaths", 0))),
        g.call("gamestate.set", key="shrine_score", value=0),
        g.call("gamestate.set", key="shrine_base", value=0),
        g.call("scene.transition_to", scene_path="scenes/game_over.owl", kind="fade", duration=0.6, rgba=LOOK.sweep),
    )
    won = g.join(
        [rising["False"], again["False"], reloaded],
        g.branch(g.op("logic.and", state("shrine_won", False), g.op("logic.not", g.get("leaving")))),
    )
    finished = g.chain(won["True"], call("finish"))
    g.join([won["False"], finished], g.node("func.return", function="rules"))

    # finish(): the score and the tally of the quest.
    g.function("finish")
    entry_node = g.entry("finish")
    seconds = g.node("math.floor", A=state("shrine_time", 0))["Result"]
    score = g.node(
        "math.max",
        A=0,
        B=g.op(
            "math.add",
            g.op(
                "math.subtract",
                20000,
                g.op(
                    "math.add",
                    g.op("math.multiply", g.node("math.floor", A=state("shrine_time", 0))["Result"], 20),
                    g.op("math.multiply", state("shrine_deaths", 0), 1500),
                ),
            ),
            g.op("math.multiply", state("shrine_kills", 0), 100),
        ),
    )["Result"]
    g.chain(
        entry_node,
        g.set("leaving", True),
        g.call("gamestate.set", key="shrine_score", value=score),
        g.call("gamestate.set", key="shrine_base", value=0),
        g.call("gamestate.set", key="shrine_line_1", value="THE HEART OF THE SHRINE IS YOURS"),
        g.call(
            "gamestate.set",
            key="shrine_line_2",
            value=g.fmt(
                "TIME             %d:%02d",
                g.node("math.floor", A=g.op("math.divide", seconds, 60))["Result"],
                g.op("math.modulo", g.node("math.floor", A=state("shrine_time", 0))["Result"], 60),
            ),
        ),
        g.call("gamestate.set", key="shrine_line_3", value=g.fmt("CREATURES        %d", state("shrine_kills", 0))),
        g.call("gamestate.set", key="shrine_line_4", value=g.fmt("FALLS            %d", state("shrine_deaths", 0))),
        g.call("scene.transition_to", scene_path=g.get("end_scene"), kind="fade", duration=1.0, rgba=(1, 0.95, 0.8, 1)),
        g.node("func.return", function="finish"),
    )

    # say(text).
    g.function("say", [("text", "string")])
    entry_node = g.entry("say")
    g.chain(
        entry_node,
        g.call("ui.set_text", entity_id=widget("Message"), text=entry_node["text"]),
        g.call("ui.set_visible", entity_id=widget("Message"), visible=True),
        g.set("message_time", 3.0),
        g.call("gamestate.set", key="shrine_message", value=entry_node["text"]),
        g.node("func.return", function="say"),
    )

    # set_paused(paused).
    g.function("set_paused", [("paused", "boolean")])
    entry_node = g.entry("set_paused")
    show = g.branch(entry_node["paused"])
    g.chain(
        entry_node,
        g.set("paused", entry_node["paused"]),
        g.call("time.set_paused", paused=entry_node["paused"]),
        g.call("ui.set_visible", entity_id=widget("PauseWindow"), visible=entry_node["paused"]),
        g.call("gamestate.set", key="shrine_paused", value=entry_node["paused"]),
        show,
    )
    g.chain(
        show["True"],
        g.call("ui.raise_window", entity_id=widget("PauseWindow")),
        g.call("ui.set_focus", entity_id=widget("ResumeButton")),
    )
    resume = g.event("on_resume_clicked")
    g.chain(resume, call("set_paused", paused=False))
    quit_ = g.event("on_quit_clicked")
    g.chain(
        quit_,
        g.call("time.set_paused", paused=False),
        g.call("scene.transition_to", scene_path="scenes/title.owl", kind="fade", duration=0.5, rgba=LOOK.sweep),
    )
    return g.text()


def _near_door(g: Graph) -> Pin:
    """Distance from the hero to the locked door of the room (far when there is none)."""
    return g.lua(
        MODULE,
        "distance",
        pure=True,
        a=g.call("scene.find_entity", name="Player")["entity_id"],
        b=g.call("scene.find_entity", name="Door")["entity_id"],
    )["distance"]


def player_graph() -> str:
    """The hero: walks, strikes with the sword once it has it, uses what is in front of it, is hurt by creatures."""
    g = Graph(6710000000000000000)
    g.var("speed", "float", 4.5, low=0)
    g.var("reach", "float", 0.8, low=0)
    g.var("facing", "vec2", (0, -1), exposed=False)
    g.var("hurt_time", "float", 1.5, exposed=False)
    g.var("strike_time", "float", 0.0, exposed=False)
    find, widget, pressed, state = _helpers(g)

    def playing() -> Pin:
        return g.op(
            "logic.and",
            g.op("compare.equal", state("shrine_state", "play"), "play"),
            g.op("logic.not", state("shrine_talking", False)),
        )

    # The hero enters a room facing the way it walked out of the last one.
    g.chain(
        g.event("on_create"),
        g.set(
            "facing",
            g.node("math.make_vector2", X=state("shrine_face_x", 0), Y=state("shrine_face_y", -1))["Result"],
        ),
    )

    tick = g.event("on_tick")
    walk = g.node("math.break_vector2", Value=g.call("input.get_action_vector", action="move")["xy"])
    direction = g.node("math.make_vector2", X=walk["X"], Y=g.op("math.negate", walk["Y"]))["Result"]
    moving = g.branch(playing())
    g.chain(tick, moving)
    velocity = g.call("physics.set_velocity", velocity=g.op("math.vector2_scale", direction, Factor=g.get("speed")))
    turning = g.branch(g.op("compare.greater", g.node("math.vector2_length", A=direction)["Result"], 0.3))
    g.chain(moving["True"], velocity, turning)
    g.chain(
        turning["True"],
        g.set(
            "facing",
            g.lua(
                "scripts.game.shrine",
                "axis",
                pure=True,
                direction=g.node("math.make_vector2", X=walk["X"], Y=g.op("math.negate", walk["Y"]))["Result"],
            )["axis"],
        ),
    )
    g.chain(moving["False"], g.call("physics.set_velocity", velocity=(0, 0)))

    def hurt_shade() -> Pin:
        """Green and blue of the hero's tint: lowered while it is hurt, a red flash."""
        return g.node(
            "logic.select",
            Condition=g.op("compare.greater", g.get("hurt_time"), 0.6),
            **{"True": 0.4, "False": 1.0},
        )["Result"]

    update = g.event("on_update")
    face = g.node("math.break_vector2", Value=g.get("facing"))
    position = g.node("math.break_vector3", Value=g.call("transform.get_position")["xyz"])
    front = g.node(
        "math.make_vector2",
        X=g.op("math.add", position["X"], g.op("math.multiply", face["X"], g.get("reach"))),
        Y=g.op("math.add", position["Y"], g.op("math.multiply", face["Y"], g.get("reach"))),
    )["Result"]
    nodes = [
        g.set("hurt_time", g.op("math.subtract", g.get("hurt_time"), update["dt"])),
        g.set("strike_time", g.op("math.subtract", g.get("strike_time"), update["dt"])),
        g.call("gamestate.set", key="shrine_face_x", value=face["X"]),
        g.call("gamestate.set", key="shrine_face_y", value=face["Y"]),
        g.call("gamestate.set", key="shrine_x", value=position["X"]),
        g.call("gamestate.set", key="shrine_y", value=position["Y"]),
        g.lua(
            "scripts.game.shrine",
            "pose",
            hero=find("Player"),
            slash=find("Slash"),
            blade=find("Blade"),
            face=g.get("facing"),
            reach=g.get("reach"),
            left=g.get("strike_time"),
            length=0.3,
        ),
        g.call("entity.set_tint", entity_id=None, r=1, g=hurt_shade(), b=hurt_shade(), a=1),
    ]
    g.chain(update, *nodes)
    strike = g.branch(
        g.op(
            "logic.and",
            g.op("logic.and", pressed("attack"), state("shrine_sword", False)),
            g.op("logic.and", g.op("compare.less_equal", g.get("strike_time"), 0), playing()),
        )
    )
    g.chain(nodes[-1], strike)
    hits = g.call("physics.overlap_circle", xy=front, radius=0.75)["entity_ids"]
    each = g.node("flow.for_each", List=hits)
    g.chain(
        strike["True"],
        g.set("strike_time", 0.3),
        g.call("sound.play", asset_path="sounds/game/sword.wav"),
        each,
    )
    g.chain(
        each["Body"],
        g.call(
            "gamestate.set",
            key=g.op("text.concat", "shrine_hit_", g.call("entity.get_name", entity_id=each["Item"])["name"]),
            value=True,
        ),
    )
    use = g.join(
        [strike["False"], each["Completed"]],
        g.branch(g.op("logic.and", pressed("use"), g.op("compare.equal", state("shrine_state", "play"), "play"))),
    )
    usable = g.lua(
        "scripts.game.shrine",
        "usable",
        pure=True,
        ids=g.call("physics.overlap_circle", xy=front, radius=0.6)["entity_ids"],
    )
    chosen = g.chain(
        use["True"],
        g.call("gamestate.set", key="shrine_use", value=usable["name"]),
        g.call(
            "gamestate.set",
            key="shrine_use_name",
            value=g.lua(
                "scripts.game.shrine",
                "usable",
                pure=True,
                ids=g.call("physics.overlap_circle", xy=front, radius=0.6)["entity_ids"],
            )["name"],
        ),
    )
    # Talking goes on with the same key, wherever the hero faces.
    talking = g.join([chosen], g.branch(state("shrine_talking", False)))
    told = g.chain(talking["True"], g.call("gamestate.set", key="shrine_use", value="OldMan"))

    # Creatures touching the hero hurt it, once a second at most (twice that with the gentle creatures).
    touch = g.lua(
        "scripts.game.shrine",
        "creature",
        pure=True,
        ids=g.call(
            "physics.overlap_circle",
            xy=g.node("math.make_vector2", X=position["X"], Y=position["Y"])["Result"],
            radius=0.62,
        )["entity_ids"],
    )["kind"]
    hurt = g.branch(
        g.op(
            "logic.and",
            g.op("compare.not_equal", touch, "none"),
            g.op(
                "logic.and",
                g.op("compare.less_equal", g.get("hurt_time"), 0),
                g.op("compare.equal", state("shrine_state", "play"), "play"),
            ),
        )
    )
    g.join([use["False"], talking["False"], told], hurt)
    gentle = g.call("settings.get", key="shrine_gentle", default=False)["value"]
    damage = g.node(
        "logic.select",
        Condition=g.op(
            "compare.equal",
            g.lua(
                "scripts.game.shrine",
                "creature",
                pure=True,
                ids=g.call(
                    "physics.overlap_circle",
                    xy=g.node("math.make_vector2", X=position["X"], Y=position["Y"])["Result"],
                    radius=0.62,
                )["entity_ids"],
            )["kind"],
            "Guardian",
        ),
        **{"True": 2, "False": 1},
    )["Result"]
    hurt_time = g.node("logic.select", Condition=gentle, **{"True": 2.0, "False": 1.0})["Result"]
    g.chain(
        hurt["True"],
        g.set("hurt_time", hurt_time),
        g.call(
            "gamestate.set",
            key="shrine_hearts",
            value=g.node("math.max", A=0, B=g.op("math.subtract", state("shrine_hearts", 5), damage))["Result"],
        ),
        g.call("sound.play", asset_path="sounds/game/hurt.wav"),
    )
    return g.text()


def creature_graph(guardian: bool) -> str:
    """A creature (or the guardian): takes the blows of the sword, backs off, stays in the room; the guardian grows
    faster by phase and its fall is remembered."""
    g = Graph(6720000000000000000 + (10000000 if guardian else 0))
    g.var("hits", "float", 10.0 if guardian else 2.0, low=1)
    g.var("half_width", "float", 6.2)
    g.var("half_height", "float", 3.2)
    if guardian:
        g.var("phase", "int", 1, exposed=False)
    find, widget, pressed, state = _helpers(g)
    update = g.event("on_update")
    me = g.call("entity.get_name", entity_id=None)["name"]
    key = g.op("text.concat", "shrine_hit_", me)
    where = g.node("math.break_vector3", Value=g.call("transform.get_position")["xyz"])
    nodes = [
        g.call("bt.set_value", key="player", value=find("Player")),
        g.lua(MODULE, "keep_inside", id=None, half_width=g.get("half_width"), half_height=g.get("half_height")),
        g.call("gamestate.set", key=g.op("text.concat", "shrine_x_", me), value=where["X"]),
        g.call(
            "gamestate.set",
            key=g.op("text.concat", "shrine_y_", g.call("entity.get_name", entity_id=None)["name"]),
            value=where["Y"],
        ),
    ]
    g.chain(update, *nodes)
    hit = g.branch(g.call("gamestate.get", key=key, default=False)["value"])
    g.chain(nodes[-1], hit)
    nodes = [
        g.call(
            "gamestate.set",
            key=g.op("text.concat", "shrine_hit_", g.call("entity.get_name", entity_id=None)["name"]),
            value=False,
        ),
        g.set("hits", g.op("math.subtract", g.get("hits"), 1)),
        g.call("bt.set_value", key="hurt", value=True),
        g.call("sound.play", asset_path="sounds/game/hit.wav"),
    ]
    g.chain(hit["True"], *nodes)
    gone = g.branch(g.op("compare.less_equal", g.get("hits"), 0))
    g.chain(nodes[-1], gone)
    defeated = [
        g.call("gamestate.set", key="shrine_kills", value=g.op("math.add", state("shrine_kills", 0), 1)),
        g.call("sound.play", asset_path="sounds/game/defeat.wav"),
    ]
    if guardian:
        defeated += [
            g.call("gamestate.set", key="shrine_guardian", value=True),
            g.call("ui.show_subtitle", text="[the guardian falls]", seconds=2),
        ]
    defeated.append(g.call("scene.destroy_entity"))
    g.chain(gone["True"], *defeated)
    if guardian:
        angry = g.branch(
            g.op("logic.and", g.op("compare.less_equal", g.get("hits"), 6), g.op("compare.equal", g.get("phase"), 1))
        )
        g.chain(gone["False"], angry)
        g.chain(
            angry["True"],
            g.set("phase", 2),
            g.call("steer.set_max_speed", speed=3.4),
            g.call("sound.play", asset_path="sounds/game/roar.wav"),
            g.call("ui.show_subtitle", text="[a roar]", seconds=1.5),
        )
        rage = g.branch(
            g.op("logic.and", g.op("compare.less_equal", g.get("hits"), 3), g.op("compare.equal", g.get("phase"), 2))
        )
        g.chain(angry["False"], rage)
        g.chain(
            rage["True"],
            g.set("phase", 3),
            g.call("steer.set_max_speed", speed=4.8),
            g.call("entity.set_tint", rgba=(1, 0.55, 0.5, 1)),
            g.call("sound.play", asset_path="sounds/game/roar.wav"),
        )
    # The steering decides the velocity; the kinematic body carries it (the physics owns the pose of a body).
    tick = g.event("on_tick")
    steered = g.node("math.break_vector3", Value=g.call("steer.get_velocity")["xyz"])
    g.chain(
        tick,
        g.call("physics.set_velocity", velocity=g.node("math.make_vector2", X=steered["X"], Y=steered["Y"])["Result"]),
    )
    create = g.event("on_create")
    gentle = g.call("settings.get", key="shrine_gentle", default=False)["value"]
    slower = g.branch(gentle)
    g.chain(create, slower, g.call("steer.set_max_speed", speed=1.4))
    return g.text()


def exit_graph() -> str:
    """An exit: the hero walking in goes to the next room, by the entry of that room."""
    g = Graph(6730000000000000000)
    g.var("target_scene", "string", "scenes/room_2.owl")
    g.var("entry", "string", "SpawnS")
    g.var("leaving", "bool", False, exposed=False)
    enter = g.event("on_trigger_enter")
    hero = g.op("compare.equal", enter["other_id"], g.call("scene.find_entity", name="Player")["entity_id"])
    # The hero must have been in the room a moment and walk: it never leaves by the exit it came in by, nor pushed out
    # by a creature.
    walking = g.op(
        "compare.greater",
        g.node("math.vector2_length", A=g.call("input.get_action_vector", action="move")["xy"])["Result"],
        0.3,
    )
    settled = g.op(
        "logic.and",
        walking,
        g.op("compare.greater", g.call("gamestate.get", key="shrine_in_room", default=0)["value"], 0.5),
    )
    go = g.branch(g.op("logic.and", g.op("logic.and", hero, settled), g.op("logic.not", g.get("leaving"))))
    g.chain(
        enter,
        go,
        g.set("leaving", True),
        g.call("gamestate.set", key="shrine_entry", value=g.get("entry")),
        g.call("gamestate.set", key="shrine_state", value="leaving"),
        g.call("scene.transition_to", scene_path=g.get("target_scene"), kind="fade", duration=0.35, rgba=LOOK.sweep),
    )
    return g.text()


def heart_graph() -> str:
    """The heart of the shrine: the hero touching it ends the quest."""
    g = Graph(6740000000000000000)
    enter = g.event("on_trigger_enter")
    hero = g.op("compare.equal", enter["other_id"], g.call("scene.find_entity", name="Player")["entity_id"])
    go = g.branch(
        g.op("logic.and", hero, g.op("logic.not", g.call("gamestate.get", key="shrine_won", default=False)["value"]))
    )
    g.chain(
        enter,
        go,
        g.call("gamestate.set", key="shrine_won", value=True),
        g.call("sound.play", asset_path="sounds/game/heart.wav"),
    )
    return g.text()


# --------------------------------------------------------------------------------------------------------- project

PROJECT = """OwlProject:
  name: OwlSunkenShrine
  firstScene: scenes/title.owl
  author: Silmaen
  description: The Sunken Shrine, a Zelda-like sample game of the Owl Engine
  icon: logo/logo_owl.png
  gamepadMappings: input/gamecontrollerdb.txt
  window:
    width: 1280
    height: 720
    fullscreen: false
    resizable: true
  RendererStack:
    - Type: Renderer2D
      Name: world
    - Type: Renderer2D
      Name: ui
    - Type: Renderer2D
      Name: overlay
"""
"""The project."""

SETTINGS = """GameSettings:
  - {key: resolution_width, type: int, value: 1280}
  - {key: resolution_height, type: int, value: 720}
  - {key: fullscreen, type: bool, value: false}
  - {key: resizable, type: bool, value: true}
  - {key: volume_master, type: float, value: 1.0}
  - {key: volume_music, type: float, value: 0.8}
  - {key: volume_sfx, type: float, value: 1.0}
  - {key: ui_scale, type: float, value: 1.0}
  - {key: color_blind_mode, type: string, value: none}
  - {key: reduced_motion, type: bool, value: false}
  - {key: subtitles, type: bool, value: true}
  - {key: shrine_gentle, type: bool, value: false}
  - {key: shrine_bright, type: bool, value: false}
  - {key: shrine_best, type: int, value: 0}
"""
"""The settings and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of The Sunken Shrine, on top of the built-in ones (`move` walks; the `ui_*` actions drive the menus). Read
# by scripts/game/player.owlvs and scripts/game/room.owlvs.
actions:
  - name: move
    type: vector
    bindings:
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: S, component: y}
      - {key: W, component: y, scale: -1}
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_stick: left}
      - {gamepad_button: DpadRight, component: x}
      - {gamepad_button: DpadLeft, component: x, scale: -1}
      - {gamepad_button: DpadDown, component: y}
      - {gamepad_button: DpadUp, component: y, scale: -1}
  - name: attack
    type: button
    bindings:
      - {key: Space}
      - {mouse_button: Left}
      - {gamepad_button: A}
  - name: use
    type: button
    bindings:
      - {key: E}
      - {key: Enter}
      - {mouse_button: Right}
      - {gamepad_button: X}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input actions."""


def main() -> None:
    """Write the project, the tileset, the rooms, the trees, the dialogue, the HUD and the graphs."""
    for folder in ("scenes", "huds", "tilesets", "tilemaps", "dialogues", "scripts/game"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    (ROOT / "tilesets" / "dungeon.owltileset").write_text(tileset())
    for number, room in enumerate(lay.ROOMS, start=1):
        (ROOT / "tilemaps" / f"room_{number}.owltilemap").write_bytes(tilemap(f"room_{number}", room))
        (ROOT / "scenes" / f"room_{number}.owl").write_text(room_scene(number, room))
    (ROOT / "scripts" / "game" / "creature.owlbt").write_text(creature_tree(False))
    (ROOT / "scripts" / "game" / "bat.owlbt").write_text(creature_tree(True))
    (ROOT / "scripts" / "game" / "guardian.owlbt").write_text(guardian_tree())
    (ROOT / "dialogues" / "old_man.owldlg").write_text(dialogue())
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    (ROOT / "scripts" / "game" / "room.owlvs").write_text(room_graph())
    (ROOT / "scripts" / "game" / "player.owlvs").write_text(player_graph())
    (ROOT / "scripts" / "game" / "creature.owlvs").write_text(creature_graph(False))
    (ROOT / "scripts" / "game" / "guardian.owlvs").write_text(creature_graph(True))
    (ROOT / "scripts" / "game" / "exit.owlvs").write_text(exit_graph())
    (ROOT / "scripts" / "game" / "heart.owlvs").write_text(heart_graph())


if __name__ == "__main__":
    main()
