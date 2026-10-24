#!/usr/bin/env python3
"""
Write the project files, the level scenes, the prefabs and the play HUD of the Tower defence sample game into
``samples/tower_defence/``, from the level data of ``maps.py``.

A level is a scene of 3D tiles of the Kenney Tower Defense Kit (``models/td/``): one ``Terrain`` entity per cell (and
a stretched slab under the raised ones), the road tiles turned after their neighbours, the river and its bridge, the
decor, the ``Pad`` entities the towers are built on, the ``Waypoint NN`` entities the enemies walk from the ``Gate``
to the ``Keep`` (Kenney Castle Kit, ``models/castle/``), the sun, the sky and its day-night cycle, the torches lit at
night, the minimap camera, the hidden sprites of the build ring and the ``Game`` entity running
``scripts/game/defence.lua`` with the waves of the level. Every one of them is edited in Owl Nest afterwards.

Render stack of a level: ``mesh_world`` (terrain, towers, keep), ``ground`` (flat sprites on the terrain, depth
tested: range ring, cursor, shadows), ``sprites`` (billboards facing the camera, depth tested: enemies, projectiles,
effects, health bars) and ``marks`` (billboards over everything: the badges of the build ring).

The prefabs are the enemies (8 directions x 4 walk frames), the towers (one prefab per level and power-up, a root
tagged ``Tower`` holding its meshes), the projectiles, the effects and the health bar the script spawns.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_maps``.
"""

from __future__ import annotations

import math
import random
import shutil
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.tower_defence import generate_menus as menus
from tools.samples.tower_defence import maps as mp
from tools.samples.tower_defence.creatures import CELL, CREATURES, DIRECTIONS, FRAMES, PPU

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "tower_defence"
"""The sample project."""
DEMO = REPOSITORY / "engine_demo"
"""The Engine Demo, source of the gamepad mappings."""
TD = "models/td"
"""Folder of the Tower Defense Kit models."""
CASTLE = "models/castle"
"""Folder of the Castle Kit models."""
VS = mp.HEIGHT_STEP / 0.2
"""Vertical scale of the tiles: a kit tile is 0.2 high, a height step is ``HEIGHT_STEP``."""
GRASS_TOP = 0.2 * VS
"""Height of the grass over the base of a tile."""
ROAD_TOP = 0.1 * VS
"""Height of the road over the base of a tile."""
BRIDGE_TOP = 0.2 * VS
"""Height of the bridge deck over the base of its tile."""
PAD_TOP = 0.21
"""Height of a pad (``tower-round-base``) over the grass."""
SPRITE_SCALE = 1.3
"""Enemies are drawn larger than their models, to read at the distance of the camera."""
SPRITE_SIZE = CELL / PPU * SPRITE_SCALE
"""World size of an enemy sprite cell."""
MESH = "RendererTag:\n  Name: mesh_world\n"
GROUND = "RendererTag:\n  Name: ground\n"
SPRITES = "RendererTag:\n  Name: sprites\n"
MARKS = "RendererTag:\n  Name: marks\n"
FLAT = (-math.pi / 2, 0.0, 0.0)
"""Rotation laying a sprite of the ``ground`` layer flat on the terrain, facing up."""

N, E, S, W = 1, 2, 4, 8
"""Sides of a cell: north (-z), east (+x), south (+z), west (-x)."""
TURN = {S: E, E: N, N: W, W: S}
"""Where each side goes when a model turns by a quarter (+90 degrees around Y)."""


def turned(mask: int, quarters: int) -> int:
    """A set of sides after a number of quarter turns."""
    for _ in range(quarters % 4):
        mask = sum(TURN[side] for side in (N, E, S, W) if mask & side)
    return mask


def orient(base: int, wanted: int) -> int | None:
    """The quarter turns bringing the sides of a model to the sides wanted, ``None`` when none does."""
    for quarters in range(4):
        if turned(base, quarters) == wanted:
            return quarters
    return None


def mesh(path: str, colour: y.Colour = (1, 1, 1, 1)) -> str:
    """A ``MeshRenderer`` of a model of the project, routed to the mesh layer."""
    return f"MeshRenderer:\n  mesh: {path}\n  color: {y.vec(*colour)}\n  transparent: false\n" + MESH


THEMES: dict[str, tuple[str, y.Colour, y.Colour]] = {
    "grass": ("", (1, 1, 1, 1), (1, 1, 1, 1)),
    "marsh": ("", (0.56, 0.7, 0.58, 1), (0.48, 0.62, 0.5, 1)),
    "dry": ("snow-", (0.96, 0.8, 0.52, 1), (0.66, 0.52, 0.32, 1)),
    "snow": ("snow-", (1, 1, 1, 1), (1, 1, 1, 1)),
    "ash": ("snow-", (0.5, 0.42, 0.4, 1), (0.3, 0.25, 0.24, 1)),
}
"""Per theme of a level: the prefix of the kit models, the tint of the tiles and of the trees and rocks."""


def tile_mesh(level: mp.LevelData, name: str, decor: bool = False) -> str:
    """The ``MeshRenderer`` of a tile or decor model of the kit in the theme of a level."""
    prefix, tile_tint, decor_tint = THEMES[level.theme]
    return mesh(f"{TD}/{prefix}{name}.glb", decor_tint if decor else tile_tint)


def light2d(
    kind: str, colour: tuple[float, float, float], intensity: float, radius: float, falloff: float, enabled: bool = True
) -> str:
    """A ``Light2D`` lighting the sprites around it (a point light) or all of them (an ambient light)."""
    return (
        f"Light2D: {{type: {kind}, enabled: {'true' if enabled else 'false'}, color: {y.vec(*colour)}, "
        f"intensity: {y.num(intensity)}, radius: {y.num(radius)}, falloff: {y.num(falloff)}, height: 1, "
        "castShadows: false}\n"
    )


