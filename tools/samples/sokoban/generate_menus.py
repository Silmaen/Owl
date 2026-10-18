#!/usr/bin/env python3
"""
Write the menu scenes of the Sokoban sample game into ``samples/sokoban/scenes/``: the title, the level select, the
settings and the victory screen, in the wooden warehouse look of the game.

Every menu stands over a warehouse corridor at the bottom of the screen (world tiles of ``textures/sokoban_tiles.png``);
on the title the hero pushes a crate along it onto its goal (``scripts/ui/warehouse.lua``). The title is a crate per
letter, the buttons are planks (``textures/ui/plank.png`` over the button colour) with an amber focus frame, and the
title has two columns: the buttons on the left, the warehouse log (progress) on the right. The canvas is 1280 x 720
reference pixels, every size and offset in screen-height units. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sokoban.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.sokoban.generate_assets import COLUMNS, ROWS, TILES

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sokoban"
"""The sample project."""
SHEET = "textures/sokoban_tiles.png"
"""The tile sheet of the game."""
BACKGROUND = (0.09, 0.06, 0.04, 1)
"""Colour behind everything: a dark warehouse."""
CREAM = (1, 0.93, 0.78, 1)
"""Colour of the titles and labels."""
SOFT = (0.9, 0.82, 0.68, 0.95)
"""Colour of the secondary texts."""
INK = (0.24, 0.13, 0.05, 1)
"""Outline of the texts: burnt wood."""
AMBER = (1, 0.72, 0.2, 1)
"""Focus frame."""
BUTTON = (0.35, 0.22, 0.1)
"""Button colour under the plank."""
BUTTON_HOVER = (0.85, 0.58, 0.22, 1)
"""Hovered or focused button: the plank lights up."""
BUTTON_PRESSED = (0.22, 0.13, 0.05, 1)
"""Pressed button."""
HEADING = "OpenSans-ExtraBold"
"""Font of the titles."""
LABEL = "OpenSans-Bold"
"""Font of the buttons."""
TILE = 1.25
"""World size of a tile of the corridor."""
CORRIDOR = 26
"""Tiles of the corridor (wide enough for 21:9)."""
FLOOR_Y, WALL_Y = -6.9, -5.65
"""Heights of the floor row and of the wall row behind it."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""


def _tile(entities: y.Entities, tag: str, frame: int, x: float, height: float, z: float, parent: int = 0) -> int:
    components = [y.frames(SHEET, COLUMNS, ROWS, frame)]
    return entities.add(tag, components, position=(x, height, z), scale=(TILE + 0.03, TILE + 0.03, 1), parent=parent)


def _world(entities: y.Entities, crates: str) -> None:
    """Camera, background and the corridor; ``crates`` is ``"push"`` (the title: a hero and a crate that move),
    ``"gold"`` (every crate on its goal) or ``"stacked"`` (crates waiting along the wall)."""
    entities.add("MainCamera", [y.camera(15)], position=(0, 0, 5))
    entities.add("Background", [y.background(BACKGROUND)], position=(0, 0, -1), scale=(60, 30, 1))
    corridor = entities.add("Corridor", [y.script("scripts/ui/warehouse.lua")])
    for index in range(CORRIDOR):
        x = (index - (CORRIDOR - 1) / 2) * TILE
        wall = TILES["wall_mossy"] if index % 5 == 2 else TILES["wall"]
        _tile(entities, "CorridorWall", wall, x, WALL_Y, 0, corridor)
        floor = TILES["floor_worn"] if index % 3 == 1 else TILES["floor"]
        _tile(entities, "CorridorFloor", floor, x, FLOOR_Y, 0, corridor)
    goals = [-5, 0, 5]
    for goal in goals:
        _tile(entities, "CorridorGoal", TILES["goal"], goal * TILE, FLOOR_Y, 0.02, corridor)
    if crates == "push":
        _tile(entities, "MenuCrate", TILES["box"], -12 * TILE, FLOOR_Y, 0.05, corridor)
        _tile(entities, "MenuHero", TILES["hero_idle"], -13 * TILE, FLOOR_Y, 0.06, corridor)
        _tile(entities, "MenuCrateDone", TILES["box_on_goal"], goals[2] * TILE, FLOOR_Y, 0.05, corridor)
    else:
        frame = TILES["box_on_goal"] if crates == "gold" else TILES["box"]
        for goal in goals:
            _tile(entities, "MenuCrateDone", frame, goal * TILE, FLOOR_Y, 0.05, corridor)


def label(value: str, size: int, colour: y.Colour = CREAM, align: str = "Center", font: str = LABEL) -> str:
    """A text of the menus: cream (or another colour) outlined in burnt wood."""
    return y.text(value, size, align, colour, font=font, outline=INK, outline_width=2)


