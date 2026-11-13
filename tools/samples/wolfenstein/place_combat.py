#!/usr/bin/env python3
"""
Place the guards, the gear and the arsenal of the Wolfenstein combat into the floors of ``samples/wolfenstein/``, and
write the two prefabs the guards drop (deterministic).

The floors themselves come from ``generate_levels.py`` and stay editable in Owl Nest: this script only replaces the
entities it owns in ``scenes/level_N.owl`` (tags ``Guard``, ``Arsenal``, ``Status``, ``Ammo``, ``FirstAid``,
``MachineGun``, ``ChainGun``), so it can run again after an edit of the floor in Owl Nest. Every guard and pick-up is
checked to stand on a floor cell, a patrol to run along a row or a column of floor cells.

- A guard (``Guard``): the guard sheet (``textures/guard/guard.png``, 8 x 7 frames) on an ``AnimatedSpriteRenderer``,
  a dynamic body, ``scripts/game/guard.owlbt`` on a ``BehaviourTreeAgent`` and ``scripts/game/guard.owlvs``; its patrol
  cell as script properties (none: it stands guard where it faces).
- ``Arsenal``: the entity of ``scripts/game/arsenal.owlvs`` (weapons, health, death), its floor as a property.
- ``Status``: the entity of ``scripts/game/status.owlvs`` (the face, the weapon icon and the red of the hits and of
  low health on the status bar of the HUD).
- Gear: ``Ammo``, ``FirstAid``, ``MachineGun``, ``ChainGun`` pick-ups of ``scripts/game/pickup.owlvs``.
- ``prefabs/clip.owlprefab`` (the ammo clip a guard drops) and ``prefabs/guard_corpse.owlprefab`` (its body).

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.wolfenstein.place_combat``.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass

from tools.samples.wolfenstein import generate_levels as levels

ROOT = levels.ROOT
"""The sample project."""
OWNED = ("Guard", "Arsenal", "Status", "Ammo", "FirstAid", "MachineGun", "ChainGun")
"""Tags of the entities this script writes into a floor."""
EAST, WEST, NORTH, SOUTH = -math.pi / 2, math.pi / 2, 0.0, math.pi
"""Raycast yaws: forward is (-sin(yaw), cos(yaw)), north is up."""


@dataclass(frozen=True)
class Guard:
    """A guard: its cell, the way it faces, and the other end of its patrol (none: it stands)."""

    cell: tuple[int, int]
    yaw: float
    patrol: tuple[int, int] | None = None


FLOORS: dict[int, tuple[list[Guard], list[tuple[str, tuple[int, int]]]]] = {
    1: (
        [
            Guard((5, 25), NORTH),  # dozing in the start room, its back turned
            Guard((14, 24), EAST, (22, 24)),  # pillar hall
            Guard((18, 18), SOUTH),  # corridor north
            Guard((14, 9), EAST, (20, 9)),  # mess hall
            Guard((2, 6), EAST, (8, 6)),  # dining room
            Guard((25, 9), EAST, (30, 9)),  # storage
            Guard((27, 16), WEST),
        ],
        [("MachineGun", (2, 22)), ("Ammo", (20, 30)), ("FirstAid", (30, 16)), ("Ammo", (8, 1))],
    ),
    2: (
        [
            Guard((12, 25), EAST, (19, 25)),
            Guard((4, 25), EAST, (9, 25)),
            Guard((14, 15), EAST, (17, 15)),
            Guard((17, 19), WEST, (14, 19)),
            Guard((12, 7), SOUTH),
            Guard((17, 10), WEST),
            Guard((22, 25), EAST, (24, 25)),
            Guard((27, 20), SOUTH),
        ],
        [("Ammo", (6, 21)), ("FirstAid", (12, 29)), ("Ammo", (12, 4)), ("FirstAid", (26, 28))],
    ),
    3: (
        [
            Guard((11, 4), EAST, (21, 4)),
            Guard((15, 8), NORTH),
            Guard((12, 13), EAST, (19, 13)),
            Guard((16, 17), WEST),
            Guard((22, 13), EAST, (29, 13)),
            Guard((26, 2), SOUTH),
            Guard((3, 17), EAST, (8, 17)),
            Guard((15, 23), NORTH),
            Guard((14, 27), EAST, (17, 27)),
        ],
        [("ChainGun", (14, 2)), ("Ammo", (8, 26)), ("FirstAid", (24, 6)), ("Ammo", (27, 16)), ("FirstAid", (3, 22))],
    ),
}
"""Per floor: the guards, the gear on the floor."""

GEAR_SPRITES = {"Ammo": "ammo", "FirstAid": "health", "MachineGun": "machine_gun", "ChainGun": "machine_super_gun"}
"""The sprite of each kind of gear (``textures/wolf3d/sprites/``)."""
WALKABLE = set(".") | set(levels.ITEMS) | {p for p, (_, _, solid) in levels.PROPS.items() if not solid}
"""Cells a guard or a pick-up may stand on: the floor, a pick-up, a prop without a body."""


def check(number: int, grid: list[list[str]]) -> None:
    """Every guard, patrol and pick-up of a floor on floor cells."""

    def on_floor(cell: tuple[int, int], what: str) -> None:
        x, y = cell
        if grid[y][x] not in WALKABLE:
            raise SystemExit(f"floor {number}: {what} at {cell} stands on '{grid[y][x]}'")

    guards, gear = FLOORS[number]
    for guard in guards:
        on_floor(guard.cell, "a guard")
        if guard.patrol is not None:
            (x0, y0), (x1, y1) = guard.cell, guard.patrol
            if x0 != x1 and y0 != y1:
                raise SystemExit(f"floor {number}: the patrol from {guard.cell} is no straight line")
            for x in range(min(x0, x1), max(x0, x1) + 1):
                for y in range(min(y0, y1), max(y0, y1) + 1):
                    on_floor((x, y), "a patrol")
    for kind, cell in gear:
        on_floor(cell, kind)


def guard_entity(out: levels.SceneWriter, guard: Guard) -> None:
    """A guard of the floor."""
    x, y = levels.world(*guard.cell)
    properties: list[tuple[str, str, str]] = []
    if guard.patrol is not None:
        properties = [("patrol_c", "int", str(guard.patrol[0])), ("patrol_r", "int", str(guard.patrol[1]))]
    out.entity(
        "Guard",
        [
            "AnimatedSpriteRenderer:\n  color: [1, 1, 1, 1]\n  texture: nam:textures/guard/guard.png\n  columns: 8\n"
            "  rows: 7\n  firstFrame: 0\n  lastFrame: 0\n  frameDuration: 1\n  loop: false",
            "PhysicBody:\n  type: Dynamic\n  fixedRotation: true\n  colliderSize: [0.5, 0.5, 1]\n  density: 1\n"
            "  restitution: 0\n  friction: 0",
            "BehaviourTreeAgent:\n  enabled: true\n  tree: scripts/game/guard.owlbt",
            levels.visible(),
            levels.script("scripts/game/guard.owlvs", properties),
            levels.tag_for("raycast_world"),
        ],
        x=x,
        y=y,
        yaw=guard.yaw,
    )


def gear_entity(out: levels.SceneWriter, kind: str, cell: tuple[int, int]) -> None:
    """A pick-up of gear."""
    x, y = levels.world(*cell)
    out.entity(
        kind,
        [
            levels.sprite(GEAR_SPRITES[kind]),
            "Trigger:\n  Type: LuaCallback\n  CallbackName: on_pickup",
            levels.visible(),
            levels.script("scripts/game/pickup.owlvs", [("item", "string", kind)]),
            levels.tag_for("raycast_world"),
        ],
        x=x,
        y=y,
    )


def place(number: int) -> None:
    """Replace the combat entities of a floor scene."""
    level = levels.LEVELS[number - 1]()
    check(number, level.grid)
    path = ROOT / "scenes" / f"level_{number}.owl"
    text = path.read_text()
    head, _, body = text.partition("Entities:\n")
    blocks = re.split(r"(?m)^(?=  - Entity: )", body)
    kept = [b if b.endswith("\n") else b + "\n" for b in blocks if b]
    kept = [b for b in kept if not re.search(r"(?m)^    Tag:\n      tag: (" + "|".join(OWNED) + r")\n", b)]
    out = levels.SceneWriter(7000 + number)
    guards, gear = FLOORS[number]
    out.entity(
        "Arsenal",
        [levels.visible(), levels.script("scripts/game/arsenal.owlvs", [("floor_scene", "string",
                                                                          f"scenes/level_{number}.owl")])],
    )
    for guard in guards:
        guard_entity(out, guard)
    for kind, cell in gear:
        gear_entity(out, kind, cell)
    out.entity("Status", [levels.visible(), levels.script("scripts/game/status.owlvs")])
    path.write_text(head + "Entities:\n" + "".join(kept) + "\n".join(out.lines) + "\n")


def prefabs() -> None:
    """The clip and the corpse a guard leaves."""
    clip = (
        "Prefab: Clip\nVersion: 1\nEntities:\n  - Entity: 7100000000000000001\n    Tag:\n      tag: Clip\n"
        "    Transform:\n      translation: [0, 0, 0]\n      rotation: [0, 0, 0]\n      scale: [1, 1, 1]\n"
        "    Visibility:\n      gameVisible: true\n    Hierarchy: {}\n    SpriteRenderer:\n      color: [1, 1, 1, 1]\n"
        "      tilingFactor: 1\n      texture: nam:textures/wolf3d/sprites/ammo.png\n    Trigger:\n"
        "      Type: LuaCallback\n      CallbackName: on_pickup\n    LuaScript:\n"
        "      scriptPath: scripts/game/pickup.owlvs\n      properties:\n        - name: item\n"
        "          type: string\n          value: Clip\n    RendererTag:\n      Name: raycast_world\n"
    )
    corpse = (
        "Prefab: GuardCorpse\nVersion: 1\nEntities:\n  - Entity: 7100000000000000002\n    Tag:\n"
        "      tag: GuardCorpse\n    Transform:\n      translation: [0, 0, 0]\n      rotation: [0, 0, 0]\n"
        "      scale: [1, 1, 1]\n    Visibility:\n      gameVisible: true\n    Hierarchy: {}\n    SpriteRenderer:\n"
        "      color: [1, 1, 1, 1]\n      tilingFactor: 1\n      texture: nam:textures/guard/corpse.png\n"
        "    RendererTag:\n      Name: raycast_world\n"
    )
    (ROOT / "prefabs").mkdir(exist_ok=True)
    (ROOT / "prefabs" / "clip.owlprefab").write_text(clip)
    (ROOT / "prefabs" / "guard_corpse.owlprefab").write_text(corpse)


def main() -> None:
    """Place the combat of the three floors, write the prefabs."""
    for number in FLOORS:
        place(number)
    prefabs()


if __name__ == "__main__":
    main()
