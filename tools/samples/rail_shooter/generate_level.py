#!/usr/bin/env python3
"""
Write the level of the Rail shooter sample game into ``samples/rail_shooter/``: the project files, the level scene
(its rail, its asteroid field, the planet below), the wave table, the prefabs the script spawns, the three sequences
(intro, boss arrival, end) and the play HUD.

- The rail is a Catmull-Rom ``Spline`` of ``RAIL_POINTS`` gentle curves, 3.9 km long; the ``RailCarrier`` rides it
  with a ``SplineFollower`` and carries the camera, the ship, the two reticles (HUD markers) and the sequence
  players: everything the player steers lives in the frame of the carrier.
- The asteroid field is made of ``Asteroid`` entities placed beside the rail from its rail coordinates (distance,
  across, up), in calm stretches and dense belts (``BELTS``) with rocks on the centre line that must be shot or
  dodged; the ``FarRock`` entities are scenery out of reach. Every rock is an entity of the scene: move, add or
  remove them in Owl Nest.
- The planet is a ``Planet`` of the planet layer, its sea 1.1 km below the rail, under a floating origin that keeps the
  camera near the origin of the scene as the carrier flies on.
- ``tables/waves.owltable`` lists the waves: at which distance a group comes, its kind (a row of
  ``tables/enemies.owltable``: prefab, hull, radius, points, reload), its count, its pattern.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.rail_shooter.generate_level``.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass
from pathlib import Path

from tools.samples import scene_yaml as y

ROOT = Path(__file__).resolve().parents[3] / "samples" / "rail_shooter"
"""The sample project."""
MESH = "RendererTag:\n  Name: mesh_world\n"
"""Routes a mesh to the mesh layer."""
Vec3 = tuple[float, float, float]

RAIL_STEP = 200.0
"""Distance between two control points of the rail, along -Z."""
RAIL_POINTS = [
    (120.0 * math.sin(i * 0.55), 35.0 * math.sin(i * 0.37 + 1.0) - 35.0 * math.sin(1.0), -RAIL_STEP * i)
    for i in range(21)
]
"""Control points of the rail."""
RAIL_SPEED = 20.0
"""Speed of the carrier along the rail, in metres per second."""
BOSS_DISTANCE = 3150.0
"""Distance along the rail where the boss arrives."""
PLANET_CENTRE = (1500.0, -31100.0, -2200.0)
"""Centre of the planet, in the world: its sea 1.1 km under the rail, its horizon in the lower part of the view."""
PLANET_RADIUS = 30000.0
"""Sea radius of the planet."""


@dataclass
class Wave:
    """A row of the wave table."""

    key: str
    distance: float
    kind: str
    count: int
    pattern: str
    x: float
    y: float


WAVES = [
    Wave("w01", 260, "fighter", 3, "line", 0, 2),
    Wave("w02", 460, "fighter", 4, "vee", 0, -1),
    Wave("w03", 680, "fighter", 3, "swoop_left", 0, 1),
    Wave("w04", 860, "fighter", 3, "swoop_right", 0, -1),
    Wave("w05", 1120, "interceptor", 2, "hover", 0, 1),
    Wave("w06", 1380, "fighter", 5, "vee", 0, 2),
    Wave("w07", 1620, "fighter", 3, "swoop_left", 0, 2),
    Wave("w08", 1700, "fighter", 3, "swoop_right", 0, -2),
    Wave("w09", 1960, "interceptor", 3, "hover", 0, 0),
    Wave("w10", 2240, "fighter", 4, "line", 0, -2),
    Wave("w11", 2500, "interceptor", 2, "hover", 0, 2),
    Wave("w12", 2760, "fighter", 6, "vee", 0, 0),
    Wave("w13", BOSS_DISTANCE, "boss", 1, "boss", 0, 2),
]
"""The waves of the level, in the order of their distance."""

BELTS = [
    (200.0, 1000.0, 0.05),
    (1000.0, 1300.0, 0.11),
    (1300.0, 2000.0, 0.06),
    (2000.0, 2400.0, 0.12),
    (2400.0, 3050.0, 0.05),
]
"""Stretches of the field: from, to, rocks per metre."""
FIELD_HALF = (12.0, 7.5)
"""Half extent across and up of the playable field around the rail."""


# --------------------------------------------------------------------------------------------------------- the rail


class Rail:
    """The rail sampled like the engine does (uniform Catmull-Rom, the end points doubled), by distance."""

    def __init__(self, points: list[Vec3], per_segment: int = 64) -> None:
        padded = [points[0], *points, points[-1]]
        self.samples: list[Vec3] = []
        for i in range(1, len(padded) - 2):
            p0, p1, p2, p3 = padded[i - 1], padded[i], padded[i + 1], padded[i + 2]
            for k in range(per_segment):
                t = k / per_segment
                self.samples.append(
                    tuple(  # type: ignore[arg-type]
                        0.5
                        * (
                            2 * p1[c]
                            + (-p0[c] + p2[c]) * t
                            + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t * t
                            + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t * t * t
                        )
                        for c in range(3)
                    )
                )
        self.samples.append(points[-1])
        self.lengths = [0.0]
        for a, b in zip(self.samples, self.samples[1:], strict=False):
            self.lengths.append(self.lengths[-1] + math.dist(a, b))

    @property
    def length(self) -> float:
        """Length of the rail."""
        return self.lengths[-1]

    def sample(self, distance: float) -> tuple[Vec3, Vec3]:
        """Position and unit tangent at a distance along the rail."""
        distance = max(0.0, min(self.length, distance))
        low, high = 0, len(self.lengths) - 1
        while high - low > 1:
            mid = (low + high) // 2
            if self.lengths[mid] <= distance:
                low = mid
            else:
                high = mid
        a, b = self.samples[low], self.samples[high]
        span = self.lengths[high] - self.lengths[low] or 1.0
        t = (distance - self.lengths[low]) / span
        point = tuple(a[c] + (b[c] - a[c]) * t for c in range(3))
        tangent = tuple((b[c] - a[c]) / span for c in range(3))
        return point, tangent  # type: ignore[return-value]

    def place(self, distance: float, across: float, up: float) -> Vec3:
        """World position of rail coordinates: the right is the tangent crossed with +Y, the up the right crossed
        with the tangent (the frame of the script)."""
        point, t = self.sample(distance)
        right = (-t[2], 0.0, t[0])
        norm = math.hypot(right[0], right[2]) or 1.0
        right = (right[0] / norm, 0.0, right[2] / norm)
        upward = (
            right[1] * t[2] - right[2] * t[1],
            right[2] * t[0] - right[0] * t[2],
            right[0] * t[1] - right[1] * t[0],
        )
        return tuple(point[c] + right[c] * across + upward[c] * up for c in range(3))  # type: ignore[return-value]


# ------------------------------------------------------------------------------------------------------ components


def mesh(path: str, colour: y.Colour = (1, 1, 1, 1), glow: float = 0.0) -> str:
    """A ``MeshRenderer`` of a model of the project, its first material glowing at ``glow`` nits when above 0."""
    out = f"MeshRenderer:\n  mesh: {path}\n  color: {y.vec(*colour)}\n  transparent: false\n"
    if glow > 0:
        out += f"  materials:\n    - {{overrides: {{emissiveLuminance: {y.num(glow)}}}}}\n"
    return out + MESH


def trail(colour: y.Colour, width: float, lifetime: float, emissive: float) -> str:
    """A ``Trail`` tapering and fading, additive and glowing."""
    return (
        f"Trail:\n  enabled: true\n  lifetime: {y.num(lifetime)}\n  minDistance: 0.15\n  maxPoints: 48\n"
        f"  width: {y.num(width)}\n  color: {y.vec(*colour)}\n  emissive: {y.num(emissive)}\n  blend: Additive\n"
    )


def burst(
    colours: list[tuple[float, float, float, float, float]],
    speed: tuple[float, float],
    size: float,
    emissive: float,
    count: int,
    life: tuple[float, float],
    blend: str = "Additive",
) -> str:
    """A ``ParticleEmitter`` that only bursts when the script asks (``entity.particles_emit``), in world space."""
    gradient = ", ".join(y.vec(*c) for c in colours)
    return (
        f"ParticleEmitter:\n  playOnStart: false\n  maxParticles: {count}\n  rate: 0\n  burst: 0\n  duration: 0\n"
        f"  loop: false\n  lifetimeMin: {y.num(life[0])}\n  lifetimeMax: {y.num(life[1])}\n  shape: Sphere\n"
        f"  shapeSize: [0.4, 0.4, 0.4]\n  speedMin: {y.num(speed[0])}\n  speedMax: {y.num(speed[1])}\n"
        "  gravity: [0, 0, 0]\n  drag: 1.2\n"
        f"  sizeMin: {y.num(size * 0.5)}\n  sizeMax: {y.num(size)}\n"
        "  sizeOverLife: {interpolation: Linear, keys: [[0, 1], [1, 0.2]]}\n  color: [1, 1, 1, 1]\n"
        f"  colorOverLife: [{gradient}]\n  emissive: {y.num(emissive)}\n  blend: {blend}\n  space: World\n"
    )


def hud_marker(
    icon: str, size: float, colour: y.Colour, label: str = "", offscreen: bool = False, enabled: bool = True
) -> str:
    """A ``HudMarker`` drawing an icon over the entity, projected through the mesh layer that draws the ships."""
    return (
        f"HudMarker:\n  enabled: {'true' if enabled else 'false'}\n  icon: nam:{icon}\n  color: {y.vec(*colour)}\n"
        f"  size: [{y.num(size)}, {y.num(size)}]\n  offset: [0, 0, 0]\n  label: {y.quote(label)}\n"
        "  labelColor: [1, 0.85, 0.85, 1]\n  fontSize: 16\n  showDistance: false\n  maxDistance: 0\n"
        f"  offscreenIndicator: {'true' if offscreen else 'false'}\n  edgeMargin: 40\n"
    ) + MESH


def perspective(fov: float, primary: bool, post: bool) -> str:
    """A perspective camera reaching the planet; ``post`` adds the bloom and the vignette of the game."""
    out = (
        "Camera:\n  camera:\n    projectionType: Perspective\n"
        f"    perspectiveFOV: {y.num(fov)}\n    perspectiveNear: 0.1\n    perspectiveFar: 200000\n"
        "    orthographicSize: 8\n    orthographicNear: -1\n    orthographicFar: 10\n    toneMapping: Aces\n"
    )
    if post:
        out += (
            "    postProcess: {ev100: 10, exposure: 0, autoExposure: false, bloom: true, bloomThreshold: 1.4,"
            " bloomKnee: 0.5, bloomIntensity: 0.12, bloomScatter: 0.7, vignette: true, vignetteIntensity: 0.35,"
            " vignetteSmoothness: 0.5, vignetteColor: [0, 0, 0.02]}\n"
        )
    return out + f"  primary: {'true' if primary else 'false'}\n  fixedAspectRatio: false\n"


def sequence_player(path: str, camera: int, on_start: bool, finished: str) -> str:
    """A ``SequencePlayer`` driving a camera of the carrier."""
    return (
        f"SequencePlayer:\n  sequence: {path}\n  camera: {camera}\n  playOnStart: {'true' if on_start else 'false'}\n"
        f"  loop: false\n  speed: 1\n  onFinished: {finished}\n"
    )


def sky() -> str:
    """The starfield lighting the scene, shown behind it."""
    return (
        "Environment: {hdri: textures/sky/starfield.hdr, ambient: [0.1, 0.1, 0.12], intensity: 14000, "
        "rotation: 0.6, showSky: true}\n"
    )


SUN = "Light: {type: Directional, enabled: true, color: [1, 0.96, 0.9], intensity: 3200, castShadows: false}\n"
"""The sun, behind the camera and above."""
SUN_ROTATION = (-0.5, 0.5, 0.0)
"""Rotation of the sun light (its direction is its -Z axis)."""
SHINE = "Light: {type: Directional, enabled: true, color: [0.45, 0.65, 0.8], intensity: 700, castShadows: false}\n"
"""The light of the planet below, bluish, lighting the undersides."""
SHINE_ROTATION = (1.3, 0.0, 0.0)
"""Rotation of the planet light: upwards."""


def planet() -> str:
    """A blue and green world with an atmosphere, its relief from the detail noise."""
    return (
        "Planet:\n  graph: ''\n  seed: 7\n  heightResolution: 256\n"
        f"  radius: {y.num(PLANET_RADIUS)}\n  heightScale: 600\n  seaLevel: 0.52\n  detailAmplitude: 0.35\n"
        "  detailFrequency: 6\n  maxLevel: 10\n  lodFactor: 2\n  chunkResolution: 16\n"
        "  seaColor: [0.06, 0.2, 0.42]\n  lowColor: [0.4, 0.54, 0.38]\n  highColor: [0.66, 0.6, 0.47]\n"
        "  peakColor: [0.9, 0.92, 0.96]\n  atmosphere: true\n  atmosphereHeight: 1800\n"
        "  rayleighDepth: [0.046, 0.108, 0.265]\n  mieDepth: 0.02\n  scatteringIntensity: 3\n"
        "  scaledSpaceDistance: 0\n"
    )


def world_position(point: Vec3) -> str:
    """A ``WorldPosition`` of a root entity."""
    return f"WorldPosition: {{position: {y.vec(*point)}}}\n"


def spline(points: list[Vec3]) -> str:
    """The rail."""
    lines = "".join(f"    - {y.vec(*p)}\n" for p in points)
    return (
        "Spline:\n  interpolation: CatmullRom\n  closed: false\n  resolution: 32\n  up: [0, 1, 0]\n"
        f"  points:\n{lines}  lanes: []\n"
    )


def follower(rail: int) -> str:
    """The carrier riding the rail once."""
    return (
        f"SplineFollower:\n  enabled: true\n  spline: {rail}\n  lane: ''\n  offset: 0\n"
        f"  speed: {y.num(RAIL_SPEED)}\n  startDistance: 0\n  mode: Once\n  orient: true\n"
    )


def sound(path: str, category: str, volume: float) -> str:
    """A looping, non-spatial ``SoundSource`` playing at start."""
    return y.music(path, volume).replace("category: Music", f"category: {category}")


# ------------------------------------------------------------------------------------------------------- the field


def asteroid_field(entities: y.Entities, rail: Rail) -> int:
    """The rocks of the field and the scenery out of reach; the count of playable rocks."""
    rng = random.Random(7)
    models = ["models/asteroid_a.glb", "models/asteroid_b.glb", "models/asteroid_c.glb"]
    count = 0
    for start, end, density in BELTS:
        distance = start
        while distance < end:
            distance += rng.expovariate(density)
            if distance >= end:
                break
            across = rng.uniform(-FIELD_HALF[0], FIELD_HALF[0])
            up = rng.uniform(-FIELD_HALF[1], FIELD_HALF[1])
            size = rng.choice([0.7, 0.8, 1.0, 1.2, 1.5, 2.2, 2.8])
            _rock(entities, rail, rng, models, distance, across, up, size, "Asteroid")
            count += 1
        # A big rock on the centre line now and then: shoot it or steer round it.
        for distance in range(int(start) + 120, int(end), 260):
            _rock(entities, rail, rng, models, distance, rng.uniform(-1.5, 1.5), rng.uniform(-1, 1), 2.6, "Asteroid")
            count += 1
    for _ in range(140):
        distance = rng.uniform(0.0, rail.length)
        side = rng.choice([-1.0, 1.0])
        across = side * rng.uniform(22.0, 90.0)
        up = rng.uniform(-40.0, 40.0)
        _rock(entities, rail, rng, models, distance, across, up, rng.uniform(4.0, 14.0), "FarRock")
    return count


def _rock(
    entities: y.Entities,
    rail: Rail,
    rng: random.Random,
    models: list[str],
    distance: float,
    across: float,
    up: float,
    size: float,
    tag: str,
) -> None:
    grey = rng.uniform(0.55, 0.95)
    tint = (grey, grey * rng.uniform(0.85, 0.97), grey * rng.uniform(0.75, 0.92), 1)
    rotation = (rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3))
    entities.add(
        tag,
        [mesh(rng.choice(models), tint)],
        position=rail.place(distance, across, up),
        scale=(size, size, size),
        rotation=rotation,
    )


# ----------------------------------------------------------------------------------------------------------- level


def level(rail: Rail) -> tuple[str, int]:
    """The level scene and the count of its playable rocks."""
    entities = y.Entities(7700000000000000000)
    rail_id = entities.add("Rail", [spline(RAIL_POINTS)])
    carrier = entities.add("RailCarrier", [follower(rail_id)])
    entities.add(
        "MainCamera",
        [perspective(0.9, True, True), "SoundListener:\n  primary: true\n"],
        position=(0, 1.3, 10),
        parent=carrier,
    )
    cut = entities.add("CutCamera", [perspective(0.9, False, True)], position=(0, 1.3, 10), parent=carrier)
    ship = entities.add("Ship", [], parent=carrier)
    entities.add("ShipModel", [mesh("models/player.glb")], parent=ship)
    for side in (-1, 1):
        entities.add(
            f"EngineTrail{'L' if side < 0 else 'R'}",
            [trail((0.4, 0.8, 1, 0.9), 0.16, 0.14, 5000)],
            position=(side * 0.42, 0.05, 0.95),
            parent=ship,
        )
    entities.add(
        "Reticle",
        [hud_marker("textures/hud/reticle.png", 56, (0.6, 0.95, 1, 0.95))],
        position=(0, 0, -40),
        parent=carrier,
    )
    entities.add(
        "ReticleNear",
        [hud_marker("textures/hud/reticle_near.png", 34, (0.6, 0.95, 1, 0.8))],
        position=(0, 0, -18),
        parent=carrier,
    )
    entities.add(
        "IntroDirector", [sequence_player("sequences/intro.owlseq", cut, True, "on_intro_finished")], parent=carrier
    )
    entities.add(
        "BossDirector", [sequence_player("sequences/boss.owlseq", cut, False, "on_boss_intro_finished")], parent=carrier
    )
    entities.add(
        "EndDirector", [sequence_player("sequences/outro.owlseq", cut, False, "on_outro_finished")], parent=carrier
    )
    entities.add("Sun", [SUN], rotation=SUN_ROTATION)
    entities.add("PlanetShine", [SHINE], rotation=SHINE_ROTATION)
    entities.add("Sky", [sky()])
    entities.add("Planet", [world_position(PLANET_CENTRE), planet()], position=PLANET_CENTRE)
    properties: list[tuple[str, str, object]] = [
        ("waves", "string", "tables/waves.owltable"),
        ("enemies", "string", "tables/enemies.owltable"),
        ("rail_speed", "float", RAIL_SPEED),
        ("boss_speed", "float", 7.0),
        ("hull", "float", 100.0),
    ]
    entities.add("Music", [sound("sounds/music/flight.wav", "Music", 0.6)])
    for index in range(1, 7):
        entities.add(
            f"Burst{index}",
            [
                burst(
                    [(0, 1, 0.95, 0.7, 1), (0.35, 1, 0.55, 0.15, 0.9), (1, 0.6, 0.1, 0.02, 0)],
                    (4, 14),
                    0.9,
                    9000,
                    160,
                    (0.4, 1.1),
                )
            ],
        )
        entities.add(
            f"Debris{index}",
            [burst([(0, 0.75, 0.68, 0.6, 1), (1, 0.45, 0.4, 0.36, 0)], (3, 9), 0.6, 0, 120, (0.6, 1.4), "Alpha")],
        )
        entities.add(
            f"Spark{index}", [burst([(0, 0.7, 1, 1, 1), (1, 0.2, 0.5, 1, 0)], (6, 16), 0.25, 12000, 40, (0.15, 0.35))]
        )
    rocks = asteroid_field(entities, rail)
    header = y.script("scripts/game/rail.lua", properties).replace("LuaScript:", "SceneScript:") + (
        "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
        "  - {Name: planet_world, Enabled: true}\n  - {Name: particles, Enabled: true}\n"
        "  - {Name: ui, Enabled: false}\nFloatingOrigin:\n  enabled: true\n  shiftRadius: 1000\n  origin: [0, 0, 0]\n"
    )
    return y.scene("level", entities, "huds/play.owlhud", header), rocks


# --------------------------------------------------------------------------------------------------------- prefabs


def prefab(name: str, entities: y.Entities) -> str:
    """A prefab file of the entities added."""
    return f"Prefab: {name}\nVersion: 1\nEntities:\n" + entities.text()


def prefabs() -> dict[str, str]:
    """Every prefab the script spawns, by file name."""
    out: dict[str, str] = {}
    bolt = y.Entities(7710000000000000000)
    bolt.add("Bolt", [mesh("models/bolt.glb", glow=30000), trail((0.5, 0.95, 1, 1), 0.22, 0.12, 12000)])
    out["bolt"] = prefab("Bolt", bolt)
    enemy_bolt = y.Entities(7711000000000000000)
    enemy_bolt.add("EnemyBolt", [mesh("models/enemy_bolt.glb", glow=30000), trail((1, 0.5, 0.15, 1), 0.3, 0.15, 12000)])
    out["enemy_bolt"] = prefab("EnemyBolt", enemy_bolt)
    ships = [
        ("fighter", "Fighter", "textures/hud/target.png", 30, "", False),
        ("interceptor", "Interceptor", "textures/hud/target.png", 38, "", True),
        ("boss", "Boss", "textures/hud/boss_target.png", 120, "WARDEN", True),
    ]
    for index, (key, tag, icon, size, label, offscreen) in enumerate(ships):
        ship = y.Entities(7712000000000000000 + index * 1000000000000)
        root = ship.add(tag, [hud_marker(icon, size, (1, 0.45, 0.35, 0.9), label, offscreen)])
        ship.add(f"{tag}Model", [mesh(f"models/{key}.glb")], parent=root)
        width = 0.3 if key != "boss" else 1.2
        for side in (-1, 1):
            back = 1.0 if key != "boss" else 8.0
            spread = 0.35 if key != "boss" else 4.2
            ship.add(
                f"{tag}Trail",
                [trail((1, 0.45, 0.2, 0.8), width, 0.3, 5000)],
                position=(side * spread, 0.1, back),
                parent=root,
            )
        out[key] = prefab(tag, ship)
    return out


# ------------------------------------------------------------------------------------------------------- sequences


def _look(position: Vec3, target: Vec3) -> Vec3:
    """Rotation of a camera at ``position`` looking at ``target`` (pitch, yaw; -Z forward)."""
    d = [target[c] - position[c] for c in range(3)]
    length = math.sqrt(sum(v * v for v in d))
    return math.asin(d[1] / length), math.atan2(-d[0], -d[2]), 0.0


def _sequence(duration: float, keys: list[tuple[float, Vec3, Vec3]], events: list[tuple[float, str]]) -> str:
    key_lines = "".join(
        f"      - {{time: {y.num(t)}, position: {y.vec(*p)}, rotation: {y.vec(*r)}, fov: 0.9, size: 8, "
        "easing: Smooth}\n"
        for t, p, r in keys
    )
    event_lines = "".join(f"      - {{time: {y.num(t)}, function: {name}}}\n" for t, name in events)
    out = (
        f"Sequence: ''\nFormatVersion: 1\nduration: {y.num(duration)}\ncameraTracks:\n  - name: Shot\n"
        f"    muted: false\n    keys:\n{key_lines}"
    )
    if events:
        out += f"eventTracks:\n  - name: Events\n    muted: false\n    keys:\n{event_lines}"
    else:
        out += "eventTracks: []\n"
    return out


HOME = ((0.0, 1.3, 10.0), (0.0, 0.0, 0.0))
"""Pose of the play camera in the carrier."""


def sequences() -> dict[str, str]:
    """The intro (around the ship), the boss arrival (towards the boss), the end (the ship flying away)."""
    ship = (0.0, 0.0, 0.0)
    intro = [
        (0.0, (5.0, 1.5, -12.0), _look((5.0, 1.5, -12.0), ship)),
        (2.2, (-7.0, 2.5, -2.0), _look((-7.0, 2.5, -2.0), ship)),
        (4.2, (-2.0, 2.0, 9.0), _look((-2.0, 2.0, 9.0), (0.0, 0.5, -10.0))),
        (5.0, *HOME),
    ]
    boss_point = (0.0, 2.0, -70.0)
    boss = [
        (0.0, *HOME),
        (0.8, (10.0, 4.0, -12.0), _look((10.0, 4.0, -12.0), boss_point)),
        (3.0, (5.0, 3.0, -24.0), _look((5.0, 3.0, -24.0), boss_point)),
        (4.0, *HOME),
    ]
    outro = [
        (0.0, *HOME),
        (1.2, (8.0, 2.0, -6.0), _look((8.0, 2.0, -6.0), (0.0, 0.0, -30.0))),
        (4.5, (8.0, 2.0, -6.0), _look((8.0, 2.0, -6.0), (0.0, 0.0, -140.0))),
    ]
    return {
        "intro": _sequence(5.0, intro, [(0.3, "on_intro_title")]),
        "boss": _sequence(4.0, boss, [(0.1, "on_boss_alarm")]),
        "outro": _sequence(4.5, outro, [(3.6, "on_outro_fade")]),
    }


# ------------------------------------------------------------------------------------------------------------- HUD


CYAN = (0.55, 0.92, 1, 1)
"""Colour of the HUD texts."""
AMBER = (1, 0.78, 0.3, 1)
"""Colour of the highlights."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
MONO = "JetBrainsMono-Regular"
"""Font of the counters."""
FONT = "OpenSans-ExtraBoldItalic"
"""Font of the titles and of the buttons."""
CONTROLS = (
    "STEER     W A S D, left stick\n"
    "AIM       mouse, arrows, right stick\n"
    "FIRE      Space, left click, RT\n"
    "ROLL      Q / E, LB / RB\n"
    "PAUSE     Escape, P, Start"
)
"""The controls, in the How to play window of the title and in the pause window."""


