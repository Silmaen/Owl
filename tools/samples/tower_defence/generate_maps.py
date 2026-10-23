#!/usr/bin/env python3
"""
Write the project files, the tileset, the map tilemaps and scenes, the prefabs and the play HUD of the Tower defence
sample game into ``samples/tower_defence/``, from the map data of ``maps.py``.

A map is one isometric ``.owltilemap`` of two layers in the binary layout Owl Nest saves: ``ground`` (grass) and
``roads`` (the road tiles, whose tileset connections make the graph the enemies walk, and the build pads, pavement
tiles off the graph). Each map scene holds the `Ground` tilemap, the `Gate` and the `Keep`, the decor, the camera, the
hidden sprites of the build ring and the `Game` entity running ``scripts/game/defence.lua`` with the waves of the map.
The prefabs are the enemies, the towers, the projectiles, the health bar and the puff the script spawns.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_maps``.
"""

from __future__ import annotations

import random
import shutil
import struct
from pathlib import Path

from tools.iso_atlas.terrain import EAST, NORTH, ROAD_MASKS, SOUTH, WEST
from tools.samples import scene_yaml as y
from tools.samples.tower_defence import generate_menus as menus
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence.generate_assets import DECOR, ENEMIES, TOWERS

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "tower_defence"
"""The sample project."""
DEMO = REPOSITORY / "engine_demo"
"""The Engine Demo, source of the terrain tileset and of the gamepad mappings."""
CHUNK = 32
"""Side of a tilemap chunk."""
EMPTY = 0xFFFF
"""Tile id of an empty cell."""
GRASS, DARK_GRASS, FLOWERS, PAVEMENT = 0, 6, 7, 5
"""Tiles of ``iso_ground.owltileset`` used by the maps; ``PAVEMENT`` marks a build pad."""
ISO = "RendererTag:\n  Name: iso_world\n"
"""Route of an entity to the isometric layer."""


def fnv1a(data: bytes) -> int:
    """32-bit FNV-1a hash, the checksum of a tilemap payload."""
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def check(data: mp.MapData) -> None:
    """Refuse a map whose pads, gate or keep do not fit its road."""
    road = set(mp.road_cells(data))
    keep = {(data.keep[0] + dx, data.keep[1] + dy) for dx in (0, 1) for dy in (0, 1)}
    for pad in data.pads:
        if pad in road or pad in keep or not (0 <= pad[0] < data.size and 0 <= pad[1] < data.size):
            raise ValueError(f"map {data.number}: pad {pad} is not on a free cell")
        if not any(abs(pad[0] - x) + abs(pad[1] - y) <= 2 for x, y in road):
            raise ValueError(f"map {data.number}: pad {pad} is too far from the road")
    if len(set(data.pads)) != len(data.pads):
        raise ValueError(f"map {data.number}: a pad is listed twice")
    end = mp.road_cells(data)[-1]
    if not any(abs(end[0] - x) + abs(end[1] - y) == 1 for x, y in keep):
        raise ValueError(f"map {data.number}: the road does not end beside the keep")


def layers(data: mp.MapData) -> tuple[dict[mp.Cell, int], dict[mp.Cell, int]]:
    """The ground tile and the road or pad overlay of each cell."""
    rng = random.Random(data.seed)
    ground = {}
    for yy in range(data.size):
        for x in range(data.size):
            roll = rng.random()
            ground[(x, yy)] = GRASS if roll < 0.72 else (DARK_GRASS if roll < 0.9 else FLOWERS)
    road = set(mp.road_cells(data))

    def mask(x: int, yy: int) -> int:
        bits = (NORTH if (x, yy - 1) in road else 0) | (EAST if (x + 1, yy) in road else 0)
        bits |= (SOUTH if (x, yy + 1) in road else 0) | (WEST if (x - 1, yy) in road else 0)
        first = mp.road_cells(data)[0]
        if (x, yy) == first:
            # The road enters by the edge of the map: open on the side of the edge.
            bits |= WEST if x == 0 else NORTH if yy == 0 else EAST if x == data.size - 1 else SOUTH
        return bits

    overlay = {cell: ROAD_MASKS[mask(*cell)] for cell in road}
    overlay.update(dict.fromkeys(data.pads, PAVEMENT))
    return ground, overlay


