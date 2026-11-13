#!/usr/bin/env python3
"""
Write the menu scenes of the Sokoban sample game into ``samples/sokoban/scenes/``: the title, the level select, the
settings and the victory screen, in the wooden warehouse look of the game.

Every menu stands over a warehouse corridor at the bottom of the screen (world tiles of ``textures/sokoban_tiles.png``);
on the title the hero pushes a crate along it onto its goal (``scripts/ui/warehouse.owlvs``). The title is a crate per
letter with a stencilled letter; the buttons of the title are a row of crates side by side, their names stencilled in
dark paint (``textures/ui/crate_face.png``, a nine-slice tinted by the button colour: the wood lights up when hovered or
focused, an amber frame around the focused one), the other buttons the same crate wood stretched into a board. The
progress, the settings, the level list and the victory are pinned on a clipboard (``textures/ui/clipboard.png``) in
typewriter ink. Labels in the stencil font ``BlackOpsOne-Regular`` (``fonts/``). The art is written here too. The canvas
is 1280 x 720 reference pixels, every size and offset in screen-height units. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sokoban.generate_menus``.
"""

from __future__ import annotations

import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

from tools.samples import scene_yaml as y
from tools.samples.sokoban.generate_assets import COLUMNS, ROWS, TILES, WOOD_DARK, _wood

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sokoban"
"""The sample project."""
SHEET = "textures/sokoban_tiles.png"
"""The tile sheet of the game."""
BACKGROUND = (0.09, 0.06, 0.04, 1)
"""Colour behind everything: a dark warehouse."""
CREAM = (1, 0.93, 0.78, 1)
"""Colour of the titles and labels on dark wood."""
PAINT = (0.13, 0.07, 0.03, 1)
"""Stencil paint on the crates."""
TYPE = (0.17, 0.15, 0.2, 1)
"""Typewriter ink on the clipboard."""
STAMP = (0.66, 0.13, 0.08, 1)
"""Red of the rubber stamp: the headings of the clipboard."""
SOFT = (0.9, 0.82, 0.68, 0.95)
"""Colour of the secondary texts."""
INK = (0.24, 0.13, 0.05, 1)
"""Outline of the texts: burnt wood."""
AMBER = (1, 0.72, 0.2, 1)
"""Focus frame."""
BUTTON = (0.35, 0.22, 0.1)
"""Button colour under the plank (the buttons of the level HUD)."""
BUTTON_HOVER = (0.85, 0.58, 0.22, 1)
"""Hovered or focused plank button: the plank lights up."""
BUTTON_PRESSED = (0.22, 0.13, 0.05, 1)
"""Pressed plank button."""
CRATE_WOOD = (0.7, 0.56, 0.42)
"""Tint of the crate wood of a menu button."""
CRATE_HOVER = (1, 0.93, 0.72, 1)
"""Hovered or focused crate button: the wood lights up."""
CRATE_PRESSED = (0.5, 0.38, 0.27, 1)
"""Pressed crate button."""
CRATE = y.box(
    shadow=(0, 0, 0, 0.55),
    shadow_offset=(4, 6),
    shadow_blur=6,
    texture="textures/ui/crate_face.png",
    insets=(22, 22, 22, 22),
)
"""A crate face: the nine-slice of its frame boards, a shadow on the floor."""
HEADING = "OpenSans-ExtraBold"
"""Font of the titles of the level HUD."""
LABEL = "OpenSans-Bold"
"""Font of the plank buttons of the level HUD."""
STENCIL = "BlackOpsOne-Regular"
"""Font of the menus: stencil."""
TYPEWRITER = "JetBrainsMono-Regular"
"""Font of what is typed on the clipboard."""
CLIPBOARD = y.panel(
    (1, 1, 1, 1),
    box_style=y.box(
        shadow=(0, 0, 0, 0.5),
        shadow_offset=(6, 8),
        shadow_blur=10,
        texture="textures/ui/clipboard.png",
        insets=(40, 70, 40, 30),
    ),
)
"""The clipboard: a board, a metal clip at the top, a sheet of paper."""
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
    corridor = entities.add("Corridor", [y.script("scripts/ui/warehouse.owlvs")])
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
    """A plank button in a layout (the level HUD): the button colour shows through the plank, the label is cream on
    burnt wood."""
    components = [
        y.child_rect(size),
        y.button(callback, BUTTON, BUTTON_HOVER, BUTTON_PRESSED, (0.04, 0.03, 0.02, 1)),
        label(text, 30),
        y.focus(initial=initial, focusable=focusable, colour=AMBER, thickness=3),
    ]
    uuid = entities.add(tag, components, parent=parent)
    entities.add(f"{tag}Plank", [y.child_rect((1, 1)), y.image("textures/ui/plank.png")], parent=uuid)
    return uuid


