#!/usr/bin/env python3
"""
Write the project files, the five landing scenes, the play HUD and the game graphs of the Lunar lander sample game
into ``samples/lunar_lander/``.

- ``owl_project.yml``, ``game_settings.yml``, ``input_actions.yml``;
- ``scenes/landing_1.owl`` to ``landing_5.owl``: the moon (its mesh, a static mesh body, a spherical gravity field),
  the five pads (only the target one marked), the Earth on the planet layer, the lander (a hull and four legs held by
  breakable fixed joints), the beads of the predicted path, the chase camera; the scene script
  ``scripts/game/landing.owlvs`` with the properties of the landing;
- ``huds/play.owlhud``: landing, pad, score, fuel, altitude and speeds, a message line, the pause window;
- ``scripts/game/landing.owlvs`` (the rules of a landing) and ``scripts/game/hull.owlvs`` (the hull reports a crash
  or a broken leg); the flight maths is the Lua module ``scripts/game/lander.lua``, written by hand.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.lunar_lander.generate_level``;
then ``OwlNest --upgrade-assets samples/lunar_lander`` gives the new files their ``.owlmeta``.
"""

from __future__ import annotations

import math
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.lunar_lander import layout as lay
from tools.samples.lunar_lander.generate_models import pad_height
from tools.samples.vs_graph import Graph