def sides(level: mp.LevelData, cell: mp.Cell, kinds: str, edge: bool = False) -> int:
    """The sides of a cell whose neighbour is one of ``kinds`` (or off the map, with ``edge``)."""
    x, z = cell
    mask = 0
    for side, other in ((N, (x, z - 1)), (E, (x + 1, z)), (S, (x, z + 1)), (W, (x - 1, z))):
        char = level.at(other)
        if char in kinds or (edge and char == " "):
            mask |= side
    return mask


ROAD_TILES = ((S, "tile-end"), (N | S, "tile-straight"), (E | S, "tile-corner-square"), (E | S | W, "tile-split"))
"""Road models and the sides their road opens to, unturned."""


def road_tile(mask: int) -> tuple[str, int]:
    """The road model and its quarter turns for the sides a road cell joins."""
    for base, name in ROAD_TILES:
        quarters = orient(base, mask)
        if quarters is not None:
            return name, quarters
    return "tile-straight", 0


def cell_base(level: mp.LevelData, cell: mp.Cell) -> float:
    """Height of the base of the top tile of a cell."""
    return level.height(cell) * mp.HEIGHT_STEP


def road_height(level: mp.LevelData, cell: mp.Cell) -> float:
    """Height of the road surface at the centre of a road cell."""
    char = level.at(cell)
    base = cell_base(level, cell)
    if char == "/":
        return base + ROAD_TOP + mp.HEIGHT_STEP * 0.5
    if char == "=":
        return base + BRIDGE_TOP
    return base + ROAD_TOP


def check(level: mp.LevelData) -> list[list[mp.Cell]]:
    """Refuse a level whose grids, roads, pads or keep do not fit; return its roads (one per gate)."""
    if len(level.heights) != level.depth or any(len(row) != level.width for row in level.layout + level.heights):
        raise ValueError(f"level {level.number}: the layout and the heights differ in size")
    roads = mp.routes(level)
    cells = mp.road_cells(level)
    for road in roads:
        if sides(level, road[-1], "K") == 0:
            raise ValueError(f"level {level.number}: a road does not end beside the keep")
    for pad in level.cells("P"):
        if not any(abs(pad[0] - x) + abs(pad[1] - z) <= 2 for x, z in cells):
            raise ValueError(f"level {level.number}: pad {pad} is too far from the road")
    for cell in cells:
        if level.at(cell) == "/":
            higher = [c for c in cells if abs(c[0] - cell[0]) + abs(c[1] - cell[1]) == 1]
            if max(level.height(c) for c in higher) <= level.height(cell):
                raise ValueError(f"level {level.number}: ramp {cell} climbs to nothing")
    return roads


BORDER = 5
"""Cells of countryside drawn around the grid, so that the view never shows its edge."""


def nearest_inside(level: mp.LevelData, cell: mp.Cell) -> mp.Cell:
    """The cell of the grid nearest to a cell, itself when inside."""
    return min(max(cell[0], 0), level.width - 1), min(max(cell[1], 0), level.depth - 1)


def terrain(entities: y.Entities, level: mp.LevelData, parent: int) -> None:
    """The tiles of every cell and of the countryside around the grid, under a ``Terrain`` parent."""
    rng = random.Random(level.number * 31 + 7)
    road = mp.road_cells(level)
    for z in range(-BORDER, level.depth + BORDER):
        for x in range(-BORDER, level.width + BORDER):
            cell = (x, z)
            inside = nearest_inside(level, cell)
            char = level.at(inside)
            if inside != cell:
                char = "~" if char in "~=" else ("T" if rng.random() < 0.3 else ".")
            steps = level.height(inside)
            base = steps * mp.HEIGHT_STEP
            if steps > 0:
                entities.add(
                    "Terrain",
                    [tile_mesh(level, "tile")],
                    position=(x, 0, z),
                    scale=(1, VS * steps, 1),
                    parent=parent,
                )
            name, quarters = "tile", 0
            if char in "#G":
                mask = sides(level, cell, mp.ROAD + "K")
                if char == "G":
                    name, quarters = "tile-spawn", orient(N | S, mask | turned(mask, 2)) or 0
                else:
                    name, quarters = road_tile(mask)
            elif char == "/":
                higher = max((c for c in road if abs(c[0] - x) + abs(c[1] - z) == 1), key=lambda c: level.height(c))
                side = {(0, -1): N, (1, 0): E, (0, 1): S, (-1, 0): W}[(higher[0] - x, higher[1] - z)]
                name, quarters = "tile-straight-slope", orient(N, side) or 0
            elif char == "~":
                mask = sides(level, inside, "~=", edge=True)
                name, quarters = "tile-river-straight", (0 if mask & (N | S) else 1)
            elif char == "=":
                mask = sides(level, cell, "#/G")
                name, quarters = "tile-river-bridge", (0 if mask & (E | W) else 1)
            elif char == "h":
                name, quarters = "tile-hill", rng.randrange(4)
            entities.add(
                "Terrain",
                [tile_mesh(level, name)],
                position=(x, base, z),
                rotation=(0, quarters * math.pi / 2, 0),
                scale=(1, VS, 1),
                parent=parent,
            )
            top = base + GRASS_TOP
            if char == "T":
                for _ in range(rng.choice((1, 1, 2, 3))):
                    model = rng.choice(("detail-tree", "detail-tree", "detail-tree-large"))
                    dx, dz = rng.uniform(-0.28, 0.28), rng.uniform(-0.28, 0.28)
                    size = rng.uniform(0.85, 1.2)
                    entities.add(
                        "Tree",
                        [tile_mesh(level, model, decor=True)],
                        position=(x + dx, top, z + dz),
                        rotation=(0, rng.uniform(0, 6.28), 0),
                        scale=(size, size, size),
                        parent=parent,
                    )
            elif char == "r":
                model = rng.choice(("detail-rocks", "detail-rocks-large"))
                entities.add(
                    "Rocks",
                    [tile_mesh(level, model, decor=True)],
                    position=(x, top, z),
                    rotation=(0, rng.uniform(0, 6.28), 0),
                    parent=parent,
                )


