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

from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence import scenery
from tools.samples.tower_defence import world_map_art as art
from tools.samples.tower_defence.creatures import CELL, DIRECTIONS, FOOT, FRAMES

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
PARADE = ("goblin", "wolf", "grunt", "sapper", "troll", "ogre", "ogre_mage", "grunt", "chieftain")
"""The Horde marching under the menus, from the left (``textures/enemies/<kind>.png``)."""
PARADE_COLUMN = 6
"""Column of a sheet facing the left of the screen."""
PARADE_SIZE = 2.6
"""World size of a parade sprite cell."""
PIXEL = PARADE_SIZE / CELL
"""World size of a pixel of the menu scenery (``scenery.py``), the pixel size of the parade."""
FOOT_SHARE = (FOOT[1] - CELL / 2) / CELL
"""Share of a cell between its centre and the foot of the creature."""

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


def world(entities: y.Entities, fireworks: bool = False, fallen: bool = False) -> None:
    """Camera, night sky, the meadow strip and its road, an archer tower and the parade (``parade.lua``); the victory
    adds the keep and fireworks, the game over a fallen keep (tinted dark) and no tower."""
    entities.add("MainCamera", [y.camera(15)], position=(0, 0, 5))
    entities.add("Background", [y.background(NIGHT)], position=(0, 0, -1), scale=(60, 30, 1))
    entities.add("Sky", [y.sprite("textures/menu/sky.png")], position=(0, 0, -0.9), scale=(*scenery.SKY_WORLD, 1))
    for share in scenery.TORCHES:
        x, y_ = (share[0] - 0.5) * scenery.SKY_WORLD[0], (0.5 - share[1]) * scenery.SKY_WORLD[1]
        flame = y.frames("textures/fx/fire.png", 4, 1, 0).replace("loop: false", "loop: true")
        flame = flame.replace("lastFrame: 0", "lastFrame: 3").replace("frameDuration: 0.1", "frameDuration: 0.09")
        entities.add("TitleTorch", [flame], position=(x, y_ + 0.25, -0.85), scale=(0.45, 0.6, 1))
        glow = y.sprite("textures/puff.png", (1, 0.6, 0.25, 0.35))
        entities.add("TorchGlow", [glow], position=(x, y_ + 0.2, -0.86), scale=(2.2, 2.2, 1))
    ground = y.sprite("textures/menu/ground.png", tiling=(9, 1))
    entities.add("Meadow", [ground], position=(0, -6.5, -0.5), scale=(240 * PIXEL * 9, 112 * PIXEL, 1))
    properties: list[tuple[str, str, object]] = [("fireworks", "bool", fireworks), ("marching", "bool", not fallen)]
    parade = entities.add("Parade", [y.script("scripts/ui/parade.lua", properties)])
    for index, kind in enumerate(PARADE):
        sprite = y.frames(f"textures/enemies/{kind}.png", DIRECTIONS, FRAMES, PARADE_COLUMN)
        size = PARADE_SIZE * (1.35 if kind == "chieftain" else 1.0)
        x = -14 + index * 2.9
        position = (x, GROUND_Y + size * FOOT_SHARE, 0.1)
        entities.add("ParadeEnemy", [sprite], position=position, scale=(size, size, 1), parent=parade)
    if not fallen:
        tower = y.sprite("textures/menu/tower.png")
        position = (9.5, GROUND_Y + (136 - 75) * PIXEL, 0.05)
        entities.add("ParadeTower", [tower], position=position, scale=(96 * PIXEL, 150 * PIXEL, 1))
    keep = y.sprite("textures/menu/keep_fallen.png" if fallen else "textures/menu/keep.png")
    position = (-10.5, GROUND_Y + (160 - 100) * PIXEL, 0.05)
    entities.add("ParadeKeep", [keep], position=position, scale=(240 * PIXEL, 200 * PIXEL, 1))
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
    board_id = entities.add(
        "MapBoard",
        [y.rect((0.5, 0), (0.6, 0.46), (0.33, 0.29), SCREEN, SCREEN), y.image("textures/ui/panel_board.png")],
        parent=root,
    )
    log = entities.add(
        "MapBoardBody",
        [y.rect((0.5, 0.5), (0.8, 0.76), units=("ParentWidth", "ParentHeight")), y.layout("Column", 4, 2)],
        parent=board_id,
    )
    ink, red = (0.24, 0.13, 0.05, 1), (0.48, 0.1, 0.05, 1)

    def parchment_label(value: str, size: int, colour: y.Colour, font: str = LABEL) -> str:
        return y.text(value, size, "Center", colour, font=font, outline_width=0)

    entities.add("BestLabel", [y.child_rect((0.9, 0.14)), parchment_label("", 24, ink)], parent=log)
    state = parchment_label("Not defended yet", 26, ink)
    entities.add("StateLabel", [y.child_rect((0.9, 0.14)), state], parent=log)
    campaign = parchment_label("Level 1: Greenmarch Ford", 30, ink)
    entities.add("MapLabel", [y.child_rect((0.9, 0.18)), campaign], parent=log)
    title_text = parchment_label("THE REALM", 36, red, HEADING)
    entities.add("BoardTitle", [y.child_rect((0.9, 0.2)), title_text], parent=log)
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
        [y.rect((0.5, 0), (0.95, 0.84), (0, 0.02), SCREEN, SCREEN), board(), y.layout("Column", 4, 8)],
        parent=root,
    )

    def slider(callback: str, value: float, initial: bool) -> list[str]:
        bar = y.slider(callback, value, (0.04, 0.05, 0.1, 1), GOLD, STONE)
        return [y.child_rect((0.86, 0.05)), bar, y.focus(initial, True, "on_slider_focused", GOLD, 3)]

    rows: list[tuple[str, list[str]]] = [
        ("TitleText", [y.child_rect((0.9, 0.11)), label("Settings", 56, GOLD, font=HEADING)]),
        ("MasterVolLabel", [y.child_rect((0.9, 0.07)), label("Master Volume: 100%", 28, SOFT)]),
        ("MasterVolSlider", slider("on_master_vol_changed", 1.0, True)),
        ("UiScaleLabel", [y.child_rect((0.9, 0.07)), label("Interface Size: 100%", 28, SOFT)]),
        ("UiScaleSlider", slider("on_ui_scale_changed", 0.2, False)),
        ("ProgressLabel", [y.child_rect((0.9, 0.07)), label("Maps defended: 0 / 10", 28)]),
    ]
    button(entities, panel, "BackButton", "Back", "go_back", (0.6, 0.08), False)
    button(entities, panel, "ResetButton", "Reset defaults", "on_reset_clicked", (0.6, 0.08), False)
    button(entities, panel, "ResetTutorialButton", "Reset tutorial", "on_reset_tutorial_clicked", (0.6, 0.08), False)
    button(entities, panel, "ResetProgressButton", "Reset progress", "on_reset_progress_clicked", (0.6, 0.08), False)
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


