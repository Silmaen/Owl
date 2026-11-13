#!/usr/bin/env python3
"""
Write the scenes and the data of The Moon Lantern, the point-and-click sample game, into ``samples/point_and_click/``:

- ``scenes/``: the three rooms (``cottage``, ``glade``, ``bridge``) from the layout of ``rooms.py``, the ``title``,
  the ``ending``, and through ``generate_menus.py`` the ``title`` and the ``settings``;
- ``huds/play.owlhud``: the HUD of the rooms (the name under the cursor, the hero's remarks, the inventory, the
  dialogue panel and the pause window);
- ``dialogues/barnaby.owldlg`` and ``dialogues/troll.owldlg``, ``sequences/ending.owlseq``;
- ``localisation.yml`` (English and French, from ``texts.py``), ``game_settings.yml``, ``input_actions.yml``;
- an ``.owlmeta`` beside each picture (nearest filtering: the art is pixel art), its GUID kept, or derived from its
  path for a new file.

A room is the backdrop and its walk-behind masks and props in the depth-tested 2D layer ``world``, the 3D actors in
the mesh layer ``actors`` (orthographic camera, so the floor of a room is the plane of its backdrop), the cursor in the
screen layer ``cursor``; the floor is a ``WalkArea``, the hero and the other characters are ``WalkAgent``s scaled and
sorted by depth. A hotspot is an entity ``Hotspot_<id>`` (its rectangle) with a child ``Stand_<id>``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.point_and_click.generate_scenes``.
"""

from __future__ import annotations

import hashlib
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.point_and_click import rooms
from tools.samples.point_and_click.rooms import Room
from tools.samples.point_and_click.texts import STRINGS

ROOT = Path(__file__).resolve().parents[3] / "samples" / "point_and_click"
"""The sample project."""
ORTHO = 9.0
"""World height shown by the camera of a room."""
CAMERA_Z = 20.0
"""Height of the camera above the plane of the rooms."""
BACKDROP_Z = -40.0
"""Depth of the backdrops, behind every actor."""
PROP_Z = -30.0
"""Depth of the props of the back wall."""
DEPTH_PER_UNIT = 3.0
"""Z gained by an actor per unit walked towards the viewer."""
FONT = "MedievalSharp"
"""Font of the menus and of the HUD."""
PARCHMENT = (0.96, 0.9, 0.74, 1)
INK = (0.12, 0.07, 0.05, 1)
GOLD = (0.97, 0.78, 0.36, 1)
NIGHT_PANEL = (0.07, 0.06, 0.12, 0.86)
BUTTON = (0.28, 0.17, 0.12)
BUTTON_HOVER = (0.62, 0.42, 0.2, 1)
BUTTON_PRESSED = (0.16, 0.09, 0.05, 1)

HERO_MESH = "meshes/hero.glb"
HERO_SCALE = 2.2
WIZARD_SCALE = 1.15
TROLL_SCALE = 1.6
MESH_PARTS = {"meshes/hero.glb": 17, "meshes/wizard.glb": 6, "meshes/troll.glb": 1}
"""Submeshes of each character: one material slot each, all drawn with the toon shading."""
TOON = "{shadingModel: Toon, toonBands: 3, toonRim: 0.2}"
"""Material override of the characters: flat bands of light, like the painted backdrops."""
HERO = "CharacterArmature|"
ENEMY = "EnemyArmature|EnemyArmature|EnemyArmature|"


def guid(path: str) -> str:
    """The GUID of an asset of the project: the one of its sidecar, else one derived from its path."""
    meta = ROOT / f"{path}.owlmeta"
    if meta.exists():
        for line in meta.read_text().splitlines():
            if line.startswith("Guid: "):
                return line.removeprefix("Guid: ").strip()
    return hashlib.sha1(f"point_and_click/{path}".encode()).hexdigest()[:16]


def ref(path: str) -> str:
    """A reference to an asset by GUID."""
    return f"gid:{guid(path)}:{path}"


def write(relative: str, text: str) -> None:
    """Write a file of the project."""
    path = ROOT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def write_metas() -> None:
    """The sidecars of the pictures, scripts, sounds, meshes, fonts and data of the project."""
    importers = {".png": "Texture", ".lua": "Script", ".wav": "Sound", ".glb": "Mesh", ".ttf": "Font"}
    for path in sorted(ROOT.rglob("*")):
        if path.suffix not in importers and path.suffix not in (".owl", ".owlhud", ".owldlg", ".owlseq"):
            continue
        relative = path.relative_to(ROOT).as_posix()
        importer = importers.get(path.suffix, "")
        text = f"FormatVersion: 1\nGuid: {guid(relative)}\n"
        if importer:
            text += f"Importer: {importer}\n"
        if importer == "Texture":
            text += "Settings:\n  Nearest: true\n  Compression: None\n  Mips: false\n"
        (path.parent / (path.name + ".owlmeta")).write_text(text)


# --- components ----------------------------------------------------------------------------------------------------