def crate_button(
    entities: y.Entities,
    parent: int,
    tag: str,
    text: str,
    callback: str,
    size: tuple[float, float],
    initial: bool = False,
) -> int:
    """A crate button of the menus in a layout: crate wood tinted by the button colour, its name stencilled in dark
    paint."""
    painted = y.text(text.upper(), 28, "Center", PAINT, wrap=True, font=STENCIL, outline_width=0)
    components = [
        y.child_rect(size),
        y.button(callback, CRATE_WOOD, CRATE_HOVER, CRATE_PRESSED, (0.3, 0.27, 0.24, 1), CRATE),
        painted,
        y.focus(initial=initial, colour=AMBER, thickness=3),
    ]
    return entities.add(tag, components, parent=parent)


def typed(value: str, size: int, colour: y.Colour = TYPE, align: str = "Center") -> str:
    """A line typed on the clipboard."""
    return y.text(value, size, align, colour, font=TYPEWRITER, outline_width=0)


def stamped(value: str, size: int) -> str:
    """A heading of the clipboard, rubber-stamped in red."""
    return y.text(value, size, "Center", STAMP, font=STENCIL, outline_width=0)


def title() -> str:
    """Title: the crates of the title, a row of crate buttons, the clipboard of the progress under them, the How to
    play window."""
    entities = y.Entities(7300000000000000000)
    _world(entities, "push")
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.owlvs")])
    art = y.image("textures/ui/title.png")
    entities.add("TitleArt", [y.rect((0.5, 0), (1.2, 0.189), (0, 0.03), SCREEN, SCREEN), art], parent=root)
    panel = entities.add(
        "ButtonPanel",
        [y.rect((0.5, 0), (1.06, 0.2), (0, 0.26), SCREEN, SCREEN), y.layout("Row", 10)],
        parent=root,
    )
    rows = [
        ("PlayButton", "Play", "on_play_clicked"),
        ("ContinueButton", "Continue", "on_continue_clicked"),
        ("HowToButton", "How to play", "on_how_to_clicked"),
        ("SettingsButton", "Settings", "on_settings_clicked"),
        ("QuitButton", "Quit", "on_quit_clicked"),
    ]
    # A row lays its children out from the right: the crates are added from the last one.
    ids = {}
    for index, (tag, text, callback) in reversed(list(enumerate(rows))):
        ids[tag] = crate_button(entities, panel, tag, text, callback, (0.17, 0.95), index == 0)
    # Up and down walk the row too, like left and right.
    tags = [tag for tag, _, _ in rows]
    for index, tag in enumerate(tags):
        moves = {}
        if index > 0:
            moves["up"] = ids[tags[index - 1]]
        if index < len(tags) - 1:
            moves["down"] = ids[tags[index + 1]]
        y.neighbours(entities, ids[tag], **moves)
    log = entities.add(
        "LogBoard",
        [y.rect((0.5, 0), (0.78, 0.3), (0, 0.485), SCREEN, SCREEN), CLIPBOARD, y.layout("Column", 2, 0)],
        parent=root,
    )
    entities.add("BestLabel", [y.child_rect((0.84, 0.1)), typed("", 20)], parent=log)
    entities.add("NextLabel", [y.child_rect((0.84, 0.12)), typed("Next: level 1", 22)], parent=log)
    bar = "UiProgressBar:\n  value: 0\n  backgroundColor: [0.82, 0.78, 0.68, 1]\n  fillColor: [0.66, 0.13, 0.08, 1]\n"
    entities.add("ProgressBar", [y.child_rect((0.8, 0.07)), bar], parent=log)
    entities.add("ProgressLabel", [y.child_rect((0.84, 0.14)), typed("Levels done: 0 / 10", 26)], parent=log)
    entities.add("LogTitle", [y.child_rect((0.84, 0.17)), stamped("WAREHOUSE LOG", 30)], parent=log)
    entities.add("LogClip", [y.child_rect((0.84, 0.25))], parent=log)
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
    crate_button(entities, window, "HowToBackButton", "Back", "on_how_to_closed", (0.4, 0.13), False)
    controls = y.text(CONTROLS, 24, "Left", AMBER, wrap=True, font="JetBrainsMono-Regular", line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.94, 0.33)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 26, "Left", SOFT, wrap=True, line_spacing=0.2)
    entities.add("HowToRules", [y.child_rect((0.94, 0.33)), rules], parent=window)


