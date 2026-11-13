#!/usr/bin/env python3
"""
Write the menu scenes of the Breakout sample game into ``samples/breakout/scenes/``: the title, the level select, the
settings, the victory and the game over screens, in the look of an arcade cabinet.

Every menu stands in front of a wall of neon bricks (world sprites of ``textures/brick.png`` that
``scripts/ui/neon_bricks.owlvs`` lights in a wave); the title adds a ball and a paddle playing by themselves. The title
is a lit marquee at the top of the screen holding the neon title and the high score. The buttons are neon tubes: hollow
pills with a cyan outline and a pink glow that fill with magenta when hovered or focused, a thin yellow frame around
the focused one; labels in the pixel font ``PressStart2P-Regular``, headings in the neon tube font ``Monoton-Regular``
(both in ``fonts/``). The canvas is 1280 x 720 reference pixels, every size and offset in screen-height units so that
no aspect ratio nor interface size breaks the layout. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.breakout.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from tools.samples import scene_yaml as y

ROOT = Path(__file__).resolve().parents[3] / "samples" / "breakout"
"""The sample project."""
BACKGROUND = (0.02, 0.01, 0.06, 1)
"""Colour behind everything."""
CYAN = (0.6, 0.97, 1, 1)
"""Colour of the button labels."""
PINK = (1, 0.3, 0.8, 1)
"""Colour of the outlines and of the focus frame."""
YELLOW = (1, 0.88, 0.3, 1)
"""Colour of the scores."""
SOFT = (0.8, 0.78, 0.95, 0.9)
"""Colour of the secondary texts."""
BUTTON = (0.05, 0.01, 0.12, 0.55)
"""Fill of a button: the dark glass of an unlit tube."""
BUTTON_HOVER = (0.78, 0.12, 0.66, 1)
"""Fill of a hovered or focused button: the tube lit in magenta."""
BUTTON_PRESSED = (1, 0.45, 0.9, 1)
"""Fill of a pressed button."""
TUBE = y.box(100, CYAN, 2.5, (1, 0.2, 0.75, 0.75), (0, 0), 14)
"""A neon tube: a pill (the radius is clamped to half the height), a cyan outline, a pink glow."""
FONT = "PressStart2P-Regular"
"""Font of the labels, the counters and the high score: arcade pixels."""
NEON = "Monoton-Regular"
"""Font of the headings: neon tubes."""
MONO = FONT
"""Font of the score counters."""
ROW_COLOURS = [(1.0, 0.25, 0.6), (1.0, 0.55, 0.15), (0.3, 0.95, 0.5), (0.3, 0.8, 1.0)]
"""Neon colour of each row of the brick wall, from the top."""
BRICK_COLUMNS = 24
"""Bricks per row of the wall (wide enough for 21:9)."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
BEZEL = y.panel((0.03, 0.01, 0.08, 0.92), box_style=y.box(26, (0.3, 0.8, 1, 0.8), 2, (0.3, 0.8, 1, 0.35), (0, 0), 20))
"""The rounded screen of the cabinet framing a menu, rimmed and glowing in cyan."""


def _world(entities: y.Entities, rows: list[float], attract: bool) -> None:
    """The camera, the backdrop, the brick wall (``rows``: world heights) and, for the title, a ball and a paddle."""
    entities.add("MainCamera", [y.camera(15)], position=(0, 0, 5))
    entities.add("Background", [y.background(BACKGROUND)], position=(0, 0, -1), scale=(60, 30, 1))
    backdrop = y.sprite("textures/backdrop.png", (0.55, 0.4, 1, 0.45), (12, 4))
    entities.add("Backdrop", [backdrop], position=(0, 0, -0.5), scale=(64, 20, 1))
    wall = entities.add("NeonWall", [y.script("scripts/ui/neon_bricks.owlvs")])
    for row, height in enumerate(rows):
        colour = (*ROW_COLOURS[row % len(ROW_COLOURS)], 1)
        for column in range(BRICK_COLUMNS):
            x = (column - (BRICK_COLUMNS - 1) / 2) * 1.6
            entities.add(
                "MenuBrick",
                [y.sprite("textures/brick.png", colour)],
                position=(x, height, 0),
                scale=(1.5, 0.5, 1),
                parent=wall,
            )
    if attract:
        entities.add("MenuPaddle", [y.sprite("textures/paddle.png")], position=(0, -7.1, 0.1), scale=(2.4, 0.36, 1))
        entities.add("MenuBall", [y.sprite("textures/ball.png")], position=(0, -6.6, 0.2), scale=(0.3, 0.3, 1))


