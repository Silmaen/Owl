#!/usr/bin/env python3
"""
Write the menu scenes of the Asteroid field sample game into ``samples/asteroid_field/scenes/`` (title, settings,
victory, game over) and their graphs into ``scripts/ui/``, through ``tools.samples.menu_kit``: a fleet look (green and
violet on black, the bold italic font, wipes) over a field of rocks under the gas giant, the convoy and its escort
crossing it; ``scripts/ui/backdrop.owlvs`` turns the view slowly.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.asteroid_field.generate_menus``.
"""

from __future__ import annotations

import math
import random
from dataclasses import replace
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.asteroid_field import generate_level as lv
from tools.samples.rail_shooter import generate_level as rail
from tools.samples.vs_graph import Graph

ROOT = Path(__file__).resolve().parents[3] / "samples" / "asteroid_field"
"""The sample project."""

HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: planet_world, Enabled: true}\n  - {Name: particles, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
    "FloatingOrigin:\n  enabled: true\n  shiftRadius: 1000\n  origin: [0, 0, 0]\n"
)
"""Root keys of every menu scene: the 3D layers, a floating origin."""


def backdrop(entities: y.Entities, name: str) -> None:
    """Rocks around the view, the planet below, three freighters and two fighters crossing."""
    rng = random.Random(len(name))
    entities.add("Sun", [rail.SUN], rotation=rail.SUN_ROTATION)
    entities.add("PlanetShine", [rail.SHINE], rotation=rail.SHINE_ROTATION)
    entities.add("Sky", [rail.sky()])
    entities.add("Planet", [rail.world_position(rail.PLANET_CENTRE), rail.planet()], position=rail.PLANET_CENTRE)
    models = ["models/asteroid_a.glb", "models/asteroid_b.glb", "models/asteroid_c.glb"]
    for _ in range(60):
        angle = rng.uniform(0, 2 * math.pi)
        radius = rng.uniform(30, 160)
        point = (math.cos(angle) * radius, rng.uniform(-40, 40), math.sin(angle) * radius)
        size = rng.uniform(1.5, 9)
        rotation = (rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3))
        entities.add(
            "MenuRock",
            [rail.mesh(rng.choice(models), (0.75, 0.72, 0.68, 1))],
            position=point,
            scale=(size,) * 3,
            rotation=rotation,
        )
    for index in range(3):
        entities.add(
            "MenuFreighter",
            [rail.mesh("models/freighter.glb", glow=900)],
            position=(-12 + index * 4, -4 + index, -20 - index * 26),
            rotation=(0, 0.35, 0),
        )
    for side in (-1, 1):
        entities.add(
            "MenuFighter",
            [rail.mesh("models/player.glb")],
            position=(side * 9 - 4, 3, -12),
            rotation=(0, 0.35, side * 0.2),
        )
    script = y.script("scripts/ui/backdrop.owlvs", [("speed", "float", 0.02 if name == "title" else 0.012)])
    pivot = entities.add("ViewPivot", [script])
    entities.add(
        "MenuCamera", [rail.perspective(0.9, True, True)], position=(18, 4, 30), rotation=(-0.06, 0.5, 0), parent=pivot
    )


def backdrop_graph() -> str:
    """Turn the entity around Y at ``speed`` radians per second."""
    g = Graph(6640000000000000000)
    g.var("speed", "float", 0.02)
    update = g.event("on_update")
    rotation = g.node("math.break_vector3", Value=g.call("transform.get_rotation")["rotation"])
    turned = g.node(
        "math.make_vector3",
        X=rotation["X"],
        Y=g.op("math.add", rotation["Y"], g.op("math.multiply", g.get("speed"), update["dt"])),
        Z=rotation["Z"],
    )
    g.chain(update, g.call("transform.set_rotation", rotation=turned["Result"]))
    return g.text()


RULES = (
    "Three freighters fly through the asteroid field to the jump gate; you and two wingmen escort them. Waves of "
    "fighters come out of the field for the convoy or for you: shoot them down, lock the one ahead of you to aim at "
    "it, and order your wings to attack or to keep their slot. Rocks hurt: fly around them. The mission is won when "
    "the convoy jumps, lost when it is destroyed or when your fighter is. Each freighter saved, each kill and the "
    "hull left make the score."
)
"""The rules, in the How to play window."""


def main() -> None:
    """Write the four menu scenes, their graphs and the backdrop graph."""
    look = replace(lv.LOOK, header=HEADER, backdrop=backdrop)
    mk.write_menus(ROOT, look, RULES, lv.CONTROLS, "THE CONVOY JUMPED", "MISSION FAILED")
    (ROOT / "scripts" / "ui" / "backdrop.owlvs").write_text(backdrop_graph())


if __name__ == "__main__":
    main()