UNLOCKS = (
    "The second power-up of each tower",
    "Rain of arrows: a spell on the enemy nearest the keep",
    "Royal treasury: 60 more gold at the start of every level",
    "Fletchers' guild: archers cost 10% less",
    "Shield wall: a spell stopping the enemies around the front one",
    "Watchtowers: every tower sees 5% further",
    "Harvest tithe: 10% more gold per kill",
    "Night watch: towers see 10% further at night",
    "Royal guard: 3 more lives on every level",
    "Keep walls, with 3 stars on all ten: 5 more lives",
)
"""What winning each level unlocks, in the order of the campaign (``defence.lua`` applies them)."""
CAMPAIGN = tuple(zip(mp.LEVELS, UNLOCKS, strict=True))
"""The levels of the world map with what winning them unlocks."""
MAP_SIZE = art.WORLD
"""World size of the parchment map (the camera shows 18 units of height)."""
MODIFIER_NAMES = {
    "fog": "fog",
    "blizzard": "blizzard",
    "blood_moon": "blood moon",
    "drought": "drought",
    "gale": "gale",
    "plunder": "plunder",
    "long_night": "long night",
}
"""Names of the level modifiers on the world map (their rules are in ``defence.lua``)."""
FOE_NAMES = {
    "goblin": "goblins",
    "grunt": "orc grunts",
    "ogre": "ogres",
    "wolf": "wolf riders",
    "troll": "trolls",
    "ogre_mage": "ogre mages",
    "sapper": "goblin sappers",
    "berserker": "orc berserkers",
    "chieftain": "the orc chieftain",
}
"""Names of the kinds of the Horde on the world map (the enemies a level brings first)."""
FLAG_OFFSET = (1.35, 0.525)
"""World offset of the banner of a site from its centre: its cloth starts at the top of the pole of the site."""
CLOUDS = ((0.2, 0.3), (0.55, 0.15), (0.8, 0.62), (0.38, 0.72))
"""Starting places of the clouds drifting over the map."""


