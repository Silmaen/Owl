#!/usr/bin/env python3
"""
Write the project files, the mission scene, the prefabs, the behaviour trees, the wave table, the play HUD and the
mission graph of the Asteroid field sample game into ``samples/asteroid_field/``.

- ``owl_project.yml``, ``game_settings.yml``, ``input_actions.yml``;
- ``scenes/mission.owl``: the route of the convoy (a spline), three freighters riding it, the player's fighter (a
  ``ShipController`` and its chase camera), two wingmen in formation on it (``SteeringAgent`` and
  ``BehaviourTreeAgent``), the rocks of the field (static spheres), the jump gate, the gas giant of the planet layer,
  the explosion emitters; the scene script ``scripts/game/mission.owlvs``;
- ``prefabs/``: the fighter and the interceptor of the waves (a ``SteeringAgent``, the ``scripts/game/enemy.owlbt``
  tree, a HUD marker, an engine trail), the lasers;
- ``scripts/game/wingman.owlbt``: escort the player or go for the enemies, by the order the player gives;
- ``tables/waves.owltable``: the waves, by the distance of the convoy along the route;
- ``huds/play.owlhud``: hull, convoy, kills, wave, the order of the wings, the route left, the aim, the pause window.
  The fight itself (lasers, hits, waves, lock) is the Lua module ``scripts/game/combat.lua``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.asteroid_field.generate_level``;
then ``OwlNest --upgrade-assets samples/asteroid_field`` gives the new files their ``.owlmeta``.
"""

from __future__ import annotations

import math
import random
from collections.abc import Mapping
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.rail_shooter import generate_level as rail
from tools.samples.vs_graph import Graph, Node, Pin

ROOT = Path(__file__).resolve().parents[3] / "samples" / "asteroid_field"
"""The sample project."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
TEXT = (0.75, 1, 0.85, 1)
"""Colour of the HUD texts."""
ACCENT = (0.8, 0.6, 1, 1)
"""Colour of the highlights."""
SOFT = (0.7, 0.85, 0.8, 1)
"""Colour of the secondary texts."""
FONT = "OpenSans-BoldItalic"
"""Font of the titles and of the buttons."""
MONO = "JetBrainsMono-Regular"
"""Font of the counters."""
ROUTE = [(0.0, 0.0, 0.0), (35.0, 12.0, -220.0), (-30.0, -14.0, -440.0), (25.0, 6.0, -660.0), (0.0, 0.0, -880.0)]
"""Control points of the route of the convoy."""
CONVOY_SPEED = 6.5
"""Speed of the convoy along the route."""
CONTROLS = (
    "PITCH, YAW    arrows, A / D, left stick, hold right click: toward the aim\n"
    "AIM           mouse (click the view), the X leads a lock\n"
    "THROTTLE      W / S raise / lower (kept), Backspace cuts, RT / LT\n"
    "ROLL          Q / E, LB / RB\n"
    "SLIDE         X / C, R / V, right stick\n"
    "FIRE          Space, left click, A\n"
    "LOCK TARGET   F, middle click, X     NEXT  wheel, T / G, d-pad\n"
    "WINGS ATTACK  1, Y     WINGS ESCORT  2, B\n"
    "ASSIST        Z, right stick click     PAUSE  Escape, P, Start"
)
"""The controls, in the How to play window of the title and in the pause window."""

BRIEFING = (
    "Three freighters fly to the jump gate (the violet marker).\n"
    "Enemy fighters attack them in waves: shoot them down.\n"
    "The convoy jumps when its last freighter reaches the gate."
)
"""The briefing shown when the mission starts, until the first shot or after a few seconds."""

Vec3 = tuple[float, float, float]


# ------------------------------------------------------------------------------------------------------ components


def steering(speed: float, weights: Mapping[str, float], leader: int = 0, slot: Vec3 = (0, 0, -2)) -> str:
    """A ``SteeringAgent`` in open space."""
    names = ["seek", "flee", "arrive", "pursue", "evade", "wander", "separation", "alignment", "cohesion"]
    names += ["avoidance", "formation"]
    weight_text = ", ".join(f"{n}: {y.num(weights.get(n, 0))}" for n in names)
    return (
        f"SteeringAgent:\n  enabled: true\n  plane: Space\n  maxSpeed: {y.num(speed)}\n  maxAcceleration: 28\n"
        f"  radius: 2.5\n  turn: true\n  target: 0\n  targetPoint: [0, 0, 0]\n  weights: {{{weight_text}}}\n"
        "  slowRadius: 6\n  panicDistance: 0\n  prediction: 1\n  wander: {radius: 2, distance: 6, jitter: 2}\n"
        "  flock: ''\n  neighbourRadius: 12\n  separationDistance: 5\n  lookAhead: 1.5\n"
        f"  leader: {leader}\n  slot: {y.vec(*slot)}\n"
    )


def tree(path: str, overrides: str = "") -> str:
    """A ``BehaviourTreeAgent`` running a tree of the project."""
    out = f"BehaviourTreeAgent:\n  enabled: true\n  tree: {path}\n"
    return out + (f"  overrides:\n{overrides}" if overrides else "  overrides: []\n")


def follower(route: int, start: float) -> str:
    """A freighter riding the route once from a distance."""
    return (
        f"SplineFollower:\n  enabled: true\n  spline: {route}\n  lane: ''\n  offset: 0\n"
        f"  speed: {y.num(CONVOY_SPEED)}\n  startDistance: {y.num(start)}\n  mode: Once\n  orient: true\n"
    )


def spatial_loop(path: str, volume: float, reach: float) -> str:
    """A looping spatial sound started with its entity, pitched by the Doppler effect as it passes the listener."""
    return (
        f"SoundSource:\n  soundAsset: {path}\n  category: SFX\n  volume: {y.num(volume)}\n  pitch: 1\n  loop: true\n"
        f"  spatial: true\n  playOnStart: true\n  maxDistance: {y.num(reach)}\n  rolloff: 1\n  doppler: true\n"
    )


def ship(thrust: float, speed: float) -> str:
    """The player's ``ShipController``."""
    return (
        f"ShipController: {{enabled: true, thrust: {y.num(thrust)}, strafeThrust: 30, maxSpeed: {y.num(speed)},"
        " turnRate: 1.4, rollRate: 1.8, mouseSensitivity: 0.0015, angularResponse: 0, assistDamping: 6,"
        " flightAssist: true, captureCursor: true, mouseSteer: false, levelRate: 1.2, maxPitch: 1.2, throttleRate: 0.6,"
        " minThrottle: -0.4}\n"
    )


