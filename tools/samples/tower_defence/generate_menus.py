#!/usr/bin/env python3
"""
Write the menu scenes of the Tower defence sample game into ``samples/tower_defence/scenes/``: the title, the
settings, the victory and the game over screens, in the medieval look of the game (``medieval_ui.py``).

Every menu stands over a strip of meadow at the bottom of the screen where a wave marches past an archer tower
(``scripts/ui/parade.lua``, the sprites of the game in the 2D world layer); the victory adds fireworks over the keep.
The title is gold capitals, the boards dark wood with a gold rim, the buttons wooden planks with iron bands
(``textures/ui/banner.png`` over the button colour) and a gold focus frame; titles in ``Cinzel``, labels in
``MedievalSharp`` (both in ``fonts/``). The canvas is 1280 x 720 reference pixels, sizes and offsets in screen
heights. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_menus``.
"""

from __future__ import annotations

import math
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence.medieval_ui import CAPITAL, SITES

ROOT = Path(__file__).resolve().parents[3] / "samples" / "tower_defence"
"""The sample project."""
NIGHT = (0.06, 0.08, 0.14, 1)
"""Colour behind everything: a night sky."""
STONE = (0.95, 0.88, 0.72, 1)
"""Colour of the titles and labels: parchment."""
SOFT = (0.88, 0.8, 0.64, 0.95)
"""Colour of the secondary texts."""
GOLD = (0.97, 0.77, 0.3, 1)
"""Headings and focus frame."""
INK = (0.16, 0.09, 0.04, 1)
"""Outline of the texts, and the text on parchment."""
BUTTON = (0.3, 0.18, 0.09)
"""Button colour under the plank frame: dark wood."""
BUTTON_HOVER = (0.74, 0.5, 0.2, 1)
"""Hovered or focused button."""
BUTTON_PRESSED = (0.16, 0.09, 0.04, 1)
"""Pressed button."""
WINDOW_BODY = (0.17, 0.1, 0.06, 0.97)
"""Body of the windows."""
WINDOW_TITLE = (0.38, 0.13, 0.08, 1)
"""Title bar of the windows."""
HEADING = "Cinzel"
"""Font of the titles."""
LABEL = "MedievalSharp"
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
    """A plank button in a layout: the wood colour shows through the plank, the label is parchment on ink."""
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
    """A dark wooden board with a gold rim."""
    return y.panel((0.17, 0.1, 0.06, 0.92), (0.84, 0.66, 0.25, 1), 3)


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
        [y.rect((0.5, 0), (1.1, 0.72), (0, 0.04), SCREEN, SCREEN), board(), y.layout("Column", 4, 10)],
        parent=root,
    )
    row = entities.add("ButtonRow", [y.child_rect((0.95, 0.13)), y.layout("Row", 16)], parent=group)
    buttons = [("AgainButton", "Play again", "on_again_clicked"), ("TitleButton", "World map", "on_title_clicked")]
    for index, (tag, text, callback) in reversed(list(enumerate(buttons))):
        button(entities, row, tag, text, callback, (0.44, 0.85), index == 0)
    for index in reversed(range(6)):
        line = label("", 26 if index < 5 else 24, SOFT if index < 5 else GOLD, "Left", font="JetBrainsMono-Regular")
        entities.add(f"StatLine{index + 1}", [y.child_rect((0.8, 0.075)), line], parent=group)
    colour = GOLD if fireworks else (0.95, 0.45, 0.35, 1)
    entities.add("ResultTitle", [y.child_rect((0.95, 0.18)), label(heading, 62, colour, font=HEADING)], parent=group)
    return y.scene(scene_name, entities)


def victory() -> str:
    """Victory: the keep holds, the stats of the map counted up, fireworks."""
    return _result("victory", 7420000000000000000, "The keep holds!", "scripts/ui/victory.lua", True, False)


def game_over() -> str:
    """Game over: the keep has fallen, the wave reached."""
    return _result("game_over", 7430000000000000000, "The keep has fallen", "scripts/ui/game_over.lua", False, True)


CAMPAIGN = (
    (mp.LEVELS[0], "The second power-up of each tower"),
    (mp.LEVELS[1], "Rain of arrows: a spell on the enemy nearest the keep"),
    (mp.LEVELS[2], "Royal treasury: 60 more gold at the start of every level"),
    (None, "Shield wall: a spell stopping the enemies around the front one"),
    (None, "Keep walls, with 3 stars on all five: 5 more lives"),
)
"""The levels of the world map with what winning them unlocks; ``None`` is a level still to come."""
PLANNED = ("Frostmere", "The Black Gate")
"""Names of the levels still to come."""
MAP_SIZE = (32.0, 18.0)
"""World size of the parchment map (the camera shows 18 units of height)."""


def map_point(share: tuple[float, float]) -> tuple[float, float]:
    """The world point of a place of the map given in shares of its width and height from the top left."""
    return (share[0] - 0.5) * MAP_SIZE[0], (0.5 - share[1]) * MAP_SIZE[1]


def _bend(a: tuple[float, float], b: tuple[float, float], index: int) -> list[tuple[float, float]]:
    """Dots along a gently bent road from one site to the next, about 0.65 units apart."""
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    nx, ny = -(b[1] - a[1]), b[0] - a[0]
    length = math.hypot(nx, ny) or 1.0
    bend = 1.2 if index % 2 == 0 else -1.2
    cx, cy = mx + nx / length * bend, my + ny / length * bend
    count = max(3, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.65))
    dots = []
    for k in range(1, count):
        s = k / count
        dots.append(
            (
                (1 - s) ** 2 * a[0] + 2 * (1 - s) * s * cx + s * s * b[0],
                (1 - s) ** 2 * a[1] + 2 * (1 - s) * s * cy + s * s * b[1],
            )
        )
    return dots


