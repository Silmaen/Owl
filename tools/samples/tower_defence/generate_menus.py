#!/usr/bin/env python3
"""
Write the menu scenes of the Tower defence sample game into ``samples/tower_defence/scenes/``: the title, the
settings, the victory and the game over screens, in the night-watch look of the game.

Every menu stands over a strip of meadow at the bottom of the screen where a wave marches past an archer tower
(``scripts/ui/parade.lua``, the sprites of the game in the 2D world layer); the victory adds fireworks over the keep.
The title is stone letters with a gold rim, the buttons crimson banners with a gold frame (``textures/ui/banner.png``
over the button colour) and a gold focus frame. The canvas is 1280 x 720 reference pixels, sizes and offsets in screen
heights. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from tools.samples import scene_yaml as y

ROOT = Path(__file__).resolve().parents[3] / "samples" / "tower_defence"
"""The sample project."""
NIGHT = (0.06, 0.08, 0.14, 1)
"""Colour behind everything: a night sky."""
STONE = (0.95, 0.92, 0.84, 1)
"""Colour of the titles and labels."""
SOFT = (0.82, 0.84, 0.9, 0.95)
"""Colour of the secondary texts."""
GOLD = (0.97, 0.77, 0.3, 1)
"""Headings and focus frame."""
INK = (0.1, 0.05, 0.04, 1)
"""Outline of the texts."""
BUTTON = (0.48, 0.11, 0.11)
"""Button colour under the banner frame."""
BUTTON_HOVER = (0.78, 0.24, 0.18, 1)
"""Hovered or focused button."""
BUTTON_PRESSED = (0.3, 0.06, 0.06, 1)
"""Pressed button."""
WINDOW_BODY = (0.08, 0.1, 0.17, 0.97)
"""Body of the windows."""
WINDOW_TITLE = (0.42, 0.1, 0.1, 1)
"""Title bar of the windows."""
HEADING = "OpenSans-ExtraBold"
"""Font of the titles."""
LABEL = "OpenSans-Bold"
"""Font of the buttons."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
GROUND_Y = -5.9
"""Height of the feet of the parade in the 2D world (camera of 15 units high)."""

HOW_TO_PLAY = (
    "The Horde marches from its gate to the human keep: goblins, orc grunts and ogres. Build towers on the stone pads "
    "beside the road: the archer shoots fast at one enemy, the cannon bursts on a group, the frost tower slows them. "
    "Raise a tower to level 3, then give it one power-up of two. A pad on high ground shoots further. Each enemy "
    "reaching the keep costs a life; kills pay gold, calling a wave early pays a bonus, a tower sells for half."
)
"""The rules, in the How to play window."""
CONTROLS = (
    "Build, upgrade  click a pad or a tower, then a badge; right click closes\n"
    "Pads            Q / E or Tab, D-pad; Enter or pad A opens and picks\n"
    "                1 2 3 pick a badge, X or pad X sells, Backspace or pad B closes\n"
    "Next wave       Space, pad Y, the Call wave button (bonus gold)\n"
    "Speed x2        F, pad RB          Pause   Escape, P, pad Start\n"
    "Camera          WASD, arrows, left stick, mouse on an edge\n"
    "Turn, zoom      Z / C, pad LB / D-pad up and down; wheel, R / V, right stick"
)
"""The controls, in the How to play window and the pause window of a level."""


def label(value: str, size: int, colour: y.Colour = STONE, align: str = "Center", font: str = LABEL) -> str:
    """A text of the menus, outlined in ink."""
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
    font_size: int = 30,
) -> int:
    """A banner button in a layout: the crimson shows inside the gold frame, the label is stone on ink."""
    components = [
        y.child_rect(size),
        y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED, (0.12, 0.1, 0.12, 1)),
        label(text, font_size),
        y.focus(initial=initial, focusable=focusable, colour=GOLD, thickness=3),
    ]
    uuid = entities.add(tag, components, parent=parent)
    entities.add(f"{tag}Frame", [y.child_rect((1, 1)), y.image("textures/ui/banner.png")], parent=uuid)
    return uuid


def column(entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float) -> None:
    """Buttons from top to bottom (a layout lays its children out from the last one added), the first focused."""
    for position, (tag, text, callback) in reversed(list(enumerate(rows))):
        button(entities, parent, tag, text, callback, (0.92, height), position == 0)


def board() -> str:
    """A dark blue board with a gold rim."""
    return y.panel((0.07, 0.09, 0.16, 0.9), (0.85, 0.65, 0.25, 1), 3)