def level_select() -> str:
    """Level select: the ten levels in a list (done marks and best moves from the settings), Back and Play."""
    entities = y.Entities(7310000000000000000)
    _world(entities, "stacked")
    root = entities.add("LevelSelectCanvas", [y.canvas(), y.script("scripts/ui/level_select.owlvs")])
    column = entities.add(
        "SelectColumn",
        [y.rect((0.5, 0), (1.0, 0.8), (0, 0.01), SCREEN, SCREEN), CLIPBOARD, y.layout("Column", 6, 0)],
        parent=root,
    )
    entities.add("SelectFoot", [y.child_rect((0.86, 0.02))], parent=column)
    row = entities.add("SelectButtons", [y.child_rect((0.86, 0.1)), y.layout("Row", 16)], parent=column)
    crate_button(entities, row, "BackButton", "Back", "go_back", (0.44, 1), False)
    crate_button(entities, row, "PlayButton", "Play", "on_play_clicked", (0.44, 1), False)
    entities.add("LevelInfo", [y.child_rect((0.86, 0.07)), typed("Choose a level", 24)], parent=column)
    entities.add(
        "LevelList",
        [
            y.child_rect((0.86, 0.52)),
            y.ui_list(
                10,
                33,
                24,
                ((0.93, 0.9, 0.82, 1), (0.87, 0.84, 0.75, 1), (0.96, 0.82, 0.55, 1), (0.85, 0.62, 0.3, 1), TYPE),
                "on_level_selected",
                "on_level_bind",
            ),
            y.focus(initial=True, colour=AMBER, thickness=3),
        ],
        parent=column,
    )
    entities.add("TitleText", [y.child_rect((0.86, 0.09)), stamped("CHOOSE A LEVEL", 44)], parent=column)
    entities.add("SelectClip", [y.child_rect((0.86, 0.09))], parent=column)
    return y.scene("level_select", entities)


