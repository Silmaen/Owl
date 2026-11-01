#!/usr/bin/env python3
"""
Write the menu scenes of the Rail shooter sample game into ``samples/rail_shooter/scenes/``: the title, the settings,
the victory and the game over screens, in the cockpit look of the game.

Every menu is a live 3D scene behind its canvas: the starfield, the planet under a floating origin, rocks of the
ring streaming past (``scripts/ui/backdrop.lua`` flies the camera on and brings the rocks passed back ahead); the
title and the victory add the player's ship cruising with its engine trails. The buttons are dark blue with the
cut-corner cyan frame (``textures/ui/button_frame.png``) and an amber focus frame, in the italic bold font of the
engine; the counters are in the monospace one. The canvas is 1280 x 720 reference pixels, every size and offset in
screen-height units so that no aspect ratio nor interface size breaks the layout. The scripts live in
``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.rail_shooter.generate_menus``.
"""

from __future__ import annotations

import random
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.rail_shooter import generate_level as lv

ROOT = Path(__file__).resolve().parents[3] / "samples" / "rail_shooter"
"""The sample project."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
SOFT = (0.78, 0.86, 0.95, 0.95)
"""Colour of the secondary texts."""
RED = (1, 0.4, 0.4, 1)
"""Colour of the game over title."""
HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: planet_world, Enabled: true}\n  - {Name: particles, Enabled: true}\n"
    "  - {Name: ui, Enabled: false}\nFloatingOrigin:\n  enabled: true\n  shiftRadius: 1000\n  origin: [0, 0, 0]\n"
)
"""Root keys of every menu scene: the 3D layers on, the 2D ones off, a floating origin."""


def _backdrop(entities: y.Entities, ship: bool, speed: float, seed: int) -> None:
    """The camera, the sky, the sun, the planet and the rocks streaming past; the ship when told."""
    rng = random.Random(seed)
    entities.add("MenuCamera", [lv.perspective(0.9, True, True)], position=(0, 0, 0), rotation=(-0.12, 0, 0))
    entities.add("Sun", [lv.SUN], rotation=lv.SUN_ROTATION)
    entities.add("PlanetShine", [lv.SHINE], rotation=lv.SHINE_ROTATION)
    entities.add("Sky", [lv.sky()])
    entities.add("Planet", [lv.world_position(lv.PLANET_CENTRE), lv.planet()], position=lv.PLANET_CENTRE)
    properties: list[tuple[str, str, object]] = [("speed", "float", speed)]
    entities.add("Backdrop", [y.script("scripts/ui/backdrop.lua", properties)])
    models = ["models/asteroid_a.glb", "models/asteroid_b.glb", "models/asteroid_c.glb"]
    for _ in range(46):
        side = rng.choice([-1.0, 1.0])
        position = (side * rng.uniform(4.0, 40.0), rng.uniform(-14.0, 16.0), -rng.uniform(10.0, 300.0))
        size = rng.uniform(0.8, 4.5)
        grey = rng.uniform(0.55, 0.95)
        tint = (grey, grey * 0.97, grey * 0.95, 1)
        rotation = (rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3))
        entities.add(
            "MenuRock",
            [lv.mesh(rng.choice(models), tint)],
            position=position,
            scale=(size, size, size),
            rotation=rotation,
        )
    if ship:
        root = entities.add("MenuShip", [], position=(2.6, -1.4, -9.0))
        entities.add("MenuShipModel", [lv.mesh("models/player.glb")], parent=root)
        for side in (-1, 1):
            entities.add(
                "MenuShipTrail",
                [lv.trail((0.4, 0.8, 1, 0.9), 0.16, 0.18, 5000)],
                position=(side * 0.42, 0.05, 0.95),
                parent=root,
            )


def _label(value: str, size: int, colour: y.Colour = lv.CYAN, align: str = "Center", font: str = lv.FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.02, 0.06, 0.15, 1), outline_width=2)


def _menu_music(entities: y.Entities) -> None:
    entities.add("MenuMusic", [lv.sound("sounds/music/menu.wav", "Music", 0.55)])


def _column(
    entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float, width: float = 0.9
) -> None:
    """Buttons from top to bottom (a layout lays its children out from the last one added), the first focused."""
    for position, (tag, label, callback) in reversed(list(enumerate(rows))):
        lv.hud_button(entities, parent, tag, label, callback, (width, height), position == 0)


HOW_TO_PLAY = (
    "Fly the lane through the ring of Kael: your ship rides the rail, you steer it inside the frame and aim with "
    "the reticles. Shoot the rocks in your way and the squadrons that come at you; a barrel roll turns their shots "
    "away. The hull recharges when nothing hits it for a while. Destroy the Warden at the end of the lane. Score, "
    "hit ratio and the hull left give a bronze, silver or gold medal."
)
"""The rules, in the How to play window."""


def title() -> str:
    """Title: the title art, the best score and its medal, four buttons and the How to play window."""
    entities = y.Entities(7800000000000000000)
    _backdrop(entities, True, 14.0, 1)
    _menu_music(entities)
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.lua")])
    entities.add(
        "TitleArt",
        [y.rect((0.5, 0), (1.2, 0.29), (0, 0.04), SCREEN, SCREEN), y.image("textures/ui/title.png")],
        parent=root,
    )
    best = entities.add(
        "BestRow", [y.rect((0.5, 0), (0.9, 0.07), (0, 0.34), SCREEN, SCREEN), y.layout("Row", 10)], parent=root
    )
    entities.add("BestText", [y.child_rect((0.82, 1)), _label("BEST 000000", 28, lv.AMBER, font=lv.MONO)], parent=best)
    medal = (
        "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/hud/medals.png\n  columns: 4\n  rows: 1\n  frame: 0\n"
    )
    entities.add("BestMedal", [y.child_rect((0.11, 1)), medal], parent=best)
    panel = entities.add(
        "ButtonPanel", [y.rect((0.5, 0), (0.52, 0.46), (0, 0.45), SCREEN, SCREEN), y.layout("Column", 8)], parent=root
    )
    _column(
        entities,
        panel,
        [
            ("PlayButton", "LAUNCH", "on_play_clicked"),
            ("HowToButton", "HOW TO PLAY", "on_how_to_clicked"),
            ("SettingsButton", "SETTINGS", "on_settings_clicked"),
            ("QuitButton", "QUIT", "on_quit_clicked"),
        ],
        0.2,
    )
    window = entities.add(
        "HowToWindow",
        [
            y.rect((0.5, 0.5), (1.24, 0.86)),
            y.window("HOW TO PLAY", (0.02, 0.05, 0.1, 0.97), (0.05, 0.2, 0.35, 1), lv.CYAN, 40, 28),
            y.layout("Column", 8, 12),
        ],
        parent=root,
        visible=False,
    )
    lv.hud_button(entities, window, "HowToBackButton", "BACK", "on_how_to_closed", (0.4, 0.12), False)
    controls = y.text(lv.CONTROLS, 22, "Left", lv.AMBER, wrap=True, font=lv.MONO, line_spacing=0.25)
    entities.add("HowToControls", [y.child_rect((0.94, 0.36)), controls], parent=window)
    rules = y.text(HOW_TO_PLAY, 22, "Left", SOFT, wrap=True, line_spacing=0.2)
    entities.add("HowToRules", [y.child_rect((0.94, 0.38)), rules], parent=window)
    return y.scene("title", entities, header=HEADER)


def settings() -> str:
    """Settings: master, music and effects volumes (the mixer buses), interface size, inverted steering, reset,
    Back (saves)."""
    entities = y.Entities(7810000000000000000)
    _backdrop(entities, False, 8.0, 2)
    _menu_music(entities)
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.lua")])
    column = entities.add(
        "SettingsList", [y.rect((0.5, 0), (0.9, 0.9), (0, 0.05), SCREEN, SCREEN), y.layout("Column", 5, 4)], parent=root
    )
    rows = [
        ("MasterVolLabel", "MasterVolSlider", "on_master_vol_changed", 1.0, "MASTER VOLUME 100%"),
        ("MusicVolLabel", "MusicVolSlider", "on_music_vol_changed", 0.8, "MUSIC 80%"),
        ("SfxVolLabel", "SfxVolSlider", "on_sfx_vol_changed", 1.0, "EFFECTS 100%"),
        ("UiScaleLabel", "UiScaleSlider", "on_ui_scale_changed", 0.2, "INTERFACE SIZE 100%"),
    ]
    lv.hud_button(entities, column, "BackButton", "BACK", "go_back", (0.6, 0.085), False)
    lv.hud_button(entities, column, "ResetButton", "RESET DEFAULTS", "on_reset_clicked", (0.6, 0.085), False)
    lv.hud_button(entities, column, "InvertButton", "INVERT STEERING: OFF", "on_invert_clicked", (0.6, 0.085), False)
    for index, (label, slider, callback, value, initial_text) in reversed(list(enumerate(rows))):
        focus = y.focus(initial=index == 0, callback="on_slider_focused", colour=lv.AMBER, thickness=3)
        bar = y.slider(callback, value, (0.05, 0.12, 0.2, 1), (0.4, 0.85, 1, 1), lv.AMBER)
        entities.add(slider, [y.child_rect((0.9, 0.03)), bar, focus], parent=column)
        entities.add(label, [y.child_rect((0.9, 0.065)), _label(initial_text, 24, SOFT, font=lv.MONO)], parent=column)
    entities.add("TitleText", [y.child_rect((0.9, 0.12)), _label("SETTINGS", 56, lv.CYAN)], parent=column)
    return y.scene("settings", entities, header=HEADER)


def _buttons(entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float) -> None:
    row = entities.add("ButtonRow", [y.child_rect((1, height)), y.layout("Row", 16)], parent=parent)
    for index, (tag, label, callback) in reversed(list(enumerate(rows))):
        lv.hud_button(entities, row, tag, label, callback, (0.46, 1), index == 0)


STAT_LINES = 6
"""Lines of the victory stats."""


def victory() -> str:
    """Victory: SECTOR CLEARED, the stats counted up line by line, the medal, the best score, Fly again and Title."""
    entities = y.Entities(7820000000000000000)
    _backdrop(entities, True, 18.0, 3)
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.lua")])
    group = entities.add(
        "ResultGroup", [y.rect((0.5, 0), (1.0, 0.92), (0, 0.04), SCREEN, SCREEN), y.layout("Column", 4)], parent=root
    )
    _buttons(
        entities,
        group,
        [("RetryButton", "FLY AGAIN", "on_retry_clicked"), ("MenuButton", "TITLE", "on_menu_clicked")],
        0.11,
    )
    entities.add("BestLabel", [y.child_rect((1, 0.065)), _label("", 26, lv.AMBER, font=lv.MONO)], parent=group)
    medal = (
        "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/hud/medals.png\n  columns: 4\n  rows: 1\n  frame: 0\n"
    )
    medal_row = entities.add("MedalRow", [y.child_rect((1, 0.16))], parent=group)
    entities.add("Medal", [y.child_rect((0.16, 1), (0.5, 0.5)), medal], parent=medal_row)
    for index in reversed(range(1, STAT_LINES + 1)):
        entities.add(
            f"StatLine{index}",
            [y.child_rect((0.8, 0.06)), _label("", 26, lv.CYAN, "Left", lv.MONO)],
            parent=group,
            visible=False,
        )
    entities.add("ResultTitle", [y.child_rect((1, 0.13)), _label("SECTOR CLEARED", 64, lv.CYAN)], parent=group)
    return y.scene("victory", entities, header=HEADER)


def game_over() -> str:
    """Game over: HULL BREACHED, the score and how far the lane was flown, the best score, Retry and Title."""
    entities = y.Entities(7830000000000000000)
    _backdrop(entities, False, 5.0, 4)
    _menu_music(entities)
    root = entities.add("GameOverCanvas", [y.canvas(), y.script("scripts/ui/game_over.lua")])
    group = entities.add(
        "ResultGroup", [y.rect((0.5, 0), (1.0, 0.62), (0, 0.2), SCREEN, SCREEN), y.layout("Column", 8)], parent=root
    )
    _buttons(
        entities,
        group,
        [("RetryButton", "RETRY", "on_retry_clicked"), ("MenuButton", "TITLE", "on_menu_clicked")],
        0.17,
    )
    entities.add("BestLabel", [y.child_rect((1, 0.1)), _label("", 26, lv.AMBER, font=lv.MONO)], parent=group)
    entities.add("DistanceLabel", [y.child_rect((1, 0.1)), _label("", 26, SOFT, font=lv.MONO)], parent=group)
    entities.add(
        "ScoreLabel", [y.child_rect((1, 0.12)), _label("SCORE 000000", 34, lv.CYAN, font=lv.MONO)], parent=group
    )
    entities.add("ResultTitle", [y.child_rect((1, 0.26)), _label("HULL BREACHED", 76, RED)], parent=group)
    return y.scene("game_over", entities, header=HEADER)


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