def map_point(share: tuple[float, float]) -> tuple[float, float]:
    """The world point of a place of the map given in shares of its width and height from the top left."""
    return (share[0] - 0.5) * MAP_SIZE[0], (0.5 - share[1]) * MAP_SIZE[1]


def world_map() -> str:
    """The world map: under ``MapRoot`` (scaled by the script to fit the screen) the inked map, the capital and the five
    sites with their banners and stars, the road traced between them (``PathDot N`` dashes, shown up to the last site
    open), the clouds and the glints of the sea; the panel of the chosen site and the bonuses window."""
    entities = y.Entities(7440000000000000000)
    entities.add("MainCamera", [y.camera(18)], position=(0, 0, 5))
    entities.add("Background", [y.background((0.1, 0.07, 0.04, 1))], position=(0, 0, -1), scale=(80, 40, 1))
    root_map = entities.add("MapRoot", [])
    entities.add(
        "Map", [y.sprite("textures/ui/world_map.png")], position=(0, 0, -0.5), scale=(*MAP_SIZE, 1), parent=root_map
    )
    for share in art.SEA_GLINTS:
        glint = y.sprite("textures/ui/wave.png", (1, 1, 1, 0))
        entities.add("SeaGlint", [glint], position=(*map_point(share), -0.45), scale=(0.8, 0.3, 1), parent=root_map)
    capital = map_point(art.CAPITAL)
    entities.add(
        "Capital",
        [y.frames("textures/ui/sites.png", 4, 1, 3)],
        position=(*capital, 0),
        scale=(2.4, 2.4, 1),
        parent=root_map,
    )
    places = (art.CAPITAL, *art.SITES)
    for index, share in enumerate(art.SITES):
        point = map_point(share)
        for dash, heading in art.road_points(places[index], share, index):
            entities.add(
                f"PathDot {index + 1}",
                [y.sprite("textures/ui/path_dash.png")],
                position=(*map_point(dash), 0.05),
                rotation=(0, 0, heading),
                scale=(0.4, 0.15, 1),
                visible=False,
                parent=root_map,
            )
        ring = y.sprite("textures/fx/ring.png", (1, 0.82, 0.3, 0.9))
        entities.add(
            "SiteRing",
            [ring],
            position=(point[0], point[1] - 0.8, 0.08),
            scale=(2.8, 1.3, 1),
            visible=False,
            parent=root_map,
        )
        entities.add(
            "Site",
            [y.frames("textures/ui/sites.png", 4, 1, 0)],
            position=(*point, 0.1),
            scale=(2.4, 2.4, 1),
            parent=root_map,
        )
        flag = (point[0] + FLAG_OFFSET[0], point[1] + FLAG_OFFSET[1], 0.11)
        entities.add(
            "SiteFlag",
            [y.frames("textures/ui/flags.png", 4, 2, 0)],
            position=flag,
            scale=(0.8, 0.8, 1),
            visible=False,
            parent=root_map,
        )
        for star in range(3):
            entities.add(
                "SiteStar",
                [y.frames("textures/ui/stars.png", 2, 1, 0)],
                position=(point[0] + (star - 1) * 0.62, point[1] - 1.55, 0.12),
                scale=(0.6, 0.6, 1),
                parent=root_map,
            )
    for share in CLOUDS:
        cloud = y.sprite("textures/ui/cloud.png", (1, 1, 1, 0.5))
        entities.add("Cloud", [cloud], position=(*map_point(share), 0.3), scale=(6, 2.75, 1), parent=root_map)
    levels, scenes = [], []
    seen: set[str] = set()
    for level, unlock in CAMPAIGN:
        kinds = [group.split()[0] for wave in level.waves for group in wave.split(";")]
        new = [FOE_NAMES[kind] for kind in dict.fromkeys(kinds) if kind not in seen]
        seen.update(kinds)
        twists = ", ".join(MODIFIER_NAMES[key] for key in level.modifiers)
        levels.append(f"{level.name}|{len(level.waves)}|{unlock}|{', '.join(new)}|{twists}")
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
    panel = entities.add(
        "SitePanel",
        [y.rect((0.5, 1), (1.3, 0.21), (0, -0.015), SCREEN, SCREEN), board(), y.layout("Row", 10, 8)],
        parent=root,
    )
    buttons = entities.add("SiteButtons", [y.child_rect((0.3, 0.88)), y.layout("Column", 2)], parent=panel)
    button(entities, buttons, "BackButton", "Back", "on_back_clicked", (1, 0.21), False, font_size=22)
    button(entities, buttons, "BonusButton", "Bonuses", "on_bonus_clicked", (1, 0.21), False, font_size=22)
    button(entities, buttons, "UpgradesButton", "Upgrades", "on_upgrades_clicked", (1, 0.21), False, font_size=22)
    button(entities, buttons, "MarchButton", "March!", "on_march_clicked", (1, 0.23), True, font_size=26)
    texts = entities.add("SiteTexts", [y.child_rect((0.64, 0.84)), y.layout("Column", 2, 0, "Start")], parent=panel)
    entities.add("SiteUnlock", [y.child_rect((1, 0.28)), label("", 20, SOFT, "Left")], parent=texts)
    entities.add("SiteInfo", [y.child_rect((1, 0.28)), label("", 22, STONE, "Left")], parent=texts)
    entities.add("SiteName", [y.child_rect((1, 0.34)), label("", 32, GOLD, "Left", font=HEADING)], parent=texts)
    bonus = entities.add(
        "BonusWindow",
        [y.rect((0.5, 0.5), (1.2, 0.88)), window("Bonuses"), y.layout("Column", 3, 8)],
        parent=root,
        visible=False,
    )
    button(entities, bonus, "BonusBackButton", "Back", "on_bonus_closed", (0.4, 0.09), False)
    for index in reversed(range(len(CAMPAIGN))):
        entities.add(f"BonusLine{index + 1}", [y.child_rect((0.95, 0.072)), label("", 18, SOFT, "Left")], parent=bonus)
    return y.scene("world_map", entities)