def settings() -> str:
    """Settings: master, music and effects volumes, interface size, reset of the progress, Back (saves)."""
    entities = y.Entities(7320000000000000000)
    _world(entities, "stacked")
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.owlvs")])
    board = entities.add(
        "SettingsList",
        [y.rect((0.5, 0), (0.95, 0.9), (0, 0.01), SCREEN, SCREEN), CLIPBOARD, y.layout("Column", 5, 0)],
        parent=root,
    )
    slider_focus = "on_slider_focused"

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(callback, value, (0.8, 0.76, 0.66, 1), STAMP, TYPE, y.box(border=TYPE, border_width=1.5))
        return [y.child_rect((0.8, 0.032)), bar, y.focus(initial, True, slider_focus, AMBER, 3)]

    rows: list[tuple[str, list[str]]] = [
        ("SettingsClip", [y.child_rect((0.86, 0.07))]),
        ("TitleText", [y.child_rect((0.86, 0.09)), stamped("SETTINGS", 46)]),
        ("MasterVolLabel", [y.child_rect((0.86, 0.05)), typed("Master Volume: 100%", 24)]),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("MusicVolLabel", [y.child_rect((0.86, 0.05)), typed("Music: 80%", 24)]),
        ("MusicVolSlider", slider("on_music_vol_changed", 0.8, False)),
        ("SfxVolLabel", [y.child_rect((0.86, 0.05)), typed("Effects: 100%", 24)]),
        ("SfxVolSlider", slider("on_sfx_vol_changed", 1.0, False)),
        ("UiScaleLabel", [y.child_rect((0.86, 0.05)), typed("Interface Size: 100%", 24)]),
        ("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False)),
        ("ProgressLabel", [y.child_rect((0.86, 0.05)), typed("Levels done: 0 / 10", 24, STAMP)]),
    ]
    entities.add("SettingsFoot", [y.child_rect((0.86, 0.01))], parent=board)
    crate_button(entities, board, "BackButton", "Back", "go_back", (0.5, 0.074), False)
    crate_button(entities, board, "ResetButton", "Reset defaults", "on_reset_clicked", (0.5, 0.074), False)
    crate_button(
        entities, board, "ResetProgressButton", "Reset progress", "on_reset_progress_clicked", (0.5, 0.074), False
    )
    for tag, components in reversed(rows):
        entities.add(tag, components, parent=board)
    return y.scene("settings", entities)


def victory() -> str:
    """Victory: every level of the game played through, the levels done and the sum of the best moves."""
    entities = y.Entities(7330000000000000000)
    _world(entities, "gold")
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.owlvs")])
    group = entities.add(
        "ResultGroup",
        [y.rect((0.5, 0), (1.0, 0.72), (0, 0.04), SCREEN, SCREEN), CLIPBOARD, y.layout("Column", 8, 0)],
        parent=root,
    )
    entities.add("ResultFoot", [y.child_rect((0.86, 0.02))], parent=group)
    row = entities.add("ButtonRow", [y.child_rect((0.86, 0.15)), y.layout("Row", 16)], parent=group)
    buttons = [("LevelsButton", "Level select", "on_levels_clicked"), ("TitleButton", "Title", "on_title_clicked")]
    for index, (tag, text, callback) in reversed(list(enumerate(buttons))):
        crate_button(entities, row, tag, text, callback, (0.44, 0.85), index == 0)
    score = typed("Levels done: 10 / 10", 28).replace("wrap: false", "wrap: true")
    entities.add("ScoreLabel", [y.child_rect((0.86, 0.26)), score], parent=group)
    entities.add("VictoryTitle", [y.child_rect((0.86, 0.24)), stamped("WAREHOUSE CLEARED", 64)], parent=group)
    entities.add("ResultClip", [y.child_rect((0.86, 0.12))], parent=group)
    return y.scene("victory", entities)


def crate_face() -> Image.Image:
    """A crate face for the buttons (128 x 128, a nine-slice of 22 pixels): light planks across, a frame of darker
    boards nailed at the corners; the button colour tints it."""
    side = 128
    img = _wood((side, side), 21)
    img = Image.blend(img, Image.new("RGBA", img.size, (236, 196, 140, 255)), 0.35)
    draw = ImageDraw.Draw(img)
    for y0 in (42, 85):
        draw.line([(0, y0), (side, y0)], fill=(120, 76, 38, 255), width=2)
    frame = _wood((side, side), 22)
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rectangle([0, 0, side - 1, side - 1], fill=255)
    ImageDraw.Draw(mask).rectangle([18, 18, side - 19, side - 19], fill=0)
    img.paste(frame, (0, 0), mask)
    draw.rectangle([0, 0, side - 1, side - 1], outline=(*WOOD_DARK, 255), width=3)
    draw.rectangle([18, 18, side - 19, side - 19], outline=(*WOOD_DARK, 255), width=2)
    draw.line([(3, 3), (side - 4, 3)], fill=(222, 170, 110, 255), width=2)
    for cx, cy in ((9, 9), (side - 10, 9), (9, side - 10), (side - 10, side - 10)):
        draw.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=(150, 150, 156, 255))
    return img