def rock_body() -> str:
    """A static sphere on layer 1: the rocks the lasers and the ships meet."""
    return (
        "PhysicBody:\n  type: Static\n  dimension: Space\n  shape: Sphere\n  fixedRotation: false\n"
        "  colliderSize: [1.6, 1.6, 1.6]\n  density: 1\n  restitution: 0\n  friction: 0.5\n  layer: 1\n  mask: 0\n"
    )


SPARKS = rail.burst(
    [(0, 1, 1, 0.9, 1), (0.4, 1, 0.75, 0.35, 0.9), (1, 0.6, 0.25, 0.08, 0)],
    (10, 28),
    0.6,
    9000,
    64,
    (0.15, 0.45),
    end_size=0.2,
    drag=3,
    trail_length=4,
)
"""The sparks of a hit on a hull (the ``Spark`` emitters)."""


HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: planet_world, Enabled: true}\n  - {Name: particles, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
    "FloatingOrigin:\n  enabled: true\n  shiftRadius: 1000\n  origin: [0, 0, 0]\n"
    "Physics:\n  solverSubSteps: 1\n  interpolate: true\n  workerCount: 0\n  gravity: [0, 0, 0]\n"
)
"""Root keys of the mission: the 3D layers, a floating origin, no gravity."""


def route_point(t: float) -> Vec3:
    """A point of the polyline of the route, ``t`` from 0 to 1 (close enough to the spline to keep rocks off it)."""
    segments = len(ROUTE) - 1
    index = min(segments - 1, int(t * segments))
    local = t * segments - index
    a, b = ROUTE[index], ROUTE[index + 1]
    return tuple(a[k] + (b[k] - a[k]) * local for k in range(3))  # type: ignore[return-value]


def rocks(entities: y.Entities) -> None:
    """The field: rocks clear of the route by 20 units, bigger and farther out to the sides."""
    rng = random.Random(9)
    models = ["models/asteroid_a.glb", "models/asteroid_b.glb", "models/asteroid_c.glb"]
    samples = [route_point(i / 400) for i in range(401)]
    placed = 0
    while placed < 170:
        z = rng.uniform(-900.0, 60.0)
        point = (rng.uniform(-130.0, 130.0), rng.uniform(-70.0, 70.0), z)
        clearance = min(math.dist(point, p) for p in samples)
        size = rng.choice([2.0, 3.0, 4.0, 5.5, 7.0, 9.0])
        if clearance < 20.0 + size:
            continue
        grey = rng.uniform(0.5, 0.9)
        rotation = (rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3))
        tint = (grey, grey * 0.95, grey * 0.9, 1)
        entities.add(
            "Rock",
            [rail.mesh(rng.choice(models), tint), rock_body()],
            position=point,
            scale=(size,) * 3,
            rotation=rotation,
        )
        placed += 1
    for _ in range(90):
        point = (rng.uniform(-400, 400), rng.uniform(-200, 200), rng.uniform(-1100, 200))
        if abs(point[0]) < 150 and abs(point[1]) < 90:
            continue
        size = rng.uniform(10, 30)
        entities.add(
            "FarRock", [rail.mesh(rng.choice(models), (0.6, 0.58, 0.55, 1))], position=point, scale=(size,) * 3
        )


def mission_scene() -> str:
    """The mission."""
    entities = y.Entities(7600000000000000000)
    entities.add("Sun", [rail.SUN], rotation=rail.SUN_ROTATION)
    entities.add("PlanetShine", [rail.SHINE], rotation=rail.SHINE_ROTATION)
    entities.add("Sky", [rail.sky()])
    entities.add("Planet", [rail.world_position(rail.PLANET_CENTRE), rail.planet()], position=rail.PLANET_CENTRE)
    route = entities.add("Route", [rail.spline(ROUTE)])
    for index, start in enumerate((60.0, 36.0, 12.0), start=1):
        marker = rail.hud_marker("textures/hud/target.png", 40, (0.45, 1, 0.6, 1), f"FREIGHTER {index}", True)
        obstacle = "NavObstacle: {radius: 5, height: 5}\n"
        root = entities.add(f"Freighter{index}", [follower(route, start), marker, obstacle])
        entities.add(f"Freighter{index}Model", [rail.mesh("models/freighter.glb", glow=900)], parent=root)
    player = entities.add("Player", [ship(60, 70)], position=(0, 6, 70))
    entities.add("PlayerModel", [rail.mesh("models/player.glb")], parent=player)
    for side in (-1, 1):
        entities.add(
            "PlayerTrail",
            [rail.trail((0.4, 1, 0.7, 0.9), 0.16, 0.2, 5000)],
            position=(side * 0.42, 0.05, 0.95),
            parent=player,
        )
    entities.add(
        "PlayerCamera",
        [rail.perspective(0.9, True, True), "SoundListener: {primary: true}\n"],
        position=(0, 2.4, 9.5),
        rotation=(-0.12, 0, 0),
        parent=player,
    )
    for index, side in enumerate((-1, 1), start=1):
        weights = {"formation": 1, "separation": 1, "avoidance": 1}
        root = entities.add(
            f"Wing{index}",
            [
                steering(60, weights, player, (side * 7.0, 1.0, 6.0)),
                tree("scripts/game/wingman.owlbt"),
                rail.hud_marker("textures/hud/target.png", 26, (0.5, 0.8, 1, 1), f"WING {index}", False),
            ],
            position=(side * 7.0, 7, 76),
        )
        entities.add(
            f"Wing{index}Model",
            [rail.mesh("models/player.glb", (0.7, 0.85, 1, 1))],
            rotation=(0, math.pi, 0),
            parent=root,
        )
    gate = entities.add(
        "Gate",
        [
            rail.mesh("models/gate.glb", glow=4000),
            rail.hud_marker("textures/hud/target.png", 48, (0.8, 0.6, 1, 1), "JUMP GATE", True),
        ],
        position=ROUTE[-1],
    )
    entities.add("LeadMark", [rail.hud_marker("textures/hud/lead.png", 30, (0.8, 0.6, 1, 1), enabled=False)])
    entities.add(
        "GateLight",
        [rail.FLASH.replace("enabled: false", "enabled: true").replace("intensity: 0", "intensity: 60000")],
        parent=gate,
    )
    rocks(entities)
    for index in range(1, 7):
        entities.add(f"Burst{index}", [rail.FIREBALL])
    for index in range(1, 5):
        entities.add(f"Spark{index}", [SPARKS])
    entities.add("CombatMusic", [y.music("sounds/music/combat.wav", 0.45)])
    script = y.script("scripts/game/mission.owlvs", [("waves", "string", "tables/waves.owltable")])
    return y.scene("mission", entities, "huds/play.owlhud", script.replace("LuaScript:", "SceneScript:") + HEADER)