def _label(value: str, size: int, colour: y.Colour = CYAN, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.25, 0.02, 0.3, 1), outline_width=2)


def _heading(value: str, size: int, colour: y.Colour = PINK) -> str:
    """A heading in neon tubes, with a white core outline."""
    return y.text(value, size, "Center", colour, font=NEON, outline=(1, 0.85, 0.97, 0.55), outline_width=1)


def _button(
    entities: y.Entities, parent: int, tag: str, label: str, callback: str, size: tuple[float, float], initial: bool
) -> int:
    """A neon tube button in a layout: a hollow pill lit in magenta when hovered or focused, a pixel label, a thin
    yellow focus frame around it."""
    components = [
        y.child_rect(size),
        y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED, (0.06, 0.04, 0.08, 0.6), TUBE),
        _label(label, 22, (0.9, 0.99, 1, 1)),
        y.focus(initial=initial, colour=YELLOW, thickness=2),
    ]
    return entities.add(tag, components, parent=parent)


def _column(
    entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float, width: float = 0.9
) -> None:
    """Buttons from top to bottom (a layout lays its children out from the last one added), the first focused."""
    for position, (tag, label, callback) in reversed(list(enumerate(rows))):
        _button(entities, parent, tag, label, callback, (width, height), position == 0)


def title() -> str:
    """Title: the marquee (the neon title, its halo flickering, and the high score), five tube buttons under the
    brick wall and the How to play window."""
    entities = y.Entities(7200000000000000000)
    _world(entities, [2.6, 2.0, 1.4], attract=True)
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.owlvs")])
    marquee = entities.add(
        "Marquee",
        [
            y.rect((0.5, 0), (1.22, 0.3), (0, 0.015), SCREEN, SCREEN),
            y.panel(
                (0.09, 0.01, 0.16, 0.94),
                box_style=y.box(18, PINK, 3, (1, 0.25, 0.8, 0.55), (0, 0), 22, (0.35, 0.3, 0.6, 1)),
            ),
        ],
        parent=root,
    )
    art = entities.add("TitleArt", [y.rect((0.5, 0), (0.86, 0.215), (0, 0.012), SCREEN, SCREEN)], parent=marquee)
    entities.add("TitleFace", [y.child_rect((1, 1)), y.image("textures/ui/title.png")], parent=art)
    entities.add("TitleGlow", [y.child_rect((1, 1)), y.image("textures/ui/title_glow.png")], parent=art)
    bar = _label("HI-SCORE 000000     CLEARED 0/5", 20, YELLOW, font=MONO)
    entities.add("ScoreBar", [y.child_rect((0.9, 0.16), (0.5, 0.9)), bar], parent=marquee)
    panel = entities.add(
        "ButtonPanel", [y.rect((0.5, 0), (0.5, 0.4), (0, 0.555), SCREEN, SCREEN), y.layout("Column", 9)], parent=root
    )
    _column(
        entities,
        panel,
        [
            ("PlayButton", "PLAY", "on_play_clicked"),
            ("LevelsButton", "LEVEL SELECT", "on_levels_clicked"),
            ("HowToButton", "HOW TO PLAY", "on_how_to_clicked"),
            ("SettingsButton", "SETTINGS", "on_settings_clicked"),
            ("QuitButton", "QUIT", "on_quit_clicked"),
        ],
        0.14,
    )
    _how_to_play(entities, root)
    return y.scene("title", entities)


