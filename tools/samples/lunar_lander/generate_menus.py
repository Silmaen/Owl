#!/usr/bin/env python3
"""
Write the menu scenes of the Lunar lander sample game into ``samples/lunar_lander/scenes/`` (title, settings,
victory, game over) and their graphs into ``scripts/ui/``, through ``tools.samples.menu_kit``: a mission control
look (silver and amber on black, the Inter and JetBrains Mono fonts, slow fades) over a live view of the moon turning
under the Earth, a lander parked on the first pad (``scripts/ui/backdrop.owlvs`` turns the view).

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.lunar_lander.generate_menus``.
"""

from __future__ import annotations

from dataclasses import replace
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.lunar_lander import generate_level as lv
from tools.samples.lunar_lander import layout as lay
from tools.samples.vs_graph import Graph

ROOT = Path(__file__).resolve().parents[3] / "samples" / "lunar_lander"
"""The sample project."""

HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: planet_world, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
)
"""Root keys of every menu scene: the 3D layers on, the 2D world off."""

SPEEDS = {"title": 0.05, "settings": 0.03, "victory": 0.04, "game_over": 0.02}
"""Turn rate of the view behind each menu, in radians per second."""


def backdrop(entities: y.Entities, name: str) -> None:
    """The moon and the Earth, the camera on a pivot turning around the moon, a lander parked on the first pad."""
    entities.add("Sun", [lv.SUN], rotation=lv.SUN_ROTATION)
    entities.add("Sky", [lv.sky()])
    entities.add(
        "Earth", [f"WorldPosition: {{position: {y.vec(*lv.EARTH_CENTRE)}}}\n", lv.earth()], position=lv.EARTH_CENTRE
    )
    entities.add("Moon", [lv.mesh("models/moon.glb")])
    position, rotation = lv.pad_pose(lay.PADS[0])
    pad = entities.add("Pad1", [lv.mesh("models/pad.glb", glow=3000)], position=position, rotation=rotation)
    parked = entities.add("ParkedLander", [lv.mesh("models/lander_hull.glb")], position=(0, 1.6, 0), parent=pad)
    for index in range(4):
        import math

        angle = math.pi / 4 + index * math.pi / 2
        entities.add(
            f"ParkedLeg{index + 1}",
            [lv.mesh("models/lander_leg.glb")],
            position=(1.05 * math.cos(angle), -0.75, 1.05 * math.sin(angle)),
            rotation=(0, -angle + math.pi, 0),
            parent=parked,
        )
    script = y.script("scripts/ui/backdrop.owlvs", [("speed", "float", SPEEDS.get(name, 0.03))])
    pivot = entities.add("ViewPivot", [script], rotation=(0.12, -0.25, 0.0))
    entities.add("MenuCamera", [lv.camera()], position=(30, 60, 260), rotation=(-0.14, 0.12, 0), parent=pivot)


def backdrop_graph() -> str:
    """Turn the entity around Y at ``speed`` radians per second."""
    g = Graph(6420000000000000000)
    g.var("speed", "float", 0.03)
    update = g.event("on_update")
    rotation = g.node("math.break_vector3", Value=g.call("transform.get_rotation", entity_id=None)["rotation"])
    turned = g.node(
        "math.make_vector3",
        X=rotation["X"],
        Y=g.op("math.add", rotation["Y"], g.op("math.multiply", g.get("speed"), update["dt"])),
        Z=rotation["Z"],
    )
    g.chain(update, g.call("transform.set_rotation", rotation=turned["Result"]))
    return g.text()


RULES = (
    "Bring the lander down on the lit pad, gently and upright. The main engine pushes along the lander, so tilt "
    "it to steer; the side jets slide it a little. The beads show where you are going and the red cross where you "
    "would hit the ground. Land too hard and a leg snaps; touch the ground with the hull, miss the pad or run dry "
    "in the air and the landing is lost. Five landings, from a hover to a descent from orbit: fuel left, precision "
    "and a soft touchdown make the score."
)
"""The rules, in the How to play window."""


def main() -> None:
    """Write the four menu scenes, their graphs and the backdrop graph."""
    look = replace(lv.LOOK, header=HEADER, backdrop=backdrop)
    mk.write_menus(ROOT, look, RULES, lv.CONTROLS, "MISSION COMPLETE", "MISSION LOST")
    (ROOT / "scripts" / "ui" / "backdrop.owlvs").write_text(backdrop_graph())


if __name__ == "__main__":
    main()