def window(title: str) -> str:
    """A fixed window of the menus."""
    return y.window(title, WINDOW_BODY, WINDOW_TITLE, STONE, 40, 28)


def _flat(colour: y.Colour) -> str:
    """A tinted white quad (the health bar texture is plain white)."""
    return y.sprite("textures/bar.png", colour)


def world(entities: y.Entities, fireworks: bool = False, fallen: bool = False) -> None:
    """Camera, night sky, the meadow strip and its road, an archer tower and the parade (``parade.lua``); the victory
    adds the keep and fireworks, the game over a fallen keep (tinted dark) and no tower."""
    entities.add("MainCamera", [y.camera(15)], position=(0, 0, 5))
    entities.add("Background", [y.background(NIGHT)], position=(0, 0, -1), scale=(60, 30, 1))
    entities.add("Meadow", [_flat((0.16, 0.3, 0.14, 1))], position=(0, -6.6, -0.5), scale=(60, 2.6, 1))
    entities.add("Road", [_flat((0.45, 0.38, 0.26, 1))], position=(0, -6.0, -0.4), scale=(60, 0.5, 1))
    properties: list[tuple[str, str, object]] = [("fireworks", "bool", fireworks), ("marching", "bool", not fallen)]
    parade = entities.add("Parade", [y.script("scripts/ui/parade.lua", properties)])
    for index in range(7):
        kind = (0, 0, 1, 0, 2, 1, 0)[index]
        sprite = y.frames("textures/enemies.png", 4, 3, kind * 4)
        x = -14 + index * 3.4
        entities.add("ParadeEnemy", [sprite], position=(x, GROUND_Y, 0.1), scale=(1.8, 1.8, 1), parent=parade)
    if not fallen:
        tower = y.sprite("textures/towers/archer.png")
        entities.add("ParadeTower", [tower], position=(9.5, GROUND_Y + 1.4, 0.05), scale=(2.2, 3.85, 1))
    keep_tint = (0.45, 0.35, 0.35, 1) if fallen else (1, 1, 1, 1)
    keep = y.sprite("textures/keep.png", keep_tint)
    entities.add("ParadeKeep", [keep], position=(-10.5, GROUND_Y + 1.6, 0.05), scale=(3.6, 4.95, 1))
    if fireworks:
        for _ in range(8):
            spark = y.sprite("textures/puff.png", (1, 0.8, 0.3, 0))
            entities.add("Spark", [spark], position=(0, 0, 0.2), scale=(1.5, 1.5, 1), parent=parade)


def title() -> str:
    """Title: the title art, the buttons on the left, the map board (progress) on the right, the How to play window."""
    entities = y.Entities(7400000000000000000)
    world(entities)
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.lua")])
    art = y.image("textures/ui/title.png")
    entities.add("TitleArt", [y.rect((0.5, 0), (1.25, 0.232), (0, 0.03), SCREEN, SCREEN), art], parent=root)
    panel = entities.add(
        "ButtonPanel",
        [y.rect((0.5, 0), (0.56, 0.46), (-0.31, 0.29), SCREEN, SCREEN), y.layout("Column", 8)],
        parent=root,
    )
    column(
        entities,
        panel,
        [
            ("PlayButton", "Play", "on_play_clicked"),
            ("HowToButton", "How to play", "on_how_to_clicked"),
            ("SettingsButton", "Settings", "on_settings_clicked"),
            ("QuitButton", "Quit", "on_quit_clicked"),
        ],
        0.2,
    )
    log = entities.add(
        "MapBoard",
        [y.rect((0.5, 0), (0.56, 0.46), (0.31, 0.29), SCREEN, SCREEN), board(), y.layout("Column", 8, 12)],
        parent=root,
    )
    entities.add("BestLabel", [y.child_rect((0.92, 0.16)), label("", 24, SOFT, font="")], parent=log)
    entities.add("StateLabel", [y.child_rect((0.92, 0.16)), label("Not defended yet", 26, SOFT, font="")], parent=log)
    entities.add("MapLabel", [y.child_rect((0.92, 0.2)), label("Level 1: Greenmarch Ford", 30)], parent=log)
    entities.add("BoardTitle", [y.child_rect((0.92, 0.2)), label("THE REALM", 34, GOLD, font=HEADING)], parent=log)
    how_to = entities.add(
        "HowToWindow",
        [y.rect((0.5, 0.5), (1.25, 0.84)), window("How to play"), y.layout("Column", 8, 10)],
        parent=root,
        visible=False,
    )
    button(entities, how_to, "HowToBackButton", "Back", "on_how_to_closed", (0.4, 0.11), False)
    controls = y.text(CONTROLS, 21, "Left", GOLD, wrap=True, font="JetBrainsMono-Regular", line_spacing=0.2)
    entities.add("HowToControls", [y.child_rect((0.95, 0.4)), controls], parent=how_to)
    rules = y.text(HOW_TO_PLAY, 23, "Left", SOFT, wrap=True, line_spacing=0.15)
    entities.add("HowToRules", [y.child_rect((0.95, 0.34)), rules], parent=how_to)
    return y.scene("title", entities)