HOW_TO_PLAY = (
    "Keep the ball in play and break every brick to clear a level; a ball lost below costs one of three lives. "
    "Where it lands on the paddle sets its angle. Grey bricks take three hits, silver ones two. "
    "Catch the capsules: W widens the paddle, S slows the ball."
)
"""The rules, in the How to play window."""
CONTROLS = (
    "MOVE    A / D, arrows, mouse, left stick, D-pad\n"
    "SERVE   Space, Up, left click, pad A\n"
    "PAUSE   Escape, P, pad Start\n"
    "MENUS   arrows + Enter, mouse, D-pad + A / B"
)
"""The controls, in the How to play window and the pause window of the levels."""


def _how_to_play(entities: y.Entities, root: int) -> None:
    window = entities.add(
        "HowToWindow",
        [
            y.rect((0.5, 0.5), (1.2, 0.84)),
            y.window("HOW TO PLAY", (0.04, 0.02, 0.1, 0.97), (0.35, 0.05, 0.45, 1), CYAN, 40, 28),
            y.layout("Column", 8, 10),
        ],
        parent=root,
        visible=False,
    )
    _button(entities, window, "HowToBackButton", "BACK", "on_how_to_closed", (0.4, 0.12), False)
    controls = y.text(CONTROLS, 24, "Left", YELLOW, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.94, 0.34)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 26, "Left", SOFT, wrap=True, line_spacing=0.2)
    entities.add("HowToRules", [y.child_rect((0.94, 0.34)), rules], parent=window)


def level_select() -> str:
    """Level select: the five levels in a list (cleared marks and best scores from the settings), Play and Back."""
    entities = y.Entities(7210000000000000000)
    _world(entities, [5.9, 5.3], attract=False)
    root = entities.add("LevelSelectCanvas", [y.canvas(), y.script("scripts/ui/level_select.owlvs")])
    column = entities.add(
        "SelectColumn",
        [y.rect((0.5, 0), (1.04, 0.8), (0, 0.18), SCREEN, SCREEN), BEZEL, y.layout("Column", 10, 14)],
        parent=root,
    )
    row = entities.add("SelectButtons", [y.child_rect((0.86, 0.11)), y.layout("Row", 16)], parent=column)
    _button(entities, row, "BackButton", "BACK", "go_back", (0.44, 1), False)
    _button(entities, row, "PlayButton", "PLAY", "on_play_clicked", (0.44, 1), False)
    entities.add("LevelInfo", [y.child_rect((0.86, 0.09)), _label("Choose a level", 26, SOFT)], parent=column)
    entities.add(
        "LevelList",
        [
            y.child_rect((0.86, 0.42)),
            y.ui_list(
                5,
                50,
                30,
                ((0.08, 0.03, 0.16, 0.9), (0.11, 0.04, 0.2, 0.9), BUTTON_HOVER, (0.55, 0.1, 0.6, 1), (0.9, 0.98, 1, 1)),
                "on_level_selected",
                "on_level_bind",
            ),
            y.focus(initial=True, colour=YELLOW, thickness=2),
        ],
        parent=column,
    )
    entities.add("TitleText", [y.child_rect((0.86, 0.12)), _heading("SELECT  LEVEL", 64)], parent=column)
    return y.scene("level_select", entities)


