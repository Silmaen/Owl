#!/usr/bin/env python3
"""
Write the menu scenes and the play HUD of the Wolfenstein mini sample game into ``samples/wolfenstein/``: the title,
the settings, the tally between two floors, the victory and ``huds/play.owlhud``, in the dark stone and military grey
look of the game.

The title stands in front of a raycast view of the courtyard of floor 3, turning slowly in a dark fog
(``scripts/ui/title_view.lua``): the title is engraved in a riveted steel plate, the buttons are steel plates
(``textures/ui/plate.png`` over the button colour, red when hovered) with a red focus frame, the menu on the left and
the service record (the best tallies) on the right. The play HUD shows the floor, the score and the keys only: the
ratios of the floor wait for the tally, the controls for the pause window. The canvas is 1280 x 720 reference pixels,
every size and offset in screen-height units. The scripts live in ``scripts/ui/`` and ``scripts/game/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.wolfenstein.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.wolfenstein import generate_levels as levels

ROOT = Path(__file__).resolve().parents[3] / "samples" / "wolfenstein"
"""The sample project."""
BACKGROUND = (0.035, 0.035, 0.04, 1)
"""Colour behind the menus without a raycast view."""
STEEL = (0.86, 0.87, 0.84, 1)
"""Colour of the labels: pale steel."""
SOFT = (0.68, 0.7, 0.68, 0.95)
"""Colour of the secondary texts."""
RED = (0.82, 0.16, 0.12, 1)
"""Red of the titles and of the focus frame."""
INK = (0.04, 0.04, 0.05, 1)
"""Outline of the texts."""
BUTTON = (0.2, 0.21, 0.22)
"""Button colour under the plate."""
BUTTON_HOVER = (0.62, 0.1, 0.07, 1)
"""Hovered or focused button: the plate turns red."""
BUTTON_PRESSED = (0.35, 0.05, 0.04, 1)
"""Pressed button."""
HEADING = "OpenSans-ExtraBold"
"""Font of the titles and buttons."""
MONO = "JetBrainsMono-Regular"
"""Font of the numbers."""
FADE = (0.22, 0.02, 0.02)
"""Colour of the fades between the screens: dark blood red."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""


def label(value: str, size: int, colour: y.Colour = STEEL, align: str = "Center", font: str = HEADING) -> str:
    """A text of the menus, outlined in black."""
    return y.text(value, size, align, colour, font=font, outline=INK, outline_width=2)


def button(
    entities: y.Entities,
    parent: int,
    tag: str,
    text: str,
    callback: str,
    size: tuple[float, float],
    initial: bool = False,
) -> int:
    """A steel plate button in a layout, its label in capitals."""
    components = [
        y.child_rect(size),
        y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED, (0.06, 0.06, 0.07, 1)),
        label(text.upper(), 28),
        y.focus(initial=initial, colour=RED, thickness=3),
    ]
    uuid = entities.add(tag, components, parent=parent)
    entities.add(f"{tag}Plate", [y.child_rect((1, 1)), y.image("textures/ui/plate.png")], parent=uuid)
    return uuid


def _dossier(colour: y.Colour = (0.05, 0.05, 0.06, 0.88)) -> str:
    """A dark panel with a thin steel rim."""
    return y.panel(colour, (0.45, 0.46, 0.46, 0.9), 2)


def _flat_world(entities: y.Entities) -> None:
    entities.add("MainCamera", [y.camera(10)], position=(0, 0, 5))
    entities.add("Background", [y.background(BACKGROUND)], position=(0, 0, -1), scale=(60, 30, 1))


RAYCAST_HEADER = (
    "EnabledRenderers:\n"
    "  - Name: world\n"
    "    Enabled: false\n"
    "  - Name: raycast_world\n"
    "    Enabled: true\n"
    "    Overrides:\n"
    "      FloorColor: [0.16, 0.14, 0.12, 1]\n"
    "      CeilingColor: [0.08, 0.07, 0.07, 1]\n"
    "      FogColor: [0.02, 0.02, 0.025, 1]\n"
    "      FogStart: 1.5\n"
    "      FogEnd: 9\n"
)
"""The renderer stack of the title: the raycaster only, in a dark fog."""


