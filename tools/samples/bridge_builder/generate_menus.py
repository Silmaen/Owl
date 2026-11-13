#!/usr/bin/env python3
"""
Write the menu scenes of the Bridge builder sample game into ``samples/bridge_builder/scenes/`` (title, level select,
settings, victory) and their graphs into ``scripts/ui/``, through ``tools.samples.menu_kit``: a workshop look (cream
and rust on dark wood, riveted frames, wipes in rust) over the valley of the levels, a finished truss bridge across
it and a car on the deck; ``scripts/ui/backdrop.owlvs`` sways the view.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.bridge_builder.generate_menus``.
"""

from __future__ import annotations

import math
from dataclasses import replace
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.bridge_builder import generate_level as lv
from tools.samples.vs_graph import Graph

ROOT = Path(__file__).resolve().parents[3] / "samples" / "bridge_builder"
"""The sample project."""

SPAN = 12
"""Width of the gorge behind the menus."""


def _stretch(entities: y.Entities, tag: str, model: str, a: tuple[float, float], b: tuple[float, float]) -> None:
    """A part of the bridge of the backdrop, a model only."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    entities.add(
        tag,
        [lv.mesh(model)],
        position=((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 0),
        rotation=(0, 0, math.atan2(dy, dx)),
        scale=(math.hypot(dx, dy), 1, 1),
    )


def backdrop(entities: y.Entities, name: str) -> None:
    """The valley, a gorge spanned by a truss bridge, a car on it; the camera on a pivot that sways."""
    lv.valley(entities, SPAN, 7)
    for tag, x0, x1 in (("CliffLeft", -40.0, 0.0), ("CliffRight", float(SPAN), SPAN + 40.0)):
        entities.add(tag, [lv.mesh("models/cliff.glb")], position=((x0 + x1) / 2, -10, 0), scale=(x1 - x0, 20, 30))
    for x in range(0, SPAN, 3):
        _stretch(entities, "Deck", "models/road.glb", (x, 0), (x + 3, 0))
    for x in range(0, SPAN, 3):
        _stretch(entities, "Truss", "models/steel.glb", (x, 0), (x + 1.5, -2))
        _stretch(entities, "Truss", "models/steel.glb", (x + 1.5, -2), (x + 3, 0))
        if x + 4.5 <= SPAN:
            _stretch(entities, "Chord", "models/wood.glb", (x + 1.5, -2), (x + 4.5, -2))
    for x in range(0, SPAN + 1, 3):
        entities.add("Bolt", [lv.mesh("models/node.glb")], position=(x, 0, 0))
    entities.add("Car", [lv.mesh("models/car.glb")], position=(4.5, 0.62, 0))
    script = y.script("scripts/ui/backdrop.owlvs", [("amplitude", "float", 0.12 if name == "title" else 0.06)])
    pivot = entities.add("ViewPivot", [script], position=(SPAN / 2, -1, 0))
    entities.add("MenuCamera", [lv.camera()], position=(0, 1.5, 24), rotation=(-0.06, 0, 0), parent=pivot)


def backdrop_graph() -> str:
    """Sway the entity around Y: ``amplitude`` radians, a full swing every ``period`` seconds."""
    g = Graph(6540000000000000000)
    g.var("amplitude", "float", 0.1)
    g.var("period", "float", 24.0, low=0.1)
    g.var("clock", "float", 0.0, exposed=False)
    update = g.event("on_update")
    angle = g.op(
        "math.multiply",
        g.get("amplitude"),
        g.node("math.sin", A=g.op("math.multiply", g.get("clock"), g.op("math.divide", 6.2832, g.get("period"))))[
            "Result"
        ],
    )
    g.chain(
        update,
        g.set("clock", g.op("math.add", g.get("clock"), update["dt"])),
        g.call("transform.set_rotation", rotation=g.node("math.make_vector3", X=0, Y=angle, Z=0)["Result"]),
    )
    return g.text()


RULES = (
    "Lay roads, wooden and steel beams from node to node across the gap, within the budget. Every vehicle must drive "
    "from the left cliff to the right one. Press TEST RUN: the bridge becomes real, its parts turn red as their "
    "joints take the load, and a joint pulled too hard snaps. Roads carry the vehicles; beams brace them, triangles "
    "hold best. A bridge that holds earns a star, two or three when it costs little. Edit it and try again as often "
    "as you like: the design is kept."
)
"""The rules, in the How to play window."""


def main() -> None:
    """Write the menu scenes, the level select, their graphs and the backdrop graph."""
    look = replace(lv.LOOK, header=lv.GENERIC_HEADER, backdrop=backdrop)
    mk.write_menus(ROOT, look, RULES, lv.CONTROLS, "THE VALLEY IS CROSSED")
    (ROOT / "scenes" / "level_select.owl").write_text(lv.level_select_scene(backdrop))
    (ROOT / "scripts" / "ui" / "backdrop.owlvs").write_text(backdrop_graph())


if __name__ == "__main__":
    main()