def camera(size: float = ORTHO) -> str:
    """The orthographic camera of a room, looking down -Z at the plane of the backdrop."""
    return (
        "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.7853982\n"
        f"    perspectiveNear: 0.1\n    perspectiveFar: 200\n    orthographicSize: {y.num(size)}\n"
        "    orthographicNear: 0.1\n    orthographicFar: 100\n    toneMapping: None\n  primary: true\n"
        "  fixedAspectRatio: false\n"
    )


def sprite(texture: str, colour: y.Colour = (1, 1, 1, 1)) -> str:
    """An unlit sprite of a picture of the project."""
    return f"SpriteRenderer:\n  color: {y.vec(*colour)}\n  tilingFactor: 1\n  texture: {ref(texture)}\n  lit: false\n"


def frames(texture: str, columns: int, frame: int = 0, rows: int = 1) -> str:
    """An unlit sheet of frames shown one at a time (the scripts pick the frame)."""
    return (
        f"AnimatedSpriteRenderer:\n  color: [1, 1, 1, 1]\n  texture: {ref(texture)}\n  columns: {columns}\n"
        f"  rows: {rows}\n  firstFrame: {frame}\n  lastFrame: {frame}\n  frameDuration: 0.1\n  loop: false\n"
        "  lit: false\n"
    )


def looping(texture: str, columns: int, duration: float) -> str:
    """An unlit sheet of frames that plays in a loop."""
    return (
        f"AnimatedSpriteRenderer:\n  color: [1, 1, 1, 1]\n  texture: {ref(texture)}\n  columns: {columns}\n"
        f"  rows: 1\n  firstFrame: 0\n  lastFrame: {columns - 1}\n  frameDuration: {y.num(duration)}\n"
        "  loop: true\n  lit: false\n"
    )


def layer(name: str) -> str:
    """The layer of the renderer stack that draws the entity."""
    return f"RendererTag:\n  Name: {name}\n"


ROOM_NAV = "NavMeshVolume:\n  size: [40, 40, 40]\n  sources: WalkAreas\n"
"""The navigation mesh of a room, read from the floors of its walk area."""


def walk_area(room: Room) -> str:
    """The floor of a room, scaled and sorted by depth."""

    def ring(points: list[rooms.Point]) -> str:
        return "[" + ", ".join(y.vec(*rooms.world(p)) for p in points) + "]"

    holes = "".join(f"\n        - {ring(hole)}" for hole in room.holes)
    return (
        f"WalkArea:\n  polygons:\n    - outline: {ring(room.outline)}\n      holes:{holes if holes else ' []'}\n"
        f"  depthScale: true\n  nearY: {y.num(rooms.world_y(room.near))}\n  nearScale: 1\n"
        f"  farY: {y.num(rooms.world_y(room.far))}\n  farScale: {y.num(room.far_scale)}\n  depthSort: true\n"
        f"  depthPerUnit: {y.num(DEPTH_PER_UNIT)}\n"
    )


def walk_agent(area: int, speed: float, facing: str = "TurnY") -> str:
    """A character walking on the floor of its room."""
    return (
        f"WalkAgent:\n  area: {area}\n  speed: {y.num(speed)}\n  scaleSpeed: true\n  facing: {facing}\n"
        "  speedParameter: speed\n"
    )


def walk_behind(area: int, baseline: float) -> str:
    """A walk-behind mask whose baseline is a row of the backdrop."""
    return f"WalkBehind:\n  area: {area}\n  baseline: {y.num(rooms.world_y(baseline))}\n"


def mesh(path: str) -> str:
    """A character mesh of the project, every part toon shaded."""
    slots = "".join(f"    - overrides: {TOON}\n" for _ in range(MESH_PARTS[path]))
    return f"MeshRenderer:\n  mesh: {ref(path)}\n  color: [1, 1, 1, 1]\n  transparent: false\n  materials:\n{slots}"


def animator(idle: str, walk: str | None, extra: dict[str, tuple[str, bool]]) -> str:
    """An Animator: idle and walk driven by the speed, each extra state fired by a trigger of its name."""
    parameters = ["{name: speed, type: Float, value: 0}"]
    states = [f"{{name: idle, clip: '{idle}', speed: 1, loop: true}}"]
    transitions = []
    if walk is not None:
        states.append(f"{{name: walk, clip: '{walk}', speed: 1.2, loop: true}}")
        transitions += [
            "{from: idle, to: walk, duration: 0.2, exitTime: -1, conditions: [{parameter: speed, op: Greater, "
            "threshold: 0.15}]}",
            "{from: walk, to: idle, duration: 0.2, exitTime: -1, conditions: [{parameter: speed, op: Less, "
            "threshold: 0.15}]}",
        ]
    for name, (clip, loop) in extra.items():
        parameters.append(f"{{name: {name}, type: Trigger, value: 0}}")
        states.append(f"{{name: {name}, clip: '{clip}', speed: 1, loop: {'true' if loop else 'false'}}}")
        transitions.append(
            f"{{from: '*', to: {name}, duration: 0.15, exitTime: -1, conditions: [{{parameter: {name}, op: IsTrue, "
            "threshold: 0}]}"
        )
        transitions.append(f"{{from: {name}, to: idle, duration: 0.25, exitTime: 1, conditions: []}}")
    lines = "\n".join(f"    - {t}" for t in transitions)
    return (
        "Animator:\n  playing: true\n  speed: 1\n  entry: idle\n  parameters:\n"
        + "".join(f"    - {p}\n" for p in parameters)
        + "  states:\n"
        + "".join(f"    - {s}\n" for s in states)
        + "  transitions:\n"
        + lines
        + "\n"
    )