UPGRADE_ROWS = 6
"""Families of upgrades on the upgrades screen (``scripts/ui/upgrades.lua`` names them)."""


def upgrades() -> str:
    """Upgrades: the stars won to spend, a row per family (name, the next tier, three pips, Buy), Refund all, Back."""
    entities = y.Entities(7450000000000000000)
    world(entities)
    root = entities.add("UpgradesCanvas", [y.canvas(), y.script("scripts/ui/upgrades.lua")])
    board_id = entities.add(
        "UpgradesBoard",
        [y.rect((0.5, 0.5), (1.3, 0.96), (0, 0), SCREEN, SCREEN), board(), y.layout("Column", 2, 8)],
        parent=root,
    )
    row = entities.add("UpgradeButtons", [y.child_rect((0.62, 0.085)), y.layout("Row", 12)], parent=board_id)
    button(entities, row, "RefundButton", "Refund all", "on_refund_clicked", (0.45, 0.95), False, font_size=24)
    button(entities, row, "BackButton", "World map", "on_back_clicked", (0.45, 0.95), False, font_size=24)
    for index in reversed(range(1, UPGRADE_ROWS + 1)):
        line = entities.add(f"UpgradeRow{index}", [y.child_rect((0.96, 0.095)), y.layout("Row", 5)], parent=board_id)
        button(entities, line, f"UpgradeBuy{index}", "Buy", f"on_buy_{index}", (0.15, 0.85), index == 1, font_size=20)
        entities.add(f"UpgradeCost{index}", [y.child_rect((0.09, 0.9)), label("", 20, GOLD)], parent=line)
        for _ in range(4):
            star = y.image("textures/ui/stars.png") + "  columns: 2\n  rows: 1\n  frame: 0\n"
            entities.add(f"UpgradePip{index}", [y.child_rect((0.035, 0.6)), star], parent=line)
        entities.add(f"UpgradeText{index}", [y.child_rect((0.32, 0.9)), label("", 18, SOFT, "Left")], parent=line)
        entities.add(f"UpgradeName{index}", [y.child_rect((0.2, 0.9)), label("", 24, STONE, "Left")], parent=line)
    entities.add("StarsText", [y.child_rect((0.9, 0.06)), label("", 26, GOLD)], parent=board_id)
    entities.add(
        "UpgradesTitle", [y.child_rect((0.9, 0.1)), label("Upgrades", 52, GOLD, font=HEADING)], parent=board_id
    )
    return y.scene("upgrades", entities)


def main() -> None:
    """Write the menu scenes."""
    scenes = ROOT / "scenes"
    scenes.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title())
    (scenes / "world_map.owl").write_text(world_map())
    (scenes / "upgrades.owl").write_text(upgrades())
    (scenes / "settings.owl").write_text(settings())
    (scenes / "victory.owl").write_text(victory())
    (scenes / "game_over.owl").write_text(game_over())


if __name__ == "__main__":
    main()