def tilemap(data: mp.MapData) -> bytes:
    """A map as a ``.owltilemap``: its YAML header, then the chunks of its two layers (flat: no corner heights)."""
    ground, overlay = layers(data)
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, 2)
    for cells, is_overlay in ((ground, False), (overlay, True)):
        terrain = [EMPTY] * CHUNK * CHUNK
        over = [EMPTY] * CHUNK * CHUNK
        for (x, yy), tile in cells.items():
            (over if is_overlay else terrain)[yy * CHUNK + x] = tile
        body += struct.pack("<III", 1, 0, 0) + struct.pack(f"<{CHUNK * CHUNK}H", *terrain)
        body += struct.pack(f"<{CHUNK * CHUNK}H", *over) + bytes(CHUNK * CHUNK)
    body += b"\x00"
    payload = body + struct.pack("<I", fnv1a(body))
    name = f"map_{data.number:02d}"
    header = (
        f"Tilemap: {name}\nFormatVersion: 3\ntilesetPath: tilesets/iso_ground.owltileset\nwidth: {data.size}\n"
        f"height: {data.size}\ncellSize: 1\nlayers:\n  - name: ground\n  - name: roads\npayloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def decor_cells(data: mp.MapData) -> list[tuple[mp.Cell, int]]:
    """Trees and rocks on free cells, never on the road, a pad or beside them, nor on the keep."""
    rng = random.Random(data.seed * 7 + 3)
    taken = set(mp.road_cells(data)) | set(data.pads)
    taken |= {(data.keep[0] + dx, data.keep[1] + dy) for dx in (-1, 0, 1, 2) for dy in (-1, 0, 1, 2)}
    blocked = {(x + dx, yy + dy) for x, yy in taken for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    free = [(x, yy) for yy in range(data.size) for x in range(data.size) if (x, yy) not in blocked]
    rng.shuffle(free)
    return [(cell, rng.choice((0, 0, 1, 1, 2))) for cell in free[: data.trees]]


def map_scene(data: mp.MapData) -> str:
    """The scene of a map."""
    entities = y.Entities(7500000000000000000 + data.number * 1000000)
    header = (
        "EnabledRenderers:\n- {Name: world, Enabled: true}\n- Name: iso_world\n  Enabled: true\n"
        "  Overrides: {Zoom: 0}\n- Name: ui\n  Enabled: true\n  Overrides: {Space: Screen}\n"
        "- {Name: overlay, Enabled: true}\n"
    )
    camera = y.camera(10) + "SoundListener:\n  primary: true\n"
    entities.add("MainCamera", [camera], position=(*data.camera, 5))
    sky = y.background((0.13, 0.22, 0.12, 1)) + "RendererTag:\n  Name: world\n"
    entities.add("Background", [sky], position=(12, 12, -5), scale=(400, 400, 1))
    navigation = (
        f"Tilemap:\n  tilemapPath: tilemaps/map_{data.number:02d}.owltilemap\n  navigation:\n    mode: connections\n"
        "    layer: 1\n    source: overlay\n    maxRise: 1\n    climbCost: 0.5\n    allowSteep: false\n"
    )
    entities.add("Ground", [navigation, ISO])
    first = mp.road_cells(data)[0]
    entities.add(
        "Gate", [y.sprite("textures/gate.png"), ISO], position=(first[0] + 1, first[1] + 1, 0), scale=(1, 1.75, 1)
    )
    keep_foot = (data.keep[0] + 2, data.keep[1] + 2, 0)
    entities.add("Keep", [y.sprite("textures/keep.png"), ISO], position=keep_foot, scale=(2, 2.75, 1))
    for (x, yy), frame in decor_cells(data):
        sprite = y.frames("textures/decor.png", len(DECOR), 1, frame)
        entities.add("Decor", [sprite, ISO], position=(x + 1, yy + 1, 0), scale=(1, 1.5, 1))
    entities.add("Cursor", [y.sprite("textures/cursor.png"), ISO], visible=False)
    entities.add("RangeRing", [y.sprite("textures/ring.png", (1, 1, 1, 0.55)), ISO], visible=False)
    for _ in range(3):
        badge = y.frames("textures/icons.png", 4, 1, 0)
        entities.add("BuildChoice", [badge, ISO], scale=(0.72, 0.72, 1), visible=False)
    name = f"map_{data.number:02d}"
    next_scene = f"scenes/map_{data.number + 1:02d}.owl" if data.number < len(mp.MAPS) else "scenes/victory.owl"
    waves = "".join(f"\n        - {y.quote(wave)}" for wave in data.waves)
    properties = (
        "LuaScript:\n  scriptPath: scripts/game/defence.lua\n  properties:\n"
        f"    - name: map_number\n      type: int\n      value: {data.number}\n"
        f"    - name: scene_path\n      type: string\n      value: scenes/{name}.owl\n"
        f"    - name: next_scene\n      type: string\n      value: {next_scene}\n"
        f"    - name: waves\n      type: strings\n      value:{waves}\n"
        f"    - name: start_gold\n      type: int\n      value: {data.start_gold}\n"
        f"    - name: lives\n      type: int\n      value: {data.lives}\n"
    )
    entities.add("Game", [properties])
    return y.scene(name, entities, hud="huds/play.owlhud", header=header)


def prefab(name: str, tag: str, components: list[str], scale: tuple[float, float, float]) -> str:
    """A prefab of one entity."""
    entities = y.Entities(7600000000000000000 + sum(map(ord, name)) * 1000)
    entities.add(tag, components, scale=scale)
    return f"Prefab: {name}\nVersion: 1\nEntities:\n" + entities.text()


def prefabs() -> dict[str, str]:
    """Every prefab the script spawns, by file name."""
    out = {}
    for row, kind in enumerate(ENEMIES):
        sheet = y.frames("textures/enemies.png", 4, len(ENEMIES), row * 4)
        out[f"enemy_{kind}"] = prefab(f"Enemy{kind.title()}", "Enemy", [sheet, ISO], (1.2, 1.2, 1))
    for kind in TOWERS:
        sprite = y.sprite(f"textures/towers/{kind}.png")
        out[f"tower_{kind}"] = prefab(f"Tower{kind.title()}", "Tower", [sprite, ISO], (1, 1.75, 1))
    for index, kind in enumerate(("arrow", "ball", "shard")):
        sheet = y.frames("textures/projectiles.png", 3, 1, index)
        out[f"projectile_{kind}"] = prefab(f"Projectile{kind.title()}", "Projectile", [sheet, ISO], (0.25, 0.25, 1))
    bar = y.sprite("textures/bar.png", (0.3, 0.9, 0.3, 1))
    out["health_bar"] = prefab("HealthBar", "HealthBar", [bar, ISO], (0.5, 0.094, 1))
    puff = y.sprite("textures/puff.png", (1, 1, 1, 0.8))
    out["puff"] = prefab("Puff", "Puff", [puff, ISO], (0.6, 0.42, 1))
    return out


def play_hud() -> str:
    """The HUD of a map: gold and lives at the top left, the wave and its countdown at the top, the info panel of the
    build ring at the bottom left, the Call wave / speed / Menu buttons at the bottom right, a message line and the
    pause window, the only place of a map that shows the controls."""
    entities = y.Entities(7490000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    stats = entities.add(
        "StatsBar",
        [y.rect((0, 0), (0.4, 0.065), (16, 8)), y.panel(), y.layout("Row", 8)],
        parent=root,
    )
    entities.add("LivesText", [y.child_rect((0.45, 1)), menus.label("Lives 20", 26, (1, 0.6, 0.55, 1))], parent=stats)
    entities.add("GoldText", [y.child_rect((0.45, 1)), menus.label("Gold 220", 26, menus.GOLD)], parent=stats)
    wave = menus.label("Wave 0 / 8 - next in 25 s", 26, menus.STONE)
    entities.add("WaveText", [y.rect((0.5, 0), (0.4, 0.065), (0, 8)), y.panel(), wave], parent=root)
    bar = entities.add("ButtonBar", [y.rect((1, 1), (0.5, 0.075), (-16, -8)), y.layout("Row", 6)], parent=root)
    for tag, text, callback, width in (
        ("MenuButton", "Menu", "on_menu_clicked", 0.2),
        ("SpeedButton", "Speed x1", "on_speed_clicked", 0.28),
        ("CallButton", "Call wave", "on_call_clicked", 0.44),
    ):
        menus.button(entities, bar, tag, text, callback, (width, 0.9), focusable=False, font_size=24)
    info = entities.add(
        "InfoPanel",
        [y.rect((0, 1), (0.72, 0.15), (16, -8)), menus.board(), y.layout("Row", 10)],
        parent=root,
        visible=False,
    )
    texts = entities.add("InfoTexts", [y.child_rect((0.74, 0.9)), y.layout("Column", 2, 0, "Start")], parent=info)
    entities.add("InfoText", [y.child_rect((1, 0.5)), menus.label("", 20, menus.SOFT, "Left", "")], parent=texts)
    entities.add("InfoTitle", [y.child_rect((1, 0.45)), menus.label("", 26, menus.GOLD, "Left")], parent=texts)
    icon = "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/icons.png\n  columns: 4\n  rows: 1\n  frame: 0\n"
    entities.add("InfoIcon", [y.child_rect((0.16, 0.85)), icon], parent=info)
    message = menus.label("", 40, menus.STONE, font=menus.HEADING).replace("wrap: false", "wrap: true")
    entities.add("MessageText", [y.rect((0.5, 0.3), (0.9, 0.14)), message], parent=root, visible=False)
    window = entities.add(
        "PauseWindow",
        [y.rect((0.5, 0.5), (1.25, 0.86)), menus.window("Paused"), y.layout("Column", 8, 10)],
        parent=root,
        visible=False,
    )
    controls = y.text(menus.CONTROLS, 19, "Left", menus.GOLD, wrap=True, font="JetBrainsMono-Regular", line_spacing=0.2)
    entities.add("ControlsText", [y.child_rect((0.96, 0.38)), controls], parent=window)
    for tag, text, callback, initial in (
        ("QuitButton", "Quit to title", "on_pause_quit", False),
        ("RestartButton", "Restart map", "on_pause_restart", False),
        ("ResumeButton", "Resume", "on_pause_resume", True),
    ):
        menus.button(entities, window, tag, text, callback, (0.6, 0.12), initial)
    return y.hud_file("play", entities)


PROJECT = """OwlProject:
  name: OwlTowerDefence
  firstScene: scenes/title.owl
  author: Silmaen
  description: Tower defence, a sample game of the Owl Engine
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
    - Type: RendererIsometric
      Name: iso_world
      DefaultConfig:
        TileWidth: 64
        TileHeight: 32
        ZStep: 16
        Zoom: 0
    - Type: Renderer2D
      Name: ui
    - Type: Renderer2D
      Name: overlay
"""
"""The project file."""

SETTINGS = """GameSettings:
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
  - key: ui_scale
    type: float
    value: 1.0
"""
"""The game settings and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of Tower defence, on top of the built-in ones (`primary` / `secondary` are the mouse buttons, the `ui_*`
# actions drive the menus). Read by scripts/game/defence.lua.
actions:
  - name: pan
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
  - name: next_pad
    type: button
    bindings:
      - {key: E}
      - {key: Tab}
      - {gamepad_button: DpadRight}
  - name: prev_pad
    type: button
    bindings:
      - {key: Q}
      - {gamepad_button: DpadLeft}
  - name: confirm
    type: button
    bindings:
      - {key: Enter}
      - {gamepad_button: A}
  - name: cancel
    type: button
    bindings:
      - {key: Backspace}
      - {gamepad_button: B}
  - name: build_1
    type: button
    bindings:
      - {key: D1}
  - name: build_2
    type: button
    bindings:
      - {key: D2}
  - name: build_3
    type: button
    bindings:
      - {key: D3}
  - name: sell
    type: button
    bindings:
      - {key: X}
      - {gamepad_button: X}
  - name: call_wave
    type: button
    bindings:
      - {key: Space}
      - {gamepad_button: Y}
  - name: fast_forward
    type: button
    bindings:
      - {key: F}
      - {gamepad_button: RightBumper}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The action map."""


def main() -> None:
    """Write every generated file of the maps."""
    for folder in ("tilesets", "tilemaps", "scenes", "huds", "prefabs", "input"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    shutil.copyfile(DEMO / "input" / "gamecontrollerdb.txt", ROOT / "input" / "gamecontrollerdb.txt")
    shutil.copyfile(DEMO / "tilesets" / "iso_ground.owltileset", ROOT / "tilesets" / "iso_ground.owltileset")
    (ROOT / "huds" / "play.owlhud").write_text(play_hud())
    for name, text in prefabs().items():
        (ROOT / "prefabs" / f"{name}.owlprefab").write_text(text)
    for data in mp.MAPS:
        check(data)
        name = f"map_{data.number:02d}"
        (ROOT / "tilemaps" / f"{name}.owltilemap").write_bytes(tilemap(data))
        (ROOT / "scenes" / f"{name}.owl").write_text(map_scene(data))


if __name__ == "__main__":
    main()