def sound(path: str, category: str, volume: float, subtitle: str = "") -> str:
    """A looping sound playing from the start (not spatial)."""
    out = (
        f"SoundSource:\n  soundAsset: {ref(path)}\n  category: {category}\n  volume: {y.num(volume)}\n  pitch: 1\n"
        "  loop: true\n  spatial: false\n  playOnStart: true\n  maxDistance: 50\n  rolloff: 1\n"
    )
    if subtitle:
        out += f"  subtitle: {subtitle}\n"
    return out


def light() -> str:
    """The light of the actors: from the viewer's side, high on the left."""
    return "Light: {type: Directional, enabled: true, color: [1, 0.95, 0.88], intensity: 100000, castShadows: false}\n"


def scene_script(path: str, properties: str = "") -> str:
    """The root keys of a scene script."""
    return f"SceneScript:\n  scriptPath: {ref(path)}\n{properties}"


KEYS = {key: values[0] for table in STRINGS.values() for key, values in table.items()}
"""English text of every key."""


def text(value: str, size: int, colour: y.Colour = PARCHMENT, align: str = "Center", wrap: bool = False) -> str:
    """A HUD text in the font of the game, outlined in ink; a key of the string tables is shown translated."""
    if value in KEYS:
        block = y.text(KEYS[value], size, align, colour, wrap=wrap, font=FONT, outline=INK, outline_width=2)
        return block.replace("UiText:\n", f"UiText:\n  textKey: {value}\n", 1)
    return y.text(value, size, align, colour, wrap=wrap, font=FONT, outline=INK, outline_width=2)


def button(
    entities: y.Entities,
    parent: int,
    tag: str,
    label: str,
    callback: str,
    size: tuple[float, float],
    initial: bool = False,
    font_size: int = 30,
) -> int:
    """A wooden button of the menus, framed in gold when focused."""
    return entities.add(
        tag,
        [
            y.child_rect(size),
            y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED),
            text(label, font_size),
            y.focus(initial=initial, colour=GOLD, thickness=3),
        ],
        parent=parent,
    )


# --- rooms ---------------------------------------------------------------------------------------------------------


def common(entities: y.Entities, backdrop: str) -> None:
    """Camera, light, backdrop and the cursor sprites of a room."""
    entities.add("MainCamera", [camera()], position=(0, 0, CAMERA_Z))
    entities.add("Sun", [light()], rotation=(-0.6, 0.45, 0))
    entities.add("Backdrop", [sprite(backdrop), layer("world")], position=(0, 0, BACKDROP_Z), scale=(16, 9, 1))


def cursors(entities: y.Entities) -> None:
    """The action cursor, the object in hand and the pointer of the keyboard and gamepad, in the screen layer."""
    entities.add(
        "Cursor", [frames("textures/cursors.png", 8), layer("cursor")], position=(0, 0, 0.1), scale=(32, 32, 1)
    )
    entities.add(
        "ItemCursor",
        [frames("textures/items.png", 6), layer("cursor")],
        position=(0, 0, 0.1),
        scale=(40, 40, 1),
        visible=False,
    )
    entities.add(
        "Pointer",
        [frames("textures/cursors.png", 8, 7), layer("cursor")],
        position=(0, 0, 0.2),
        scale=(32, 32, 1),
        visible=False,
    )


def rect_world(rect: rooms.Rect) -> tuple[tuple[float, float], tuple[float, float]]:
    """Centre and size, in world units, of a rectangle of a backdrop."""
    x0, y0, x1, y1 = rect
    centre = rooms.world(((x0 + x1 + 1) / 2, (y0 + y1 + 1) / 2))
    return centre, ((x1 - x0 + 1) / rooms.PIXELS_PER_UNIT, (y1 - y0 + 1) / rooms.PIXELS_PER_UNIT)


def hotspots(entities: y.Entities, room: Room, parents: dict[str, int] | None = None) -> None:
    """The hotspot rectangles of a room, each with its stand point as a child."""
    for spot in room.hotspots:
        centre, size = rect_world(spot.rect)
        uuid = entities.add(f"Hotspot_{spot.id}", [], position=(*centre, 0), scale=(*size, 1))
        stand = rooms.world(spot.stand)
        local = ((stand[0] - centre[0]) / size[0], (stand[1] - centre[1]) / size[1], 0)
        entities.add(f"Stand_{spot.id}", [], position=local, parent=uuid)
    _ = parents