ROOT = Path(__file__).resolve().parents[3] / "samples" / "lunar_lander"
"""The sample project."""
MESH = "RendererTag:\n  Name: mesh_world\n"
"""Draw an entity in the mesh layer."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
TEXT = (0.9, 0.93, 1.0, 1)
"""Colour of the HUD texts."""
AMBER = (1, 0.72, 0.25, 1)
"""Colour of the highlights."""
SOFT = (0.7, 0.75, 0.85, 1)
"""Colour of the secondary texts."""
MONO = "JetBrainsMono-Regular"
"""Font of the read-outs."""
FONT = "Inter-SemiBold"
"""Font of the titles and of the buttons."""
BEADS = 24
"""Beads of the predicted path."""
LEG_BREAK_FORCE = 125.0
"""Force a leg joint holds before it breaks, in newtons."""
CONTROLS = (
    "MAIN ENGINE   Space, left click, RT (or A)\n"
    "TILT          W A S D, left stick\n"
    "SIDE JETS     arrows, d-pad\n"
    "TURN VIEW     Q / E, R / F, right drag, right stick\n"
    "ZOOM          Z / X, wheel, LB / RB\n"
    "PAUSE         Escape, P, Start"
)
"""The controls, in the How to play window of the title and in the pause window."""

Vec3 = tuple[float, float, float]


# ----------------------------------------------------------------------------------------------------------- maths


def _add(a: Vec3, b: Vec3, k: float = 1.0) -> Vec3:
    return a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k


def _scale(a: Vec3, k: float) -> Vec3:
    return a[0] * k, a[1] * k, a[2] * k


def _dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a: Vec3, b: Vec3) -> Vec3:
    return a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]


def _unit(a: Vec3) -> Vec3:
    length = math.sqrt(_dot(a, a)) or 1.0
    return a[0] / length, a[1] / length, a[2] / length


def euler(x: Vec3, up: Vec3, z: Vec3) -> Vec3:
    """Euler angles (Rz * Ry * Rx, as ``math::Transform``) of the basis whose columns are x, up, z."""
    ry = math.asin(max(-1.0, min(1.0, -x[2])))
    if math.cos(ry) < 1e-5:
        # Gimbal lock: only rx - rz (or rx + rz) is known, rz is taken as 0.
        return (math.atan2(up[0], up[1]) if x[2] < 0 else math.atan2(-up[0], up[1])), ry, 0.0
    rx = math.atan2(up[2], z[2])
    rz = math.atan2(x[1], x[0])
    return rx, ry, rz


def frame(up: Vec3) -> tuple[Vec3, Vec3]:
    """North and east on the ground of a local vertical."""
    east = _unit(_cross((0.0, 1.0, 0.0), up))
    north = _cross(up, east)
    return north, east


def pad_pose(pad: lay.Pad) -> tuple[Vec3, Vec3]:
    """Position and rotation of a pad entity: its deck on the flattened ground, its Y along the local vertical.

    The deck, 0.4 thick, is set 0.1 into the ground: the facets of the moon mesh sag that much below the sphere of the
    flattened ground under a pad, so the deck rests on them all around instead of floating."""
    up = pad.up
    north, east = frame(up)
    position = _scale(up, lay.MOON_RADIUS + pad_height(pad) + 0.1)
    return position, euler(east, up, _scale(north, -1.0))


def start(landing: lay.Landing) -> tuple[Vec3, Vec3, Vec3]:
    """Position, rotation and velocity of the lander at the start of a landing: upright, its nose along its way."""
    pad = lay.PADS[landing.pad]
    up = pad.up
    north, east = frame(up)
    heading = math.radians(landing.heading)
    travel = _add(_scale(north, math.cos(heading)), east, math.sin(heading))
    side = _cross(travel, up)
    arc = landing.downrange / lay.MOON_RADIUS
    where = _unit(
        _add(_add(_scale(up, math.cos(arc)), travel, -math.sin(arc)), side, landing.crossrange / lay.MOON_RADIUS)
    )
    radius = lay.MOON_RADIUS + pad_height(pad) + 1.2 + landing.height
    position = _scale(where, radius)
    towards = _unit(_add(up, where, -_dot(up, where)))
    if landing.downrange <= 0.0:
        towards = _unit(_add(travel, where, -_dot(travel, where)))
    speed = math.sqrt(lay.GRAVITY * lay.MOON_RADIUS**2 / radius) if landing.orbit else landing.speed
    velocity = _scale(towards, speed)
    back = _scale(towards, -1.0)
    return position, euler(_cross(where, back), where, back), velocity


# ------------------------------------------------------------------------------------------------------ components


def mesh(path: str, colour: y.Colour = (1, 1, 1, 1), glow: float = 0.0) -> str:
    """A ``MeshRenderer`` of a model of the project, its materials glowing at ``glow`` nits when above 0."""
    out = f"MeshRenderer:\n  mesh: {path}\n  color: {y.vec(*colour)}\n  transparent: false\n"
    if glow > 0:
        out += f"  materials:\n    - {{overrides: {{emissiveLuminance: {y.num(glow)}}}}}\n"
    return out + MESH


def body(
    kind: str, shape: str, size: Vec3, layer: int = 0, mask: int = 255, density: float = 1.0, friction: float = 0.8
) -> str:
    """A 3D ``PhysicBody``."""
    out = (
        f"PhysicBody:\n  type: {kind}\n  dimension: Space\n  shape: {shape}\n  fixedRotation: false\n"
        f"  colliderSize: {y.vec(*size)}\n  density: {y.num(density)}\n  restitution: 0\n"
        f"  friction: {y.num(friction)}\n"
    )
    if layer or mask != 255:
        out += f"  layer: {layer}\n  mask: {mask}\n"
    return out


def joint(hull: int, anchor: Vec3) -> str:
    """A fixed joint of a leg on the hull, breaking under a hard landing."""
    return (
        f"PhysicJoint:\n  type: Fixed\n  connected: {hull}\n  anchor: {y.vec(*anchor)}\n  connectedAnchor: [0, 0, 0]\n"
        "  axis: [0, 0, 1]\n  limited: false\n  lowerLimit: 0\n  upperLimit: 0\n  motor: false\n  motorSpeed: 0\n"
        f"  motorForce: 100\n  springFrequency: 0\n  springDamping: 0\n  breakForce: {y.num(LEG_BREAK_FORCE)}\n"
        f"  breakTorque: {y.num(LEG_BREAK_FORCE * 0.6)}\n"
    )


def camera(primary: bool = True) -> str:
    """The perspective camera, reaching the Earth, with a soft bloom."""
    return (
        "Camera:\n  camera:\n    projectionType: Perspective\n    perspectiveFOV: 0.9\n    perspectiveNear: 0.1\n"
        "    perspectiveFar: 400000\n    orthographicSize: 8\n    orthographicNear: -1\n    orthographicFar: 10\n"
        "    toneMapping: Aces\n"
        "    postProcess: {ev100: 10, exposure: 0, autoExposure: false, bloom: true, bloomThreshold: 1.6,"
        " bloomKnee: 0.5, bloomIntensity: 0.1, bloomScatter: 0.7, vignette: true, vignetteIntensity: 0.3,"
        " vignetteSmoothness: 0.5, vignetteColor: [0, 0, 0]}\n"
        f"  primary: {'true' if primary else 'false'}\n  fixedAspectRatio: false\n"
    )


SUN = "Light: {type: Directional, enabled: true, color: [1, 0.97, 0.92], intensity: 3600, castShadows: false}\n"
"""The sun."""
SUN_ROTATION = (-0.35, 0.9, 0.0)
"""Where the sun shines from."""
EARTH_CENTRE = (-26000.0, 9000.0, -32000.0)
"""The Earth, far in the sky."""


def sky() -> str:
    """The starfield of the Rail shooter, lighting the scene a little."""
    return (
        "Environment: {hdri: textures/sky/starfield.hdr, ambient: [0.05, 0.05, 0.06], intensity: 9000, "
        "rotation: 0.2, showSky: true}\n"
    )


def earth() -> str:
    """A blue planet with clouds of colour and an atmosphere, on the planet layer."""
    return (
        "Planet:\n  graph: ''\n  seed: 3\n  heightResolution: 128\n  radius: 6000\n  heightScale: 90\n"
        "  seaLevel: 0.55\n  detailAmplitude: 0.3\n  detailFrequency: 5\n  maxLevel: 6\n  lodFactor: 2\n"
        "  chunkResolution: 16\n  seaColor: [0.05, 0.18, 0.45]\n  lowColor: [0.25, 0.45, 0.25]\n"
        "  highColor: [0.6, 0.55, 0.45]\n  peakColor: [0.95, 0.95, 0.97]\n  atmosphere: true\n  atmosphereHeight: 400\n"
        "  rayleighDepth: [0.046, 0.108, 0.265]\n  mieDepth: 0.02\n  scatteringIntensity: 3\n  scaledSpaceDistance: 0\n"
    )


def point_light(colour: Vec3, intensity: float, range_: float) -> str:
    """A point light."""
    return (
        f"Light:\n  type: Point\n  enabled: true\n  color: {y.vec(*colour)}\n  intensity: {y.num(intensity)}\n"
        f"  range: {y.num(range_)}\n"
    )


def hud_marker(icon: str, colour: y.Colour, label: str, enabled: bool) -> str:
    """A ``HudMarker`` with the distance, kept on the screen edge when out of view."""
    return (
        f"HudMarker:\n  enabled: {'true' if enabled else 'false'}\n  icon: nam:{icon}\n  color: {y.vec(*colour)}\n"
        f"  size: [28, 28]\n  offset: [0, 1.5, 0]\n  label: {y.quote(label)}\n  labelColor: [1, 0.9, 0.7, 1]\n"
        "  fontSize: 16\n  showDistance: true\n  maxDistance: 0\n  offscreenIndicator: true\n  edgeMargin: 40\n"
    ) + MESH


HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: planet_world, Enabled: true}\n  - {Name: ui, Enabled: false}\n"
    "Physics:\n  solverSubSteps: 2\n  interpolate: true\n  workerCount: 0\n  gravity: [0, 0, 0]\n"
)
"""Root keys of every scene: the 3D layers, no world gravity (the moon pulls)."""


def world(entities: y.Entities, target: int) -> dict[str, int]:
    """The sun, the sky, the Earth, the moon and the five pads; only the ``target`` pad is lit and marked."""
    ids: dict[str, int] = {}
    entities.add("Sun", [SUN], rotation=SUN_ROTATION)
    entities.add("Sky", [sky()])
    entities.add("Earth", [f"WorldPosition: {{position: {y.vec(*EARTH_CENTRE)}}}\n", earth()], position=EARTH_CENTRE)
    gravity = (
        f"GravitySource: {{type: Spherical, enabled: true, strength: {y.num(lay.GRAVITY)}, "
        f"radius: {y.num(lay.MOON_RADIUS)}, range: 0, direction: [0, -1, 0]}}\n"
    )
    ids["Moon"] = entities.add("Moon", [mesh("models/moon.glb"), body("Static", "Mesh", (1, 1, 1)), gravity])
    for index, pad in enumerate(lay.PADS):
        position, rotation = pad_pose(pad)
        lit = index == target
        components = [mesh("models/pad.glb", glow=6000 if lit else 400), body("Static", "Box", (8.4, 0.4, 8.4))]
        components.append(hud_marker("textures/hud/pad.png", (1, 0.8, 0.35, 1), pad.name, lit))
        ids[pad.tag] = entities.add(pad.tag, components, position=position, rotation=rotation)
        if lit:
            entities.add("PadLight", [point_light((1, 0.75, 0.4), 4000, 18)], position=(0, 3, 0), parent=ids[pad.tag])
    return ids


def lander(entities: y.Entities, position: Vec3, rotation: Vec3) -> int:
    """The hull (the root, scripted), its four legs on breakable joints and the engine plume."""
    hull_script = y.script("scripts/game/hull.owlvs")
    hull = entities.add(
        "Lander",
        [mesh("models/lander_hull.glb"), body("Dynamic", "Box", (1.6, 1.1, 1.6), 1, 1, 1.0, 0.6), hull_script],
        position=position,
        rotation=rotation,
    )
    for index in range(4):
        angle = math.pi / 4 + index * math.pi / 2
        offset = (1.05 * math.cos(angle), -0.75, 1.05 * math.sin(angle))
        entities.add(
            f"Leg{index + 1}",
            [
                mesh("models/lander_leg.glb"),
                body("Dynamic", "Box", (0.45, 1.0, 0.45), 1, 1, 0.8, 0.9),
                joint(hull, (0, 0.5, 0)),
            ],
            position=offset,
            rotation=(0, -angle + math.pi, 0),
            parent=hull,
        )
    flame = entities.add(
        "Flame",
        [mesh("models/flame.glb", glow=20000)],
        position=(0, -0.95, 0),
        scale=(1, 0.01, 1),
        parent=hull,
        visible=False,
    )
    entities.add("FlameLight", [point_light((1, 0.6, 0.3), 0, 12)], position=(0, -1.0, 0), parent=flame)
    return hull


def landing_scene(number: int, landing: lay.Landing) -> str:
    """One landing."""
    entities = y.Entities(7400000000000000000 + number * 10000)
    position, rotation, velocity = start(landing)
    world(entities, landing.pad)
    lander(entities, position, rotation)
    entities.add("ChaseCamera", [camera()], position=_add(position, _unit(position), 8.0))
    for index in range(1, BEADS + 1):
        entities.add(f"Bead{index}", [mesh("models/bead.glb", glow=3000)], visible=False)
    marker = hud_marker("textures/hud/impact.png", (1, 0.4, 0.3, 1), "", False).replace(
        "showDistance: true", "showDistance: false"
    )
    entities.add("ImpactMarker", [mesh("models/impact.glb", glow=5000), marker], visible=False)
    entities.add("FlightMusic", [y.music("sounds/music/flight.wav", 0.5)])
    pad = lay.PADS[landing.pad]
    following = f"scenes/landing_{number + 1}.owl" if number < len(lay.LANDINGS) else "scenes/victory.owl"
    properties = [
        ("landing", "int", number),
        ("this_scene", "string", f"scenes/landing_{number}.owl"),
        ("next_scene", "string", following),
        ("pad_tag", "string", pad.tag),
        ("pad_name", "string", pad.name),
        ("fuel_max", "float", landing.fuel),
        ("start_velocity", "vec3", y.vec(*velocity)),
    ]
    script = y.script("scripts/game/landing.owlvs", properties).replace("LuaScript:", "SceneScript:")
    header = script + HEADER
    return y.scene(f"landing_{number}", entities, "huds/play.owlhud", header)


# ------------------------------------------------------------------------------------------------------------- HUD


def _label(value: str, size: int, colour: y.Colour = TEXT, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.02, 0.03, 0.08, 1), outline_width=2)


LOOK = mk.Look(
    prefix="lander",
    title_font=FONT,
    mono_font=MONO,
    text=TEXT,
    soft=SOFT,
    accent=AMBER,
    danger=(1, 0.45, 0.4, 1),
    button=(0.07, 0.08, 0.12),
    button_hover=(0.16, 0.18, 0.26, 1),
    button_pressed=(0.24, 0.27, 0.36, 1),
    window_body=(0.04, 0.05, 0.08, 0.96),
    window_title=(0.14, 0.16, 0.24, 1),
    outline=(0.02, 0.03, 0.08, 1),
    transition="fade",
    sweep=(0.0, 0.0, 0.02, 1),
    header="",
    backdrop=lambda _e, _n: None,
    title_art="textures/ui/title.png",
    frame="textures/ui/frame.png",
    toggles=[("lander_invert_pitch", "INVERT TILT"), ("lander_hide_path", "HIDE PREDICTED PATH")],
    first_scene="scenes/landing_1.owl",
    # A flight console on the left of the screen, the moon in view on the right: read-outs in a monospaced font.
    layout=mk.Layout(
        title=((0, 0), (0.96, 0.24), (0.06, 0.05)),
        best=((0, 0), (0.8, 0.05), (0.09, 0.31)),
        best_align="Left",
        buttons=((0, 0), (0.62, 0.5), (0.08, 0.4)),
        button_width=0.92,
        button_spacing=4,
        label_size=24,
        label_align="Left",
        label_mono=True,
        label_pad="   ",
        box=((0.02, 0.03, 0.06, 0.72), (1, 0.72, 0.25, 0.45)),
        box_padding=10,
        result_align="Left",
        focus_thickness=2,
        result=((0, 0), (1.1, 0.92), (0.06, 0.04)),
        settings=((0, 0), (1.28, 0.94), (0.02, 0.03)),
    ),
)
"""The look of the game, the backdrop set by ``generate_menus``."""


def hud() -> str:
    """Landing, pad and score at the top left, fuel at the top right, the read-outs at the left, a message line and the
    pause window (the only place of a landing showing the controls)."""
    entities = y.Entities(7490000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    left = entities.add(
        "TopLeft",
        [y.rect((0, 0), (0.6, 0.15), (0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 0, cross="Start")],
        parent=root,
    )
    entities.add("ScoreText", [y.child_rect((1, 0.3)), _label("SCORE 0", 22, AMBER, "Left", MONO)], parent=left)
    entities.add("PadText", [y.child_rect((1, 0.3)), _label("", 22, SOFT, "Left")], parent=left)
    entities.add("LandingText", [y.child_rect((1, 0.38)), _label("LANDING 1 / 5", 28, TEXT, "Left")], parent=left)
    right = entities.add(
        "TopRight",
        [y.rect((1, 0), (0.42, 0.1), (-0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 4, cross="End")],
        parent=root,
    )
    bar = "UiProgressBar:\n  value: 1\n  backgroundColor: [0.08, 0.09, 0.14, 0.8]\n  fillColor: [1, 0.72, 0.25, 1]\n"
    entities.add("FuelBar", [y.child_rect((1, 0.25)), bar], parent=right)
    entities.add("FuelText", [y.child_rect((1, 0.5)), _label("FUEL 100%", 24, TEXT, "Right", MONO)], parent=right)
    readout = entities.add(
        "Readout",
        [
            y.rect((0, 0.5), (0.36, 0.2), (0.03, 0), SCREEN, SCREEN),
            y.panel((0.02, 0.03, 0.06, 0.55), (0.6, 0.65, 0.8, 0.4)),
            y.layout("Column", 2, 8, "Start"),
        ],
        parent=root,
    )
    entities.add("HsText", [y.child_rect((0.9, 0.26)), _label("H/S   0.0", 22, TEXT, "Left", MONO)], parent=readout)
    entities.add("VsText", [y.child_rect((0.9, 0.26)), _label("V/S   0.0", 22, TEXT, "Left", MONO)], parent=readout)
    entities.add("AltText", [y.child_rect((0.9, 0.26)), _label("ALT   0.0", 22, TEXT, "Left", MONO)], parent=readout)
    entities.add(
        "Message",
        [y.rect((0.5, 0.5), (1.2, 0.09), (0, -0.22), SCREEN, SCREEN), _label("", 40, AMBER)],
        parent=root,
        visible=False,
    )
    pause = mk.window(LOOK, entities, root, "PauseWindow", "PAUSED", (1.0, 0.8))
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
        mk.button(LOOK, entities, pause, tag, text, callback, (0.6, 0.1), index == 0)
    controls = y.text(CONTROLS, 20, "Left", AMBER, wrap=True, font=MONO, line_spacing=0.25)
    entities.add("PauseControls", [y.child_rect((0.94, 0.42)), controls], parent=pause)
    return y.hud_file("play", entities)


# ---------------------------------------------------------------------------------------------------------- graphs

MODULE = "scripts.game.lander"
"""The Lua module of the flight maths."""


def landing_graph() -> str:
    """The rules of a landing: fly, read the instruments, land on the pad or crash, pause."""
    g = Graph(6400000000000000000)
    g.var("landing", "int", 1)
    g.var("this_scene", "string", "scenes/landing_1.owl")
    g.var("next_scene", "string", "scenes/landing_2.owl")
    g.var("pad_tag", "string", "Pad1")
    g.var("pad_name", "string", "TRANQUILITY BASE")
    g.var("fuel_max", "float", 50.0, low=1)
    g.var("start_velocity", "vec3", (0, 0, 0))
    g.var("thrust", "float", 3.4, low=0)
    g.var("turn_rate", "float", 1.1, low=0)
    g.var("rcs", "float", 0.8, low=0)
    g.var("burn_rate", "float", 4.0, low=0)
    g.var("chase_distance", "float", 13.0, low=2)
    g.var("beads", "int", BEADS, low=0)
    g.var("landings", "int", len(lay.LANDINGS), low=1)
    for name, kind, default in (
        ("state", "string", "fly"),
        ("fuel", "float", 0.0),
        ("throttle", "float", 0.0),
        ("clock", "float", 0.0),
        ("rest", "float", 0.0),
        ("touchdown", "float", -1.0),
        ("engine_clock", "float", 0.0),
        ("warned", "bool", False),
        ("paused", "bool", False),
        ("leaving", "bool", False),
        ("message_time", "float", 0.0),
    ):
        g.var(name, kind, default, exposed=False)

    def find(tag: object) -> object:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    def widget(tag: str) -> object:
        return g.call("ui.find", name=tag)["entity_id"]

    hull = lambda: find("Lander")  # noqa: E731

    # ---------------------------------------------------------------- on_create
    create = g.event("on_create")
    branch = g.branch(g.op("logic.not", g.call("ui.is_transition_active")["active"]))
    g.chain(create, branch)
    fade_in = g.call("ui.transition_play", kind="fade", duration=0.8, rgba=(0, 0, 0.02, 1))
    g.chain(branch["True"], fade_in)
    base = g.call("gamestate.get", key="lander_score", default=0)["value"]
    nodes = [
        g.set("fuel", g.get("fuel_max")),
        g.lua(MODULE, "launch", velocity=g.get("start_velocity")),
        g.call("gamestate.set", key="lander_base", value=base),
        g.call(
            "gamestate.set", key="lander_score", value=g.call("gamestate.get", key="lander_score", default=0)["value"]
        ),
        g.call("gamestate.set", key="lander_retry", value=g.get("this_scene")),
        g.call("gamestate.set", key="lander_landing", value=g.get("landing")),
        g.call("gamestate.set", key="lander_state", value="fly"),
        g.call("gamestate.set", key="lander_crash", value="none"),
        g.call("gamestate.set", key="lander_paused", value=False),
        g.call(
            "ui.set_text",
            entity_id=widget("LandingText"),
            text=g.fmt("LANDING %d / %d", g.get("landing"), g.get("landings")),
        ),
        g.call("ui.set_text", entity_id=widget("PadText"), text=g.get("pad_name")),
        g.call(
            "ui.set_text",
            entity_id=widget("ScoreText"),
            text=g.fmt("SCORE %d", g.call("gamestate.get", key="lander_score", default=0)["value"]),
        ),
        g.node("func.call", function="say", text=g.op("text.concat", "LAND ON ", g.get("pad_name"))),
    ]
    g.join([branch["False"], fade_in], nodes[0])
    g.chain(nodes[0], *nodes[1:])

    # ---------------------------------------------------------------- on_tick: the flight
    tick = g.event("on_tick")
    flying = g.branch(
        g.op("logic.and", g.op("compare.equal", g.get("state"), "fly"), g.op("logic.not", g.get("paused")))
    )
    g.chain(tick, flying)
    has_fuel = g.op("compare.greater", g.get("fuel"), 0)
    throttle = g.node(
        "logic.select",
        Condition=has_fuel,
        **{
            "True": g.node(
                "math.clamp", Value=g.call("input.get_action_value", action="thrust")["value"], Min=0, Max=1
            )["Result"],
            "False": 0,
        },
    )
    attitude = g.call("input.get_action_vector", action="attitude")["xy"]
    tilt = g.node("math.break_vector2", Value=attitude)
    invert = g.node(
        "logic.select",
        Condition=g.call("settings.get", key="lander_invert_pitch", default=False)["value"],
        **{"True": -1, "False": 1},
    )["Result"]
    slide = g.node("math.break_vector2", Value=g.call("input.get_action_vector", action="rcs")["xy"])
    set_throttle = g.set("throttle", throttle["Result"])
    fly = g.lua(
        MODULE,
        "fly",
        hull=hull(),
        throttle=g.get("throttle"),
        pitch=g.op("math.multiply", tilt["Y"], invert),
        roll=tilt["X"],
        slide_x=g.node("logic.select", Condition=has_fuel, **{"True": slide["X"], "False": 0})["Result"],
        slide_z=g.node(
            "logic.select", Condition=g.op("compare.greater", g.get("fuel"), 0), **{"True": slide["Y"], "False": 0}
        )["Result"],
        thrust=g.get("thrust"),
        turn_rate=g.get("turn_rate"),
        rcs=g.get("rcs"),
    )
    burn = g.set(
        "fuel",
        g.node(
            "math.max",
            A=0,
            B=g.op(
                "math.subtract",
                g.get("fuel"),
                g.op("math.multiply", g.op("math.multiply", fly["burn"], g.get("burn_rate")), tick["dt"]),
            ),
        )["Result"],
    )
    g.chain(flying["True"], set_throttle, fly, burn)

    # ---------------------------------------------------------------- on_update: instruments, camera, rules
    update = g.event("on_update")
    pause_pressed = g.op(
        "logic.and",
        g.call("input.is_action_just_pressed", action="pause")["pressed"],
        g.op("compare.equal", g.get("state"), "fly"),
    )
    pause_branch = g.branch(pause_pressed)
    g.chain(update, pause_branch)
    toggle = g.node("func.call", function="set_paused", paused=g.op("logic.not", g.get("paused")))
    g.chain(pause_branch["True"], toggle)
    running = g.join([pause_branch["False"], toggle], g.branch(g.get("paused")))
    telemetry = g.lua(MODULE, "telemetry", hull=hull())
    predict = g.lua(
        MODULE,
        "predict",
        hull=hull(),
        count=g.get("beads"),
        visible=g.op(
            "logic.and",
            g.op("compare.equal", g.get("state"), "fly"),
            g.op("logic.not", g.call("settings.get", key="lander_hide_path", default=False)["value"]),
        ),
    )
    view = g.lua(
        MODULE,
        "camera",
        camera=find("ChaseCamera"),
        hull=hull(),
        distance=g.get("chase_distance"),
        altitude=telemetry["altitude"],
        dt=update["dt"],
    )
    nodes = [
        telemetry,
        g.call("ui.set_text", entity_id=widget("AltText"), text=g.fmt("ALT %7.1f m", telemetry["altitude"])),
        g.call("ui.set_text", entity_id=widget("VsText"), text=g.fmt("V/S %+7.1f m/s", telemetry["vertical"])),
        g.call("ui.set_text", entity_id=widget("HsText"), text=g.fmt("H/S %7.1f m/s", telemetry["horizontal"])),
        g.call(
            "ui.set_progress", entity_id=widget("FuelBar"), value=g.op("math.divide", g.get("fuel"), g.get("fuel_max"))
        ),
        g.call(
            "ui.set_text",
            entity_id=widget("FuelText"),
            text=g.fmt(
                "FUEL %d%%",
                g.node(
                    "math.ceil", A=g.op("math.multiply", g.op("math.divide", g.get("fuel"), g.get("fuel_max")), 100)
                )["Result"],
            ),
        ),
        g.call("gamestate.set", key="lander_alt", value=telemetry["altitude"]),
        g.call("gamestate.set", key="lander_fuel", value=g.get("fuel")),
        g.call("gamestate.set", key="lander_vs", value=telemetry["vertical"]),
        g.call(
            "gamestate.set", key="lander_upright", value=g.lua(MODULE, "upright", pure=True, hull=hull())["upright"]
        ),
        view,
        g.call("gamestate.set", key="lander_view_yaw", value=view["yaw"]),
        g.call("gamestate.set", key="lander_view_zoom", value=view["zoom"]),
        predict,
        g.call("gamestate.set", key="lander_impact", value=predict["impact"]),
        g.call("gamestate.set", key="lander_hs", value=telemetry["horizontal"]),
        g.call("ui.set_visible", entity_id=find("Flame"), visible=g.op("compare.greater", g.get("throttle"), 0.03)),
        g.call(
            "transform.set_scale",
            entity_id=find("Flame"),
            scale=g.node(
                "math.make_vector3", X=1, Y=g.op("math.add", 0.05, g.op("math.multiply", g.get("throttle"), 1.8)), Z=1
            )["Result"],
        ),
        g.call(
            "entity.set_light_intensity",
            entity_id=find("FlameLight"),
            intensity=g.op("math.multiply", g.get("throttle"), 6000),
        ),
        g.set("engine_clock", g.op("math.subtract", g.get("engine_clock"), update["dt"])),
    ]
    rumble = g.branch(
        g.op(
            "logic.and",
            g.op("compare.greater", g.get("throttle"), 0.1),
            g.op("compare.less_equal", g.get("engine_clock"), 0),
        )
    )
    g.chain(running["False"], *nodes, rumble)
    grain = g.chain(
        rumble["True"], g.call("sound.play", asset_path="sounds/game/engine.wav"), g.set("engine_clock", 0.32)
    )
    rules = g.join([rumble["False"], grain], g.sequence())
    g.chain(
        rules["Then 1"],
        g.node("func.call", function="check_flight", altitude=telemetry["altitude"], ground=telemetry["ground"]),
    )
    g.chain(rules["Then 2"], g.node("func.call", function="check_end", dt=update["dt"]))

    # check_flight(altitude, ground): touchdown, crash, fuel, rest on the ground.
    g.function("check_flight", [("altitude", "number"), ("ground", "entity")])
    entry = g.entry("check_flight")
    is_flying = g.branch(g.op("compare.equal", g.get("state"), "fly"))
    g.chain(entry, is_flying)
    # The touchdown is the first sole on the ground: a short ray from the foot of each leg, not the hull's altitude.
    feet = g.lua(MODULE, "feet", hull=hull(), margin=0.06)
    first_touch = g.branch(
        g.op("logic.and", g.op("compare.less", g.get("touchdown"), 0), g.op("compare.greater", feet["contacts"], 0))
    )
    g.chain(
        is_flying["True"],
        feet,
        g.call("gamestate.set", key="lander_feet", value=feet["contacts"]),
        g.call("gamestate.set", key="lander_clearance", value=feet["clearance"]),
        first_touch,
    )
    speed = g.lua(MODULE, "speed", hull=hull())
    touched = g.chain(
        first_touch["True"],
        speed,
        g.set("touchdown", speed["speed"]),
        g.call("sound.play", asset_path="sounds/game/touchdown.wav"),
        g.call("gamestate.set", key="lander_touchdown", value=g.get("touchdown")),
        g.call("gamestate.set", key="lander_touch_alt", value=entry["altitude"]),
    )
    reason = g.call("gamestate.get", key="lander_crash", default="none")["value"]
    crashed = g.join([first_touch["False"], touched], g.branch(g.op("compare.not_equal", reason, "none")))
    g.chain(
        crashed["True"],
        g.node(
            "func.call",
            function="lose",
            reason=g.call("gamestate.get", key="lander_crash", default="none")["value"],
            sound="sounds/game/crash.wav",
        ),
    )
    dry = g.branch(
        g.op("logic.and", g.op("compare.less_equal", g.get("fuel"), 0), g.op("compare.greater", entry["altitude"], 3))
    )
    g.chain(crashed["False"], dry)
    g.chain(dry["True"], g.node("func.call", function="lose", reason="OUT OF FUEL", sound="sounds/game/low_fuel.wav"))
    lost = g.branch(g.op("compare.greater", entry["altitude"], 600))
    g.chain(dry["False"], lost)
    g.chain(
        lost["True"], g.node("func.call", function="lose", reason="LOST IN SPACE", sound="sounds/game/low_fuel.wav")
    )
    low = g.branch(
        g.op(
            "logic.and",
            g.op("logic.not", g.get("warned")),
            g.op("compare.less", g.get("fuel"), g.op("math.multiply", g.get("fuel_max"), 0.2)),
        )
    )
    g.chain(lost["False"], low)
    warned = g.chain(
        low["True"],
        g.set("warned", True),
        g.call("sound.play", asset_path="sounds/game/low_fuel.wav"),
        g.node("func.call", function="say", text="LOW FUEL"),
    )
    resting = g.join([low["False"], warned], g.lua(MODULE, "resting", hull=hull(), tilt=0.9))
    settled = g.branch(g.op("logic.and", resting["resting"], g.op("compare.greater_equal", feet["contacts"], 3)))
    g.chain(resting, settled)
    g.chain(settled["False"], g.set("rest", 0))
    long_enough = g.branch(g.op("compare.greater", g.get("rest"), 1.2))
    g.chain(
        settled["True"], g.set("rest", g.op("math.add", g.get("rest"), g.call("time.delta")["seconds"])), long_enough
    )
    on_pad = g.branch(g.op("compare.equal", entry["ground"], find(g.get("pad_tag"))))
    g.chain(long_enough["True"], on_pad)
    g.chain(on_pad["True"], g.node("func.call", function="win"))
    g.chain(
        on_pad["False"],
        g.node("func.call", function="lose", reason="LANDED OFF THE PAD", sound="sounds/ui/game_over.wav"),
    )
    g.chain(is_flying["False"], g.node("func.return", function="check_flight"))

    # check_end(dt): a few seconds after the end of a landing, the next scene.
    g.function("check_end", [("dt", "number")])
    entry = g.entry("check_end")
    fading = g.set("message_time", g.op("math.subtract", g.get("message_time"), entry["dt"]))
    expired = g.branch(
        g.op("logic.and", g.op("compare.less", g.get("message_time"), 0), g.op("compare.equal", g.get("state"), "fly"))
    )
    g.chain(entry, fading, expired)
    hidden = g.chain(expired["True"], g.call("ui.set_visible", entity_id=widget("Message"), visible=False))
    over = g.join(
        [expired["False"], hidden],
        g.branch(
            g.op("logic.and", g.op("compare.not_equal", g.get("state"), "fly"), g.op("logic.not", g.get("leaving")))
        ),
    )
    tick_clock = g.set("clock", g.op("math.add", g.get("clock"), entry["dt"]))
    done = g.branch(g.op("compare.greater", g.get("clock"), 3.0))
    g.chain(over["True"], tick_clock, done)
    target = g.node(
        "logic.select",
        Condition=g.op("compare.equal", g.get("state"), "landed"),
        **{"True": g.get("next_scene"), "False": "scenes/game_over.owl"},
    )
    g.chain(
        done["True"],
        g.set("leaving", True),
        g.call("scene.transition_to", scene_path=target["Result"], kind="fade", duration=0.8, rgba=(0, 0, 0.02, 1)),
    )

    # win(): the score of the landing, its line in the tally.
    g.function("win")
    entry = g.entry("win")
    distance = g.lua(MODULE, "pad_distance", hull=hull(), pad=find(g.get("pad_tag")))
    score = g.lua(
        MODULE,
        "score",
        pure=True,
        fuel=g.get("fuel"),
        distance=distance["distance"],
        impact=g.node("math.max", A=0, B=g.get("touchdown"))["Result"],
    )
    total = g.op("math.add", g.call("gamestate.get", key="lander_score", default=0)["value"], score["score"])
    line = g.fmt("%d  %-18s %6d", g.get("landing"), g.get("pad_name"), score["score"])
    g.chain(
        entry,
        g.set("state", "landed"),
        g.set("clock", 0),
        distance,
        g.call("gamestate.set", key="lander_score", value=total),
        g.call("gamestate.set", key="lander_state", value="landed"),
        g.call("gamestate.set", key=g.op("text.concat", "lander_line_", g.get("landing")), value=line),
        g.call(
            "ui.set_text",
            entity_id=widget("ScoreText"),
            text=g.fmt("SCORE %d", g.call("gamestate.get", key="lander_score", default=0)["value"]),
        ),
        g.call("sound.play", asset_path="sounds/game/landed.wav"),
        g.call("ui.show_subtitle", text="[landing chime]", seconds=2),
        g.node("func.call", function="say", text=g.fmt("TOUCHDOWN  +%d", score["score"])),
        g.node("func.return", function="win"),
    )

    # lose(reason, sound): the end of the run, its line on the game over screen.
    g.function("lose", [("reason", "string"), ("sound", "string")])
    entry = g.entry("lose")
    g.chain(
        entry,
        g.set("state", "lost"),
        g.set("clock", 0),
        g.set("throttle", 0),
        g.call("gamestate.set", key="lander_state", value="lost"),
        g.call("gamestate.set", key="lander_reason", value=entry["reason"]),
        g.call(
            "gamestate.set",
            key=g.op("text.concat", "lander_line_", g.get("landing")),
            value=g.fmt("%d  %-18s %s", g.get("landing"), g.get("pad_name"), entry["reason"]),
        ),
        g.call("sound.play", asset_path=entry["sound"]),
        g.call("ui.show_subtitle", text=g.fmt("[%s]", g.node("text.lower", Text=entry["reason"])["Result"]), seconds=2),
        g.node("func.call", function="say", text=entry["reason"]),
        g.node("func.return", function="lose"),
    )

    # say(text): the message line.
    g.function("say", [("text", "string")])
    entry = g.entry("say")
    g.chain(
        entry,
        g.call("ui.set_text", entity_id=widget("Message"), text=entry["text"]),
        g.call("ui.set_visible", entity_id=widget("Message"), visible=True),
        g.set("message_time", 3.0),
        g.call("gamestate.set", key="lander_message", value=entry["text"]),
        g.node("func.return", function="say"),
    )

    # set_paused(paused): the scene clock stops, the pause window shows the controls.
    g.function("set_paused", [("paused", "boolean")])
    entry = g.entry("set_paused")
    show = g.branch(entry["paused"])
    g.chain(
        entry,
        g.set("paused", entry["paused"]),
        g.call("time.set_paused", paused=entry["paused"]),
        g.call("ui.set_visible", entity_id=widget("PauseWindow"), visible=entry["paused"]),
        g.call("gamestate.set", key="lander_paused", value=entry["paused"]),
        show,
    )
    g.chain(
        show["True"],
        g.call("ui.raise_window", entity_id=widget("PauseWindow")),
        g.call("ui.set_focus", entity_id=widget("ResumeButton")),
    )

    resume = g.event("on_resume_clicked")
    g.chain(resume, g.node("func.call", function="set_paused", paused=False))
    restart = g.event("on_restart_clicked")
    g.chain(
        restart,
        g.call("time.set_paused", paused=False),
        g.call(
            "gamestate.set", key="lander_score", value=g.call("gamestate.get", key="lander_base", default=0)["value"]
        ),
        g.call("scene.transition_to", scene_path=g.get("this_scene"), kind="fade", duration=0.4, rgba=(0, 0, 0.02, 1)),
    )
    quit_ = g.event("on_quit_clicked")
    g.chain(
        quit_,
        g.call("time.set_paused", paused=False),
        g.call("scene.transition_to", scene_path="scenes/title.owl", kind="fade", duration=0.4, rgba=(0, 0, 0.02, 1)),
    )
    return g.text()


def hull_graph() -> str:
    """The hull: a broken leg or the hull touching the ground ends the landing (``lander_crash``)."""
    g = Graph(6410000000000000000)
    broke = g.event("on_joint_break")
    first = g.branch(
        g.op("compare.equal", g.call("gamestate.get", key="lander_crash", default="none")["value"], "none")
    )
    g.chain(broke, first)
    g.chain(
        first["True"],
        g.call("sound.play", asset_path="sounds/game/leg_break.wav"),
        g.call("gamestate.set", key="lander_crash", value="LEG BROKEN"),
    )
    hit = g.event("on_collision")
    landed = g.call("gamestate.get", key="lander_state", default="fly")["value"]
    first = g.branch(
        g.op(
            "logic.and",
            g.op("compare.equal", g.call("gamestate.get", key="lander_crash", default="none")["value"], "none"),
            g.op("compare.equal", landed, "fly"),
        )
    )
    g.chain(hit, first, g.call("gamestate.set", key="lander_crash", value="HULL CRASHED"))
    return g.text()


# --------------------------------------------------------------------------------------------------------- project

PROJECT = """OwlProject:
  name: OwlLunarLander
  firstScene: scenes/title.owl
  author: Silmaen
  description: Lunar Lander, a sample game of the Owl Engine
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
        SunDirection: [-0.5, -0.4, -0.75]
        Ambient: [0.04, 0.04, 0.05]
    - Type: RendererPlanet
      Name: planet_world
      DefaultConfig:
        SunDirection: [-0.5, -0.4, -0.75]
        Ambient: [0.02, 0.02, 0.03]
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
  - {key: lander_invert_pitch, type: bool, value: false}
  - {key: lander_hide_path, type: bool, value: false}
  - {key: lander_best, type: int, value: 0}
