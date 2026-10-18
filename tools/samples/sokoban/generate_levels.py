#!/usr/bin/env python3
"""
Write the tileset, the ten level tilemaps, the ten level scenes and the level HUD of the Sokoban sample game into
``samples/sokoban/``, from the level texts of ``levels.py``.

A level is one ``.owltilemap`` of three layers, in the binary layout Owl Nest saves: ``floor`` (floor and walls),
``goals`` (the goal marks) and ``boxes`` (the crates and the hero tile where the player starts). Each level scene points
its ``Level`` entity at its tilemap (one scene per level, gap G14) and holds the player, the sliding crate, the camera,
the music and the ``Game`` entity running ``scripts/game/sokoban.lua``. The levels are edited in the tilemap editor of
Owl Nest afterwards; running this script again overwrites those edits.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sokoban.generate_levels``.
"""

from __future__ import annotations

import struct
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.sokoban import levels as lv
from tools.samples.sokoban.generate_assets import COLUMNS, ROWS, TILE, TILES

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sokoban"
"""The sample project."""
SHEET = "textures/sokoban_tiles.png"
"""The tile sheet, relative to the project."""
EMPTY = 0xFFFF
"""Tile id of an empty cell in a tilemap payload."""
CHUNK = 32
"""Side of a tilemap chunk (``TilemapLayer::chunkSize``)."""
LAYERS = ("floor", "goals", "boxes")
"""The layers of a level tilemap, in drawing order."""


def fnv1a(data: bytes) -> int:
    """32-bit FNV-1a hash, the checksum of a tilemap payload."""
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def layer_cells(lvl: lv.Level) -> list[dict[lv.Cell, int]]:
    """The tile of each painted cell of each layer of a level."""
    floor: dict[lv.Cell, int] = {}
    for x, yy in lvl.floor:
        floor[(x, yy)] = TILES["floor_worn"] if (x * 7 + yy * 13) % 5 == 0 else TILES["floor"]
    for x, yy in lvl.walls:
        near = any((x + dx, yy + dy) in lvl.floor for dx in (-1, 0, 1) for dy in (-1, 0, 1))
        if near:
            floor[(x, yy)] = TILES["wall_mossy"] if (x * 5 + yy * 3) % 7 == 0 else TILES["wall"]
    goals = dict.fromkeys(lvl.goals, TILES["goal"])
    boxes = {cell: TILES["box_on_goal"] if cell in lvl.goals else TILES["box"] for cell in lvl.boxes}
    boxes[lvl.player] = TILES["start"]
    return [floor, goals, boxes]


def tilemap(name: str, lvl: lv.Level) -> bytes:
    """A level as a ``.owltilemap``: its YAML header, then the chunks of its layers."""
    if lvl.width > CHUNK or lvl.height > CHUNK:
        raise ValueError(f"{name}: larger than one chunk")
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, len(LAYERS))
    for cells in layer_cells(lvl):
        if not cells:
            body += struct.pack("<I", 0)
            continue
        terrain = [EMPTY] * CHUNK * CHUNK
        for (x, yy), tile in cells.items():
            terrain[yy * CHUNK + x] = tile
        body += struct.pack("<III", 1, 0, 0) + struct.pack(f"<{CHUNK * CHUNK}H", *terrain)
        body += struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK)) + bytes(CHUNK * CHUNK)
    body += b"\x00"  # flat: no corner heights
    payload = body + struct.pack("<I", fnv1a(body))
    layers = "".join(f"  - name: {layer}\n" for layer in LAYERS)
    header = (
        f"Tilemap: {name}\nFormatVersion: 3\ntilesetPath: tilesets/sokoban.owltileset\nwidth: {lvl.width}\n"
        f"height: {lvl.height}\ncellSize: 1\nlayers:\n{layers}payloadBytes: {len(payload)}\n"
    )
    return header.encode() + b"...\n" + payload


def tileset() -> str:
    """The tileset of the tile sheet: the walls collide (for the editor's collision view), the rest is decoration."""
    names = {index: name for name, index in TILES.items()}
    lines = [
        "Tileset: sokoban",
        "FormatVersion: 1",
        f"texture: nam:{SHEET}",
        f"tileWidth: {TILE}",
        f"tileHeight: {TILE}",
        f"columns: {COLUMNS}",
        f"rows: {ROWS}",
        "filterMode: Nearest",
        "tiles:",
    ]
    for index in sorted(names):
        lines.append(f"  - index: {index}")
        if names[index].startswith("wall"):
            lines.append("    collidable: true")
        lines.append(f"    name: {names[index]}")
    return "\n".join(lines) + "\n"


def cell_position(lvl: lv.Level, cell: lv.Cell, z: float) -> tuple[float, float, float]:
    """World centre of a cell: the grid is centred on the ``Level`` entity, at the origin."""
    return cell[0] - (lvl.width - 1) / 2.0, (lvl.height - 1) / 2.0 - cell[1], z


def camera_size(lvl: lv.Level) -> float:
    """Height seen by the camera: the level, a margin for the HUD bars, and its width on a 4:3 screen."""
    return max(lvl.height + 3.0, (lvl.width + 2.0) * 0.75)