# ----------------------------------------------------------------------------------------------------------- waves

WAVES = [
    ("w1", 40, 3, "fighter", 170, "convoy"),
    ("w2", 220, 3, "fighter", 160, "convoy"),
    ("w3", 220, 2, "fighter", 120, "player"),
    ("w4", 420, 4, "fighter", 170, "convoy"),
    ("w5", 420, 2, "interceptor", 150, "player"),
    ("w6", 640, 4, "interceptor", 170, "convoy"),
    ("w7", 640, 2, "fighter", 140, "player"),
]
"""The waves: key, distance of the convoy, count, kind, how far ahead of the convoy, whom they go for."""


def wave_table() -> str:
    """The waves, one row each."""
    rows = "".join(
        f"  - key: {key}\n    distance: {distance}\n    count: {count}\n    kind: {kind}\n    ahead: {ahead}\n"
        f"    target: {target}\n"
        for key, distance, count, kind, ahead, target in WAVES
    )
    return (
        "DataTable: ''\nFormatVersion: 1\ncolumns:\n  - {name: distance, type: Float}\n"
        "  - {name: count, type: Integer}\n"
        "  - {name: kind, type: Enum, choices: [fighter, interceptor]}\n  - {name: ahead, type: Float}\n"
        "  - {name: target, type: Enum, choices: [convoy, player]}\n"
        f"rows:\n{rows}"
    )


# --------------------------------------------------------------------------------------------------------- prefabs


def prefab(name: str, entities: y.Entities) -> str:
    """A prefab file of the entities added."""
    return f"Prefab: {name}\nVersion: 1\nEntities:\n" + entities.text()


def prefabs() -> dict[str, str]:
    """The enemies and the lasers, by file name."""
    out: dict[str, str] = {}
    for index, (kind, colour) in enumerate((("fighter", (1, 0.4, 0.35, 1)), ("interceptor", (1, 0.65, 0.25, 1)))):
        entities = y.Entities(7690000000000000000 + index * 100)
        root = entities.add(
            "Enemy",
            [
                steering(30, {"pursue": 1, "separation": 1, "avoidance": 1}),
                tree("scripts/game/enemy.owlbt"),
                rail.hud_marker("textures/hud/target.png", 30, colour, "", True),
            ],
        )
        entities.add("EnemyModel", [rail.mesh(f"models/{kind}.glb")], rotation=(0, math.pi, 0), parent=root)
        entities.add(
            "EnemyTrail", [rail.trail((1, 0.5, 0.3, 0.9), 0.2, 0.25, 4000)], position=(0, 0, -1.2), parent=root
        )
        entities.add("EnemyEngine", [spatial_loop("sounds/game/engine_hum.wav", 0.7, 90)], parent=root)
        out[f"{kind}.owlprefab"] = prefab(kind.capitalize(), entities)
    lasers: list[tuple[str, str, y.Colour]] = [
        ("bolt", "models/bolt.glb", (0.5, 1.0, 0.7, 1.0)),
        ("enemy_bolt", "models/enemy_bolt.glb", (1.0, 0.5, 0.2, 1.0)),
    ]
    for index, (name, model, glow) in enumerate(lasers):
        entities = y.Entities(7691000000000000000 + index * 100)
        sound = spatial_loop("sounds/game/bolt_hiss.wav", 0.6, 40)
        entities.add("Bolt", [rail.mesh(model, glow=30000), rail.trail(glow, 0.2, 0.1, 12000), sound])
        out[f"{name}.owlprefab"] = prefab(name, entities)
    return out


# --------------------------------------------------------------------------------------------------- behaviour trees


class TreeWriter:
    """A behaviour tree being written: nodes, links and the blackboard."""

    def __init__(self, base: int) -> None:
        self.next_id = base
        self.nodes: list[str] = []
        self.links: list[str] = []
        self.board: list[str] = []
        self.row = 0

    def key(self, name: str, kind: str, value: str, description: str) -> None:
        """A key of the blackboard."""
        self.board += [
            f"    - name: {name}",
            f"      type: {kind}",
            f"      value: {value}",
            f"      description: {y.quote(description)}",
        ]

    def node(self, kind: str, parent: int | None, depth: int, comment: str = "", **values: object) -> int:
        """A node under its parent, placed by depth and order."""
        self.next_id += 1
        self.row += 1
        lines = [
            f"    - id: {self.next_id}",
            f"      type: {kind}",
            f"      position: [{depth * 220}, {self.row * 96}]",
        ]
        if values:
            lines.append("      values:")
            lines += [f"        {k}: {v}" for k, v in sorted(values.items())]
        if comment:
            lines.append(f"      comment: {y.quote(comment)}")
        self.nodes += lines
        if parent is not None:
            self.links.append(f"    - {{parent: {parent}, child: {self.next_id}}}")
        return self.next_id

    def text(self) -> str:
        """The ``.owlbt`` file."""
        return (
            "\n".join(
                [
                    "FormatVersion: 1",
                    "BehaviourTree:",
                    "  blackboard:",
                    *self.board,
                    "  nodes:",
                    *self.nodes,
                    "  links:",
                    *self.links,
                ]
            )
            + "\n"
        )