def props(entities: y.Entities, room: Room, area: int, on_table: tuple[str, ...] = ()) -> None:
    """The props of a room: sprites at their rectangle, the ones on a mask sorted with it."""
    for prop in room.props:
        centre, size = rect_world(prop.rect)
        texture = f"textures/rooms/{room.name}_{prop.name}.png"
        if prop.name == "fire":
            component = looping(texture, prop.frames, 0.12)
        elif prop.frames > 1:
            component = frames(texture, prop.frames)
        else:
            component = sprite(texture)
        # A quarter of a row in front of the mask it lies on: drawn over it, never at the same depth.
        extra = [walk_behind(area, room.masks[0].baseline + 0.25)] if prop.name in on_table else []
        entities.add(
            f"Prop_{prop.name}", [component, layer("world"), *extra], position=(*centre, PROP_Z), scale=(*size, 1)
        )


def masks(entities: y.Entities, room: Room, area: int) -> None:
    """The walk-behind masks of a room."""
    for mask in room.masks:
        centre, size = rect_world(mask.rect)
        texture = f"textures/rooms/{room.name}_{mask.name}.png"
        entities.add(
            f"Mask_{mask.name}",
            [sprite(texture), walk_behind(area, mask.baseline), layer("world")],
            position=(*centre, 0),
            scale=(*size, 1),
        )


def hero(entities: y.Entities, area: int, at: rooms.Point) -> None:
    """The hero, Wren: idle, walk, reach out and wave."""
    x, wy = rooms.world(at)
    entities.add(
        "Hero",
        [
            mesh(HERO_MESH),
            animator(f"{HERO}Idle", f"{HERO}Walk", {"act": (f"{HERO}Interact", False), "talk": (f"{HERO}Wave", False)}),
            walk_agent(area, 2.4),
            layer("actors"),
        ],
        position=(x, wy, 0),
        scale=(HERO_SCALE, HERO_SCALE, HERO_SCALE),
        rotation=(0, 0.6, 0),
    )


def entries(entities: y.Entities, room: Room) -> None:
    """The points where the hero enters, by the room he comes from."""
    for name, point in room.entries.items():
        entities.add(f"Entry_{name}", [], position=(*rooms.world(point), 0))


def room_scene(room: Room, base: int, script: str, ambient: str, build_extra=None) -> str:
    """A room: its layers, floor, hotspots, props, masks, characters, sounds and script."""
    entities = y.Entities(base)
    common(entities, f"textures/rooms/{room.name}.png")
    area = entities.add("Floor", [walk_area(room)])
    masks(entities, room, area)
    props(entities, room, area, ("crust",) if room.name == "cottage" else ())
    start = room.entries.get("start", next(iter(room.entries.values())))
    hero(entities, area, start)
    entries(entities, room)
    if build_extra is not None:
        build_extra(entities, area)
    hotspots(entities, room)
    cursors(entities)
    entities.add("Music", [sound("sounds/music.wav", "Music", 0.45)])
    entities.add("Ambience", [sound(ambient, "Ambient", 0.5)])
    header = (
        scene_script(script) + "EnabledRenderers:\n- {Name: actors, Enabled: true}\n- {Name: world, Enabled: "
        "true}\n- {Name: cursor, Enabled: true}\n"
    )
    # The navigation mesh of the room, baked from its walk area when the room starts (`adventure.lua`).
    entities.add("RoomNav", [ROOM_NAV])
    return y.scene(room.name, entities, ref("huds/play.owlhud"), header)


def cottage_extra(entities: y.Entities, area: int) -> None:
    """Barnaby by the fire."""
    x, wy = rooms.world(rooms.COTTAGE.npcs["barnaby"])
    entities.add(
        "Barnaby",
        [
            mesh("meshes/wizard.glb"),
            animator(f"{HERO}Idle", None, {"talk": (f"{HERO}Yes", False)}),
            walk_agent(area, 0.0, "None"),
            layer("actors"),
        ],
        position=(x, wy, 0),
        scale=(WIZARD_SCALE, WIZARD_SCALE, WIZARD_SCALE),
        rotation=(0, -0.8, 0),
    )


def bridge_extra(entities: y.Entities, area: int) -> None:
    """The troll on the bridge and the place he walks to once paid."""
    x, wy = rooms.world(rooms.BRIDGE.npcs["troll"])
    entities.add(
        "Troll",
        [
            mesh("meshes/troll.glb"),
            animator(f"{ENEMY}Idle", f"{ENEMY}Walk", {"talk": (f"{ENEMY}Jump", False)}),
            walk_agent(area, 1.2),
            layer("actors"),
        ],
        position=(x, wy, 0),
        scale=(TROLL_SCALE, TROLL_SCALE, TROLL_SCALE),
        rotation=(0, -1.0, 0),
    )
    entities.add("TrollAside", [], position=(*rooms.world(rooms.BRIDGE.npcs["troll_aside"]), 0))