def keep(entities: y.Entities, level: mp.LevelData) -> None:
    """The human keep on the ``K`` cells: a wall with its gate facing the end of the road, corner towers, the main
    tower and banners."""
    cells = level.cells("K")
    xs = [c[0] for c in cells]
    zs = [c[1] for c in cells]
    x0, x1, z0, z1 = min(xs), max(xs), min(zs), max(zs)
    end = mp.route(level)[-1]
    base = cell_base(level, (x0, z0)) + GRASS_TOP
    root = entities.add("Keep", [], position=((x0 + x1) / 2, base, end[1]))
    gate_z = end[1]
    for z in range(z0, z1 + 1):
        local = (x0 - (x0 + x1) / 2, 0, z - gate_z)
        if z in (z0, z1):
            entities.add("KeepTower", [mesh(f"{CASTLE}/tower-hexagon-base.glb")], position=local, parent=root)
            roof = (local[0], 1.31, local[2])
            entities.add("KeepTower", [mesh(f"{CASTLE}/tower-hexagon-roof.glb")], position=roof, parent=root)
        elif z == gate_z:
            entities.add(
                "KeepGate",
                [mesh(f"{CASTLE}/wall-narrow-gate.glb")],
                position=local,
                rotation=(0, math.pi, 0),
                parent=root,
            )
        else:
            entities.add("KeepWall", [mesh(f"{CASTLE}/wall.glb")], position=local, parent=root)
    for x in range(x0 + 1, x1 + 1):
        for z in (z0, z1):
            local = (x - (x0 + x1) / 2, 0, z - gate_z)
            entities.add(
                "KeepWall", [mesh(f"{CASTLE}/wall.glb")], position=local, rotation=(0, math.pi / 2, 0), parent=root
            )
    hall = (x1 - (x0 + x1) / 2 - 0.3, 0, 0)
    for model, height in (
        ("tower-square-base", 0),
        ("tower-square-mid-windows", 1.01),
        ("tower-square-top-roof-high", 2.02),
    ):
        entities.add(
            "KeepHall", [mesh(f"{CASTLE}/{model}.glb")], position=(hall[0], height, 0), scale=(1.3, 1, 1.3), parent=root
        )
    for z in (z0, z1):
        flag = (x0 - (x0 + x1) / 2, 2.15, z - gate_z)
        entities.add("KeepFlag", [mesh(f"{CASTLE}/flag.glb")], position=flag, parent=root)
    entities.add("KeepFlag", [mesh(f"{CASTLE}/flag-banner-long.glb")], position=(hall[0], 3.35, 0), parent=root)


