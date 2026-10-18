#!/usr/bin/env python3
"""
Write the menu scenes of the Sokoban sample game into ``samples/sokoban/scenes/``: the title, the level select, the
settings and the victory screen. Their layout follows the Engine Demo menus (``scenes/main_menu.owl``,
``scenes/settings_menu.owl``, ``scenes/victory.owl``): a canvas of 1280 x 720 reference pixels, buttons in a column with
the gold focus frame, so that keyboard, mouse and gamepad work the same way. Their scripts live in ``scripts/ui/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sokoban.generate_menus``.
"""

from __future__ import annotations

from pathlib import Path

from tools.samples import scene_yaml as y

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sokoban"
"""The sample project."""
GOLD = (1, 0.82, 0.3, 1)
"""Colour of the titles."""
SOFT = (0.85, 0.8, 0.75, 0.9)
"""Colour of the secondary texts."""


def _backdrop(entities: y.Entities, colour: tuple[float, ...]) -> None:
    entities.add("MainCamera", [y.camera(10)], position=(0, 0, 5))
    entities.add("Background", [y.background(colour)], position=(0, 0, -1), scale=(40, 30, 1))


def _buttons(
    entities: y.Entities, parent: int, rows: list[tuple[str, str, str, tuple[float, float, float]]], height: float
) -> dict[str, int]:
    """Buttons in a layout, the first one with the initial focus; ``rows`` from top to bottom."""
    ids: dict[str, int] = {}
    for position, (tag, label, callback, colour) in reversed(list(enumerate(rows))):
        components = [
            y.child_rect((0.94, height)),
            y.button(callback, colour),
            y.text(label, 34),
            y.focus(initial=position == 0),
        ]
        ids[tag] = entities.add(tag, components, parent=parent)
    return ids


def title() -> str:
    """Title screen: the name, Play (level select), Continue (first level not done, disabled when there is none),
    Settings, Quit."""
    entities = y.Entities(7300000000000000000)
    _backdrop(entities, (0.09, 0.06, 0.05, 1))
    root = entities.add("MenuCanvas", [y.canvas(), y.script("scripts/ui/title.lua")])
    progress = y.text("Levels done: 0 / 10", 28, "Right", GOLD)
    progress_rect = y.rect((1, 0), (0.18, 0.06), (-16, 16), ("ScreenWidth", "ScreenHeight"))
    entities.add("ProgressLabel", [progress_rect, progress], parent=root)
    hint = "Move: arrows, WASD, stick or D-pad   Undo: Z or pad B   Restart: R or pad Y   Menu: Esc or Start"
    hint_rect = y.rect((0.5, 1), (0.9, 0.05), (0, -16), ("ScreenWidth", "ScreenHeight"))
    entities.add("ControlsLabel", [hint_rect, y.text(hint, 22, colour=SOFT)], parent=root)
    panel_rect = y.rect((0.5, 0), (0.5, 0.4), (0, 0.42), offset_units=("Pixels", "ScreenHeight"))
    panel = entities.add("ButtonPanel", [panel_rect, y.panel(), y.layout("Column", 3, 4)], parent=root)
    _buttons(
        entities,
        panel,
        [
            ("PlayButton", "Play", "on_play_clicked", (0.15, 0.35, 0.65)),
            ("ContinueButton", "Continue", "on_continue_clicked", (0.2, 0.45, 0.25)),
            ("SettingsButton", "Settings", "on_settings_clicked", (0.3, 0.3, 0.3)),
            ("QuitButton", "Quit", "on_quit_clicked", (0.4, 0.15, 0.15)),
        ],
        0.225,
    )
    header_rect = y.rect((0.5, 0), (0.56, 0.3), (0, 40), ("ScreenWidth", "ScreenHeight"))
    header = entities.add("MenuHeader", [header_rect, y.layout("Column", 0)], parent=root)
    subtitle = y.text("An Owl Engine sample game", 26, colour=SOFT)
    entities.add("SubtitleText", [y.child_rect((1, 0.14)), subtitle], parent=header)
    entities.add("TitleText", [y.child_rect((1, 0.45)), y.text("SOKOBAN", 96, colour=GOLD)], parent=header)
    return y.scene("title", entities)