# --- HUD -----------------------------------------------------------------------------------------------------------


WIDE = ("ParentWidth", "ScreenHeight")
"""Width as a share of the screen, height in screen heights: a bar that fits any aspect."""


def modal(body: y.Colour = NIGHT_PANEL) -> str:
    """A modal window without title bar: shown, it takes the mouse and the focus."""
    return y.window("", body, body, PARCHMENT, 0, 1).replace("closable: false", "closable: false\n  modal: true")


def play_hud() -> str:
    """The HUD of the rooms: the name under the cursor and the remarks at the top, the bag button and the bag (a panel
    that leaves the room clickable), the object in hand as a tool cursor; the dialogue and the pause are modal
    windows."""
    entities = y.Entities(9300000000000000000)
    root = entities.add("PlayCanvas", [y.canvas()])
    top = entities.add(
        "TopBar", [y.rect((0.5, 0), (0.9, 0.22), (0, 10), WIDE), y.layout("Column", 4, 0, "Center")], parent=root
    )
    speech = entities.add(
        "SpeechPanel", [y.child_rect((1, 0.62)), y.panel(NIGHT_PANEL, GOLD, 2)], parent=top, visible=False
    )
    entities.add(
        "SpeechText", [y.child_rect((0.94, 0.86), (0.5, 0.5)), text("", 28, PARCHMENT, wrap=True)], parent=speech
    )
    entities.add("HoverLabel", [y.child_rect((1, 0.3)), text("", 30, GOLD)], parent=top)
    entities.add(
        "BagButton",
        [
            y.rect((1, 1), (0.12, 0.07), (-12, -12)),
            y.button("on_bag_clicked", BUTTON, BUTTON_HOVER, BUTTON_PRESSED),
            text("hud.bag", 26),
            y.focus(focusable=False, colour=GOLD),
        ],
        parent=root,
    )
    inventory = entities.add(
        "InventoryPanel",
        [y.rect((0.5, 1), (0.6, 0.16), (0, -12), WIDE), y.panel(NIGHT_PANEL, GOLD, 2), y.layout("Row", 4, 6)],
        parent=root,
        visible=False,
    )
    for slot in reversed(range(1, 7)):
        uuid = entities.add(
            f"Slot{slot}",
            [
                y.child_rect((0.13, 0.8)),
                y.button(f"on_slot_{slot}", (0.18, 0.12, 0.14), (0.46, 0.32, 0.2, 1), (0.1, 0.06, 0.06, 1)),
                y.focus(colour=GOLD, thickness=3),
            ],
            parent=inventory,
        )
        icon = (
            f"UiImage:\n  tint: [1, 1, 1, 1]\n  texture: {ref('textures/items.png')}\n  columns: 6\n  rows: 1\n"
            "  frame: 0\n"
        )
        entities.add(f"SlotIcon{slot}", [y.child_rect((0.8, 0.8), (0.5, 0.5)), icon], parent=uuid, visible=False)
    talk = entities.add(
        "DialoguePanel",
        [y.rect((0.5, 1), (0.92, 0.46), (0, -12), WIDE), modal(), y.layout("Column", 2, 6)],
        parent=root,
        visible=False,
    )
    for index in reversed(range(1, 4)):
        answer = button(entities, talk, f"Answer{index}", "", f"on_answer_{index}", (0.94, 0.12), font_size=26)
        entities.add(f"Answer{index}Text", [y.child_rect((0.96, 0.9), (0.5, 0.5)), text("", 26)], parent=answer)
    button(entities, talk, "DialogueNext", "hud.next", "on_dialogue_next", (0.3, 0.12), font_size=26)
    entities.add("DialogueText", [y.child_rect((0.95, 0.24)), text("", 28, PARCHMENT, "Left", True)], parent=talk)
    entities.add("DialogueSpeaker", [y.child_rect((0.95, 0.1)), text("", 30, GOLD, "Left")], parent=talk)
    for item in rooms.ITEMS:
        entities.add(
            f"Held_{item}",
            [
                f"UiToolCursor:\n  texture: {ref(f'textures/items/{item}.png')}\n  tint: [1, 1, 1, 1]\n"
                "  size: [56, 56]\n  hotspot: [0.5, 0.5]\n  active: false\n"
            ],
            parent=root,
        )
    pause = entities.add(
        "PauseWindow",
        [
            y.rect((0.5, 0.5), (0.6, 0.5)),
            y.window("Pause", (0.08, 0.06, 0.12, 0.96), (0.32, 0.18, 0.12, 1), PARCHMENT, 44, 30).replace(
                "closable: false", "closable: false\n  modal: true"
            ),
            y.layout("Column", 6, 12),
        ],
        parent=root,
        visible=False,
    )
    button(entities, pause, "PauseQuit", "pause.quit", "on_pause_title", (0.8, 0.2))
    button(entities, pause, "PauseSave", "pause.save", "on_pause_save", (0.8, 0.2))
    button(entities, pause, "PauseResume", "pause.resume", "on_pause_resume", (0.8, 0.2), initial=True)
    return y.hud_file("play", entities)


