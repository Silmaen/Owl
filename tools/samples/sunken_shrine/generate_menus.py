#!/usr/bin/env python3
"""
Write the menu scenes of the Sunken Shrine sample game into ``samples/sunken_shrine/scenes/`` (title, settings,
victory, game over) and their graphs into ``scripts/ui/``, through ``tools.samples.menu_kit``: a torchlit look (gold
on dark plum, fades) over the hall of the shrine, lit by its torches, the old man waiting in the middle. The title
has Continue: the save of the last room entered.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sunken_shrine.generate_menus``.
"""

from __future__ import annotations

from dataclasses import replace
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.sunken_shrine import generate_level as lv
from tools.samples.sunken_shrine import layout as lay

ROOT = Path(__file__).resolve().parents[3] / "samples" / "sunken_shrine"
"""The sample project."""

HEADER = "EnabledRenderers:\n  - {Name: world, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
"""Root keys of every menu scene."""


def backdrop(entities: y.Entities, name: str) -> None:
    """The hall of the shrine in the light of its torches, dimmer behind the settings."""
    entities.add("MenuCamera", [y.camera(11.0)], position=(0, -0.6, 5))
    entities.add("Background", [y.background((0.08, 0.05, 0.07, 1))], position=(0, 0, -1), scale=(40, 26, 1))
    entities.add("Room", ["Tilemap:\n  tilemapPath: tilemaps/room_1.owltilemap\n"])
    entities.add("Ambient", [lv.light((0.8, 0.75, 1.0), 0.35 if name == "title" else 0.22, 5, "Ambient")])
    for y_, row in enumerate(lay.ROOMS[0].rows):
        for x, c in enumerate(row):
            if c == "T":
                entities.add("Torch", [lv.light((1.0, 0.65, 0.3), 1.6, 5.0)], position=lay.cell_position(x, y_, 0.05))
            elif c == "O":
                entities.add("OldMan", [lv.frame("old_man")], position=lay.cell_position(x, y_, 0.05))
    entities.add("Hero", [lv.frame("hero")], position=lay.cell_position(7, 6, 0.05))


RULES = (
    "Beneath the hall of the shrine sleeps its guardian, behind three doors: a key opens the first, fire the second, "
    "stone the third. Talk to the old man, find the key in the pit, take the sword in the armoury, light the four "
    "braziers of the dark crypt, push the block onto its plate, and defeat the guardian to take the heart of the "
    "shrine. Creatures cost you a heart when they touch you; with none left you fall, and Retry takes you back to the "
    "door of the room. Each room you enter is saved: Continue on the title goes on from there."
)
"""The rules, in the How to play window."""


def main() -> None:
    """Write the menu scenes and their graphs."""
    look = replace(lv.LOOK, header=HEADER, backdrop=backdrop)
    mk.write_menus(ROOT, look, RULES, lv.CONTROLS, "THE SHRINE IS FREE", "YOU HAVE FALLEN")


if __name__ == "__main__":
    main()