def level_select() -> str:
    """Level select: the ten levels in a list (done marks and best moves from the settings), Back and Play."""
    entities = y.Entities(7310000000000000000)
    _backdrop(entities, (0.07, 0.06, 0.08, 1))
    root = entities.add("LevelSelectCanvas", [y.canvas(), y.script("scripts/ui/level_select.lua")])
    column = entities.add("SelectColumn", [y.rect((0.5, 0.5), (0.95, 0.94)), y.layout("Column", 10)], parent=root)
    row = entities.add("SelectButtons", [y.child_rect((1, 0.11)), y.layout("Row", 12)], parent=column)
    for tag, label, callback, colour in (
        ("PlayButton", "Play", "on_play_clicked", (0.15, 0.35, 0.65)),
        ("BackButton", "Back", "go_back", (0.4, 0.2, 0.2)),
    ):
        entities.add(
            tag, [y.child_rect((0.48, 1)), y.button(callback, colour), y.text(label, 32), y.focus()], parent=row
        )
    entities.add("LevelInfo", [y.child_rect((1, 0.08)), y.text("Choose a level", 26, colour=SOFT)], parent=column)
    entities.add(
        "LevelList",
        [
            y.child_rect((1, 0.62)),
            "UiList:\n  itemCount: 10\n  rowHeight: 40\n  rowSpacing: 2\n  selectedIndex: 0\n  scrollOffset: 0\n"
            "  wheelStep: 84\n  fontSize: 28\n  rowColor: [0.16, 0.13, 0.12, 0.9]\n"
            "  rowAltColor: [0.2, 0.16, 0.14, 0.9]\n  hoverColor: [0.35, 0.27, 0.18, 1]\n"
            "  selectedColor: [0.55, 0.38, 0.16, 1]\n  textColor: [1, 1, 1, 1]\n"
            "  onSelectCallback: on_level_selected\n  onBindCallback: on_level_bind\n",
            y.focus(initial=True),
        ],
        parent=column,
    )
    entities.add("TitleText", [y.child_rect((1, 0.1)), y.text("Choose a level", 56, colour=GOLD)], parent=column)
    return y.scene("level_select", entities)


def settings() -> str:
    """Settings: master volume, interface size, reset of the progress, Back (saves)."""
    entities = y.Entities(7320000000000000000)
    _backdrop(entities, (0.06, 0.06, 0.09, 1))
    root = entities.add("SettingsCanvas", [y.canvas(), y.script("scripts/ui/settings.lua")])
    column = entities.add("SettingsList", [y.rect((0.5, 0.5), (0.9, 0.9)), y.layout("Column", 6, 4)], parent=root)
    slider_focus = "on_slider_focused"
    rows: list[tuple[str, list[str]]] = [
        ("TitleText", [y.child_rect((0.9, 0.13)), y.text("Settings", 60, colour=GOLD)]),
        ("MasterVolLabel", [y.child_rect((0.9, 0.08)), y.text("Master Volume: 100%", 28)]),
        (
            "MasterVolSlider",
            [y.child_rect((0.9, 0.05)), _slider("on_master_vol_changed", 1.0), y.focus(True, True, slider_focus)],
        ),
        ("UiScaleLabel", [y.child_rect((0.9, 0.08)), y.text("Interface Size: 100%", 28)]),
        (
            "UiScaleSlider",
            [y.child_rect((0.9, 0.05)), _slider("on_ui_scale_changed", 0.2), y.focus(callback=slider_focus)],
        ),
        ("ProgressLabel", [y.child_rect((0.9, 0.08)), y.text("Levels done: 0 / 10", 28, colour=SOFT)]),
        ("ResetProgressButton", _menu_button("on_reset_progress_clicked", (0.45, 0.25, 0.1), "Reset progress")),
        ("ResetButton", _menu_button("on_reset_clicked", (0.45, 0.35, 0.1), "Reset defaults")),
        ("BackButton", _menu_button("go_back", (0.4, 0.2, 0.2), "Back")),
    ]
    for tag, components in reversed(rows):
        entities.add(tag, components, parent=column)
    return y.scene("settings", entities)


def _menu_button(callback: str, colour: tuple[float, float, float], label: str) -> list[str]:
    return [y.child_rect((0.6, 0.1)), y.button(callback, colour), y.text(label, 30), y.focus()]


def _slider(callback: str, value: float) -> str:
    return (
        f"UiSlider:\n  value: {y.num(value)}\n  minValue: 0\n  maxValue: 1\n  trackColor: [0.2, 0.2, 0.2, 1]\n"
        f"  fillColor: [0.95, 0.7, 0.3, 1]\n  handleColor: [1, 1, 1, 1]\n  onValueChangedCallback: {callback}\n"
    )


def victory() -> str:
    """Victory: every level of the game played through, the levels done and the sum of the best moves."""
    entities = y.Entities(7330000000000000000)
    _backdrop(entities, (0.05, 0.09, 0.05, 1))
    root = entities.add("VictoryCanvas", [y.canvas(), y.script("scripts/ui/victory.lua")])
    group = entities.add("ResultGroup", [y.rect((0.5, 0.5), (0.9, 0.7)), y.layout("Column", 8)], parent=root)
    row = entities.add("ButtonRow", [y.child_rect((1, 0.18)), y.layout("Row", 12)], parent=group)
    for tag, label, callback, colour, initial in (
        ("TitleButton", "Title", "on_title_clicked", (0.3, 0.3, 0.3), False),
        ("LevelsButton", "Level select", "on_levels_clicked", (0.15, 0.35, 0.65), True),
    ):
        entities.add(
            tag, [y.child_rect((0.48, 1)), y.button(callback, colour), y.text(label, 32), y.focus(initial)], parent=row
        )
    entities.add("ScoreLabel", [y.child_rect((1, 0.2)), y.text("Levels done: 10 / 10", 34, wrap=True)], parent=group)
    entities.add("VictoryTitle", [y.child_rect((1, 0.34)), y.text("Warehouse cleared!", 80, colour=GOLD)], parent=group)
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
