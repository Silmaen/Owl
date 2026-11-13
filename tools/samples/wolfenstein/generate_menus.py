#!/usr/bin/env python3
"""
Write the menu scenes and the play HUD of the Wolfenstein mini sample game into ``samples/wolfenstein/``: the title,
the settings, the tally between two floors, the victory and ``huds/play.owlhud``, in the dark stone and military grey
look of the game.

The title stands in front of a raycast view of the courtyard of floor 3, turning slowly in a dark fog
(``scripts/ui/title_view.owlvs``): the title is engraved in a riveted steel plate, the menus are the windows of a 1992
DOS game: blood red boxes with a bright rim and a hard black shadow, grey entries in the pixel font
``Jersey10-Regular`` (``fonts/``), the entry under the cursor lit by a red bar with a skull at its left
(``textures/ui/dos_select.png``, a nine-slice shown only when the entry is hovered or focused). On the title the menu
is on the left and the service record (the best tallies) on the right. The play HUD is the status bar of Wolfenstein
3D along the bottom (floor, score, face, health, ammo, keys, weapon; its art and its graph from
``generate_status.py``), the red of the hits and of low health over the view, and the pause window with the steel
plates (``textures/ui/plate.png``): the ratios of the floor wait for the tally, the controls for the pause window.
The art of the menus is written here too. The canvas is 1280 x 720 reference pixels, every size and offset in
screen-height units. The scripts live in ``scripts/ui/`` and
``scripts/game/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.wolfenstein.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples import scene_yaml as y
from tools.samples.wolfenstein import generate_levels as levels

ROOT = Path(__file__).resolve().parents[3] / "samples" / "wolfenstein"
"""The sample project."""
BACKGROUND = (0.4, 0, 0, 1)
"""Colour behind the menus without a raycast view: the red of the DOS menus."""
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
DOS = "Jersey10-Regular"
"""Font of the menus: DOS pixels."""
DOS_GREY = (0.72, 0.72, 0.72, 1)
"""Colour of a menu entry."""
DOS_LIGHT = (0.95, 0.95, 0.9, 1)
"""Colour of the headings of the menus."""
DOS_YELLOW = (1, 0.86, 0.2, 1)
"""Colour of the numbers of the menus."""
DOS_BOX = (0.24, 0, 0, 0.97)
"""Fill of a menu window."""
DOS_RIM = y.box(
    border=(0.68, 0.06, 0.04, 1), border_width=3, shadow=(0, 0, 0, 0.9), shadow_offset=(8, 8), shadow_blur=0
)
"""The rim and the hard shadow of a menu window."""
SELECT = y.box(texture="textures/ui/dos_select.png", insets=(48, 0, 8, 0))
"""The bar under the cursor: a nine-slice whose left part holds the skull."""
SELECT_ROW = 48
"""Height of the bar, in reference pixels: the skull keeps its shape at that height."""
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


def dos_button(
    entities: y.Entities,
    parent: int,
    tag: str,
    text: str,
    callback: str,
    size: tuple[float, float],
    initial: bool = False,
) -> int:
    """An entry of a DOS menu in a layout: grey text on the window, the red bar with the skull when hovered or focused
    (the bar is the button colour: none at rest), no frame."""
    components = [
        y.child_rect(size),
        y.button(callback, (1, 1, 1, 0), (1, 1, 1, 1), (0.7, 0.7, 0.7, 1), (1, 1, 1, 0), SELECT),
        y.text(text, 40, "Center", DOS_GREY, font=DOS, outline=(0, 0, 0, 1), outline_width=1),
        y.focus(initial=initial, colour=(0, 0, 0, 0), thickness=1),
    ]
    return entities.add(tag, components, parent=parent)


def dos_text(value: str, size: int, colour: y.Colour = DOS_GREY, align: str = "Center", wrap: bool = False) -> str:
    """A text of a DOS menu."""
    return y.text(value, size, align, colour, wrap=wrap, font=DOS, outline=(0, 0, 0, 1), outline_width=1)


def _dossier() -> str:
    """A DOS menu window: blood red, a bright rim, a hard black shadow."""
    return y.panel(DOS_BOX, box_style=DOS_RIM)


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
            y.script("scripts/ui/title_view.owlvs", [("turn_speed", "float", 0.12)]),
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
    "FIRE        Ctrl, F, right trigger, pad B\n"
    "WEAPONS     1 to 4, left bumper\n"
    "MAP         Tab, pad Y        SAVE / LOAD   F6 / F9\n"
    "PAUSE       Escape, pad Start"
)
"""The controls, in the How to play window and the pause window."""


def title() -> str:
    """Title: the steel title, the DOS menu on the left, the service record on the right, the How to play window."""
    entities = y.Entities(3300000000000000000)
    _raycast_world(entities)
    root = entities.add("TitleCanvas", [y.canvas(), y.script("scripts/ui/title.owlvs")])
    art = y.image("textures/ui/title.png")
    entities.add("TitleArt", [y.rect((0.5, 0), (1.2, 0.257), (0, 0.03), SCREEN, SCREEN), art], parent=root)
    menu = entities.add(
        "ButtonPanel",
        [y.rect((0, 0), (0.56, 0.5), (0.06, 0.36), SCREEN, SCREEN), _dossier(), y.layout("Column", 4, 16)],
        parent=root,
    )
    rows = [
        ("NewGameButton", "New game", "on_new_game_clicked"),
        ("ContinueButton", "Continue", "on_continue_clicked"),
        ("HowToButton", "How to play", "on_how_to_clicked"),
        ("SettingsButton", "Settings", "on_settings_clicked"),
        ("QuitButton", "Quit", "on_quit_clicked"),
    ]
    for index, (tag, text, callback) in reversed(list(enumerate(rows))):
        dos_button(entities, menu, tag, text, callback, (0.82, SELECT_ROW / (0.5 * 720)), index == 0)
    record = entities.add(
        "RecordBoard",
        [y.rect((1, 0), (0.62, 0.5), (-0.06, 0.36), SCREEN, SCREEN), _dossier(), y.layout("Column", 6, 16)],
        parent=root,
    )
    best = y.text("Best score 0", 26, "Left", DOS_YELLOW, wrap=True, font=DOS, line_spacing=0.2)
    entities.add("BestText", [y.child_rect((0.92, 0.7)), best], parent=record)
    entities.add("RecordTitle", [y.child_rect((0.92, 0.16)), dos_text("SERVICE RECORD", 36, DOS_LIGHT)], parent=record)
    _how_to_play(entities, root)
    entities.add("MenuMusic", [y.music("sounds/music/menu.wav", 0.6)])
    return y.scene("title", entities, header=RAYCAST_HEADER)


def _how_to_play(entities: y.Entities, root: int) -> None:
    window = entities.add(
        "HowToWindow",
        [
            y.rect((0.5, 0.5), (1.22, 0.84)),
            y.window("HOW TO PLAY", DOS_BOX, (0.62, 0.05, 0.03, 1), DOS_LIGHT, 40, 30),
            y.layout("Column", 8, 10),
        ],
        parent=root,
        visible=False,
    )
    dos_button(entities, window, "HowToBackButton", "Back", "on_how_to_closed", (0.4, SELECT_ROW / (0.84 * 720)))
    controls = y.text(CONTROLS, 22, "Left", DOS_YELLOW, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.95, 0.4)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 28, "Left", DOS_GREY, wrap=True, font=DOS, line_spacing=0.15)
    entities.add("HowToRules", [y.child_rect((0.95, 0.3)), rules], parent=window)


def settings() -> str:
    """Settings, in two columns: the sound (master, music, effects) and the mouse turn on the left, the interface size
    and the accessibility (colour vision, reduced motion, subtitles) on the right; reset and Back (saves) below. A row
    lays its children out from the right, a column from the bottom."""
    entities = y.Entities(3310000000000000000)
    _flat_world(entities)
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.owlvs")])
    entities.add("MenuMusic", [y.music("sounds/music/menu.wav", 0.6)])
    board = entities.add(
        "SettingsList",
        [
            y.rect((0.5, 0.5), (0.94, 0.86), units=("ScreenWidth", "ScreenHeight")),
            _dossier(),
            y.layout("Column", 8, 14),
        ],
        parent=root,
    )

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(
            callback, value, (0.05, 0, 0, 1), (0.85, 0.1, 0.06, 1), DOS_GREY, y.box(border=DOS_GREY, border_width=1)
        )
        return [y.child_rect((0.9, 0.06)), bar, y.focus(initial, True, "", DOS_YELLOW, 2)]

    def setting_label(text: str) -> list[str]:
        return [y.child_rect((0.95, 0.1)), dos_text(text, 30)]

    row = SELECT_ROW / (0.86 * 720)
    buttons = entities.add("SettingsButtons", [y.child_rect((0.9, row)), y.layout("Row", 16)], parent=board)
    back = dos_button(entities, buttons, "BackButton", "Back", "go_back", (0.42, 1))
    reset = dos_button(entities, buttons, "ResetButton", "Reset defaults", "on_reset_clicked", (0.42, 1))
    columns = entities.add("SettingsColumns", [y.child_rect((0.96, 0.66)), y.layout("Row", 16)], parent=board)
    right = entities.add("DisplayColumn", [y.child_rect((0.48, 1)), y.layout("Column", 6)], parent=columns)
    left = entities.add("SoundColumn", [y.child_rect((0.48, 1)), y.layout("Column", 6)], parent=columns)
    left_rows: list[tuple[str, list[str]]] = [
        ("MasterVolLabel", setting_label("Master volume: 100%")),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("MusicVolLabel", setting_label("Music: 80%")),
        ("MusicVolSlider", slider("on_music_vol_changed", 0.8, False)),
        ("SfxVolLabel", setting_label("Effects: 100%")),
        ("SfxVolSlider", slider("on_sfx_vol_changed", 1.0, False)),
        ("MouseLabel", setting_label("Mouse turn: 1.00")),
        ("MouseSlider", slider("on_mouse_changed", 0.27, False)),
    ]
    ids = {tag: entities.add(tag, components, parent=left) for tag, components in reversed(left_rows)}
    toggles = [
        ("SubtitlesButton", "Subtitles: on", "on_subtitles_clicked"),
        ("ReducedMotionButton", "Reduced motion: off", "on_reduced_motion_clicked"),
        ("ColorBlindButton", "Colour vision: standard", "on_color_blind_clicked"),
    ]
    for tag, text, callback in toggles:
        ids[tag] = dos_button(entities, right, tag, text, callback, (0.96, SELECT_ROW / (0.66 * 0.86 * 720)))
    entities.add("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False), parent=right)
    entities.add("UiScaleLabel", setting_label("Interface size: 100%"), parent=right)
    entities.add("TitleText", [y.child_rect((0.9, 0.12)), dos_text("SETTINGS", 60, DOS_LIGHT)], parent=board)
    # Left and right move a slider: the buttons below lead from one column to the other.
    y.neighbours(entities, reset, up=ids["MouseSlider"], right=back)
    y.neighbours(entities, back, up=ids["SubtitlesButton"], left=reset)
    y.neighbours(entities, ids["SubtitlesButton"], down=back)
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
    root = entities.add("TallyCanvas", [y.canvas(), y.script("scripts/ui/tally.owlvs")])
    group = entities.add(
        "TallyGroup",
        [y.rect((0.5, 0.5), (1.1, 0.9)), _dossier(), y.layout("Column", 6, 14)],
        parent=root,
    )
    dos_button(entities, group, "NextButton", "Next floor", "on_next_clicked", (0.48, SELECT_ROW / (0.9 * 720)), True)
    hint = dos_text("Enter, Space or pad A skips the count", 24)
    entities.add("SkipHint", [y.child_rect((0.95, 0.05)), hint], parent=group)
    entities.add("BestLabel", [y.child_rect((0.95, 0.06)), dos_text("", 26)], parent=group)
    entities.add("ScoreLabel", [y.child_rect((0.95, 0.09)), dos_text("", 36, DOS_YELLOW)], parent=group)
    for tag, _ in reversed(TALLY_LINES):
        entities.add(tag, [y.child_rect((0.7, 0.085)), dos_text("", 40, DOS_LIGHT, "Left")], parent=group)
    entities.add("TallyTitle", [y.child_rect((0.95, 0.12)), dos_text("FLOOR 1 COMPLETED", 60, DOS_LIGHT)], parent=group)
    return y.scene("tally", entities)


def victory() -> str:
    """Victory: the elevator of the last floor taken, the final score, back to the title."""
    entities = y.Entities(3330000000000000000)
    _flat_world(entities)
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.owlvs")])
    group = entities.add(
        "ResultGroup", [y.rect((0.5, 0.5), (1.1, 0.7)), _dossier(), y.layout("Column", 8, 14)], parent=root
    )
    dos_button(entities, group, "MenuButton", "Title screen", "on_menu_clicked", (0.48, SELECT_ROW / (0.7 * 720)), True)
    entities.add("ScoreLabel", [y.child_rect((0.95, 0.14)), dos_text("", 40, DOS_YELLOW)], parent=group)
    story = "The elevator of the headquarters takes you out. Every floor can be explored again for a better tally."
    story_text = dos_text(story, 32, wrap=True)
    entities.add("StoryLabel", [y.child_rect((0.95, 0.25)), story_text], parent=group)
    entities.add("VictoryTitle", [y.child_rect((0.95, 0.24)), dos_text("YOU ESCAPED!", 84, DOS_LIGHT)], parent=group)
    return y.scene("victory", entities)


BAR = 0.16
"""Height of the status bar along the bottom of the screen, in screen heights (the minimap of ``explore.owlvs`` sits
above it)."""
BAR_FILL = (0.05, 0.07, 0.34, 1)
"""The navy blue of the status bar."""
BAR_RIM = y.box(border=(0.36, 0.42, 0.78, 1), border_width=2)
"""The light rim of the status bar."""
CELL_FILL = (0.01, 0.02, 0.12, 1)
"""The dark blue of a box of the status bar."""
CELL_RIM = y.box(border=(0.2, 0.25, 0.58, 1), border_width=2)
"""The rim of a box of the status bar."""
CAPTION = (0.66, 0.72, 0.96, 1)
"""Colour of the names of the boxes."""
DIGITS = (0.96, 0.96, 0.9, 1)
"""Colour of the numbers of the status bar."""


def _sheet(texture: str, columns: int, rows: int, frame: int = 0) -> str:
    """A ``UiImage`` showing one frame of a sprite sheet (``ui.set_image_frame``)."""
    return (
        f"UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:{texture}\n  columns: {columns}\n  rows: {rows}\n"
        f"  frame: {frame}\n"
    )


BAR_BOXES: tuple[tuple[str, float], ...] = (
    ("FloorCell", 0.13),
    ("ScoreCell", 0.16),
    ("FaceCell", 0),
    ("HealthCell", 0.12),
    ("AmmoCell", 0.1),
    ("KeysRow", 0.06),
    ("WeaponCell", 0.17),
)
"""The boxes of the status bar from left to right, with their share of the width the face leaves (0 for the square
face)."""
BAR_BOX = 0.86
"""Height of a box of the status bar, in bar heights (the face is as wide)."""
BAR_GAP = 0.05
"""Space around and between the boxes of the status bar, in bar heights."""


def _bar_rects() -> dict[str, str]:
    """
    The ``UiRect`` of each box of the status bar. A box stretches between anchors spread over the whole bar by its
    share, shifted and shrunk in bar heights by what is fixed (the face, the gaps): the boxes fill the bar from edge to
    edge at any screen width, with the same gaps and a square face.
    """
    shares = sum(share for _, share in BAR_BOXES)
    fixed = BAR_BOX + BAR_GAP * (len(BAR_BOXES) + 1)
    rects: dict[str, str] = {}
    start, before = 0.0, BAR_GAP
    for tag, share in BAR_BOXES:
        span = share / shares
        width = span * -fixed if share else BAR_BOX
        rects[tag] = (
            f"UiRect:\n  anchorMin: {y.vec(start, 0)}\n  anchorMax: {y.vec(start + span, 0)}\n  pivot: [0, 0]\n"
            f"  size: {y.vec(width, BAR_BOX)}\n  sizeUnit: [ParentHeight, ParentHeight]\n"
            f"  offset: {y.vec(before - start * fixed, (1 - BAR_BOX) / 2)}\n"
            "  offsetUnit: [ParentHeight, ParentHeight]\n"
        )
        start += span
        before += (0 if share else BAR_BOX) + BAR_GAP
    return rects


def _cell(entities: y.Entities, parent: int, tag: str) -> int:
    """A dark box of the status bar, placed by ``_bar_rects``; a layout shows its children in the reverse order of
    their entities."""
    layout = y.layout("Column", 0, 2)
    return entities.add(tag, [_bar_rects()[tag], y.panel(CELL_FILL, box_style=CELL_RIM), layout], parent=parent)


def _status_bar(entities: y.Entities, root: int) -> None:
    """The bar of Wolfenstein 3D along the bottom of the screen, left to right: the floor, the score, the face, the
    health, the ammo, the keys and the weapon, filled by the graphs (``level.owlvs``, ``arsenal.owlvs``,
    ``status.owlvs``). Each box is placed by ``_bar_rects``, whatever the order of the entities."""
    bar = entities.add(
        "StatusBar",
        [
            y.rect((0.5, 1), (1, BAR), units=("ScreenWidth", "ScreenHeight")),
            y.panel(BAR_FILL, box_style=BAR_RIM),
        ],
        parent=root,
    )
    weapon = _cell(entities, bar, "WeaponCell")
    entities.add("WeaponText", [y.child_rect((0.96, 0.3)), dos_text("PISTOL", 26, CAPTION)], parent=weapon)
    entities.add(
        "WeaponIcon", [y.child_rect((0.92, 0.62)), _sheet("textures/ui/weapon_icons.png", 4, 1, 1)], parent=weapon
    )
    keys = _cell(entities, bar, "KeysRow")
    for tag, texture in (("BlueKeyIcon", "key_blue"), ("GoldKeyIcon", "key_yellow")):
        icon = [
            y.rect((0, 0), (0.4, 0.4), units=("ParentHeight", "ParentHeight")),
            y.image(f"textures/wolf3d/sprites/{texture}.png"),
        ]
        entities.add(tag, icon, parent=keys, visible=False)
    for tag, caption, text, value in (
        ("AmmoCell", "AMMO", "AmmoText", "8"),
        ("HealthCell", "HEALTH", "HealthText", "100%"),
    ):
        cell = _cell(entities, bar, tag)
        entities.add(text, [y.child_rect((0.96, 0.66)), dos_text(value, 64, DIGITS)], parent=cell)
        entities.add(f"{tag}Caption", [y.child_rect((0.96, 0.3)), dos_text(caption, 26, CAPTION)], parent=cell)
    # Square: as wide as a box is high.
    face = entities.add(
        "FaceCell",
        [_bar_rects()["FaceCell"], y.panel(CELL_FILL, box_style=CELL_RIM)],
        parent=bar,
    )
    image = [y.child_rect((0.92, 0.92), (0.5, 0.5)), _sheet("textures/ui/face.png", 5, 6, 1)]
    entities.add("FaceImage", image, parent=face)
    score = _cell(entities, bar, "ScoreCell")
    entities.add("ScoreText", [y.child_rect((0.96, 0.66)), dos_text("0", 64, DIGITS)], parent=score)
    entities.add("ScoreCellCaption", [y.child_rect((0.96, 0.3)), dos_text("SCORE", 26, CAPTION)], parent=score)
    floor = _cell(entities, bar, "FloorCell")
    floor_name = dos_text("FLOOR 1: THE CELLARS", 34, DOS_YELLOW, wrap=True)
    entities.add("FloorText", [y.child_rect((0.94, 0.92)), floor_name], parent=floor)


def hud() -> str:
    """The play HUD: the red of the hits and of low health over the view, the weapon in hand, the status bar along
    the bottom, a message above the weapon, the pause window."""
    entities = y.Entities(3100000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    # The weapon in the hands, on the status bar: a frame of the weapon sheet (a row per weapon), set by
    # `arsenal.owlvs`.
    view = (
        "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/weapons/weapons.png\n"
        "  columns: 5\n  rows: 4\n  frame: 5\n"
    )
    # One group over the view, above the bar: what it holds overlaps on purpose.
    scene = entities.add("View", [y.rect((0.5, 0), (1, 1 - BAR), units=("ScreenWidth", "ScreenHeight"))], parent=root)
    weapon_rect = y.rect((0.5, 1), (0.46, 0.46))
    entities.add("WeaponView", [weapon_rect, view], parent=scene)
    # Frames of red sheets chosen by `status.owlvs` (the first frame is clear).
    view_rect = y.child_rect((1, 1))
    entities.add("LowHealthTint", [view_rect, _sheet("textures/ui/low_health.png", 4, 2)], parent=scene)
    entities.add("HurtSide", [view_rect, _sheet("textures/ui/hurt_side.png", 4, 4)], parent=scene)
    entities.add("HurtFlash", [view_rect, _sheet("textures/ui/hurt_flash.png", 8, 1)], parent=scene)
    _status_bar(entities, root)
    toast = y.text("", 28, "Center", (1, 0.95, 0.55, 1), wrap=True, font=HEADING, outline=INK)
    entities.add(
        "ToastText",
        # above the weapon in hand
        [
            y.rect((0.5, 1), (0.4, 0.08), (0, -0.52), ("ScreenWidth", "ScreenHeight"), ("Pixels", "ScreenHeight")),
            toast,
        ],
        parent=scene,
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


SKULL = [
    "....######....",
    "..##########..",
    ".############.",
    ".############.",
    "##############",
    "###..####..###",
    "##....##....##",
    "##....##....##",
    "###..####..###",
    ".#####..#####.",
    "..####..####..",
    "...########...",
    "...#.#.#.#.#..",
    "....#.#.#.#...",
]
"""The skull of the cursor, one character per pixel (``#`` bone)."""


def dos_select() -> Image.Image:
    """The bar under the cursor (320 x 48, a nine-slice of 48 on the left, 8 on the right): a red bar lit at the top,
    shaded at the bottom, the skull in its left square, drawn at 3 pixels per skull pixel."""
    width, height = 320, SELECT_ROW
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rectangle([40, 4, width - 1, height - 5], fill=(150, 8, 4, 255))
    draw.line([(40, 4), (width - 1, 4)], fill=(222, 64, 40, 255), width=2)
    draw.line([(40, height - 6), (width - 1, height - 6)], fill=(70, 0, 0, 255), width=2)
    pixel = 3
    top = (height - len(SKULL) * pixel) // 2
    for row, line in enumerate(SKULL):
        for column, char in enumerate(line):
            if char != "#":
                continue
            x0, y0 = 2 + column * pixel, top + row * pixel
            draw.rectangle([x0 + 2, y0 + 2, x0 + pixel + 1, y0 + pixel + 1], fill=(0, 0, 0, 200))
    for row, line in enumerate(SKULL):
        for column, char in enumerate(line):
            if char != "#":
                continue
            x0, y0 = 2 + column * pixel, top + row * pixel
            shade = 236 if row < 5 else 200
            draw.rectangle([x0, y0, x0 + pixel - 1, y0 + pixel - 1], fill=(shade, shade, shade - 14, 255))
    return img


def main() -> None:
    """Write the art of the menus, the four menu scenes and the play HUD."""
    dos_select().save(ROOT / "textures" / "ui" / "dos_select.png")
    (ROOT / "scenes" / "title.owl").write_text(title())
    (ROOT / "scenes" / "settings.owl").write_text(settings())
    (ROOT / "scenes" / "tally.owl").write_text(tally())
    (ROOT / "scenes" / "victory.owl").write_text(victory())
    (ROOT / "huds" / "play.owlhud").write_text(hud())


if __name__ == "__main__":
    main()