def _label(value: str, size: int, colour: y.Colour = CYAN, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.02, 0.06, 0.15, 1), outline_width=2)


def hud_button(
    entities: y.Entities, parent: int, tag: str, label: str, callback: str, size: tuple[float, float], initial: bool
) -> int:
    """A cockpit button: a dark blue fill, the cut-corner frame, an italic label, an amber focus frame."""
    components = [
        y.child_rect(size),
        y.button(callback, (0.03, 0.08, 0.16), (0.08, 0.22, 0.38, 1), (0.12, 0.32, 0.5, 1), (0.05, 0.06, 0.08, 0.6)),
        _label(label, 28),
        y.focus(initial=initial, colour=AMBER, thickness=3),
    ]
    uuid = entities.add(tag, components, parent=parent)
    entities.add(f"{tag}Frame", [y.child_rect((1, 1)), y.image("textures/ui/button_frame.png")], parent=uuid)
    return uuid


def progress(value: float, fill: y.Colour, background: y.Colour = (0.05, 0.1, 0.18, 0.8)) -> str:
    """A ``UiProgressBar``."""
    return (
        f"UiProgressBar:\n  value: {y.num(value)}\n  backgroundColor: {y.vec(*background)}\n"
        f"  fillColor: {y.vec(*fill)}\n"
    )