"""
"""The settings and their defaults."""

ACTIONS = """FormatVersion: 1
# Actions of Lunar Lander, on top of the built-in ones (the `ui_*` actions drive the menus). Read by
# scripts/game/landing.owlvs.
actions:
  - name: thrust
    type: axis
    bindings:
      - {key: Space}
      - {mouse_button: Left}
      - {gamepad_axis: right_trigger}
      - {gamepad_button: A}
  - name: attitude
    type: vector
    bindings:
      - {key: D, component: x}
      - {key: A, component: x, scale: -1}
      - {key: S, component: y}
      - {key: W, component: y, scale: -1}
      - {gamepad_stick: left}
  - name: rcs
    type: vector
    bindings:
      - {key: Right, component: x}
      - {key: Left, component: x, scale: -1}
      - {key: Down, component: y}
      - {key: Up, component: y, scale: -1}
      - {gamepad_button: DpadRight, component: x}
      - {gamepad_button: DpadLeft, component: x, scale: -1}
      - {gamepad_button: DpadDown, component: y}
      - {gamepad_button: DpadUp, component: y, scale: -1}
  - name: camera_orbit
    type: vector
    bindings:
      - {key: E, component: x}
      - {key: Q, component: x, scale: -1}
      - {key: F, component: y}
      - {key: R, component: y, scale: -1}
      - {gamepad_stick: right}
  - name: camera_zoom
    type: axis
    bindings:
      - {key: X}
      - {key: Z, scale: -1}
      - {gamepad_button: RightBumper}
      - {gamepad_button: LeftBumper, scale: -1}
  - name: camera_drag
    type: button
    bindings:
      - {mouse_button: Right}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input actions."""


def main() -> None:
    """Write the project, the landings, the HUD and the graphs."""
    (ROOT / "scenes").mkdir(parents=True, exist_ok=True)
    (ROOT / "huds").mkdir(parents=True, exist_ok=True)
    (ROOT / "scripts" / "game").mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(SETTINGS)
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    for number, landing in enumerate(lay.LANDINGS, start=1):
        (ROOT / "scenes" / f"landing_{number}.owl").write_text(landing_scene(number, landing))
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    (ROOT / "scripts" / "game" / "landing.owlvs").write_text(landing_graph())
    (ROOT / "scripts" / "game" / "hull.owlvs").write_text(hull_graph())


if __name__ == "__main__":
    main()