def enemy_tree() -> str:
    """An enemy: flee its target for a while when hurt, fire at it when close, else chase it."""
    t = TreeWriter(3960000000000000000)
    t.key("target", "Entity", "0", "What the fighter goes for, set at its spawn.")
    t.key("hurt", "Bool", "false", "Set when its hull is low: it breaks off.")
    root = t.node("root", None, 0)
    choose = t.node(
        "composite.selector", root, 1, "The first branch that holds wins, checked at every tick.", reactive="true"
    )
    flee = t.node("composite.sequence", choose, 2, "Hurt: break off for a while.")
    t.node("condition.compare", flee, 3, key="hurt", op="'=='", value="true")
    t.node("action.steer", flee, 3, behaviour="flee", key="target", weight=1)
    t.node("action.wait", flee, 3, seconds=2.5)
    t.node("action.set", flee, 3, key="hurt", value="false")
    attack = t.node("composite.sequence", choose, 2, "Close: chase and fire.")
    t.node("condition.distance", attack, 3, key="target", op="closer than", distance=70, space="3D")
    t.node("action.steer", attack, 3, behaviour="pursue", key="target", weight=1)
    cooldown = t.node("decorator.cooldown", attack, 3, seconds=1.1)
    t.node("action.lua", cooldown, 4, "Exported by mission.owlvs.", function="enemy_fire")
    chase = t.node("composite.sequence", choose, 2, "Far: chase.")
    t.node("action.steer", chase, 3, behaviour="pursue", key="target", weight=1)
    t.node("action.wait", chase, 3, seconds=0.4)
    return t.text()


def wingman_tree() -> str:
    """A wingman: by the order of the player, go for the enemies and fire, or keep its slot in the formation."""
    t = TreeWriter(3961000000000000000)
    t.key("order", "String", "escort", "The order of the player: attack or escort.")
    t.key("target", "Entity", "0", "The enemy it goes for.")
    root = t.node("root", None, 0)
    choose = t.node("composite.selector", root, 1, "The order decides, checked at every tick.", reactive="true")
    attack = t.node("composite.sequence", choose, 2, "Attack: the nearest enemy.")
    t.node("condition.compare", attack, 3, key="order", op="'=='", value="attack")
    t.node("action.lua", attack, 3, "Exported by mission.owlvs.", function="wingman_pick")
    t.node("action.steer", attack, 3, behaviour="pursue", key="target", weight=1)
    cooldown = t.node("decorator.cooldown", attack, 3, seconds=0.6)
    t.node("action.lua", cooldown, 4, function="wingman_fire")
    escort = t.node("composite.sequence", choose, 2, "Escort: back in the slot on the player.")
    t.node("action.lua", escort, 3, function="wingman_escort")
    t.node("action.wait", escort, 3, seconds=0.5)
    return t.text()


# ------------------------------------------------------------------------------------------------------------- HUD


def _label(value: str, size: int, colour: y.Colour = TEXT, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.0, 0.08, 0.06, 1), outline_width=2)


LOOK = mk.Look(
    prefix="field",
    title_font=FONT,
    mono_font=MONO,
    text=TEXT,
    soft=SOFT,
    accent=ACCENT,
    danger=(1, 0.4, 0.4, 1),
    button=(0.02, 0.1, 0.08),
    button_hover=(0.05, 0.22, 0.17, 1),
    button_pressed=(0.08, 0.3, 0.22, 1),
    window_body=(0.01, 0.06, 0.05, 0.96),
    window_title=(0.05, 0.2, 0.16, 1),
    outline=(0.0, 0.08, 0.06, 1),
    transition="wipe_left",
    sweep=(0.02, 0.02, 0.08, 1),
    header="",
    backdrop=lambda _e, _n: None,
    title_art="textures/ui/title.png",
    frame="textures/ui/frame.png",
    toggles=[
        ("field_easy", "STURDY CONVOY"),
        ("field_no_assist", "START WITHOUT ASSIST"),
        ("field_invert_y", "INVERT PITCH"),
    ],
    first_scene="scenes/mission.owl",
    # A cockpit display on the right of the screen, the field in view on the left: notched frames, italic capitals.
    layout=mk.Layout(
        title=((1, 0), (1.1, 0.275), (-0.05, 0.05)),
        best=((1, 0), (0.8, 0.05), (-0.08, 0.33)),
        best_align="Right",
        buttons=((1, 0), (0.5, 0.46), (-0.08, 0.42)),
        button_width=1.0,
        button_spacing=8,
        label_size=26,
        result_align="Right",
        focus_thickness=2,
        result=((1, 0), (1.1, 0.92), (-0.06, 0.04)),
        settings=((1, 0), (1.28, 0.94), (-0.02, 0.03)),
    ),
)
"""The look of the game, the backdrop set by ``generate_menus``."""


def bar(value: float, fill: y.Colour) -> str:
    """A ``UiProgressBar``."""
    return (
        f"UiProgressBar:\n  value: {y.num(value)}\n  backgroundColor: [0.02, 0.08, 0.06, 0.8]\n"
        f"  fillColor: {y.vec(*fill)}\n"
    )