def world_map() -> str:
    """The world map: the parchment plan, the capital and the five sites with their stars, the road traced between
    them (``PathDot N`` dots, shown up to the last site open), the panel of the chosen site and the bonuses window."""
    entities = y.Entities(7440000000000000000)
    entities.add("MainCamera", [y.camera(18)], position=(0, 0, 5))
    entities.add("Background", [y.background(NIGHT)], position=(0, 0, -1), scale=(60, 30, 1))
    entities.add("Map", [y.sprite("textures/ui/world_map.png")], position=(0, 0, -0.5), scale=(*MAP_SIZE, 1))
    capital = map_point(CAPITAL)
    entities.add("Capital", [y.frames("textures/ui/sites.png", 4, 1, 3)], position=(*capital, 0), scale=(2.4, 2.4, 1))
    previous = capital
    for index, share in enumerate(SITES):
        point = map_point(share)
        for dot in _bend(previous, point, index):
            entities.add(
                f"PathDot {index + 1}",
                [y.sprite("textures/ui/path_dot.png")],
                position=(*dot, 0.05),
                scale=(0.32, 0.32, 1),
                visible=False,
            )
        previous = point
        ring = y.sprite("textures/fx/ring.png", (1, 0.82, 0.3, 0.9))
        entities.add("SiteRing", [ring], position=(point[0], point[1] - 0.7, 0.08), scale=(2.6, 1.3, 1), visible=False)
        entities.add("Site", [y.frames("textures/ui/sites.png", 4, 1, 0)], position=(*point, 0.1), scale=(2.4, 2.4, 1))
        for star in range(3):
            entities.add(
                "SiteStar",
                [y.frames("textures/ui/stars.png", 2, 1, 0)],
                position=(point[0] + (star - 1) * 0.62, point[1] - 1.55, 0.12),
                scale=(0.6, 0.6, 1),
            )
    levels, scenes = [], []
    for index, (level, unlock) in enumerate(CAMPAIGN):
        if level is None:
            levels.append(f"{PLANNED[index - 3]}|0|{unlock}")
            scenes.append("")
        else:
            levels.append(f"{level.name}|{len(level.waves)}|{unlock}")
            scenes.append(f"scenes/level_{level.number:02d}.owl")
    script = (
        "LuaScript:\n  scriptPath: scripts/ui/world_map.lua\n  properties:\n"
        "    - name: levels\n      type: strings\n      value:"
        + "".join(f"\n        - {y.quote(entry)}" for entry in levels)
        + "\n    - name: level_scenes\n      type: strings\n      value:"
        + "".join(f"\n        - {y.quote(entry)}" for entry in scenes)
        + "\n"
    )
    root = entities.add("MapCanvas", [y.canvas(), script])
    entities.add(
        "MapTitle",
        [y.rect((0.5, 0), (1.0, 0.09), (0, 0.015), SCREEN, SCREEN), label("The Realm", 46, GOLD, font=HEADING)],
        parent=root,
    )
    panel = entities.add(
        "SitePanel",
        [y.rect((0.5, 1), (1.25, 0.2), (0, -0.02), SCREEN, SCREEN), board(), y.layout("Row", 10, 8)],
        parent=root,
    )
    buttons = entities.add("SiteButtons", [y.child_rect((0.3, 0.84)), y.layout("Column", 4)], parent=panel)
    button(entities, buttons, "BackButton", "Back", "on_back_clicked", (1, 0.28), False, font_size=24)
    button(entities, buttons, "BonusButton", "Bonuses", "on_bonus_clicked", (1, 0.28), False, font_size=24)
    button(entities, buttons, "MarchButton", "March!", "on_march_clicked", (1, 0.28), True, font_size=26)
    texts = entities.add("SiteTexts", [y.child_rect((0.64, 0.84)), y.layout("Column", 2, 0, "Start")], parent=panel)
    entities.add("SiteUnlock", [y.child_rect((1, 0.28)), label("", 20, SOFT, "Left")], parent=texts)
    entities.add("SiteInfo", [y.child_rect((1, 0.28)), label("", 22, STONE, "Left")], parent=texts)
    entities.add("SiteName", [y.child_rect((1, 0.34)), label("", 32, GOLD, "Left", font=HEADING)], parent=texts)
    bonus = entities.add(
        "BonusWindow",
        [y.rect((0.5, 0.5), (1.1, 0.7)), window("Bonuses"), y.layout("Column", 6, 12)],
        parent=root,
        visible=False,
    )
    button(entities, bonus, "BonusBackButton", "Back", "on_bonus_closed", (0.4, 0.13), False)
    for index in reversed(range(len(CAMPAIGN))):
        entities.add(f"BonusLine{index + 1}", [y.child_rect((0.95, 0.12)), label("", 22, SOFT, "Left")], parent=bonus)
    return y.scene("world_map", entities)


def main() -> None:
    """Write the four menu scenes."""
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "world_map.owl").write_text(world_map())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())
    (scenes / "game_over.owl").write_text(game_over())


if __name__ == "__main__":
    main()