# --- menus ---------------------------------------------------------------------------------------------------------


def menu_world(entities: y.Entities, backdrop: str, hero_at: rooms.Point | None, dim: float = 1.0) -> None:
    """The backdrop of a menu, the hero standing in it."""
    common(entities, backdrop)
    if dim < 1:
        entities.add(
            "Dim", [sprite("textures/ui/vignette.png", (1, 1, 1, dim))], position=(0, 0, -5), scale=(16.2, 9.2, 1)
        )
    if hero_at is not None:
        x, wy = rooms.world(hero_at)
        entities.add(
            "Hero",
            [mesh(HERO_MESH), animator(f"{HERO}Idle", None, {"act": (f"{HERO}Wave", False)}), layer("actors")],
            position=(x, wy, -1),
            scale=(HERO_SCALE, HERO_SCALE, HERO_SCALE),
            rotation=(0, 0.5, 0),
        )


def menu_scene(name: str, entities: y.Entities, script: str) -> str:
    """A menu scene."""
    header = (
        scene_script(script) + "EnabledRenderers:\n- {Name: actors, Enabled: true}\n- {Name: world, Enabled: "
        "true}\n- {Name: cursor, Enabled: false}\n"
    )
    return y.scene(name, entities, "", header)


def ending_scene() -> str:
    """Ending: the cottage, the lantern lit, the camera closing in while three lines tell the end."""
    entities = y.Entities(9420000000000000000)
    room = rooms.COTTAGE
    player = (
        f"SequencePlayer:\n  sequence: {ref('sequences/ending.owlseq')}\n  camera: 0\n  playOnStart: true\n"
        "  loop: false\n  speed: 1\n  onFinished: on_ending_done\n"
    )
    entities.add("MainCamera", [camera(), player], position=(0, 0, CAMERA_Z))
    entities.add("Sun", [light()], rotation=(-0.6, 0.45, 0))
    entities.add(
        "Backdrop",
        [sprite("textures/rooms/cottage.png"), layer("world")],
        position=(0, 0, BACKDROP_Z),
        scale=(16, 9, 1),
    )
    area = entities.add("Floor", [walk_area(room)])
    masks(entities, room, area)
    centre, size = rect_world(room.props[2].rect)
    entities.add(
        "Lantern",
        [frames("textures/rooms/cottage_lantern.png", 2, 1), layer("world")],
        position=(*centre, PROP_Z),
        scale=(*size, 1),
    )
    fire_centre, fire_size = rect_world(room.props[3].rect)
    entities.add(
        "Fire",
        [looping("textures/rooms/cottage_fire.png", 3, 0.12), layer("world")],
        position=(*fire_centre, PROP_Z),
        scale=(*fire_size, 1),
    )
    entities.add(
        "Glow",
        [sprite("textures/ui/glow.png", (0.85, 0.92, 1.0, 0)), layer("world")],
        position=(centre[0], centre[1], PROP_Z + 1),
        scale=(5, 5, 1),
    )
    hero(entities, area, (262, 140))
    x, wy = rooms.world(room.npcs["barnaby"])
    entities.add(
        "Barnaby",
        [
            mesh("meshes/wizard.glb"),
            animator(f"{HERO}Idle", None, {"dance": (f"{HERO}Dance", True)}),
            walk_agent(area, 0.0, "None"),
            layer("actors"),
        ],
        position=(x, wy, 0),
        scale=(WIZARD_SCALE, WIZARD_SCALE, WIZARD_SCALE),
        rotation=(0, 0.4, 0),
    )
    entities.add("Jingle", [sound("sounds/jingle.wav", "Music", 0.6).replace("loop: true", "loop: false")])
    entities.add("Ambience", [sound("sounds/fire.wav", "Ambient", 0.4)])
    root = entities.add("EndingCanvas", [y.canvas()])
    line = entities.add(
        "EndLine",
        [y.rect((0.5, 1), (0.9, 0.16), (0, -30), WIDE), y.panel(NIGHT_PANEL, GOLD, 2)],
        parent=root,
        visible=False,
    )
    entities.add("EndText", [y.child_rect((0.95, 0.86), (0.5, 0.5)), text("", 30, PARCHMENT, wrap=True)], parent=line)
    entities.add(
        "TheEnd",
        [y.rect((0.5, 0), (0.8, 0.2), (0, 60), WIDE), text("ending.end", 110, GOLD)],
        parent=root,
        visible=False,
    )
    entities.add(
        "TitleButton",
        [
            y.rect((0.5, 0.5), (0.5, 0.09), (0, 40)),
            y.button("on_title_clicked", BUTTON, BUTTON_HOVER, BUTTON_PRESSED),
            text("ending.title", 30),
            y.focus(initial=True, colour=GOLD, thickness=3),
        ],
        parent=root,
        visible=False,
    )
    return menu_scene("ending", entities, "scripts/ui/ending.owlvs")