def button(
    entities: y.Entities,
    parent: int,
    tag: str,
    text: str,
    callback: str,
    size: tuple[float, float],
    initial: bool = False,
    focusable: bool = True,
) -> int:
    """A plank button in a layout: the button colour shows through the plank, the label is cream on burnt wood."""
    components = [
        y.child_rect(size),
        y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED, (0.04, 0.03, 0.02, 1)),
        label(text, 30),
        y.focus(initial=initial, focusable=focusable, colour=AMBER, thickness=3),
    ]
    uuid = entities.add(tag, components, parent=parent)
    entities.add(f"{tag}Plank", [y.child_rect((1, 1)), y.image("textures/ui/plank.png")], parent=uuid)
    return uuid


def _column(entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float) -> None:
    """Buttons from top to bottom (a layout lays its children out from the last one added), the first focused."""
    for position, (tag, label, callback) in reversed(list(enumerate(rows))):
        button(entities, parent, tag, label, callback, (0.92, height), position == 0)


def _board(colour: y.Colour = (0.16, 0.1, 0.05, 0.92)) -> str:
    """A dark wooden board with a lighter rim."""
    return y.panel(colour, (0.62, 0.42, 0.22, 1), 3)


def title() -> str:
    """Title: the crates of the title, the buttons on the left, the warehouse log on the right, the How to play
    window."""
    entities = y.Entities(7300000000000000000)
    _world(entities, "push")
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.lua")])
    art = y.image("textures/ui/title.png")
    entities.add("TitleArt", [y.rect((0.5, 0), (1.2, 0.189), (0, 0.04), SCREEN, SCREEN), art], parent=root)
    panel = entities.add(
        "ButtonPanel",
        [y.rect((0.5, 0), (0.56, 0.5), (-0.31, 0.27), SCREEN, SCREEN), y.layout("Column", 8)],
        parent=root,
    )
    _column(
        entities,
        panel,
        [
            ("PlayButton", "Play", "on_play_clicked"),
            ("ContinueButton", "Continue", "on_continue_clicked"),
            ("HowToButton", "How to play", "on_how_to_clicked"),
            ("SettingsButton", "Settings", "on_settings_clicked"),
            ("QuitButton", "Quit", "on_quit_clicked"),
        ],
        0.16,
    )
    log = entities.add(
        "LogBoard",
        [y.rect((0.5, 0), (0.56, 0.5), (0.31, 0.27), SCREEN, SCREEN), _board(), y.layout("Column", 8, 12)],
        parent=root,
    )
    entities.add("BestLabel", [y.child_rect((0.92, 0.14)), label("", 24, SOFT, font="")], parent=log)
    entities.add("NextLabel", [y.child_rect((0.92, 0.14)), label("Next: level 1", 26, SOFT, font="")], parent=log)
    bar = "UiProgressBar:\n  value: 0\n  backgroundColor: [0.1, 0.06, 0.03, 1]\n  fillColor: [0.95, 0.7, 0.25, 1]\n"
    entities.add("ProgressBar", [y.child_rect((0.92, 0.07)), bar], parent=log)
    entities.add("ProgressLabel", [y.child_rect((0.92, 0.16)), label("Levels done: 0 / 10", 30)], parent=log)
    log_title = label("WAREHOUSE LOG", 34, AMBER, font=HEADING)
    entities.add("LogTitle", [y.child_rect((0.92, 0.18)), log_title], parent=log)
    _how_to_play(entities, root)
    return y.scene("title", entities)


HOW_TO_PLAY = (
    "Push every crate onto a goal (the red marks); a crate turns gold on its goal. Crates are pushed one at a time, "
    "never pulled: a crate stuck in a corner stays there, so undo is never far. The fewest moves of a level are kept."
)
"""The rules, in the How to play window."""
CONTROLS = (
    "Step       arrows, WASD, D-pad, left stick (hold to repeat)\n"
    "Undo       Z, U, Backspace, pad B, the Undo button\n"
    "Restart    R, pad Y, the Restart button\n"
    "Pause      Escape, P, pad Start, the Menu button"
)
"""The controls, in the How to play window and the pause window of the levels."""


def _how_to_play(entities: y.Entities, root: int) -> None:
    window = entities.add(
        "HowToWindow",
        [
            y.rect((0.5, 0.5), (1.2, 0.8)),
            y.window("How to play", (0.14, 0.09, 0.05, 0.98), (0.45, 0.28, 0.12, 1), CREAM, 40, 28),
            y.layout("Column", 8, 10),
        ],
        parent=root,
        visible=False,
    )
    button(entities, window, "HowToBackButton", "Back", "on_how_to_closed", (0.4, 0.13), False)
    controls = y.text(CONTROLS, 24, "Left", AMBER, wrap=True, font="JetBrainsMono-Regular", line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.94, 0.33)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 26, "Left", SOFT, wrap=True, line_spacing=0.2)
    entities.add("HowToRules", [y.child_rect((0.94, 0.33)), rules], parent=window)