def torches(entities: y.Entities, level: mp.LevelData, road: list[mp.Cell]) -> None:
    """Torches lit at night: a point light (terrain, meshes) and a 2D light (sprites) on posts along the road and
    beside the keep gate, hidden by day."""
    picks = [road[len(road) * k // 4] for k in (1, 2, 3)] + [road[-1]]
    for x, z in picks:
        side = 0.55
        position = (x + side, road_height(level, (x, z)) + 0.9, z + side)
        light = (
            "Light: {type: Point, enabled: false, color: [1, 0.62, 0.3], intensity: 5000, range: 5.5}\n"
            "LightFlicker:\n  style: Fire\n  amplitude: 0.25\n  seed: "
            f"{x * 7 + z}\n  colorShift: [0, 0.05, 0.03]\n  positionJitter: [0.02, 0.03, 0.02]\n"
        )
        torch = entities.add("Torch", [light], position=position)
        flame = y.frames("textures/fx/fire.png", 4, 1, 0, (1, 1, 1, 1)).replace(
            "loop: false", "loop: true\n  lit: false"
        )
        flame = flame.replace("lastFrame: 0", "lastFrame: 3").replace("frameDuration: 0.1", "frameDuration: 0.09")
        entities.add(
            "TorchFlame", [flame, SPRITES], position=(0, 0, 0), scale=(0.35, 0.45, 1), parent=torch, visible=False
        )
        glow = light2d("Point", (1, 0.6, 0.3), 1.4, 3, 1.2, enabled=False)
        entities.add("TorchGlow", [glow], position=(0, 0, 0), parent=torch)
        post = (position[0], position[1] - 0.95, position[2])
        entities.add("TorchPost", [mesh(f"{CASTLE}/wall-stud.glb")], position=post, scale=(0.5, 0.8, 0.5))


def waypoints(entities: y.Entities, level: mp.LevelData, road: list[mp.Cell], letter: str) -> None:
    """The ``Waypoint NN`` entities of a road (``Waypoint B NN`` for the second gate) under a ``Route`` parent: its
    corners, the cells around a ramp or a bridge (their height changes), and a last one in the gate of the keep."""
    route = entities.add(f"Route {letter}".strip(), [])
    previous_direction = None
    for index, cell in enumerate(road):
        nxt = road[index + 1] if index + 1 < len(road) else None
        direction = (nxt[0] - cell[0], nxt[1] - cell[1]) if nxt else None
        height = road_height(level, cell)
        changes = level.at(cell) in "/=" or (nxt is not None and level.at(nxt) in "/=")
        changes = changes or (index > 0 and level.at(road[index - 1]) in "/=")
        if index == 0 or nxt is None or direction != previous_direction or changes:
            entities.add(f"Waypoint {letter}{index + 1:02d}", [], position=(cell[0], height, cell[1]), parent=route)
        previous_direction = direction
    end = road[-1]
    toward = sides(level, end, "K")
    dx = 0.6 if toward & E else (-0.6 if toward & W else 0.0)
    dz = 0.6 if toward & S else (-0.6 if toward & N else 0.0)
    position = (end[0] + dx, road_height(level, end), end[1] + dz)
    entities.add(f"Waypoint {letter}{len(road) + 1:02d}", [], position=position, parent=route)


def unlit_sprite(texture: str, colour: y.Colour = (1, 1, 1, 1)) -> str:
    """A ``SpriteRenderer`` drawn as it is, whatever the light (markers, the build wheel)."""
    return y.sprite(texture, colour).replace("tilingFactor", "lit: false\n  tilingFactor")


def world_text(colour: y.Colour, outline: y.Colour | None = None, font: str = "MedievalSharp") -> str:
    """A ``TextRenderer`` of the world (the costs and the info of the build wheel), set by the script."""
    out = f"TextRenderer:\n  color: {y.vec(*colour)}\n  kerning: 0\n  lineSpacing: 0\n  text: ''\n"
    if outline is not None:
        out += f"  outlineColor: {y.vec(*outline)}\n  outlineWidth: 0.08\n"
    return out + f"  font: {font}\n"


WHEEL_SLOTS = 4
"""Badges of the build wheel at most (three towers, or a level up, two power-ups and the sale)."""


def build_wheel(entities: y.Entities) -> None:
    """The build wheel, hidden: a ring, its badges with their prices and a parchment scroll with the info, billboards
    of the ``marks`` layer that ``defence.lua`` places beside the pad in screen space."""
    entities.add("Wheel", [unlit_sprite("textures/ui/wheel.png") + MARKS], visible=False)
    entities.add("WheelPlaque", [unlit_sprite("textures/ui/plaque.png") + MARKS], visible=False)
    for _ in range(WHEEL_SLOTS):
        badge = y.frames("textures/icons.png", 6, 2, 0).replace("loop: false", "loop: false\n  lit: false") + MARKS
        entities.add("BuildChoice", [badge], scale=(0.55, 0.55, 1), visible=False)
        cost = world_text((1, 0.86, 0.45, 1), (0.16, 0.09, 0.04, 1)) + MARKS
        entities.add("ChoiceCost", [cost], visible=False)
    entities.add("WheelTitle", [world_text((0.42, 0.07, 0.03, 1), font="Cinzel") + MARKS], visible=False)
    entities.add("WheelText", [world_text((0.2, 0.12, 0.06, 1)) + MARKS], visible=False)


def level_kinds(level: mp.LevelData) -> list[str]:
    """The kinds of the Horde a level sends, in the order they first march."""
    kinds = [group.split()[0] for wave in level.waves for group in wave.split(";")]
    return list(dict.fromkeys(kinds))


def new_enemies(level: mp.LevelData) -> list[str]:
    """The kinds a level sends that no earlier level of the campaign sent."""
    earlier = {kind for other in mp.LEVELS if other.number < level.number for kind in level_kinds(other)}
    return [kind for kind in level_kinds(level) if kind not in earlier]


def level_scene(level: mp.LevelData) -> str:
    """The scene of a level."""
    roads = check(level)
    road = roads[0]
    entities = y.Entities(7500000000000000000 + level.number * 1000000)
    header = (
        "EnabledRenderers:\n- {Name: world, Enabled: false}\n- {Name: mesh_world, Enabled: true}\n"
        "- {Name: ground, Enabled: true}\n- {Name: sprites, Enabled: true}\n- {Name: marks, Enabled: true}\n"
        "- {Name: ui, Enabled: false}\n- {Name: overlay, Enabled: false}\n"
    )
    cx, cz = (level.width - 1) / 2, (level.depth - 1) / 2
    camera = (
        "Camera:\n  camera:\n    projectionType: Perspective\n    perspectiveFOV: 0.62\n    perspectiveNear: 0.1\n"
        "    perspectiveFar: 200\n    orthographicSize: 8\n    orthographicNear: -1\n    orthographicFar: 10\n"
        "    toneMapping: Aces\n  primary: true\n  fixedAspectRatio: false\nSoundListener:\n  primary: true\n"
    )
    entities.add("MainCamera", [camera], position=(cx, 14, cz + 13), rotation=(-0.75, 0, 0))
    sun = entities.next_id + 2
    sky = (
        "ProceduralSky: {enabled: true, sunDirection: [0.3, 0.8, -0.5], turbidity: 3, groundColor: [0.3, 0.34, 0.26], "
        "nightColor: [0.42, 0.48, 0.66], nightBrightness: 0.02, stars: 1, discSize: 2.5, resolution: 256}\n"
        f"DayNightCycle: {{enabled: false, timeOfDay: {y.num(level.start_hour)}, dayLength: 600, azimuth: 120, "
        f"maxElevation: 58, sun: {sun}, sunIlluminance: 100000, sunColor: [1, 0.96, 0.9], sunsetColor: [1, 0.55, 0.3], "
        "moonIlluminance: 300, moonColor: [0.62, 0.72, 1], nightExposure: 6}\n"
    )
    environment = 'Environment: {hdri: "", ambient: [1, 1, 1], intensity: 9000, rotation: 0, showSky: true}\n'
    entities.add("Sky", [environment, sky])
    light = (
        "Light: {type: Directional, enabled: true, color: [1, 0.96, 0.9], intensity: 100000, castShadows: true, "
        "shadowDistance: 45, cascadeCount: 2, shadowResolution: 2048, shadowBias: 1.5}\n"
    )
    sun_id = entities.add("Sun", [light], rotation=(-0.9, 0.6, 0))
    assert sun_id == sun, "the sun id is written in the day-night cycle before the sun"
    minimap = (
        "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.785\n    perspectiveNear: 0.01\n"
        f"    perspectiveFar: 100\n    orthographicSize: {y.num(level.depth + 1)}\n    orthographicNear: 0\n"
        "    orthographicFar: 60\n    toneMapping: Aces\n  primary: false\n  fixedAspectRatio: true\n"
        "RenderTarget: {enabled: true, targetName: minimap, size: [288, 224], clearColor: [0.05, 0.07, 0.1, 1]}\n"
    )
    entities.add("MinimapCamera", [minimap], position=(cx, 30, cz), rotation=(-math.pi / 2, 0, 0))
    ground = entities.add("Terrain", [])
    prefix, tint, _ = THEMES[level.theme]
    shade = (0.86, 0.95, 0.86, 1) if level.theme == "grass" else (tint[0] * 0.92, tint[1] * 0.92, tint[2] * 0.92, 1)
    countryside = mesh(f"{TD}/{prefix}tile.glb", shade)
    entities.add("Countryside", [countryside], position=(cx, -0.06, cz), scale=(160, 1, 160), parent=ground)
    terrain(entities, level, ground)
    for path in roads:
        gate = path[0]
        gate_top = cell_base(level, gate) + GRASS_TOP
        gate_id = entities.add("Gate", [], position=(gate[0], gate_top - GRASS_TOP + ROAD_TOP, gate[1]))
        portal = mesh(f"{TD}/spawn-round.glb", (0.55, 0.2, 0.18, 1))
        entities.add("GatePortal", [portal], position=(0, 0.02, 0), scale=(1.2, 1, 1.2), parent=gate_id)
        across = sides(level, gate, mp.ROAD) & (N | S) == 0
        for offset in (-0.5, 0.5):
            banner = mesh(f"{CASTLE}/flag-banner-long.glb", (0.6, 0.12, 0.1, 1))
            entities.add(
                "GateBanner",
                [banner],
                position=(0, GRASS_TOP - ROAD_TOP, offset) if across else (offset, GRASS_TOP - ROAD_TOP, 0),
                rotation=(0, math.pi / 2, 0) if across else (0, 0, 0),
                scale=(0.75, 0.75, 0.75),
                parent=gate_id,
            )
    keep(entities, level)
    pads = entities.add("Pads", [])
    for cell in level.cells("P"):
        top = cell_base(level, cell) + GRASS_TOP
        pad = mesh(f"{TD}/tower-round-base.glb", (0.92, 0.9, 0.86, 1))
        entities.add("Pad", [pad], position=(cell[0], top, cell[1]), scale=(0.82, 1, 0.82), parent=pads)
    for letter, path in zip(("", "B "), roads, strict=False):
        waypoints(entities, level, path, letter)
    torches(entities, level, road)
    cursor = y.sprite("textures/fx/cursor.png", (1, 0.9, 0.5, 0.9)).replace(
        "tilingFactor", "lit: false\n  tilingFactor"
    )
    entities.add("Cursor", [cursor + GROUND], rotation=FLAT, visible=False)
    ring = y.sprite("textures/fx/ring.png", (1, 1, 1, 0.7)).replace("tilingFactor", "lit: false\n  tilingFactor")
    entities.add("RangeRing", [ring + GROUND], rotation=FLAT, visible=False)
    build_wheel(entities)
    name = f"level_{level.number:02d}"
    next_scene = "scenes/victory.owl"
    waves = "".join(f"\n        - {y.quote(wave)}" for wave in level.waves)
    modifiers_block = ""
    if level.modifiers:
        listed = "".join(f"\n        - {y.quote(key)}" for key in level.modifiers)
        modifiers_block = f"    - name: modifiers\n      type: strings\n      value:{listed}\n"
    properties = (
        "LuaScript:\n  scriptPath: scripts/game/defence.lua\n  properties:\n"
        f"    - name: level_number\n      type: int\n      value: {level.number}\n"
        f"    - name: level_name\n      type: string\n      value: {y.quote(level.name)}\n"
        f"    - name: scene_path\n      type: string\n      value: scenes/{name}.owl\n"
        f"    - name: next_scene\n      type: string\n      value: {next_scene}\n"
        f"    - name: waves\n      type: strings\n      value:{waves}\n"
        f"    - name: night_waves\n      type: string\n      value: {y.quote(' '.join(map(str, level.nights)))}\n"
        f"    - name: new_enemies\n      type: string\n      value: {y.quote(','.join(new_enemies(level)))}\n"
        + modifiers_block
        + f"    - name: start_hour\n      type: float\n      value: {y.num(level.start_hour)}\n"
        f"    - name: start_gold\n      type: int\n      value: {level.start_gold}\n"
        f"    - name: lives\n      type: int\n      value: {level.lives}\n"
        f"    - name: map_width\n      type: int\n      value: {level.width}\n"
        f"    - name: map_depth\n      type: int\n      value: {level.depth}\n"
    )
    entities.add("Game", [properties])
    return y.scene(name, entities, hud="huds/play.owlhud", header=header)


def prefab(name: str, entities: y.Entities) -> str:
    """A prefab file of the entities added."""
    return f"Prefab: {name}\nVersion: 1\nEntities:\n" + entities.text()


def single(name: str, tag: str, components: list[str], scale: tuple[float, float, float]) -> str:
    """A prefab of one entity."""
    entities = y.Entities(7600000000000000000 + sum(map(ord, name)) * 1000)
    entities.add(tag, components, scale=scale)
    return prefab(name, entities)


Part = tuple[str, float, y.Colour, tuple[float, float, float], float]
"""A mesh of a tower: model, height it adds to the stack, tint, offset from the top of the stack, scale."""
WHITE = (1, 1, 1, 1)
FROST_TINT = (0.75, 0.88, 1, 1)
FIRE_TINT = (1, 0.62, 0.45, 1)
NONE = (0.0, 0.0, 0.0)


def _part(
    model: str, height: float, tint: y.Colour = WHITE, offset: tuple[float, float, float] = NONE, scale: float = 1.0
) -> Part:
    return f"{TD}/{model}.glb", height, tint, offset, scale


def _flag(tint: y.Colour) -> Part:
    return f"{CASTLE}/flag.glb", 0.0, tint, (0.32, -0.1, 0.32), 0.9


def _tower_parts() -> dict[str, list[Part]]:
    """The meshes of each tower prefab, from the bottom: three levels, then the two power-ups of the third."""
    out: dict[str, list[Part]] = {}
    ballista = _part("weapon-ballista", 0.0)
    out["archer_1"] = [_part("tower-round-bottom-a", 0.6), _part("tower-round-top-a", 0.5), ballista]
    out["archer_2"] = [
        _part("tower-round-bottom-a", 0.6),
        _part("tower-round-middle-a", 0.6),
        _part("tower-round-top-a", 0.5),
        ballista,
    ]
    archer_3 = [_part("tower-round-bottom-b", 0.6), _part("tower-round-middle-a", 0.6), _part("tower-round-top-b", 0.5)]
    out["archer_3"] = [*archer_3, ballista, _flag((1, 0.5, 0.45, 1))]
    out["archer_pierce"] = [*archer_3, _part("weapon-ballista", 0.0, (1, 0.5, 0.4, 1)), _flag((1, 0.3, 0.25, 1))]
    out["archer_double"] = [
        *archer_3,
        _part("weapon-ballista", 0.0, offset=(-0.2, 0, 0), scale=0.75),
        _part("weapon-ballista", 0.0, offset=(0.2, 0, 0), scale=0.75),
        _flag((0.6, 0.8, 1, 1)),
    ]
    cannon = _part("weapon-cannon", 0.0)
    out["cannon_1"] = [_part("tower-square-bottom-a", 0.5), _part("tower-square-top-a", 0.5), cannon]
    out["cannon_2"] = [
        _part("tower-square-bottom-a", 0.5),
        _part("tower-square-middle-a", 0.5),
        _part("tower-square-top-a", 0.5),
        cannon,
    ]
    cannon_3 = [_part("tower-square-bottom-b", 0.5), _part("tower-square-middle-b", 0.5)]
    out["cannon_3"] = [*cannon_3, _part("tower-square-top-b", 0.5), cannon, _flag((0.8, 0.6, 1, 1))]
    out["cannon_fire"] = [
        *cannon_3,
        _part("tower-square-top-c", 0.5, FIRE_TINT),
        _part("weapon-cannon", 0.0, FIRE_TINT),
        _flag((1, 0.55, 0.2, 1)),
    ]
    out["cannon_shock"] = [
        *cannon_3,
        _part("tower-square-top-b", 0.5),
        _part("weapon-catapult", 0.0),
        _flag((0.85, 0.75, 0.5, 1)),
    ]
    crystals = _part("tower-round-crystals", 0.0, FROST_TINT, (0, 0.36, 0))
    out["frost_1"] = [
        _part("tower-round-bottom-c", 0.6, FROST_TINT),
        _part("tower-round-top-c", 0.53, FROST_TINT),
        crystals,
    ]
    out["frost_2"] = [
        _part("tower-round-bottom-c", 0.6, FROST_TINT),
        _part("tower-round-middle-c", 0.6, FROST_TINT),
        _part("tower-round-top-c", 0.53, FROST_TINT),
        crystals,
    ]
    frost_3 = [
        _part("tower-round-bottom-b", 0.6, FROST_TINT),
        _part("tower-round-middle-c", 0.6, FROST_TINT),
        _part("tower-round-top-c", 0.53, FROST_TINT),
    ]
    out["frost_3"] = [*frost_3, crystals, _flag((0.6, 0.85, 1, 1))]
    out["frost_freeze"] = [
        *frost_3,
        _part("tower-round-crystals", 0.0, (0.95, 1, 1, 1), (0, 0.36, 0), 1.15),
        _flag((0.95, 1, 1, 1)),
    ]
    out["frost_aura"] = [
        *frost_3,
        _part("tower-round-crystals", 0.0, (0.85, 0.6, 1, 1), (0, 0.36, 0), 1.15),
        _flag((0.75, 0.5, 1, 1)),
    ]
    return out


TOWER_PARTS = _tower_parts()
"""The meshes of every tower prefab."""
TOWER_SCALE = 0.78
"""Scale of the towers on their pads."""


def tower_prefab(key: str) -> str:
    """A tower: a root tagged ``Tower`` (picked as one) holding its meshes from the bottom up."""
    entities = y.Entities(7600000000000000000 + sum(map(ord, key)) * 1000)
    root = entities.add("Tower", [], scale=(TOWER_SCALE, TOWER_SCALE, TOWER_SCALE))
    height = 0.0
    for index, (model, step, tint, offset, scale) in enumerate(TOWER_PARTS[key]):
        position = (offset[0], height + offset[1], offset[2])
        tag = "TowerWeapon" if "/weapon-" in model or model.endswith("crystals.glb") else f"TowerPart{index}"
        entities.add(tag, [mesh(model, tint)], position=position, scale=(scale, scale, scale), parent=root)
        height += step
    return prefab(f"Tower_{key}", entities)


ENEMIES = tuple(CREATURES)
ENEMY_SCALE = {"chieftain": 1.35}
"""Enemies drawn bigger than the others (the boss)."""
"""Enemy kinds, a sheet each."""


def prefabs() -> dict[str, str]:
    """Every prefab the script spawns, by file name."""
    out = {}
    for kind in ENEMIES:
        sheet = y.frames(f"textures/enemies/{kind}.png", DIRECTIONS, FRAMES, 0) + SPRITES
        size = SPRITE_SIZE * ENEMY_SCALE.get(kind, 1.0)
        name = "Enemy" + kind.title().replace("_", "")
        out[f"enemy_{kind}"] = single(name, "Enemy", [sheet], (size, size, 1))
    for key in TOWER_PARTS:
        out[f"tower_{key}"] = tower_prefab(key)
    for index, kind in enumerate(("arrow", "ball", "fireball", "shard")):
        sheet = y.frames("textures/fx/projectiles.png", 4, 1, index) + SPRITES
        components = [sheet]
        if kind == "fireball":
            components.append(light2d("Point", (1, 0.55, 0.2), 0.9, 2.2, 1.4))
        out[f"projectile_{kind}"] = single(f"Projectile{kind.title()}", "Projectile", components, (0.32, 0.32, 1))
    bar = unlit_sprite("textures/bar.png", (0.3, 0.9, 0.3, 1)) + SPRITES
    out["health_bar"] = single("HealthBar", "HealthBar", [bar], (0.5, 0.07, 1))
    shadow = y.sprite("textures/fx/shadow.png", (1, 1, 1, 0.6)) + GROUND
    out["shadow"] = single("Shadow", "Shadow", [shadow], (0.5, 0.35, 1))
    puff = unlit_sprite("textures/puff.png", (1, 1, 1, 0.8)) + SPRITES
    out["puff"] = single("Puff", "Puff", [puff], (0.6, 0.6, 1))
    spark = unlit_sprite("textures/puff.png") + SPRITES
    out["spark"] = single("Spark", "Spark", [spark], (0.1, 0.1, 1))
    flame = y.frames("textures/fx/fire.png", 4, 1, 0).replace("loop: false", "loop: true\n  lit: false")
    flame = flame.replace("lastFrame: 0", "lastFrame: 3").replace("frameDuration: 0.1", "frameDuration: 0.08") + SPRITES
    glow = light2d("Point", (1, 0.5, 0.2), 1.2, 2, 1.3)
    out["fire"] = single("Fire", "Fire", [flame, glow], (0.55, 0.6, 1))
    wave = y.sprite("textures/fx/ring.png", (1, 0.85, 0.6, 0.8)) + GROUND
    out["shock_ring"] = single("ShockRing", "ShockRing", [wave], (1, 1, 1))
    return out


def play_hud() -> str:
    """The HUD of a level, in the medieval look of the menus: gold, lives and the wave on parchment at the top left,
    the minimap in a wood and iron frame at the top right, the spells at the bottom left, the Call wave / speed / Menu
    planks at the bottom right, a message line (holding the briefing card that opens a level) and the pause window,
    the only place of a level that shows the controls. The build wheel is not here: it opens in the world, beside its
    pad."""
    entities = y.Entities(7490000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    # One column at the top left, the minimap at the top right, the planks at the bottom right and the spells at the
    # bottom left: they never meet, at any interface size or aspect.
    column = entities.add(
        "TopLeft", [y.rect((0, 0), (0.5, 0.21), (16, 8)), y.layout("Column", 3, 0, "Start")], parent=root
    )
    phase = entities.add(
        "PhaseBar",
        [y.child_rect((0.78, 0.26)), y.image("textures/ui/panel_stats.png"), y.layout("Row", 6, 2, "Center")],
        parent=column,
    )
    phase_text = menus.label("Day", 20, menus.INK, "Left").replace("outlineWidth: 2", "outlineWidth: 0")
    entities.add("PhaseText", [y.child_rect((0.8, 0.75)), phase_text], parent=phase)
    icon = y.image("textures/ui/daynight.png") + "  columns: 2\n  rows: 1\n  frame: 0\n"
    entities.add("PhaseIcon", [y.child_rect((0.12, 0.75)), icon], parent=phase)
    wave = menus.label("Wave 0 / 8 - next in 25 s", 24, menus.INK).replace("outlineWidth: 2", "outlineWidth: 0")
    entities.add("WaveText", [y.child_rect((1, 0.31)), y.image("textures/ui/panel_wave.png"), wave], parent=column)
    stats = entities.add(
        "StatsBar",
        [y.child_rect((0.78, 0.31)), y.image("textures/ui/panel_stats.png"), y.layout("Row", 8)],
        parent=column,
    )
    lives = menus.label("Lives 20", 26, (0.55, 0.08, 0.05, 1)).replace("outlineWidth: 2", "outlineWidth: 0")
    gold = menus.label("Gold 240", 26, (0.45, 0.3, 0.02, 1)).replace("outlineWidth: 2", "outlineWidth: 0")
    entities.add("LivesText", [y.child_rect((0.45, 1)), lives], parent=stats)
    entities.add("GoldText", [y.child_rect((0.45, 1)), gold], parent=stats)
    frame = entities.add("Minimap", [y.rect((1, 0), (0.3, 0.24), (-16, 8))], parent=root)
    image = "UiImage:\n  tint: [1, 1, 1, 1]\n  renderTarget: minimap\n"
    entities.add(
        "MinimapImage", [y.rect((0.5, 0.5), (0.9, 0.87), units=("ParentWidth", "ParentHeight")), image], parent=frame
    )
    entities.add("MinimapFrame", [y.child_rect((1, 1)), y.image("textures/ui/frame.png")], parent=frame)
    bar = entities.add("ButtonBar", [y.rect((1, 1), (0.5, 0.075), (-16, -8)), y.layout("Row", 6)], parent=root)
    for tag, text, callback, width in (
        ("MenuButton", "Menu", "on_menu_clicked", 0.2),
        ("SpeedButton", "Speed x1", "on_speed_clicked", 0.28),
        ("CallButton", "Call wave", "on_call_clicked", 0.44),
    ):
        menus.button(entities, bar, tag, text, callback, (width, 0.9), focusable=False, font_size=24)
    spells = entities.add("SpellBar", [y.rect((0, 1), (0.5, 0.075), (16, -8)), y.layout("Row", 6)], parent=root)
    for tag, text, callback in (
        ("ShieldButton", "Shield wall", "on_shield_clicked"),
        ("RainButton", "Rain of arrows", "on_rain_clicked"),
    ):
        menus.button(entities, spells, tag, text, callback, (0.48, 0.9), focusable=False, font_size=22)
    message = menus.label("", 40, menus.STONE, font=menus.HEADING).replace("wrap: false", "wrap: true")
    message_id = entities.add("MessageText", [y.rect((0.5, 0.62), (0.7, 0.14)), message], parent=root, visible=False)
    window = entities.add(
        "PauseWindow",
        [y.rect((0.5, 0.5), (1.3, 0.94)), menus.window("Paused"), y.layout("Column", 4, 8)],
        parent=root,
        visible=False,
    )
    controls = y.text(menus.CONTROLS, 16, "Left", menus.GOLD, wrap=True, font="JetBrainsMono-Regular", line_spacing=0.1)
    entities.add("ControlsText", [y.child_rect((0.96, 0.34)), controls], parent=window)
    legend = y.text("", 15, "Left", menus.SOFT, wrap=True, line_spacing=0.05, vertical="Top")
    entities.add("EffectsText", [y.child_rect((0.96, 0.27)), legend], parent=window)
    for tag, text, callback, initial in (
        ("QuitButton", "Quit to the map", "on_pause_quit", False),
        ("RestartButton", "Restart level", "on_pause_restart", False),
        ("ResumeButton", "Resume", "on_pause_resume", True),
    ):
        menus.button(entities, window, tag, text, callback, (0.6, 0.085), initial)
    card = entities.add(
        "BriefingCard",
        [
            y.rect((0.5, 0.5), (1.1, 0.66), (0, -0.05), offset_units=("ScreenHeight", "ScreenHeight")),
            y.panel((0.9, 0.82, 0.64, 0.98), (0.84, 0.66, 0.25, 1), 4),
            y.layout("Column", 4, 20),
        ],
        parent=message_id,
        visible=False,
    )
    menus.button(entities, card, "BriefingButton", "To arms!", "on_briefing_closed", (0.4, 0.12), True)
    hint = menus.label("Click, Enter or pad A to begin", 18, (0.35, 0.22, 0.1, 1)).replace(
        "outlineWidth: 2", "outlineWidth: 0"
    )
    entities.add("BriefingHint", [y.child_rect((0.9, 0.07)), hint], parent=card)
    text = y.text("", 22, "Left", (0.2, 0.12, 0.06, 1), wrap=True, line_spacing=0.15, vertical="Top", outline_width=0)
    entities.add("BriefingText", [y.child_rect((0.76, 0.45)), text], parent=card)
    title = menus.label("", 44, (0.45, 0.08, 0.04, 1), font=menus.HEADING).replace("outlineWidth: 2", "outlineWidth: 0")
    entities.add("BriefingTitle", [y.child_rect((0.9, 0.14)), title], parent=card)
    return y.hud_file("play", entities)


PROJECT = """OwlProject:
  name: OwlTowerDefence
  firstScene: scenes/title.owl
  author: Silmaen
  description: Tower defence, a sample game of the Owl Engine
  icon: logo/logo_owl.png
  gamepadMappings: input/gamecontrollerdb.txt
  window:
    width: 1280
    height: 720
    fullscreen: false
    resizable: true
  RendererStack:
    - Type: Renderer2D
      Name: world
    - Type: RendererMesh
      Name: mesh_world
      DefaultConfig:
        SunDirection: [-0.4, -1.0, -0.6]
        Ambient: [0.4, 0.4, 0.45]
    - Type: Renderer2D
      Name: ground
      DefaultConfig:
        DepthTest: true
    - Type: Renderer2D
      Name: sprites
      DefaultConfig:
        Billboard: Full
        DepthTest: true
    - Type: Renderer2D
      Name: marks
      DefaultConfig:
        Billboard: Full
    - Type: Renderer2D
      Name: ui
    - Type: Renderer2D
      Name: overlay
"""
"""The project file."""

SETTINGS = """GameSettings:
  - key: resolution_width
    type: int
    value: 1280
  - key: resolution_height
    type: int
    value: 720
  - key: fullscreen
    type: bool
    value: false
  - key: resizable
    type: bool
    value: true
  - key: volume_master
    type: float
    value: 1.0
  - key: ui_scale
    type: float
    value: 1.0
"""
"""The game settings and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of Tower defence, on top of the built-in ones (`primary` / `secondary` are the mouse buttons, the `ui_*`
# actions drive the menus). Read by scripts/game/defence.lua.
actions:
  - name: pan
    type: vector
    bindings:
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: S, component: y}
      - {key: W, component: y, scale: -1}
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_stick: left}
  - name: turn_left
    type: button
    bindings:
      - {key: Z}
      - {gamepad_button: LeftBumper}
      - {gamepad_button: DpadUp}
  - name: turn_right
    type: button
    bindings:
      - {key: C}
      - {gamepad_button: DpadDown}
  - name: zoom
    type: axis
    bindings:
      - {key: R, scale: -1}
      - {key: V}
      - {gamepad_stick: right, component: y}
  - name: rain
    type: button
    bindings:
      - {key: D4}
      - {gamepad_button: LeftThumb}
  - name: shield
    type: button
    bindings:
      - {key: D5}
      - {gamepad_button: RightThumb}
  - name: next_pad
    type: button
    bindings:
      - {key: E}
      - {key: Tab}
      - {gamepad_button: DpadRight}
  - name: prev_pad
    type: button
    bindings:
      - {key: Q}
      - {gamepad_button: DpadLeft}
  - name: confirm
    type: button
    bindings:
      - {key: Enter}
      - {gamepad_button: A}
  - name: cancel
    type: button
    bindings:
      - {key: Backspace}
      - {gamepad_button: B}
  - name: build_1
    type: button
    bindings:
      - {key: D1}
  - name: build_2
    type: button
    bindings:
      - {key: D2}
  - name: build_3
    type: button
    bindings:
      - {key: D3}
  - name: sell
    type: button
    bindings:
      - {key: X}
      - {gamepad_button: X}
  - name: call_wave
    type: button
    bindings:
      - {key: Space}
      - {gamepad_button: Y}
  - name: fast_forward
    type: button
    bindings:
      - {key: F}
      - {gamepad_button: RightBumper}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The action map."""


def main() -> None:
    """Write every generated file of the levels."""
    for folder in ("scenes", "huds", "prefabs", "input"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    shutil.copyfile(DEMO / "input" / "gamecontrollerdb.txt", ROOT / "input" / "gamecontrollerdb.txt")
    (ROOT / "huds" / "play.owlhud").write_text(play_hud())
    for old in (ROOT / "prefabs").glob("*.owlprefab"):
        old.unlink()
    for name, text in prefabs().items():
        (ROOT / "prefabs" / f"{name}.owlprefab").write_text(text)
    for level in mp.LEVELS:
        (ROOT / "scenes" / f"level_{level.number:02d}.owl").write_text(level_scene(level))
    models = {
        line.split("mesh: ", 1)[1]
        for path in ROOT.glob("**/*.ow*")
        for line in path.read_text().splitlines()
        if "mesh: models/" in line
    }
    missing = sorted(m for m in models if not (ROOT / m).is_file())
    if missing:
        raise FileNotFoundError(f"models missing from samples/tower_defence: {missing}")


if __name__ == "__main__":
    main()