# --- data ----------------------------------------------------------------------------------------------------------


def localisation() -> str:
    """The string tables, English and French."""
    out = "Localisation: ''\nFormatVersion: 1\nfallback: en\nlanguages:\n"
    out += "  - {code: en, name: English, export: true}\n  - {code: fr, name: Français, export: true}\nstrings:\n"
    for table, entries in STRINGS.items():
        for key, (english, french) in entries.items():
            out += (
                f"  - key: {key}\n    table: {table}\n    values: {{en: {y.quote(english)}, fr: {y.quote(french)}}}\n"
            )
    return out


def dialogue_text(node_id: int, kind: str, position: tuple[int, int], fields: str) -> str:
    """A node of a dialogue."""
    return f"  - id: {node_id}\n    kind: {kind}\n    position: [{position[0]}, {position[1]}]\n{fields}"


def say(speaker: str, key: str) -> str:
    """The speaker and text fields of a line, by their keys."""
    english = {k: v[0] for k, v in STRINGS["dialogue"].items()}
    return (
        f"    speaker: {{text: {y.quote(english[speaker])}, key: {speaker}}}\n"
        f"    text: {{text: {y.quote(english[key])}, key: {key}}}\n"
    )


def option(key: str, target: int, condition: str = "") -> str:
    """An answer of a Choice."""
    english = STRINGS["dialogue"][key][0]
    extra = f"\n        condition: {condition}" if condition else ""
    return f"      - text: {{text: {y.quote(english)}, key: {key}}}{extra}\n        next: {target}\n"


def barnaby_dialogue() -> str:
    """Barnaby: what happened, where the moonstone is, what else the lantern needs."""
    b = 9500000000000000000
    name = "dialogue.barnaby.name"
    nodes = [
        dialogue_text(b + 1, "Start", (0, 120), f"    next: {b + 2}\n"),
        dialogue_text(b + 2, "Line", (240, 120), say(name, "dialogue.barnaby.hello") + f"    next: {b + 3}\n"),
        dialogue_text(
            b + 3,
            "Choice",
            (520, 120),
            say(name, "dialogue.barnaby.ask")
            + "    options:\n"
            + option("dialogue.barnaby.answer_stone", b + 4)
            + option("dialogue.barnaby.answer_help", b + 6)
            + option("dialogue.barnaby.answer_bye", b + 9),
        ),
        dialogue_text(b + 4, "Event", (820, 0), f"    function: on_heard_troll\n    next: {b + 5}\n"),
        dialogue_text(b + 5, "Line", (1080, 0), say(name, "dialogue.barnaby.stone") + f"    next: {b + 3}\n"),
        dialogue_text(
            b + 6, "Branch", (820, 200), f"    function: has_moonstone\n    next: {b + 7}\n    nextFalse: {b + 8}\n"
        ),
        dialogue_text(
            b + 7, "Branch", (1080, 140), f"    function: has_spark\n    next: {b + 10}\n    nextFalse: {b + 8}\n"
        ),
        dialogue_text(b + 8, "Line", (1340, 240), say(name, "dialogue.barnaby.spark") + f"    next: {b + 3}\n"),
        dialogue_text(b + 9, "Line", (820, 400), say(name, "dialogue.barnaby.bye") + f"    next: {b + 11}\n"),
        dialogue_text(b + 10, "Line", (1340, 100), say(name, "dialogue.barnaby.ready") + f"    next: {b + 11}\n"),
        dialogue_text(b + 11, "End", (1600, 400), ""),
    ]
    return 'Dialogue: ""\nFormatVersion: 1\nnodes:\n' + "".join(nodes)


def troll_dialogue() -> str:
    """The troll: the toll, the comfortable stone, and a pie when Wren has one."""
    b = 9510000000000000000
    name = "dialogue.troll.name"
    nodes = [
        dialogue_text(b + 1, "Start", (0, 120), f"    next: {b + 2}\n"),
        dialogue_text(b + 2, "Line", (240, 120), say(name, "dialogue.troll.hello") + f"    next: {b + 3}\n"),
        dialogue_text(
            b + 3,
            "Choice",
            (520, 120),
            say(name, "dialogue.troll.ask")
            + "    options:\n"
            + option("dialogue.troll.answer_toll", b + 4)
            + option("dialogue.troll.answer_stone", b + 5)
            + option("dialogue.troll.answer_pie", b + 6, "has_pie")
            + option("dialogue.troll.answer_bye", b + 7),
        ),
        dialogue_text(b + 4, "Line", (820, 0), say(name, "dialogue.troll.toll") + f"    next: {b + 3}\n"),
        dialogue_text(b + 5, "Line", (820, 140), say(name, "dialogue.troll.stone") + f"    next: {b + 3}\n"),
        dialogue_text(b + 6, "Line", (820, 280), say(name, "dialogue.troll.pie") + f"    next: {b + 8}\n"),
        dialogue_text(b + 7, "Line", (820, 420), say(name, "dialogue.troll.bye") + f"    next: {b + 9}\n"),
        dialogue_text(b + 8, "Event", (1080, 280), f"    function: on_troll_wants_pie\n    next: {b + 9}\n"),
        dialogue_text(b + 9, "End", (1340, 420), ""),
    ]
    return 'Dialogue: ""\nFormatVersion: 1\nnodes:\n' + "".join(nodes)