def level_select() -> str:
    """Level select: the ten levels in a list (done marks and best moves from the settings), Back and Play."""
    entities = y.Entities(7310000000000000000)
    _world(entities, "stacked")
    root = entities.add("LevelSelectCanvas", [y.canvas(), y.script("scripts/ui/level_select.lua")])
    column = entities.add(
        "SelectColumn", [y.rect((0.5, 0), (1.0, 0.8), (0, 0.02), SCREEN, SCREEN), y.layout("Column", 8)], parent=root
    )
    row = entities.add("SelectButtons", [y.child_rect((1, 0.1)), y.layout("Row", 16)], parent=column)
    button(entities, row, "BackButton", "Back", "go_back", (0.44, 1), False)
    button(entities, row, "PlayButton", "Play", "on_play_clicked", (0.44, 1), False)
    entities.add("LevelInfo", [y.child_rect((1, 0.08)), label("Choose a level", 26, SOFT)], parent=column)
    entities.add(
        "LevelList",
        [
            y.child_rect((1, 0.62)),
            y.ui_list(
                10,
                33,
                24,
                ((0.2, 0.12, 0.06, 0.95), (0.25, 0.16, 0.08, 0.95), (0.45, 0.3, 0.14, 1), (0.8, 0.52, 0.18, 1), CREAM),
                "on_level_selected",
                "on_level_bind",
            ),
            y.focus(initial=True, colour=AMBER, thickness=3),
        ],
        parent=column,
    )
    heading = label("CHOOSE A LEVEL", 52, AMBER, font=HEADING)
    entities.add("TitleText", [y.child_rect((1, 0.1)), heading], parent=column)
    return y.scene("level_select", entities)


def settings() -> str:
    """Settings: master volume, interface size, reset of the progress, Back (saves)."""
    entities = y.Entities(7320000000000000000)
    _world(entities, "stacked")
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.lua")])
    board = entities.add(
        "SettingsList",
        [y.rect((0.5, 0), (0.95, 0.78), (0, 0.03), SCREEN, SCREEN), _board(), y.layout("Column", 6, 10)],
        parent=root,
    )
    slider_focus = "on_slider_focused"

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(callback, value, (0.1, 0.06, 0.03, 1), (0.95, 0.7, 0.25, 1), CREAM)
        return [y.child_rect((0.86, 0.05)), bar, y.focus(initial, True, slider_focus, AMBER, 3)]

    rows: list[tuple[str, list[str]]] = [
        ("TitleText", [y.child_rect((0.9, 0.13)), label("Settings", 56, AMBER, font=HEADING)]),
        ("MasterVolLabel", [y.child_rect((0.9, 0.08)), label("Master Volume: 100%", 28, SOFT)]),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("UiScaleLabel", [y.child_rect((0.9, 0.08)), label("Interface Size: 100%", 28, SOFT)]),
        ("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False)),
        ("ProgressLabel", [y.child_rect((0.9, 0.08)), label("Levels done: 0 / 10", 28)]),
    ]
    button(entities, board, "BackButton", "Back", "go_back", (0.6, 0.1), False)
    button(entities, board, "ResetButton", "Reset defaults", "on_reset_clicked", (0.6, 0.1), False)
    button(entities, board, "ResetProgressButton", "Reset progress", "on_reset_progress_clicked", (0.6, 0.1), False)
    for tag, components in reversed(rows):
        entities.add(tag, components, parent=board)
    return y.scene("settings", entities)


def victory() -> str:
    """Victory: every level of the game played through, the levels done and the sum of the best moves."""
    entities = y.Entities(7330000000000000000)
    _world(entities, "gold")
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.lua")])
    group = entities.add(
        "ResultGroup",
        [y.rect((0.5, 0), (1.1, 0.7), (0, 0.06), SCREEN, SCREEN), _board(), y.layout("Column", 8, 12)],
        parent=root,
    )
    row = entities.add("ButtonRow", [y.child_rect((0.95, 0.16)), y.layout("Row", 16)], parent=group)
    buttons = [("LevelsButton", "Level select", "on_levels_clicked"), ("TitleButton", "Title", "on_title_clicked")]
    for index, (tag, text, callback) in reversed(list(enumerate(buttons))):
        button(entities, row, tag, text, callback, (0.44, 0.85), index == 0)
    score = label("Levels done: 10 / 10", 32, SOFT).replace("wrap: false", "wrap: true")
    entities.add("ScoreLabel", [y.child_rect((0.95, 0.3)), score], parent=group)
    heading = label("Warehouse cleared!", 76, AMBER, font=HEADING)
    entities.add("VictoryTitle", [y.child_rect((0.95, 0.3)), heading], parent=group)
    return y.scene("victory", entities)


def main() -> None:
    """Write the four menu scenes."""
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "level_select.owl").write_text(level_select())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())


if __name__ == "__main__":
    main()