def hud() -> str:
    """Hull and score at the top left, the convoy at the top right, the wave and the order of the wings, the route,
    the aim, a message line and the pause window (the only place of the mission showing the controls)."""
    entities = y.Entities(7699000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    left = entities.add(
        "TopLeft",
        [y.rect((0, 0), (0.5, 0.18), (0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 2, cross="Start")],
        parent=root,
    )
    entities.add("OrderText", [y.child_rect((1, 0.15)), _label("WINGS: ESCORT", 18, ACCENT, "Left", MONO)], parent=left)
    entities.add("ScoreText", [y.child_rect((1, 0.17)), _label("SCORE 0", 22, TEXT, "Left", MONO)], parent=left)
    entities.add("HullText", [y.child_rect((1, 0.17)), _label("HULL 100", 22, TEXT, "Left", MONO)], parent=left)
    entities.add("HullBar", [y.child_rect((1, 0.08)), bar(1, (0.45, 1, 0.65, 1))], parent=left)
    throttle = _label("THROTTLE 0%", 18, (0.6, 0.9, 1, 1), "Left", MONO)
    entities.add("ThrottleText", [y.child_rect((1, 0.15)), throttle], parent=left)
    entities.add("ThrottleBar", [y.child_rect((1, 0.08)), bar(0, (0.5, 0.85, 1, 1))], parent=left)
    right = entities.add(
        "TopRight",
        [y.rect((1, 0), (0.42, 0.18), (-0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 3, cross="End")],
        parent=root,
    )
    entities.add("WaveText", [y.child_rect((1, 0.2)), _label("WAVE 0", 18, SOFT, "Right", MONO)], parent=right)
    for index in reversed(range(1, 4)):
        entities.add(f"FreighterBar{index}", [y.child_rect((1, 0.1)), bar(1, (0.5, 0.9, 1, 1))], parent=right)
    entities.add("ConvoyText", [y.child_rect((1, 0.22)), _label("CONVOY 3", 22, TEXT, "Right", MONO)], parent=right)
    entities.add(
        "RouteBar",
        [y.rect((0.5, 1), (0.8, 0.012), (0, -0.03), SCREEN, SCREEN), bar(0, (0.8, 0.6, 1, 0.8))],
        parent=root,
    )
    aim = entities.add("Aim", [y.rect((0.5, 0.5), (0.06, 0.06)), y.image("textures/hud/aim.png")], parent=root)
    entities.add(
        "AimCircle",
        [
            y.rect((0.5, 0.5), (0.07, 0.07), offset_units=("ScreenHeight", "ScreenHeight")),
            y.image("textures/hud/aim_circle.png", (1, 1, 1, 0.9)),
        ],
        parent=aim,
    )
    entities.add(
        "ObjectiveText",
        [
            y.rect((0.5, 0), (1.0, 0.04), (0, 0.205), SCREEN, SCREEN),
            _label("ESCORT THE CONVOY TO THE GATE", 20, ACCENT),
        ],
        parent=root,
    )
    briefing = entities.add(
        "Briefing",
        [
            y.rect((0.5, 0.5), (1.2, 0.24), (0, 0.18), SCREEN, SCREEN),
            y.panel((0.01, 0.06, 0.05, 0.6), (0.45, 1, 0.65, 0.9), 2),
            y.layout("Column", 2, 6),
        ],
        parent=root,
    )
    # A layout lays its children out from the last added: the text first, the title over it.
    briefing_text = y.text(BRIEFING, 20, "Center", TEXT, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("BriefingText", [y.child_rect((0.96, 0.55)), briefing_text], parent=briefing)
    entities.add("BriefingTitle", [y.child_rect((0.96, 0.24)), _label("MISSION: ESCORT", 28, ACCENT)], parent=briefing)
    entities.add(
        "Message",
        [y.rect((0.5, 0.5), (1.2, 0.08), (0, -0.21), SCREEN, SCREEN), _label("", 36, ACCENT)],
        parent=root,
        visible=False,
    )
    pause = mk.window(LOOK, entities, root, "PauseWindow", "PAUSED", (1.25, 0.88))
    for index, (tag, text, callback) in reversed(
        list(
            enumerate(
                [
                    ("ResumeButton", "RESUME", "on_resume_clicked"),
                    ("RestartButton", "RESTART", "on_restart_clicked"),
                    ("QuitButton", "QUIT TO TITLE", "on_quit_clicked"),
                ]
            )
        )
    ):
        mk.button(LOOK, entities, pause, tag, text, callback, (0.6, 0.09), index == 0)
    controls = y.text(CONTROLS, 18, "Left", ACCENT, wrap=True, font=MONO, line_spacing=0.2)
    entities.add("PauseControls", [y.child_rect((0.95, 0.52)), controls], parent=pause)
    return y.hud_file("play", entities)


# ---------------------------------------------------------------------------------------------------------- graphs

MODULE = "scripts.game.combat"
"""The Lua module of the fight."""


def mission_graph() -> str:
    """The rules of the mission: the fight, the orders, the convoy to the gate or lost, pause."""
    g = Graph(6600000000000000000)
    g.var("waves", "string", "tables/waves.owltable")
    g.var("this_scene", "string", "scenes/mission.owl")
    g.var("freighter_bonus", "int", 1000, low=0)
    for name, kind, default in (
        ("state", "string", "fly"),
        ("clock", "float", 0.0),
        ("paused", "bool", False),
        ("leaving", "bool", False),
        ("message_time", "float", 0.0),
        ("brief_time", "float", 8.0),
        ("seen_wave", "int", 0),
    ):
        g.var(name, kind, default, exposed=False)

    def find(tag: object) -> Pin:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    def widget(tag: str) -> Pin:
        return g.call("ui.find", name=tag)["entity_id"]

    def pressed(action: str) -> Pin:
        return g.call("input.is_action_just_pressed", action=action)["pressed"]

    def call(function: str, **values: object) -> Node:
        return g.node("func.call", function=function, **values)

    # ---------------------------------------------------------------- on_create
    create = g.event("on_create")
    branch = g.branch(g.op("logic.not", g.call("ui.is_transition_active")["active"]))
    g.chain(create, branch)
    fade_in = g.call("ui.transition_play", kind="wipe_left", duration=0.6, rgba=LOOK.sweep)
    g.chain(branch["True"], fade_in)
    easy = g.call("settings.get", key="field_easy", default=False)["value"]
    setup = g.join([branch["False"], fade_in], g.lua(MODULE, "setup", waves=g.get("waves"), sturdy=easy))
    no_assist = g.call("settings.get", key="field_no_assist", default=False)["value"]
    g.chain(
        setup,
        g.call("entity.set_ship_assist", entity_id=find("Player"), assist=g.op("logic.not", no_assist)),
        g.call(
            "entity.set_ship_invert_pitch",
            entity_id=find("Player"),
            invert=g.call("settings.get", key="field_invert_y", default=False)["value"],
        ),
        g.call("gamestate.set", key="field_score", value=0),
        g.call("gamestate.set", key="field_base", value=0),
        g.call("gamestate.set", key="field_retry", value=g.get("this_scene")),
        g.call("gamestate.set", key="field_state", value="fly"),
        g.call("gamestate.set", key="field_paused", value=False),
        g.call("gamestate.set", key="field_freighters", value=setup["freighters"]),
        call("say", text="ESCORT THE CONVOY TO THE JUMP GATE"),
    )

    # ---------------------------------------------------------------- on_tick: the fight
    tick = g.event("on_tick")
    flying = g.branch(
        g.op("logic.and", g.op("compare.equal", g.get("state"), "fly"), g.op("logic.not", g.get("paused")))
    )
    g.chain(
        tick,
        flying,
        g.lua(MODULE, "tick", fire=g.call("input.is_action_pressed", action="fire")["pressed"], dt=tick["dt"]),
    )

    # ---------------------------------------------------------------- on_update
    update = g.event("on_update")
    toggle = call("set_paused", paused=g.op("logic.not", g.get("paused")))
    pause_branch = g.branch(g.op("logic.and", pressed("pause"), g.op("compare.equal", g.get("state"), "fly")))
    g.chain(update, pause_branch, toggle)
    running = g.join([pause_branch["False"], toggle], g.branch(g.get("paused")))
    counters = g.lua(MODULE, "counters", pure=True)
    hull = g.lua(MODULE, "hull", pure=True)["hull"]
    nodes = [
        g.set("message_time", g.op("math.subtract", g.get("message_time"), update["dt"])),
        g.set(
            "brief_time",
            g.node(
                "logic.select",
                Condition=pressed("fire"),
                **{"True": 0.0, "False": g.op("math.subtract", g.get("brief_time"), update["dt"])},
            )["Result"],
        ),
        g.call("ui.set_visible", entity_id=widget("Briefing"), visible=g.op("compare.greater", g.get("brief_time"), 0)),
        g.call(
            "ui.set_text",
            entity_id=widget("ObjectiveText"),
            text=g.fmt("ESCORT THE CONVOY TO THE GATE   %d M", g.lua(MODULE, "gate_distance", pure=True)["metres"]),
        ),
        g.call("gamestate.set", key="field_briefing", value=g.op("compare.greater", g.get("brief_time"), 0)),
        g.call(
            "ui.set_text", entity_id=widget("HullText"), text=g.fmt("HULL %d", g.node("math.ceil", A=hull)["Result"])
        ),
        g.call(
            "ui.set_progress",
            entity_id=widget("HullBar"),
            value=g.op("math.divide", g.lua(MODULE, "hull", pure=True)["hull"], 100),
        ),
        g.call(
            "ui.set_text",
            entity_id=widget("ScoreText"),
            text=g.fmt("SCORE %d", g.call("gamestate.get", key="field_score", default=0)["value"]),
        ),
        g.call(
            "ui.set_text",
            entity_id=widget("WaveText"),
            text=g.fmt("WAVE %d   KILLS %d", counters["wave"], counters["kills"]),
        ),
        g.call(
            "ui.set_text",
            entity_id=widget("ConvoyText"),
            text=g.fmt("CONVOY %d", g.lua(MODULE, "freighters_alive", pure=True)["alive"]),
        ),
        g.call("ui.set_progress", entity_id=widget("RouteBar"), value=g.lua(MODULE, "progress", pure=True)["progress"]),
        g.call("gamestate.set", key="field_hull", value=g.lua(MODULE, "hull", pure=True)["hull"]),
        g.call("gamestate.set", key="field_kills", value=counters["kills"]),
        g.call("gamestate.set", key="field_enemies", value=counters["enemies"]),
        g.call("gamestate.set", key="field_wave", value=counters["wave"]),
        g.call("gamestate.set", key="field_shots", value=counters["shots"]),
        g.call("gamestate.set", key="field_enemy_shots", value=counters["enemy_shots"]),
        g.call("gamestate.set", key="field_convoy_hull", value=counters["convoy_hull"]),
        g.call("gamestate.set", key="field_convoy", value=g.lua(MODULE, "freighters_alive", pure=True)["alive"]),
        g.call("gamestate.set", key="field_progress", value=g.lua(MODULE, "progress", pure=True)["progress"]),
        g.call(
            "gamestate.set", key="field_speed", value=g.call("entity.get_ship_speed", entity_id=find("Player"))["speed"]
        ),
    ]
    g.chain(running["False"], *nodes)
    last: Node = nodes[-1]
    loop = g.node("flow.for", First=1, Last=3)
    g.chain(last, loop)
    g.chain(
        loop["Body"],
        g.call(
            "ui.set_progress",
            entity_id=g.call("ui.find", name=g.op("text.concat", "FreighterBar", loop["Index"]))["entity_id"],
            value=g.lua(MODULE, "freighter_hull", pure=True, index=loop["Index"])["hull"],
        ),
    )
    rules = g.sequence()
    g.chain(loop["Completed"], rules)
    expired = g.branch(g.op("compare.less", g.get("message_time"), 0))
    g.chain(rules["Then 1"], expired, g.call("ui.set_visible", entity_id=widget("Message"), visible=False))
    flying = g.branch(g.op("compare.equal", g.get("state"), "fly"))
    g.chain(rules["Then 2"], flying, call("fly_update"))
    over = g.branch(
        g.op("logic.and", g.op("compare.not_equal", g.get("state"), "fly"), g.op("logic.not", g.get("leaving")))
    )
    g.chain(rules["Then 3"], over, call("end_update", dt=update["dt"]))

    # fly_update(): orders, the lock, a new wave, the end of the mission.
    g.function("fly_update")
    entry = g.entry("fly_update")
    attack = g.branch(pressed("wings_attack"))
    g.chain(entry, attack)
    attacked = g.chain(attack["True"], call("order", kind="attack"))
    escort = g.join([attack["False"], attacked], g.branch(pressed("wings_escort")))
    escorted = g.chain(escort["True"], call("order", kind="escort"))
    lock = g.join([escort["False"], escorted], g.branch(pressed("lock")))
    locked = g.lua(MODULE, "lock")
    g.chain(lock["True"], locked)
    heard = g.branch(locked["locked"])
    g.chain(locked, g.call("gamestate.set", key="field_locked", value=locked["locked"]), heard)
    beep = g.chain(heard["True"], g.call("sound.play", asset_path="sounds/game/lock.wav"))
    wave = g.lua(MODULE, "counters", pure=True)["wave"]
    new_wave = g.join(
        [lock["False"], heard["False"], beep], g.branch(g.op("compare.greater", wave, g.get("seen_wave")))
    )
    warned = g.chain(
        new_wave["True"],
        g.set("seen_wave", g.lua(MODULE, "counters", pure=True)["wave"]),
        call("say", text="ENEMY FIGHTERS INBOUND"),
    )
    dead = g.join(
        [new_wave["False"], warned], g.branch(g.op("compare.less_equal", g.lua(MODULE, "hull", pure=True)["hull"], 0))
    )
    lost_ship = g.chain(dead["True"], call("finish", won=False, reason="YOUR FIGHTER IS LOST"))
    convoy = g.branch(g.op("compare.equal", g.lua(MODULE, "freighters_alive", pure=True)["alive"], 0))
    g.chain(dead["False"], convoy)
    lost_convoy = g.chain(convoy["True"], call("finish", won=False, reason="THE CONVOY IS LOST"))
    arrived = g.branch(g.op("compare.greater_equal", g.lua(MODULE, "progress", pure=True)["progress"], 0.995))
    g.chain(convoy["False"], arrived)
    jumped = g.chain(arrived["True"], call("finish", won=True, reason="THE CONVOY JUMPED"))
    g.join([lost_ship, lost_convoy, jumped, arrived["False"]], g.node("func.return", function="fly_update"))

    # order(kind): the wingmen.
    g.function("order", [("kind", "string")])
    entry = g.entry("order")
    given = g.lua(MODULE, "order", kind=entry["kind"])
    g.chain(
        entry,
        given,
        g.call(
            "ui.set_text",
            entity_id=widget("OrderText"),
            text=g.fmt("WINGS: %s", g.node("text.upper", Text=given["order"])["Result"]),
        ),
        g.call("gamestate.set", key="field_order", value=given["order"]),
        g.call("sound.play", asset_path="sounds/game/order.wav"),
        g.node("func.return", function="order"),
    )

    # finish(won, reason): the score and the tally.
    g.function("finish", [("won", "boolean"), ("reason", "string")])
    entry = g.entry("finish")
    counters = g.lua(MODULE, "counters", pure=True)
    alive = g.lua(MODULE, "freighters_alive", pure=True)["alive"]
    bonus = g.node(
        "logic.select",
        Condition=entry["won"],
        **{"True": g.op("math.multiply", alive, g.get("freighter_bonus")), "False": 0},
    )["Result"]
    hull_bonus = g.node(
        "logic.select",
        Condition=entry["won"],
        **{
            "True": g.op(
                "math.multiply", g.node("math.ceil", A=g.lua(MODULE, "hull", pure=True)["hull"])["Result"], 10
            ),
            "False": 0,
        },
    )["Result"]
    score = g.op(
        "math.add", g.call("gamestate.get", key="field_score", default=0)["value"], g.op("math.add", bonus, hull_bonus)
    )
    accuracy = g.node(
        "math.floor",
        A=g.op(
            "math.divide",
            g.op("math.multiply", g.lua(MODULE, "counters", pure=True)["hits"], 100),
            g.node("math.max", A=1, B=g.lua(MODULE, "counters", pure=True)["shots"])["Result"],
        ),
    )["Result"]
    g.chain(
        entry,
        g.set("state", g.node("logic.select", Condition=entry["won"], **{"True": "won", "False": "lost"})["Result"]),
        g.set("clock", 0),
        g.call("gamestate.set", key="field_state", value=g.get("state")),
        g.call("gamestate.set", key="field_line_1", value=entry["reason"]),
        g.call("gamestate.set", key="field_line_2", value=g.fmt("ENEMIES DOWN     %d", counters["kills"])),
        g.call(
            "gamestate.set",
            key="field_line_3",
            value=g.fmt(
                "FREIGHTERS SAVED %d / 3",
                g.node("logic.select", Condition=entry["won"], **{"True": alive, "False": 0})["Result"],
            ),
        ),
        g.call("gamestate.set", key="field_line_4", value=g.fmt("HIT RATIO        %d%%", accuracy)),
        g.call("gamestate.set", key="field_line_5", value=g.fmt("CONVOY BONUS     %d", bonus)),
        g.call("gamestate.set", key="field_line_6", value=g.fmt("HULL BONUS       %d", hull_bonus)),
        g.call("gamestate.set", key="field_score", value=score),
        g.call(
            "sound.play",
            asset_path=g.node(
                "logic.select",
                Condition=entry["won"],
                **{"True": "sounds/game/jump.wav", "False": "sounds/game/explosion.wav"},
            )["Result"],
        ),
        call("say", text=entry["reason"]),
        g.node("func.return", function="finish"),
    )

    # end_update(dt): a few seconds after the end, the tally.
    g.function("end_update", [("dt", "number")])
    entry = g.entry("end_update")
    done = g.branch(g.op("compare.greater", g.get("clock"), 3.5))
    g.chain(entry, g.set("clock", g.op("math.add", g.get("clock"), entry["dt"])), done)
    target = g.node(
        "logic.select",
        Condition=g.op("compare.equal", g.get("state"), "won"),
        **{"True": "scenes/victory.owl", "False": "scenes/game_over.owl"},
    )["Result"]
    g.chain(
        done["True"],
        g.set("leaving", True),
        g.call("scene.transition_to", scene_path=target, kind="fade", duration=0.8, rgba=LOOK.sweep),
    )

    # say(text): the message line, for three seconds.
    g.function("say", [("text", "string")])
    entry = g.entry("say")
    g.chain(
        entry,
        g.call("ui.set_text", entity_id=widget("Message"), text=entry["text"]),
        g.call("ui.set_visible", entity_id=widget("Message"), visible=True),
        g.set("message_time", 3.0),
        g.call("gamestate.set", key="field_message", value=entry["text"]),
        g.node("func.return", function="say"),
    )

    # set_paused(paused).
    g.function("set_paused", [("paused", "boolean")])
    entry = g.entry("set_paused")
    show = g.branch(entry["paused"])
    g.chain(
        entry,
        g.set("paused", entry["paused"]),
        g.call("time.set_paused", paused=entry["paused"]),
        g.call("ui.set_visible", entity_id=widget("PauseWindow"), visible=entry["paused"]),
        g.call("gamestate.set", key="field_paused", value=entry["paused"]),
        show,
    )
    g.chain(
        show["True"],
        g.call("ui.raise_window", entity_id=widget("PauseWindow")),
        g.call("ui.set_focus", entity_id=widget("ResumeButton")),
    )

    # The leaves of the behaviour trees: global functions the agents call with (entity, dt).
    for name, function in (
        ("enemy_fire", "enemy_fire"),
        ("wingman_pick", "wingman_pick"),
        ("wingman_fire", "wingman_fire"),
        ("wingman_escort", "wingman_escort"),
    ):
        g.function(
            name,
            [("ship", "entity"), ("dt", "number")],
            [("ok", "boolean")],
            exported=True,
            category="Behaviour tree leaves",
        )
        entry = g.entry(name)
        result = g.lua(MODULE, function, ship=entry["ship"])
        g.chain(entry, result, g.node("func.return", function=name, ok=_first_output(result)))

    resume = g.event("on_resume_clicked")
    g.chain(resume, call("set_paused", paused=False))
    restart = g.event("on_restart_clicked")
    g.chain(
        restart,
        g.call("time.set_paused", paused=False),
        g.call("scene.transition_to", scene_path=g.get("this_scene"), kind="fade", duration=0.4, rgba=LOOK.sweep),
    )
    quit_ = g.event("on_quit_clicked")
    g.chain(
        quit_,
        g.call("time.set_paused", paused=False),
        g.call("scene.transition_to", scene_path="scenes/title.owl", kind="fade", duration=0.4, rgba=LOOK.sweep),
    )
    return g.text()


_LEAF_OUTPUTS = {"enemy_fire": "fired", "wingman_pick": "found", "wingman_fire": "fired", "wingman_escort": "done"}
"""The result of each leaf function of the module."""


def _first_output(node: Node) -> Pin:
    return node[_LEAF_OUTPUTS[node.values["function"]]]


# --------------------------------------------------------------------------------------------------------- project

PROJECT = """OwlProject:
  name: OwlAsteroidField
  firstScene: scenes/title.owl
  author: Silmaen
  description: Asteroid Field, an escort sample game of the Owl Engine
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
        SunDirection: [-0.42, -0.48, -0.77]
        Ambient: [0.05, 0.06, 0.08]
    - Type: RendererPlanet
      Name: planet_world
      DefaultConfig:
        SunDirection: [-0.42, -0.48, -0.77]
        Ambient: [0.03, 0.035, 0.05]
    - Type: RendererParticles
      Name: particles
    - Type: Renderer2D
      Name: ui
    - Type: Renderer2D
      Name: overlay
"""
"""The project."""

SETTINGS = """GameSettings:
  - {key: resolution_width, type: int, value: 1280}
  - {key: resolution_height, type: int, value: 720}
  - {key: fullscreen, type: bool, value: false}
  - {key: resizable, type: bool, value: true}
  - {key: volume_master, type: float, value: 1.0}
  - {key: volume_music, type: float, value: 0.8}
  - {key: volume_sfx, type: float, value: 1.0}
  - {key: ui_scale, type: float, value: 1.0}
  - {key: color_blind_mode, type: string, value: none}
  - {key: reduced_motion, type: bool, value: false}
  - {key: subtitles, type: bool, value: true}
  - {key: field_easy, type: bool, value: false}
  - {key: field_invert_y, type: bool, value: false}
  - {key: field_no_assist, type: bool, value: false}
  - {key: field_best, type: int, value: 0}
"""
"""The settings and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of Asteroid Field, on top of the built-in ones (the `ui_*` actions drive the menus); the `ship_*` actions
# and `look` fly the player's ShipController (the keys and the sticks turn it, `ship_thrust` moves its persistent
# throttle; the mouse moves the aim circle, `steer` held turns the fighter toward it, the wheel moves the lock). Read by
# scripts/game/mission.owlvs and scripts/game/combat.lua.
actions:
  - name: look
    type: vector
    bindings:
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_stick: left, dead_zone: 0.2}
  - name: ship_thrust
    type: axis
    bindings:
      - {key: W}
      - {key: S, scale: -1}
      - {gamepad_axis: right_trigger}
      - {gamepad_axis: left_trigger, scale: -1}
  - name: ship_roll
    type: axis
    bindings:
      - {key: E}
      - {key: Q, scale: -1}
      - {gamepad_button: RightBumper}
      - {gamepad_button: LeftBumper, scale: -1}
  - name: ship_strafe
    type: vector
    bindings:
      - {key: C, component: x}
      - {key: X, component: x, scale: -1}
      - {key: R, component: y}
      - {key: V, component: y, scale: -1}
      - {gamepad_stick: right, dead_zone: 0.2}
  - name: throttle_zero
    type: button
    bindings:
      - {key: Backspace}
      - {gamepad_button: LeftThumb}
  - name: steer
    type: button
    bindings:
      - {mouse_button: Right}
  - name: ship_assist
    type: button
    bindings:
      - {key: Z}
      - {gamepad_button: RightThumb}
  - name: fire
    type: button
    bindings:
      - {key: Space}
      - {mouse_button: Left}
      - {gamepad_button: A}
  - name: lock
    type: button
    bindings:
      - {key: F}
      - {mouse_button: Middle}
      - {gamepad_button: X}
  - name: wings_attack
    type: button
    bindings:
      - {key: D1}
      - {gamepad_button: Y}
  - name: wings_escort
    type: button
    bindings:
      - {key: D2}
      - {gamepad_button: B}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input actions."""


def main() -> None:
    """Write the project, the mission, the prefabs, the trees, the table, the HUD and the graph."""
    for folder in ("scenes", "huds", "prefabs", "tables", "scripts/game"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    (ROOT / "scenes" / "mission.owl").write_text(mission_scene())
    for name, text in prefabs().items():
        (ROOT / "prefabs" / name).write_text(text)
    (ROOT / "tables" / "waves.owltable").write_text(wave_table())
    (ROOT / "scripts" / "game" / "enemy.owlbt").write_text(enemy_tree())
    (ROOT / "scripts" / "game" / "wingman.owlbt").write_text(wingman_tree())
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    (ROOT / "scripts" / "game" / "mission.owlvs").write_text(mission_graph())


if __name__ == "__main__":
    main()