def settings() -> str:
    """Settings: master volume, interface size, reset of the progress, Back (saves)."""
    entities = y.Entities(7410000000000000000)
    world(entities)
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.lua")])
    panel = entities.add(
        "SettingsList",
        [y.rect((0.5, 0), (0.95, 0.74), (0, 0.03), SCREEN, SCREEN), board(), y.layout("Column", 6, 10)],
        parent=root,
    )

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(callback, value, (0.04, 0.05, 0.1, 1), GOLD, STONE)
        return [y.child_rect((0.86, 0.05)), bar, y.focus(initial, True, "on_slider_focused", GOLD, 3)]

    rows: list[tuple[str, list[str]]] = [
        ("TitleText", [y.child_rect((0.9, 0.13)), label("Settings", 56, GOLD, font=HEADING)]),
        ("MasterVolLabel", [y.child_rect((0.9, 0.08)), label("Master Volume: 100%", 28, SOFT)]),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("UiScaleLabel", [y.child_rect((0.9, 0.08)), label("Interface Size: 100%", 28, SOFT)]),
        ("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False)),
        ("ProgressLabel", [y.child_rect((0.9, 0.08)), label("Maps defended: 0 / 1", 28)]),
    ]
    button(entities, panel, "BackButton", "Back", "go_back", (0.6, 0.1), False)
    button(entities, panel, "ResetButton", "Reset defaults", "on_reset_clicked", (0.6, 0.1), False)
    button(entities, panel, "ResetProgressButton", "Reset progress", "on_reset_progress_clicked", (0.6, 0.1), False)
    for tag, components in reversed(rows):
        entities.add(tag, components, parent=panel)
    return y.scene("settings", entities)


def _result(scene_name: str, base: int, heading: str, script: str, fireworks: bool, fallen: bool) -> str:
    """The victory or game over screen: a heading, the lines of the stats counted up by the script, two buttons."""
    entities = y.Entities(base)
    world(entities, fireworks=fireworks, fallen=fallen)
    root = entities.add("ResultCanvas", [y.canvas(), y.script(script)])
    group = entities.add(
        "ResultGroup",
        [y.rect((0.5, 0), (1.1, 0.66), (0, 0.04), SCREEN, SCREEN), board(), y.layout("Column", 6, 12)],
        parent=root,
    )
    row = entities.add("ButtonRow", [y.child_rect((0.95, 0.15)), y.layout("Row", 16)], parent=group)
    buttons = [("AgainButton", "Play again", "on_again_clicked"), ("TitleButton", "Title", "on_title_clicked")]
    for index, (tag, text, callback) in reversed(list(enumerate(buttons))):
        button(entities, row, tag, text, callback, (0.44, 0.85), index == 0)
    for index in reversed(range(4)):
        line = label("", 28, SOFT, "Left", font="JetBrainsMono-Regular")
        entities.add(f"StatLine{index + 1}", [y.child_rect((0.8, 0.1)), line], parent=group)
    colour = GOLD if fireworks else (0.95, 0.45, 0.35, 1)
    entities.add("ResultTitle", [y.child_rect((0.95, 0.24)), label(heading, 68, colour, font=HEADING)], parent=group)
    return y.scene(scene_name, entities)


def victory() -> str:
    """Victory: the keep holds, the stats of the map counted up, fireworks."""
    return _result("victory", 7420000000000000000, "The keep holds!", "scripts/ui/victory.lua", True, False)


def game_over() -> str:
    """Game over: the keep has fallen, the wave reached."""
    return _result("game_over", 7430000000000000000, "The keep has fallen", "scripts/ui/game_over.lua", False, True)


def main() -> None:
    """Write the four menu scenes."""
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())
    (scenes / "game_over.owl").write_text(game_over())


if __name__ == "__main__":
    main()