def clipboard() -> Image.Image:
    """The clipboard (400 x 400, a nine-slice of 40 on the sides, 70 at the top, 30 at the bottom): a board of brown
    hardboard, a sheet of squared paper on it, a steel clip at the top."""
    width, height = 400, 400
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        [0, 0, width - 1, height - 1], 18, fill=(122, 82, 46, 255), outline=(70, 44, 22, 255), width=3
    )
    rng = random.Random(7)
    for _ in range(900):
        x, y0 = rng.randrange(4, width - 4), rng.randrange(4, height - 4)
        tone = rng.randint(-18, 14)
        draw.point((x, y0), fill=(122 + tone, 82 + tone, 46 + tone, 255))
    paper = Image.new("RGBA", (width - 44, height - 66), (238, 232, 214, 255))
    pdraw = ImageDraw.Draw(paper)
    for y0 in range(30, paper.height, 18):
        pdraw.line([(0, y0), (paper.width, y0)], fill=(196, 206, 222, 255), width=1)
    pdraw.line([(26, 0), (26, paper.height)], fill=(226, 160, 150, 255), width=1)
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rectangle([24, 50, 24 + paper.width, 50 + paper.height], fill=(0, 0, 0, 120))
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(3)))
    img.paste(paper, (20, 44))
    draw = ImageDraw.Draw(img)
    clip = [width // 2 - 70, 8, width // 2 + 70, 60]
    draw.rounded_rectangle(clip, 10, fill=(168, 172, 180, 255), outline=(80, 82, 90, 255), width=3)
    draw.rounded_rectangle([clip[0] + 14, clip[1] + 10, clip[2] - 14, clip[1] + 22], 5, fill=(214, 218, 226, 255))
    draw.ellipse([width // 2 - 9, 18, width // 2 + 9, 36], fill=(70, 72, 80, 255))
    return img


def title_art() -> Image.Image:
    """The title (1400 x 220): one wooden crate per letter of SOKOBAN, the letter stencilled in dark paint."""
    letters = "SOKOBAN"
    side, gap = 180, 20
    width = len(letters) * side + (len(letters) - 1) * gap
    img = Image.new("RGBA", (width + 20, side + 40), (0, 0, 0, 0))
    font = ImageFont.truetype(str(ROOT / "fonts" / f"{STENCIL}.ttf"), 128)
    crate = crate_face().resize((side, side), Image.Resampling.LANCZOS)
    for index, letter in enumerate(letters):
        x0, y0 = 10 + index * (side + gap), 14 + (6 if index % 2 else 0)
        shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(shadow).rectangle([x0 + 8, y0 + 12, x0 + side + 8, y0 + side + 12], fill=(0, 0, 0, 140))
        img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(6)))
        face = crate.copy()
        draw = ImageDraw.Draw(face)
        box = draw.textbbox((0, 0), letter, font=font)
        position = ((side - (box[2] - box[0])) / 2 - box[0], (side - (box[3] - box[1])) / 2 - box[1])
        draw.text(position, letter, font=font, fill=(33, 18, 8, 235))
        img.paste(face, (x0, y0), face)
    return img


def art() -> None:
    """The pictures of the menus, in ``textures/ui/``."""
    folder = ROOT / "textures" / "ui"
    crate_face().save(folder / "crate_face.png")
    clipboard().save(folder / "clipboard.png")
    title_art().save(folder / "title.png")


def main() -> None:
    """Write the pictures and the four menu scenes."""
    art()
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "level_select.owl").write_text(level_select())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())


if __name__ == "__main__":
    main()