def ending_sequence() -> str:
    """The ending: the camera closes in on the lantern while the lines are told, then pulls back."""
    lantern, _ = rect_world(rooms.COTTAGE.props[2].rect)
    # Five units high, the view must not pass the right edge of the backdrop (8 units from the centre).
    near = (min(lantern[0] - 2.5, 8 - 5 * 16 / 9 / 2), lantern[1] - 0.8)
    return (
        'Sequence: ""\nFormatVersion: 1\nduration: 16\ncameraTracks:\n  - name: Close in\n    keys:\n'
        f"      - {{time: 0, position: [0, 0, {CAMERA_Z}], rotation: [0, 0, 0], fov: 0.785, size: 9, easing: Smooth}}\n"
        f"      - {{time: 6, position: [{y.num(near[0])}, {y.num(near[1])}, {CAMERA_Z}], rotation: [0, 0, 0], "
        "fov: 0.785, size: 5, easing: Smooth}\n"
        f"      - {{time: 11, position: [{y.num(near[0])}, {y.num(near[1])}, {CAMERA_Z}], rotation: [0, 0, 0], "
        "fov: 0.785, size: 5, easing: Smooth}\n"
        f"      - {{time: 15, position: [0, 0, {CAMERA_Z}], rotation: [0, 0, 0], fov: 0.785, size: 9, "
        "easing: Smooth}\n"
        "eventTracks:\n  - name: Story\n    keys:\n"
        "      - {time: 0.5, function: on_ending_glow}\n"
        "      - {time: 1.5, function: on_ending_line_1}\n"
        "      - {time: 6, function: on_ending_line_2}\n"
        "      - {time: 10.5, function: on_ending_line_3}\n"
        "      - {time: 15.5, function: on_ending_dance}\n"
    )


GAME_SETTINGS = """GameSettings:
  - key: resolution_width
    type: int
    value: 1280
  - key: resolution_height
    type: int
    value: 720
  - key: fullscreen
    type: bool
    value: false
  - key: resizable
    type: bool
    value: true
  - key: volume_master
    type: float
    value: 1.0
  - key: volume_music
    type: float
    value: 0.8
  - key: volume_sfx
    type: float
    value: 1.0
  - key: language
    type: string
    value: en
  - key: subtitles
    type: bool
    value: true
"""
"""Defaults of the settings."""

INPUT_ACTIONS = """FormatVersion: 1
# Actions of The Moon Lantern, on top of the built-in ones (`primary` / `secondary` are the mouse buttons, the `ui_*`
# actions drive the menus). Read by scripts/game/adventure.lua.
actions:
  - name: cursor
    type: vector
    bindings:
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_stick: left}
  - name: act
    type: button
    bindings:
      - {key: Enter}
      - {key: Space}
      - {gamepad_button: A}
  - name: look
    type: button
    bindings:
      - {key: L}
      - {gamepad_button: X}
  - name: next_hotspot
    type: button
    bindings:
      - {key: Tab}
      - {key: E}
      - {gamepad_button: RightBumper}
  - name: prev_hotspot
    type: button
    bindings:
      - {key: Q}
      - {gamepad_button: LeftBumper}
  - name: inventory
    type: button
    bindings:
      - {key: I}
      - {gamepad_button: Y}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input map."""


def main() -> None:
    """Write every scene and data file, then the sidecars."""
    write(
        "scenes/cottage.owl",
        room_scene(rooms.COTTAGE, 9100000000000000000, "scripts/rooms/cottage.owlvs", "sounds/fire.wav", cottage_extra),
    )
    write(
        "scenes/glade.owl",
        room_scene(rooms.GLADE, 9110000000000000000, "scripts/rooms/glade.owlvs", "sounds/fireflies.wav"),
    )
    write(
        "scenes/bridge.owl",
        room_scene(rooms.BRIDGE, 9120000000000000000, "scripts/rooms/bridge.owlvs", "sounds/stream.wav", bridge_extra),
    )
    from tools.samples.point_and_click import generate_menus

    generate_menus.main()
    write("scenes/ending.owl", ending_scene())
    write("huds/play.owlhud", play_hud())
    write("dialogues/barnaby.owldlg", barnaby_dialogue())
    write("dialogues/troll.owldlg", troll_dialogue())
    write("sequences/ending.owlseq", ending_sequence())
    write("localisation.yml", localisation())
    write("game_settings.yml", GAME_SETTINGS)
    write("input_actions.yml", INPUT_ACTIONS)
    write_metas()


if __name__ == "__main__":
    main()