def settings() -> str:
    """Settings: master, music and effects volumes, mouse speed of the paddle, interface size, reset, Back (saves)."""
    entities = y.Entities(7220000000000000000)
    _world(entities, [5.9], attract=False)
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.owlvs")])
    column = entities.add(
        "SettingsList",
        [y.rect((0.5, 0), (0.96, 0.9), (0, 0.06), SCREEN, SCREEN), BEZEL, y.layout("Column", 6, 14)],
        parent=root,
    )
    rows: list[tuple[str, str, str, float, str]] = [
        ("MasterVolLabel", "MasterVolSlider", "on_master_vol_changed", 1.0, "Master Volume: 100%"),
        ("MusicVolLabel", "MusicVolSlider", "on_music_vol_changed", 0.8, "Music: 80%"),
        ("SfxVolLabel", "SfxVolSlider", "on_sfx_vol_changed", 1.0, "Effects: 100%"),
        ("MouseSpeedLabel", "MouseSpeedSlider", "on_mouse_speed_changed", 0.27, "Mouse Speed: 100%"),
        ("UiScaleLabel", "UiScaleSlider", "on_ui_scale_changed", 0.2, "Interface Size: 100%"),
    ]
    _button(entities, column, "BackButton", "BACK", "go_back", (0.6, 0.07), False)
    _button(entities, column, "ResetButton", "RESET DEFAULTS", "on_reset_clicked", (0.6, 0.07), False)
    for index, (label, slider, callback, value, initial_text) in reversed(list(enumerate(rows))):
        focus = y.focus(initial=index == 0, callback="on_slider_focused", colour=YELLOW, thickness=2)
        bar = y.slider(callback, value, (0.12, 0.05, 0.2, 1), (1, 0.3, 0.8, 1), CYAN, y.box(100, CYAN, 1.5))
        entities.add(slider, [y.child_rect((0.9, 0.034)), bar, focus], parent=column)
        entities.add(label, [y.child_rect((0.9, 0.055)), _label(initial_text, 20, SOFT)], parent=column)
    entities.add("TitleText", [y.child_rect((0.9, 0.12)), _heading("SETTINGS", 64)], parent=column)
    return y.scene("settings", entities)


def _result(base: int, name: str, script: str, heading: str, colour: y.Colour, rows: list[tuple[str, str, str]]) -> str:
    """Victory and game over: a big neon word, the score, two buttons side by side."""
    entities = y.Entities(base)
    _world(entities, [5.9, 5.3, 4.7], attract=False)
    root = entities.add(f"{name}Canvas", [y.canvas(), y.script(script)])
    group = entities.add(
        "ResultGroup",
        [y.rect((0.5, 0), (1.14, 0.62), (0, 0.32), SCREEN, SCREEN), BEZEL, y.layout("Column", 10, 14)],
        parent=root,
    )
    row = entities.add("ButtonRow", [y.child_rect((0.86, 0.17)), y.layout("Row", 16)], parent=group)
    for index, (tag, label, callback) in reversed(list(enumerate(rows))):
        _button(entities, row, tag, label, callback, (0.44, 0.8), index == 0)
    best = _label("", 22, YELLOW, font=MONO)
    entities.add("BestLabel", [y.child_rect((0.86, 0.1)), best], parent=group)
    entities.add(
        "ScoreLabel", [y.child_rect((0.86, 0.13)), _label("SCORE 000000", 32, YELLOW, font=MONO)], parent=group
    )
    title_text = _heading(heading, 110, colour)
    entities.add("ResultTitle", [y.child_rect((0.86, 0.28)), title_text], parent=group)
    return y.scene(name.lower(), entities)


def victory() -> str:
    """Victory: every level cleared."""
    return _result(
        7230000000000000000,
        "Victory",
        "scripts/ui/victory.owlvs",
        "YOU  WIN!",
        (0.3, 0.95, 1, 1),
        [("LevelsButton", "LEVEL SELECT", "on_levels_clicked"), ("MenuButton", "TITLE", "on_menu_clicked")],
    )


def game_over() -> str:
    """Game over: no life left."""
    return _result(
        7240000000000000000,
        "GameOver",
        "scripts/ui/game_over.owlvs",
        "GAME  OVER",
        (1, 0.25, 0.45, 1),
        [("RetryButton", "RETRY LEVEL", "on_retry_clicked"), ("MenuButton", "TITLE", "on_menu_clicked")],
    )


def main() -> None:
    """Write the five menu scenes."""
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "level_select.owl").write_text(level_select())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())
    (scenes / "game_over.owl").write_text(game_over())


if __name__ == "__main__":
    main()