def _raycast_world(entities: y.Entities) -> None:
    """The courtyard of floor 3 seen by a camera that turns slowly (`title_view.lua`), with the props of the floor."""
    floor = levels.level_3()
    entities.add(
        "TitleCamera",
        [
            "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.7853982\n"
            "    perspectiveNear: 0.01\n    perspectiveFar: 1000\n    orthographicSize: 8\n"
            "    orthographicNear: -1\n    orthographicFar: 10\n  primary: true\n  fixedAspectRatio: false\n",
            y.script("scripts/ui/title_view.lua", [("turn_speed", "float", 0.12)]),
        ],
        position=(*levels.world(17, 17), 0.5),
    )
    entities.add(
        "Walls", ["Tilemap:\n  tilemapPath: tilemaps/level_3.owltilemap\n", "RendererTag:\n  Name: raycast_world\n"]
    )
    props = entities.add("Props", [])
    for row in range(levels.SIZE):
        for column in range(levels.SIZE):
            char = floor.grid[row][column]
            if char not in levels.PROPS:
                continue
            tag, texture, _ = levels.PROPS[char]
            x, height = levels.world(column, row)
            components = [y.sprite(f"textures/wolf3d/sprites/{texture}.png"), "RendererTag:\n  Name: raycast_world\n"]
            entities.add(tag, components, position=(x, height, 0), parent=props)


HOW_TO_PLAY = (
    "Explore three floors and find the switch of the exit elevator. Treasure and food add to the score; a gold or "
    "blue key opens the door of its colour. Some walls are secret: press them to slide them away. The tally of each "
    "floor counts the items, the secrets and the map explored, each percent worth 10 points."
)
"""The rules, in the How to play window."""
CONTROLS = (
    "WALK        W / S, arrows, left stick, D-pad\n"
    "TURN        A / D, arrows, right stick, mouse with the right button\n"
    "RUN         Left Shift, stick click, right bumper\n"
    "USE         E, Space, left click, pad A or X\n"
    "MAP         Tab, pad Y        SAVE / LOAD   F6 / F9\n"
    "PAUSE       Escape, pad Start"
)
"""The controls, in the How to play window and the pause window."""


def title() -> str:
    """Title: the steel title, the menu on the left, the service record on the right, the How to play window."""
    entities = y.Entities(3300000000000000000)
    _raycast_world(entities)
    root = entities.add("TitleCanvas", [y.canvas(), y.script("scripts/ui/title.lua")])
    art = y.image("textures/ui/title.png")
    entities.add("TitleArt", [y.rect((0.5, 0), (1.2, 0.257), (0, 0.03), SCREEN, SCREEN), art], parent=root)
    menu = entities.add(
        "ButtonPanel", [y.rect((0, 0), (0.56, 0.5), (0.06, 0.36), SCREEN, SCREEN), y.layout("Column", 8)], parent=root
    )
    rows = [
        ("NewGameButton", "New game", "on_new_game_clicked"),
        ("ContinueButton", "Continue", "on_continue_clicked"),
        ("HowToButton", "How to play", "on_how_to_clicked"),
        ("SettingsButton", "Settings", "on_settings_clicked"),
        ("QuitButton", "Quit", "on_quit_clicked"),
    ]
    for index, (tag, text, callback) in reversed(list(enumerate(rows))):
        button(entities, menu, tag, text, callback, (0.95, 0.16), index == 0)
    record = entities.add(
        "RecordBoard",
        [y.rect((1, 0), (0.62, 0.5), (-0.06, 0.36), SCREEN, SCREEN), _dossier(), y.layout("Column", 6, 12)],
        parent=root,
    )
    best = y.text("Best score 0", 24, "Left", SOFT, wrap=True, font=MONO, line_spacing=0.35)
    entities.add("BestText", [y.child_rect((0.92, 0.66)), best], parent=record)
    entities.add("RecordTitle", [y.child_rect((0.92, 0.18)), label("SERVICE RECORD", 30, RED)], parent=record)
    _how_to_play(entities, root)
    return y.scene("title", entities, header=RAYCAST_HEADER)