def level_scene(index: int) -> str:
    """The scene of a level, from 0."""
    lvl = lv.level(index)
    number = index + 1
    entities = y.Entities(7200000000000000000 + number * 1000000)
    entities.add("MainCamera", [y.camera(camera_size(lvl))], position=(0, 0, 5))
    size = camera_size(lvl) * 2.4
    entities.add("Background", [y.background((0.07, 0.05, 0.06, 1))], position=(0, 0, -1), scale=(size, size, 1))
    entities.add("Level", [f"Tilemap:\n  tilemapPath: tilemaps/level_{number:02d}.owltilemap\n"])
    sheet = f"textures/{Path(SHEET).name}"
    entities.add(
        "Player", [y.frames(sheet, COLUMNS, ROWS, TILES["hero_idle"])], position=cell_position(lvl, lvl.player, 0.1)
    )
    entities.add("MovingBox", [y.frames(sheet, COLUMNS, ROWS, TILES["box"])], position=(0, 0, 0.05), visible=False)
    entities.add("Music", [y.music("sounds/music/loop.wav", 0.3)])
    next_scene = f"scenes/level_{number + 1:02d}.owl" if number < len(lv.LEVELS) else "scenes/victory.owl"
    properties = [("level", "int", number), ("level_name", "string", lvl.name), ("next_scene", "string", next_scene)]
    entities.add("Game", [y.script("scripts/game/sokoban.lua", properties)])
    return y.scene(f"level_{number:02d}", entities, hud="huds/level.owlhud")


def level_hud() -> str:
    """The HUD of a level: level, moves and best across the top, the hints and three buttons at the bottom, a message
    in the middle and the pause window."""
    entities = y.Entities(7290000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    gold = (1, 0.88, 0.5, 1)
    entities.add("LevelText", [y.rect((0, 0), (0.3, 0.07), (16, 6)), y.text("Level 1", 34, "Left", gold)], parent=root)
    entities.add("MovesText", [y.rect((0.5, 0), (0.5, 0.07), (0, 6)), y.text("Moves 0   Pushes 0", 34)], parent=root)
    entities.add("BestText", [y.rect((1, 0), (0.3, 0.07), (-16, 6)), y.text("Best -", 34, "Right", gold)], parent=root)
    hint = "Move: arrows, WASD, D-pad   Undo: Z, pad B   Restart: R, pad Y   Menu: Esc, Start"
    entities.add(
        "HintText",
        [y.rect((0, 1), (0.62, 0.05), (16, -12)), y.text(hint, 22, "Left", (0.85, 0.8, 0.75, 0.85))],
        parent=root,
    )
    bar = entities.add("ButtonBar", [y.rect((1, 1), (0.6, 0.08), (-16, -8)), y.layout("Row", 6)], parent=root)
    for tag, label, callback, colour in (
        ("MenuButton", "Menu", "on_menu_clicked", (0.3, 0.3, 0.3)),
        ("RestartButton", "Restart", "on_restart_clicked", (0.45, 0.3, 0.12)),
        ("UndoButton", "Undo", "on_undo_clicked", (0.15, 0.35, 0.6)),
    ):
        components = [
            y.child_rect((0.31, 0.9)),
            y.button(callback, colour),
            y.text(label, 28),
            y.focus(focusable=False),
        ]
        entities.add(tag, components, parent=bar)
    message = y.text("Level 1", 44, wrap=True)
    entities.add("MessageText", [y.rect((0.5, 0.55), (0.9, 0.2)), message], parent=root, visible=False)
    window = entities.add(
        "PauseWindow",
        [y.rect((0.5, 0.5), (0.62, 0.62)), y.window("Paused"), y.layout("Column", 8, 10)],
        parent=root,
        visible=False,
    )
    for tag, label, callback, colour, initial in (
        ("QuitButton", "Quit to title", "on_pause_quit", (0.45, 0.16, 0.16), False),
        ("LevelsButton", "Level select", "on_pause_levels", (0.3, 0.3, 0.3), False),
        ("PauseRestartButton", "Restart level", "on_pause_restart", (0.45, 0.3, 0.12), False),
        ("ResumeButton", "Resume", "on_pause_resume", (0.15, 0.35, 0.65), True),
    ):
        components = [y.child_rect((0.9, 0.19)), y.button(callback, colour), y.text(label, 30), y.focus(initial)]
        entities.add(tag, components, parent=window)
    return y.hud_file("level", entities)


def main() -> None:
    """Write every generated file of the levels."""
    for folder in ("tilesets", "tilemaps", "scenes", "huds"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "tilesets" / "sokoban.owltileset").write_text(tileset())
    (ROOT / "huds" / "level.owlhud").write_text(level_hud())
    for index in range(len(lv.LEVELS)):
        name = f"level_{index + 1:02d}"
        (ROOT / "tilemaps" / f"{name}.owltilemap").write_bytes(tilemap(name, lv.level(index)))
        (ROOT / "scenes" / f"{name}.owl").write_text(level_scene(index))


if __name__ == "__main__":
    main()