def hud() -> str:
    """Score and hits at the top left, the hull at the top right, the boss bar under them, a message line, the
    distance along the bottom, the pause window (the only place of the level showing the controls)."""
    entities = y.Entities(7790000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    left = entities.add(
        "TopLeft",
        [y.rect((0, 0), (0.5, 0.11), (0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 0, cross="Start")],
        parent=root,
    )
    entities.add("HitsText", [y.child_rect((1, 0.42)), _label("HITS 0%", 22, AMBER, "Left", MONO)], parent=left)
    entities.add("ScoreText", [y.child_rect((1, 0.56)), _label("SCORE 000000", 30, CYAN, "Left", MONO)], parent=left)
    right = entities.add(
        "TopRight",
        [y.rect((1, 0), (0.42, 0.11), (-0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 4, cross="End")],
        parent=root,
    )
    entities.add("HullBar", [y.child_rect((1, 0.22)), progress(1, (0.35, 0.85, 1, 1))], parent=right)
    entities.add("HullText", [y.child_rect((1, 0.5)), _label("HULL 100", 24, CYAN, "Right", MONO)], parent=right)
    boss = entities.add(
        "BossGroup",
        [y.rect((0.5, 0), (0.7, 0.07), (0, 0.15), SCREEN, SCREEN), y.layout("Column", 2)],
        parent=root,
        visible=False,
    )
    entities.add(
        "BossBar", [y.child_rect((1, 0.35)), progress(1, (1, 0.3, 0.35, 1), (0.2, 0.04, 0.06, 0.8))], parent=boss
    )
    entities.add("BossName", [y.child_rect((1, 0.55)), _label("WARDEN", 22, (1, 0.5, 0.5, 1), font=MONO)], parent=boss)
    entities.add(
        "Message",
        [y.rect((0.5, 0.5), (1.2, 0.09), (0, -0.2), SCREEN, SCREEN), _label("", 40, AMBER)],
        parent=root,
        visible=False,
    )
    entities.add(
        "DistanceBar",
        [
            y.rect((0.5, 1), (0.8, 0.012), (0, -0.03), SCREEN, SCREEN),
            progress(0, (0.55, 0.92, 1, 0.7), (0.1, 0.15, 0.25, 0.5)),
        ],
        parent=root,
    )
    pause = entities.add(
        "PauseWindow",
        [
            y.rect((0.5, 0.5), (1.0, 0.8)),
            y.window("PAUSED", (0.02, 0.05, 0.1, 0.96), (0.05, 0.2, 0.35, 1), CYAN, 40, 28),
            y.layout("Column", 8, 12),
        ],
        parent=root,
        visible=False,
    )
    for index, (tag, label, callback) in reversed(
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
        hud_button(entities, pause, tag, label, callback, (0.6, 0.1), index == 0)
    controls = y.text(CONTROLS, 22, "Left", AMBER, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("PauseControls", [y.child_rect((0.92, 0.42)), controls], parent=pause)
    return y.hud_file("play", entities)


# --------------------------------------------------------------------------------------------------------- project


def wave_table() -> str:
    """The waves, one row each."""
    rows = "".join(
        f"  - key: {w.key}\n    distance: {y.num(w.distance)}\n    kind: {w.kind}\n    count: {w.count}\n"
        f"    pattern: {w.pattern}\n    x: {y.num(w.x)}\n    y: {y.num(w.y)}\n"
        for w in WAVES
    )
    return (
        "DataTable: ''\nFormatVersion: 1\ncolumns:\n  - {name: distance, type: Float}\n"
        "  - {name: kind, type: Reference, table: tables/enemies.owltable}\n  - {name: count, type: Integer}\n"
        "  - {name: pattern, type: Enum, choices: [line, vee, swoop_left, swoop_right, hover, boss]}\n"
        "  - {name: x, type: Float}\n  - {name: y, type: Float}\n"
        f"rows:\n{rows}"
    )


ENEMIES = [
    ("fighter", 2, 1.3, 200, 3.4),
    ("interceptor", 5, 1.5, 400, 2.0),
    ("boss", 140, 7.5, 5000, 1.7),
]
"""The kinds of enemies: key (its prefab is ``prefabs/<key>.owlprefab``), hull in hits, radius, points, reload."""


def enemy_table() -> str:
    """The kinds of enemies, one row each, referenced by the waves."""
    rows = "".join(
        f"  - key: {key}\n    prefab: prefabs/{key}.owlprefab\n    hp: {hp}\n    radius: {y.num(radius)}\n"
        f"    points: {points}\n    reload: {y.num(reload)}\n"
        for key, hp, radius, points, reload in ENEMIES
    )
    return (
        "DataTable: ''\nFormatVersion: 1\ncolumns:\n  - {name: prefab, type: Asset}\n  - {name: hp, type: Integer}\n"
        "  - {name: radius, type: Float}\n  - {name: points, type: Integer}\n  - {name: reload, type: Float}\n"
        f"rows:\n{rows}"
    )


PROJECT = """OwlProject:
  name: OwlRailShooter
  firstScene: scenes/title.owl
  author: Silmaen
  description: Ring Runner, a rail shooter sample game of the Owl Engine
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
  - key: volume_music
    type: float
    value: 0.8
  - key: volume_sfx
    type: float
    value: 1.0
  - key: ui_scale
    type: float
    value: 1.0
  - key: rail_invert_y
    type: bool
    value: false
  - key: rail_best
    type: int
    value: 0
  - key: rail_best_medal
    type: int
    value: 0
"""
"""The settings of the game and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of Ring Runner, on top of the built-in ones (the `ui_*` actions drive the menus). Read by
# scripts/game/rail.lua.
actions:
  - name: steer
    type: vector
    bindings:
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: S, component: y}
      - {key: W, component: y, scale: -1}
      - {gamepad_stick: left}
  - name: aim
    type: vector
    bindings:
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_stick: right}
  - name: fire
    type: button
    bindings:
      - {key: Space}
      - {mouse_button: Left}
      - {gamepad_axis: right_trigger}
  - name: roll_left
    type: button
    bindings:
      - {key: Q}
      - {gamepad_button: LeftBumper}
  - name: roll_right
    type: button
    bindings:
      - {key: E}
      - {gamepad_button: RightBumper}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input actions of the game."""


def main() -> None:
    """Write the project files, the level, its table, prefabs, sequences and HUD."""
    rail = Rail(RAIL_POINTS)
    for folder in ("scenes", "huds", "prefabs", "sequences", "tables", "input"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    controller_db = ROOT.parent / "breakout" / "input" / "gamecontrollerdb.txt"
    (ROOT / "input" / "gamecontrollerdb.txt").write_bytes(controller_db.read_bytes())
    scene, rocks = level(rail)
    (ROOT / "scenes" / "level.owl").write_text(scene)
    (ROOT / "tables" / "waves.owltable").write_text(wave_table())
    (ROOT / "tables" / "enemies.owltable").write_text(enemy_table())
    for name, text in prefabs().items():
        (ROOT / "prefabs" / f"{name}.owlprefab").write_text(text)
    for name, text in sequences().items():
        (ROOT / "sequences" / f"{name}.owlseq").write_text(text)
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    print(f"rail {rail.length:.0f} m, {rocks} rocks in reach")


if __name__ == "__main__":
    main()