def _how_to_play(entities: y.Entities, root: int) -> None:
    window = entities.add(
        "HowToWindow",
        [
            y.rect((0.5, 0.5), (1.22, 0.84)),
            y.window("HOW TO PLAY", (0.07, 0.07, 0.08, 0.97), (0.42, 0.08, 0.06, 1), STEEL, 40, 28),
            y.layout("Column", 8, 10),
        ],
        parent=root,
        visible=False,
    )
    button(entities, window, "HowToBackButton", "Back", "on_how_to_closed", (0.4, 0.12))
    controls = y.text(CONTROLS, 22, "Left", STEEL, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.95, 0.4)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 24, "Left", SOFT, wrap=True, line_spacing=0.2)
    entities.add("HowToRules", [y.child_rect((0.95, 0.3)), rules], parent=window)


def settings() -> str:
    """Settings: master volume, mouse turn sensitivity, interface size, reset, Back (saves)."""
    entities = y.Entities(3310000000000000000)
    _flat_world(entities)
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.lua")])
    board = entities.add(
        "SettingsList",
        [y.rect((0.5, 0.5), (0.95, 0.85)), _dossier(), y.layout("Column", 6, 12)],
        parent=root,
    )

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(callback, value, (0.12, 0.12, 0.13, 1), RED, STEEL)
        return [y.child_rect((0.86, 0.05)), bar, y.focus(initial, True, "", RED, 3)]

    button(entities, board, "BackButton", "Back", "go_back", (0.6, 0.11))
    button(entities, board, "ResetButton", "Reset defaults", "on_reset_clicked", (0.6, 0.11))
    rows: list[tuple[str, list[str]]] = [
        ("TitleText", [y.child_rect((0.9, 0.14)), label("SETTINGS", 56, RED)]),
        ("MasterVolLabel", [y.child_rect((0.9, 0.08)), label("Master volume: 100%", 26, SOFT, font="")]),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("MouseLabel", [y.child_rect((0.9, 0.08)), label("Mouse turn: 1.00", 26, SOFT, font="")]),
        ("MouseSlider", slider("on_mouse_changed", 0.27, False)),
        ("UiScaleLabel", [y.child_rect((0.9, 0.08)), label("Interface size: 100%", 26, SOFT, font="")]),
        ("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False)),
    ]
    for tag, components in reversed(rows):
        entities.add(tag, components, parent=board)
    return y.scene("settings", entities)


TALLY_LINES = [
    ("ItemsLabel", "ITEMS"),
    ("SecretsLabel", "SECRETS"),
    ("MapLabel", "MAP EXPLORED"),
    ("BonusLabel", "BONUS"),
]
"""The lines the tally counts up, one after the other (``tally.lua``)."""


def tally() -> str:
    """Tally between two floors: the ratios and the bonus counted up one line after the other, the scores, Next."""
    entities = y.Entities(3320000000000000000)
    _flat_world(entities)
    root = entities.add("TallyCanvas", [y.canvas(), y.script("scripts/ui/tally.lua")])
    group = entities.add(
        "TallyGroup",
        [y.rect((0.5, 0.5), (1.1, 0.9)), _dossier(), y.layout("Column", 6, 14)],
        parent=root,
    )
    button(entities, group, "NextButton", "Next floor", "on_next_clicked", (0.48, 0.1), True)
    hint = label("Enter, Space or pad A skips the count", 20, SOFT, font="")
    entities.add("SkipHint", [y.child_rect((0.95, 0.05)), hint], parent=group)
    entities.add("BestLabel", [y.child_rect((0.95, 0.06)), label("", 22, SOFT, font="")], parent=group)
    entities.add("ScoreLabel", [y.child_rect((0.95, 0.09)), label("", 30, STEEL, font=MONO)], parent=group)
    for tag, _ in reversed(TALLY_LINES):
        entities.add(tag, [y.child_rect((0.7, 0.085)), label("", 34, STEEL, "Left", MONO)], parent=group)
    entities.add("TallyTitle", [y.child_rect((0.95, 0.12)), label("FLOOR 1 COMPLETED", 52, RED)], parent=group)
    return y.scene("tally", entities)


def victory() -> str:
    """Victory: the elevator of the last floor taken, the final score, back to the title."""
    entities = y.Entities(3330000000000000000)
    _flat_world(entities)
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.lua")])
    group = entities.add(
        "ResultGroup", [y.rect((0.5, 0.5), (1.1, 0.7)), _dossier(), y.layout("Column", 8, 14)], parent=root
    )
    button(entities, group, "MenuButton", "Title screen", "on_menu_clicked", (0.48, 0.14), True)
    entities.add("ScoreLabel", [y.child_rect((0.95, 0.14)), label("", 32, STEEL, font=MONO)], parent=group)
    story = "The elevator of the headquarters takes you out. Every floor can be explored again for a better tally."
    story_text = y.text(story, 26, "Center", SOFT, wrap=True)
    entities.add("StoryLabel", [y.child_rect((0.95, 0.25)), story_text], parent=group)
    entities.add("VictoryTitle", [y.child_rect((0.95, 0.24)), label("YOU ESCAPED!", 76, RED)], parent=group)
    return y.scene("victory", entities)


def hud() -> str:
    """The play HUD: floor and score top left, the keys top right, a message at the bottom, the pause window."""
    entities = y.Entities(3100000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    top_left = entities.add(
        "HudTopLeft", [y.rect((0, 0), (0.75, 0.13), (16, 16)), y.layout("Column", 0, 0, "Start")], parent=root
    )
    entities.add("ScoreText", [y.child_rect((1, 0.45)), label("SCORE 0", 28, STEEL, "Left", MONO)], parent=top_left)
    floor_name = label("FLOOR 1: THE CELLARS", 30, (0.9, 0.78, 0.45, 1), "Left")
    entities.add("FloorText", [y.child_rect((1, 0.55)), floor_name], parent=top_left)
    keys = entities.add("KeysRow", [y.rect((1, 0), (0.21, 0.1), (-16, 16)), y.layout("Row", 0)], parent=root)
    for tag, texture in (("BlueKeyIcon", "key_blue"), ("GoldKeyIcon", "key_yellow")):
        icon = [y.child_rect((0.47, 1)), y.image(f"textures/wolf3d/sprites/{texture}.png")]
        entities.add(tag, icon, parent=keys, visible=False)
    toast = y.text("", 28, "Center", (1, 0.95, 0.55, 1), wrap=True, font=HEADING, outline=INK)
    entities.add(
        "ToastText",
        [y.rect((0.5, 1), (0.4, 0.08), (0, -16), ("ScreenWidth", "ScreenHeight")), toast],
        parent=root,
        visible=False,
    )
    window = entities.add(
        "PauseWindow",
        [
            y.rect((0.5, 0.5), (1.1, 0.86)),
            y.window("PAUSED", (0.07, 0.07, 0.08, 0.97), (0.42, 0.08, 0.06, 1), STEEL, 40, 28),
            y.layout("Column", 8, 10),
        ],
        parent=root,
        visible=False,
    )
    controls = y.text(CONTROLS, 21, "Left", STEEL, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("ControlsText", [y.child_rect((0.95, 0.36)), controls], parent=window)
    for index, (tag, text, callback) in reversed(
        list(
            enumerate([("ResumeButton", "Resume", "on_pause_resume"), ("QuitButton", "Quit to title", "on_pause_quit")])
        )
    ):
        button(entities, window, tag, text, callback, (0.5, 0.15), index == 0)
    return y.hud_file("play", entities)


def main() -> None:
    """Write the four menu scenes and the play HUD."""
    (ROOT / "scenes" / "title.owl").write_text(title())
    (ROOT / "scenes" / "settings.owl").write_text(settings())
    (ROOT / "scenes" / "tally.owl").write_text(tally())
    (ROOT / "scenes" / "victory.owl").write_text(victory())
    (ROOT / "huds" / "play.owlhud").write_text(hud())


if __name__ == "__main__":
    main()
