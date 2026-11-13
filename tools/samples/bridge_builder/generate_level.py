#!/usr/bin/env python3
"""
Write the project files, the six level scenes, the level select, the prefabs, the play HUD and the game graphs of the
Bridge builder sample game into ``samples/bridge_builder/``.

- ``owl_project.yml``, ``game_settings.yml``, ``input_actions.yml``;
- ``scenes/level_1.owl`` to ``level_6.owl``: the gorge seen from the side (the cliffs and the rock pillars, static
  2D bodies; the anchors; the vehicles on the left cliff), the valley behind it for the look, the build cursor and the
  preview of a part; the scene script ``scripts/game/level.owlvs`` with the properties of the level;
- ``scenes/level_select.owl`` and ``scripts/ui/level_select.owlvs``: a button per level, its stars;
- ``prefabs/``: the plan of each kind of part (a model only), its body for the test run (a root and a ``Tail`` part
  holding the joint of its far end, ``scripts/game/part.owlvs`` reporting a joint that snaps), the nodes;
- ``huds/play.owlhud``: the level, the cost, the kinds of parts, Test and Undo, the load, the result and pause windows;
- ``scripts/game/vehicle.owlvs``: a vehicle drives across during the test run, then waits at the start again. The
  lists of the build (nodes, parts, undo, joints of the run) are the Lua module ``scripts/game/bridge.lua``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.bridge_builder.generate_level``;
then ``OwlNest --upgrade-assets samples/bridge_builder`` gives the new files their ``.owlmeta``.
"""

from __future__ import annotations

import random
from pathlib import Path

from tools.samples import menu_kit as mk
from tools.samples import scene_yaml as y
from tools.samples.bridge_builder import layout as lay
from tools.samples.vs_graph import Graph, Node, Pin

ROOT = Path(__file__).resolve().parents[3] / "samples" / "bridge_builder"
"""The sample project."""
MESH = "RendererTag:\n  Name: mesh_world\n"
"""Draw an entity in the mesh layer."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""
TEXT = (1, 0.95, 0.85, 1)
"""Colour of the HUD texts."""
ACCENT = (1, 0.62, 0.25, 1)
"""Colour of the highlights."""
SOFT = (0.85, 0.82, 0.75, 1)
"""Colour of the secondary texts."""
FONT = "OpenSans-ExtraBold"
"""Font of the titles and of the buttons."""
MONO = "JetBrainsMono-Regular"
"""Font of the counters."""
FOV = 0.85
"""Vertical field of view of the camera of a level."""
CONTROLS = (
    "CURSOR        mouse, arrows / W A S D, D-pad or left stick\n"
    "LAY A PART    left click, Space, A: from a node, then at its end\n"
    "ERASE         right click, X, Delete, pad X (or stop drawing)\n"
    "UNDO          Z, Backspace, B\n"
    "KIND          1 2 3, Tab, LB / RB\n"
    "TEST RUN      R, Y (again to edit)\n"
    "VIEW          wheel, Q / E, middle drag, right stick, triggers\n"
    "GRID          G, Back\n"
    "PAUSE         Escape, P, Start"
)
"""The controls, in the How to play window of the title and in the pause window."""

DECK = 0.15
"""Height of the deck of a road above the line of its nodes."""
LAYERS = {"ground": (0, 255), "road": (1, 9), "beam": (2, 1), "vehicle": (3, 3), "anchor": (4, 0)}
"""Collision layer and mask of each kind of body: the vehicles drive on the ground and the road only."""


# ------------------------------------------------------------------------------------------------------ components


def mesh(path: str, colour: y.Colour = (1, 1, 1, 1), glow: float = 0.0) -> str:
    """A ``MeshRenderer`` of a model of the project, its materials glowing at ``glow`` nits when above 0."""
    out = f"MeshRenderer:\n  mesh: {path}\n  color: {y.vec(*colour)}\n  transparent: false\n"
    if glow > 0:
        out += f"  materials:\n    - {{overrides: {{emissiveLuminance: {y.num(glow)}}}}}\n"
    return out + MESH


def body(
    kind: str,
    shape: str,
    size: tuple[float, float, float],
    layer: str,
    density: float = 1.0,
    friction: float = 0.8,
    part: bool = False,
) -> str:
    """A 2D ``PhysicBody`` (in the plane of the bridge) on the layer of its kind."""
    number, mask = LAYERS[layer]
    out = (
        f"PhysicBody:\n  type: {kind}\n  shape: {shape}\n  fixedRotation: false\n  colliderSize: {y.vec(*size)}\n"
        f"  density: {y.num(density)}\n  restitution: 0\n  friction: {y.num(friction)}\n"
    )
    if number or mask != 255:
        out += f"  layer: {number}\n  mask: {mask}\n"
    if part:
        out += "  part: true\n"
    return out


def camera() -> str:
    """The perspective camera of a level, looking straight along -Z at the plane of the bridge."""
    return (
        "Camera:\n  camera:\n    projectionType: Perspective\n"
        f"    perspectiveFOV: {y.num(FOV)}\n    perspectiveNear: 0.3\n    perspectiveFar: 800\n"
        "    orthographicSize: 8\n    orthographicNear: -1\n    orthographicFar: 10\n    toneMapping: Aces\n"
        "    postProcess: {ev100: 14, exposure: 0, autoExposure: false, bloom: true, bloomThreshold: 1,"
        " bloomKnee: 0.5, bloomIntensity: 0.05, bloomScatter: 0.7, vignette: true, vignetteIntensity: 0.25,"
        " vignetteSmoothness: 0.5, vignetteColor: [0, 0, 0]}\n"
        "  primary: true\n  fixedAspectRatio: false\n"
    )


SUN = (
    "Light: {type: Directional, enabled: true, color: [1, 0.95, 0.85], intensity: 100000, castShadows: true,"
    " shadowDistance: 60, cascadeCount: 2, shadowResolution: 2048, shadowBias: 1.5}\n"
)
"""The afternoon sun."""
SKY = (
    "Environment: {hdri: '', ambient: [1, 1, 1], intensity: 9000, rotation: 0, showSky: true}\n"
    "ProceduralSky: {enabled: true, sunDirection: [-0.4, 0.6, 0.7], turbidity: 3, groundColor: [0.32, 0.36, 0.26],"
    " nightColor: [0.42, 0.48, 0.66], nightBrightness: 0.02, stars: 0, discSize: 2.5, resolution: 256}\n"
)
"""A clear sky."""

HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: ui, Enabled: false}\n"
    "Physics:\n  solverSubSteps: 4\n  velocitySteps: 24\n  positionSteps: 6\n  interpolate: true\n  workerCount: 0\n"
    "  gravity: [0, -9.81, 0]\n"
)
"""Root keys of every level: the mesh layer, a stiff solver for long chains of joints."""


def valley(entities: y.Entities, width: float, seed: int) -> None:
    """The look behind the gorge: the sun, the sky, the river, mountains and trees."""
    rng = random.Random(seed)
    entities.add("Sun", [SUN], rotation=(-0.75, -0.55, 0.0))
    entities.add("Sky", [SKY])
    entities.add("River", [mesh("models/water.glb")], position=(width / 2, -11.5, -20), scale=(width + 80, 1, 60))
    for index in range(9):
        x = width / 2 + (index - 4) * 26 + rng.uniform(-6, 6)
        size = rng.uniform(28, 46)
        entities.add(
            "Mountain",
            [mesh("models/mountain.glb")],
            position=(x, -12, -95 - rng.uniform(0, 30)),
            scale=(size * 1.4, size, size),
        )
    for _ in range(26):
        side = rng.choice([-1, 1])
        x = width / 2 + side * (width / 2 + rng.uniform(2, 26))
        top = 0.0 if side < 0 else 0.0
        size = rng.uniform(2.5, 4.5)
        entities.add(
            "Tree", [mesh("models/tree.glb")], position=(x, top, -rng.uniform(4, 22)), scale=(size, size, size)
        )


def gorge(entities: y.Entities, level: lay.Level) -> None:
    """The two cliffs and the rock pillars: static bodies in the plane, deep blocks for the look."""
    # The tops of the cliffs are flush with the deck of a road laid from their anchors.
    for tag, x0, x1, top in (
        ("CliffLeft", -40.0, 0.0, DECK),
        ("CliffRight", float(level.width), level.width + 40.0, level.right_top + DECK),
    ):
        height = top + 20.0
        components = [mesh("models/cliff.glb"), body("Static", "Box", (1, 1, 1), "ground")]
        # The block reaches just in front of the plane of the bridge: its face at the gap stays in view.
        entities.add(tag, components, position=((x0 + x1) / 2, top - height / 2, -7), scale=(x1 - x0, height, 17))
    for x, top in level.pillars:
        height = top + 20.0
        components = [mesh("models/cliff.glb", (0.9, 0.88, 0.85, 1)), body("Static", "Box", (1, 1, 1), "ground")]
        entities.add("Pillar", components, position=(x, top - height / 2 - 0.3, 0), scale=(2.4, height, 4))
    if level.towers:
        for tower_x in (-1.0, level.width + 1.0):
            entities.add(
                "Tower",
                [mesh("models/cliff.glb", (0.8, 0.78, 0.75, 1))],
                position=(tower_x, 2.4, -1.8),
                scale=(2, 4.8, 1.6),
            )


def anchors(entities: y.Entities, level: lay.Level) -> None:
    """The fixed nodes: static bodies colliding with nothing, the joints of the run hold to them."""
    for x, y_ in level.anchors:
        components = [mesh("models/anchor.glb"), body("Static", "Sphere", (0.4, 0.4, 0.4), "anchor")]
        entities.add("Anchor", components, position=(x, y_, 0))


def vehicles(entities: y.Entities, level: lay.Level) -> None:
    """The vehicles on the left cliff, one behind the other, each with its two wheels as parts of its body."""
    x = -3.0
    for index, kind in enumerate(level.vehicles, start=1):
        length, height, density = lay.VEHICLES[kind]
        x -= length / 2
        script = y.script(
            "scripts/game/vehicle.owlvs",
            [("finish_x", "float", level.width + 5.0), ("fall_y", "float", -7.0), ("speed", "float", 4.5)],
        )
        root = entities.add(
            f"Vehicle{index}",
            [
                mesh(f"models/{kind}.glb"),
                body("Dynamic", "Box", (length * 0.9, height * 0.45, 1), "vehicle", density, 0.2),
                script,
            ],
            position=(x, 0.6 + height * 0.25, 0),
        )
        for wheel, wx in (("Back", -length * 0.33), ("Front", length * 0.33)):
            entities.add(
                f"Vehicle{index}Wheel{wheel}",
                [body("Dynamic", "Sphere", (0.7, 0.7, 0.7), "vehicle", density, 0.15, part=True)],
                position=(wx, -height * 0.25, 0),
                parent=root,
            )
        x -= length / 2 + 1.2


def level_scene(number: int, level: lay.Level) -> str:
    """One level."""
    entities = y.Entities(7500000000000000000 + number * 10000)
    valley(entities, level.width, number)
    gorge(entities, level)
    anchors(entities, level)
    vehicles(entities, level)
    distance = 20.0 + level.width * 0.6
    entities.add("LevelCamera", [camera()], position=(level.width / 2, -1.5, distance))
    entities.add("Cursor", [mesh("models/cursor.glb", glow=40000)])
    entities.add("Preview", [mesh("models/wood.glb", (0.4, 1, 0.5, 0.8))], visible=False)
    entities.add("BuildMusic", [y.music("sounds/music/build.wav", 0.45)])
    following = f"scenes/level_{number + 1}.owl" if number < len(lay.LEVELS) else "scenes/victory.owl"
    properties = [
        ("level", "int", number),
        ("level_name", "string", level.name),
        ("this_scene", "string", f"scenes/level_{number}.owl"),
        ("next_scene", "string", following),
        ("budget", "int", level.budget),
        ("star_budget", "int", level.star_budget),
        ("vehicles", "int", len(level.vehicles)),
        ("grid_min_x", "int", -2),
        ("grid_min_y", "int", -8),
        ("grid_max_x", "int", level.width + 2),
        ("grid_max_y", "int", 6),
    ]
    script = y.script("scripts/game/level.owlvs", properties).replace("LuaScript:", "SceneScript:")
    return y.scene(f"level_{number}", entities, "huds/play.owlhud", script + HEADER)


# --------------------------------------------------------------------------------------------------------- prefabs


def prefab(name: str, entities: y.Entities) -> str:
    """A prefab file of the entities added."""
    return f"Prefab: {name}\nVersion: 1\nEntities:\n" + entities.text()


def prefabs() -> dict[str, str]:
    """The plans of the parts, their bodies for the run and the nodes, by file name."""
    out: dict[str, str] = {}
    kinds = {"road": ("road", 1.0, 0.3, 30.0), "wood": ("beam", 1.0, 0.22, 18.0), "steel": ("beam", 1.0, 0.18, 40.0)}
    for index, (kind, (layer, _, thickness, density)) in enumerate(kinds.items()):
        plan = y.Entities(7590000000000000000 + index * 100)
        plan.add("Plan", [mesh(f"models/{kind}.glb", (0.85, 0.9, 1, 1))])
        out[f"plan_{kind}.owlprefab"] = prefab(f"Plan {kind}", plan)
        run = y.Entities(7591000000000000000 + index * 100)
        root = run.add(
            "Part",
            [
                mesh(f"models/{kind}.glb"),
                body("Dynamic", "Box", (1, thickness, 1), layer, density, 0.9),
                y.script("scripts/game/part.owlvs"),
            ],
        )
        run.add(
            "Tail",
            [
                body("Dynamic", "Sphere", (0.05, 0.05, 0.05), layer, density, 0.9, part=True),
                y.script("scripts/game/part.owlvs"),
            ],
            position=(0.5, 0, 0),
            parent=root,
        )
        out[f"{kind}.owlprefab"] = prefab(kind.capitalize(), run)
    node = y.Entities(7592000000000000000)
    node.add("Node", [mesh("models/node.glb")])
    out["node.owlprefab"] = prefab("Node", node)
    anchor = y.Entities(7592000000000000100)
    anchor.add("AnchorMark", [mesh("models/node.glb", (1, 0.5, 0.4, 1))], scale=(1.3, 1.3, 1.05))
    out["anchor_node.owlprefab"] = prefab("Anchor node", anchor)
    return out


# ------------------------------------------------------------------------------------------------------------- HUD


def _label(value: str, size: int, colour: y.Colour = TEXT, align: str = "Center", font: str = FONT) -> str:
    return y.text(value, size, align, colour, font=font, outline=(0.12, 0.06, 0.02, 1), outline_width=2)


LOOK = mk.Look(
    prefix="bridge",
    title_font=FONT,
    mono_font=MONO,
    text=TEXT,
    soft=SOFT,
    accent=ACCENT,
    danger=(1, 0.4, 0.3, 1),
    button=(0.28, 0.18, 0.1),
    button_hover=(0.42, 0.28, 0.15, 1),
    button_pressed=(0.52, 0.34, 0.18, 1),
    window_body=(0.14, 0.09, 0.05, 0.96),
    window_title=(0.42, 0.25, 0.12, 1),
    outline=(0.12, 0.06, 0.02, 1),
    transition="wipe_left",
    sweep=(0.32, 0.18, 0.08, 1),
    header="",
    backdrop=lambda _e, _n: None,
    title_art="textures/ui/title.png",
    frame="textures/ui/frame.png",
    toggles=[("bridge_hide_stress", "HIDE LOAD COLOURS"), ("bridge_slow_run", "SLOW TEST RUN")],
    first_scene="scenes/level_select.owl",
)
"""The look of the game, the backdrop set by ``generate_menus``."""


def tool_button(entities: y.Entities, parent: int, tag: str, text: str, callback: str, width: float) -> int:
    """A HUD button for the mouse only: the keys and the pad drive the build cursor, not the focus."""
    components = [
        y.child_rect((width, 1)),
        y.button(callback, LOOK.button, LOOK.button_hover, LOOK.button_pressed),
        _label(text, 22),
        y.focus(focusable=False, colour=ACCENT),
    ]
    return entities.add(tag, components, parent=parent)


def hud() -> str:
    """Level and cost at the top left, the tools at the top right, the load at the bottom, a message line, the result
    and the pause windows (the only place of a level showing the controls)."""
    entities = y.Entities(7599000000000000000)
    root = entities.add("HUDCanvas", [y.canvas()])
    left = entities.add(
        "TopLeft",
        [y.rect((0, 0), (0.42, 0.15), (0.03, 0.02), SCREEN, SCREEN), y.layout("Column", 0, cross="Start")],
        parent=root,
    )
    entities.add("StarText", [y.child_rect((1, 0.28)), _label("", 18, SOFT, "Left", MONO)], parent=left)
    entities.add("CostText", [y.child_rect((1, 0.32)), _label("COST 0 / 0", 24, ACCENT, "Left", MONO)], parent=left)
    entities.add("LevelText", [y.child_rect((1, 0.38)), _label("", 28, TEXT, "Left")], parent=left)
    tools = entities.add(
        "Tools", [y.rect((1, 0), (0.8, 0.06), (-0.03, 0.03), SCREEN, SCREEN), y.layout("Row", 4)], parent=root
    )
    for tag, text, callback, width in reversed(
        [
            ("RoadButton", "ROAD", "on_road_clicked", 0.15),
            ("WoodButton", "WOOD", "on_wood_clicked", 0.15),
            ("SteelButton", "STEEL", "on_steel_clicked", 0.15),
            ("UndoButton", "UNDO", "on_undo_clicked", 0.15),
            ("RunButton", "TEST RUN", "on_run_clicked", 0.26),
        ]
    ):
        tool_button(entities, tools, tag, text, callback, width)
    entities.add(
        "KindText",
        [y.rect((1, 0), (0.6, 0.045), (-0.03, 0.1), SCREEN, SCREEN), _label("", 20, SOFT, "Right", MONO)],
        parent=root,
    )
    load = entities.add(
        "LoadGroup",
        [y.rect((0.5, 1), (0.7, 0.06), (0, -0.03), SCREEN, SCREEN), y.layout("Column", 2)],
        parent=root,
        visible=False,
    )
    bar = "UiProgressBar:\n  value: 0\n  backgroundColor: [0.1, 0.07, 0.04, 0.8]\n  fillColor: [1, 0.45, 0.25, 1]\n"
    entities.add("LoadBar", [y.child_rect((1, 0.35)), bar], parent=load)
    entities.add("LoadText", [y.child_rect((1, 0.55)), _label("LOAD 0%", 18, SOFT, font=MONO)], parent=load)
    entities.add(
        "Message",
        [y.rect((0.5, 0), (1.2, 0.08), (0, 0.2), SCREEN, SCREEN), _label("", 34, ACCENT)],
        parent=root,
        visible=False,
    )
    result = mk.window(LOOK, entities, root, "ResultWindow", "BRIDGE HOLDS", (0.9, 0.76), modal=True)
    for index, (tag, text, callback) in reversed(
        list(
            enumerate(
                [
                    ("NextButton", "NEXT", "on_next_clicked"),
                    ("ReplayButton", "REPLAY THE RUN", "on_replay_clicked"),
                    ("EditButton", "IMPROVE IT", "on_edit_clicked"),
                    ("LevelsButton", "LEVELS", "on_levels_clicked"),
                ]
            )
        )
    ):
        mk.button(LOOK, entities, result, tag, text, callback, (0.6, 0.12), index == 0)
    entities.add("ResultStars", [y.child_rect((0.94, 0.12)), _label("", 30, ACCENT, font=MONO)], parent=result)
    entities.add("ResultCost", [y.child_rect((0.94, 0.12)), _label("", 26, TEXT, font=MONO)], parent=result)
    pause = mk.window(LOOK, entities, root, "PauseWindow", "PAUSED", (1.2, 0.88))
    for index, (tag, text, callback) in reversed(
        list(
            enumerate(
                [
                    ("ResumeButton", "RESUME", "on_resume_clicked"),
                    ("ClearButton", "CLEAR THE BRIDGE", "on_clear_clicked"),
                    ("QuitButton", "LEVELS", "on_levels_clicked"),
                ]
            )
        )
    ):
        mk.button(LOOK, entities, pause, tag, text, callback, (0.6, 0.09), index == 0)
    controls = y.text(CONTROLS, 18, "Left", ACCENT, wrap=True, font=MONO, line_spacing=0.2)
    entities.add("PauseControls", [y.child_rect((0.95, 0.5)), controls], parent=pause)
    return y.hud_file("play", entities)


def level_select_scene(backdrop: object) -> str:
    """The level select: a button per level (its name and its stars), Back."""
    entities = y.Entities(7598000000000000000)
    backdrop(entities, "level_select")  # type: ignore[operator]
    entities.add("MenuMusic", [y.music("sounds/music/menu.wav", 0.55)])
    root = entities.add("LevelCanvas", [y.canvas(), y.script("scripts/ui/level_select.owlvs")])
    group = entities.add(
        "LevelGroup", [y.rect((0.5, 0), (1.0, 0.92), (0, 0.04), SCREEN, SCREEN), y.layout("Column", 8)], parent=root
    )
    mk.button(LOOK, entities, group, "BackButton", "BACK", "on_back_clicked", (0.5, 0.09), False)
    for index in reversed(range(1, len(lay.LEVELS) + 1)):
        mk.button(
            LOOK, entities, group, f"LevelButton{index}", "", f"on_level_{index}_clicked", (0.9, 0.095), index == 1
        )
    entities.add("SelectTitle", [y.child_rect((1, 0.14)), mk.label(LOOK, "CHOOSE A GAP", 52)], parent=group)
    return y.scene("level_select", entities, header=GENERIC_HEADER)


GENERIC_HEADER = (
    "EnabledRenderers:\n  - {Name: world, Enabled: false}\n  - {Name: mesh_world, Enabled: true}\n"
    "  - {Name: ui, Enabled: false}\n"
)
"""Root keys of a menu: the mesh layer."""


# ---------------------------------------------------------------------------------------------------------- graphs

MODULE = "scripts.game.bridge"
"""The Lua module of the build."""


def level_graph() -> str:
    """The rules of a level: build on the grid, test run, the vehicles across or not, the stars, pause."""
    g = Graph(6500000000000000000)
    g.var("level", "int", 1)
    g.var("level_name", "string", "FIRST CROSSING")
    g.var("this_scene", "string", "scenes/level_1.owl")
    g.var("next_scene", "string", "scenes/level_2.owl")
    g.var("budget", "int", 2600, low=0)
    g.var("star_budget", "int", 1900, low=0)
    g.var("vehicles", "int", 1, low=1)
    g.var("grid_min_x", "int", -2)
    g.var("grid_min_y", "int", -8)
    g.var("grid_max_x", "int", 10)
    g.var("grid_max_y", "int", 6)
    g.var("run_time", "float", 30.0, low=1)
    g.var("fov", "float", FOV, low=0.1)
    g.var("levels", "int", len(lay.LEVELS), low=1)
    g.var("names", "strings", ", ".join(level.name for level in lay.LEVELS))
    for name, kind, default in (
        ("state", "string", "build"),
        ("clock", "float", 0.0),
        ("paused", "bool", False),
        ("leaving", "bool", False),
        ("message_time", "float", 0.0),
        ("warned", "bool", False),
        ("total", "float", 0.0),
        ("replay_wait", "float", 0.0),
    ):
        g.var(name, kind, default, exposed=False)

    def find(tag: object) -> Pin:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    def widget(tag: str) -> Pin:
        return g.call("ui.find", name=tag)["entity_id"]

    def pressed(action: str) -> Pin:
        return g.call("input.is_action_just_pressed", action=action)["pressed"]

    def key(prefix: str, value: object) -> Pin:
        return g.op("text.concat", prefix, value)

    def call(function: str, **values: object) -> Node:
        return g.node("func.call", function=function, **values)

    def state_is(name: str) -> Pin:
        return g.op("compare.equal", g.get("state"), name)

    # ---------------------------------------------------------------- on_create
    create = g.event("on_create")
    branch = g.branch(g.op("logic.not", g.call("ui.is_transition_active")["active"]))
    g.chain(create, branch)
    fade_in = g.call("ui.transition_play", kind="wipe_left", duration=0.5, rgba=LOOK.sweep)
    g.chain(branch["True"], fade_in)
    design = g.call("settings.get", key=key("bridge_design_", g.get("level")), default="none")["value"]
    setup = g.lua(
        MODULE,
        "setup",
        min_x=g.get("grid_min_x"),
        min_y=g.get("grid_min_y"),
        max_x=g.get("grid_max_x"),
        max_y=g.get("grid_max_y"),
        design=design,
    )
    g.join([branch["False"], fade_in], setup)
    g.chain(
        setup,
        g.call("gamestate.set", key="bridge_level", value=g.get("level")),
        g.call("gamestate.set", key="bridge_state", value="build"),
        g.call("gamestate.set", key="bridge_arrived", value=0),
        g.call("gamestate.set", key="bridge_fallen", value=0),
        g.call("gamestate.set", key="bridge_breaks", value=0),
        g.call("gamestate.set", key="bridge_paused", value=False),
        g.call("gamestate.set", key="bridge_budget", value=g.get("budget")),
        g.call("gamestate.set", key="bridge_anchors", value=setup["anchors"]),
        g.call("ui.set_text", entity_id=widget("LevelText"), text=g.fmt("%d. %s", g.get("level"), g.get("level_name"))),
        g.call("ui.set_text", entity_id=widget("StarText"), text=g.fmt("3 STARS UNDER %d", g.get("star_budget"))),
        call("show_kind", label=g.lua(MODULE, "set_kind", kind="road")["label"]),
        call("refresh"),
        call("say", text="BUILD A BRIDGE ACROSS"),
    )

    # ---------------------------------------------------------------- on_update
    update = g.event("on_update")
    toggle = call("set_paused", paused=g.op("logic.not", g.get("paused")))
    pause_branch = g.branch(g.op("logic.and", pressed("pause"), g.op("logic.not", state_is("won"))))
    g.chain(update, pause_branch, toggle)
    running = g.join([pause_branch["False"], toggle], g.branch(g.get("paused")))
    message = g.set("message_time", g.op("math.subtract", g.get("message_time"), update["dt"]))
    expired = g.branch(g.op("compare.less", g.get("message_time"), 0))
    g.chain(running["False"], message, expired)
    hide = g.chain(expired["True"], g.call("ui.set_visible", entity_id=widget("Message"), visible=False))
    dispatch = g.join([expired["False"], hide], g.sequence())
    building = g.branch(state_is("build"))
    g.chain(dispatch["Then 1"], building, call("build_update", dt=update["dt"]))
    run = g.branch(state_is("run"))
    g.chain(dispatch["Then 2"], run, call("run_update", dt=update["dt"]))
    priced = g.chain(
        dispatch["Then 3"], g.call("gamestate.set", key="bridge_cost", value=g.lua(MODULE, "cost", pure=True)["cost"])
    )
    # A replay waits a moment in the plan, so the bodies of the last run are gone and the vehicles are back at start.
    waiting = g.branch(g.op("compare.greater", g.get("replay_wait"), 0))
    counted = g.chain(priced, waiting, g.set("replay_wait", g.op("math.subtract", g.get("replay_wait"), update["dt"])))
    due = g.branch(g.op("compare.less_equal", g.get("replay_wait"), 0))
    g.chain(counted, due, call("begin_run"))

    # build_update(dt): the cursor, the parts laid and erased, undo, the kind, the test run.
    g.function("build_update", [("dt", "number")])
    entry = g.entry("build_update")
    over_ui = g.call("ui.is_mouse_over_ui")["over"]
    nodes = [
        g.lua(MODULE, "mouse_cursor", camera=find("LevelCamera"), fov=g.get("fov")),
        g.lua(
            MODULE,
            "stick",
            stick=g.call("input.get_action_vector", action="cursor_stick")["xy"],
            repeat_time=0.16,
            dt=entry["dt"],
        ),
    ]
    g.chain(entry, *nodes)
    exits: list[Node | Pin] = [nodes[-1]]
    for action, dx, dy in (("cursor_left", -1, 0), ("cursor_right", 1, 0), ("cursor_up", 0, 1), ("cursor_down", 0, -1)):
        test = g.join(exits, g.branch(pressed(action)))
        moved = g.lua(MODULE, "move_cursor", dx=dx, dy=dy)
        g.chain(test["True"], moved)
        exits = [test["False"], moved]
    click = g.op("logic.and", pressed("click"), g.op("logic.not", over_ui))
    place = g.join(exits, g.branch(g.op("logic.or", pressed("place"), click)))
    placed = call("place")
    g.chain(place["True"], placed)
    right_click = g.op("logic.and", pressed("click_erase"), g.op("logic.not", g.call("ui.is_mouse_over_ui")["over"]))
    erase = g.join([place["False"], placed], g.branch(g.op("logic.or", pressed("erase"), right_click)))
    erased = call("erase")
    g.chain(erase["True"], erased)
    undo = g.join([erase["False"], erased], g.branch(pressed("undo")))
    undone = call("undo")
    g.chain(undo["True"], undone)
    exits = [undo["False"], undone]
    for action, kind in (("kind_road", "road"), ("kind_wood", "wood"), ("kind_steel", "steel")):
        test = g.join(exits, g.branch(pressed(action)))
        chosen = call("show_kind", label=g.lua(MODULE, "set_kind", kind=kind)["label"])
        g.chain(test["True"], chosen)
        exits = [test["False"], chosen]
    cycle = g.join(exits, g.branch(pressed("kind_next")))
    cycled = call("show_kind", label=g.lua(MODULE, "next_kind")["label"])
    g.chain(cycle["True"], cycled)
    test_run = g.join([cycle["False"], cycled], g.branch(pressed("run")))
    started = call("begin_run")
    g.chain(test_run["True"], started)
    end = g.join([test_run["False"], started], g.lua(MODULE, "draw", cursor=find("Cursor"), preview=find("Preview")))
    cell = g.node("math.break_vector2", Value=g.lua(MODULE, "cursor", pure=True)["cell"])
    g.chain(
        end,
        g.call("gamestate.set", key="bridge_cursor_x", value=cell["X"]),
        g.call("gamestate.set", key="bridge_cursor_y", value=cell["Y"]),
        g.node("func.return", function="build_update"),
    )

    # place(): lay a part at the cursor, or say why not.
    g.function("place")
    entry = g.entry("place")
    press = g.lua(MODULE, "press", budget=g.get("budget"))
    g.chain(entry, press)
    outcome = press["result"]
    added = g.branch(g.op("compare.equal", outcome, "added"))
    g.chain(press, added)
    sound_path = g.op("text.concat", "sounds/game/place_", g.lua(MODULE, "kind", pure=True)["kind"])
    done = g.chain(
        added["True"], g.call("sound.play", asset_path=g.op("text.concat", sound_path, ".wav")), call("refresh")
    )
    refused = g.branch(
        g.op(
            "logic.or",
            g.op("compare.equal", press["result"], "long"),
            g.op(
                "logic.or",
                g.op("compare.equal", press["result"], "budget"),
                g.op("compare.equal", press["result"], "empty"),
            ),
        )
    )
    g.chain(added["False"], refused)
    reason = g.node(
        "logic.select",
        Condition=g.op("compare.equal", press["result"], "long"),
        **{
            "True": "TOO LONG FOR THIS KIND",
            "False": g.node(
                "logic.select",
                Condition=g.op("compare.equal", press["result"], "budget"),
                **{"True": "OVER BUDGET", "False": "START FROM A NODE"},
            )["Result"],
        },
    )["Result"]
    said = g.chain(refused["True"], g.call("sound.play", asset_path="sounds/game/refuse.wav"), call("say", text=reason))
    starts = g.branch(g.op("compare.equal", press["result"], "start"))
    g.chain(refused["False"], starts)
    clicked = g.chain(starts["True"], g.call("sound.play", asset_path="sounds/ui/select.wav"))
    g.join([done, said, clicked, starts["False"]], g.node("func.return", function="place"))

    # erase(): stop drawing, or remove the part under the cursor.
    g.function("erase")
    entry = g.entry("erase")
    drawing = g.branch(g.lua(MODULE, "drawing", pure=True)["drawing"])
    g.chain(entry, drawing)
    stopped = g.chain(drawing["True"], g.lua(MODULE, "cancel"))
    removed = g.lua(MODULE, "remove_at_cursor")
    g.chain(drawing["False"], removed)
    gone = g.branch(removed["removed"])
    g.chain(removed, gone)
    played = g.chain(gone["True"], g.call("sound.play", asset_path="sounds/game/remove.wav"), call("refresh"))
    g.join([stopped, played, gone["False"]], g.node("func.return", function="erase"))

    # undo(): the last part added or removed.
    g.function("undo")
    entry = g.entry("undo")
    undo_call = g.lua(MODULE, "undo")
    g.chain(entry, undo_call)
    ok = g.branch(undo_call["undone"])
    g.chain(undo_call, ok)
    played = g.chain(ok["True"], g.call("sound.play", asset_path="sounds/game/remove.wav"), call("refresh"))
    g.join([played, ok["False"]], g.node("func.return", function="undo"))

    # show_kind(label): the kind of the next parts on the HUD.
    g.function("show_kind", [("label", "string")])
    entry = g.entry("show_kind")
    g.chain(
        entry,
        g.call("ui.set_text", entity_id=widget("KindText"), text=g.fmt("LAYING %s", entry["label"])),
        g.call("gamestate.set", key="bridge_kind", value=g.lua(MODULE, "kind", pure=True)["kind"]),
        g.node("func.return", function="show_kind"),
    )

    # refresh(): the cost against the budget.
    g.function("refresh")
    entry = g.entry("refresh")
    cost = g.lua(MODULE, "cost", pure=True)["cost"]
    g.chain(
        entry,
        g.call("ui.set_text", entity_id=widget("CostText"), text=g.fmt("COST %d / %d", cost, g.get("budget"))),
        g.call("gamestate.set", key="bridge_parts", value=g.lua(MODULE, "count", pure=True)["count"]),
        g.node("func.return", function="refresh"),
    )

    # begin_run(): save the design, the bodies, the vehicles go.
    g.function("begin_run")
    entry = g.entry("begin_run")
    empty = g.branch(g.op("compare.equal", g.lua(MODULE, "count", pure=True)["count"], 0))
    g.chain(entry, empty)
    nothing = g.chain(
        empty["True"],
        g.call("sound.play", asset_path="sounds/game/refuse.wav"),
        call("say", text="LAY SOME PARTS FIRST"),
    )
    slow = g.node(
        "logic.select",
        Condition=g.call("settings.get", key="bridge_slow_run", default=False)["value"],
        **{"True": 0.5, "False": 1.0},
    )["Result"]
    ran = g.chain(
        empty["False"],
        g.call(
            "settings.set", key=key("bridge_design_", g.get("level")), value=g.lua(MODULE, "save", pure=True)["design"]
        ),
        g.call("settings.save"),
        g.lua(MODULE, "start_run"),
        g.set("state", "run"),
        g.set("clock", 0),
        g.set("warned", False),
        g.call("gamestate.set", key="bridge_state", value="run"),
        g.call("gamestate.set", key="bridge_arrived", value=0),
        g.call("gamestate.set", key="bridge_fallen", value=0),
        g.call("gamestate.set", key="bridge_breaks", value=0),
        g.call("time.set_scale", scale=slow),
        g.call("ui.set_visible", entity_id=find("Cursor"), visible=False),
        g.call("ui.set_visible", entity_id=find("Preview"), visible=False),
        g.call("ui.set_visible", entity_id=widget("LoadGroup"), visible=True),
        g.call("ui.set_text", entity_id=widget("RunButton"), text="EDIT"),
        g.call("sound.play", asset_path="sounds/game/run.wav"),
        call("say", text="TEST RUN"),
    )
    g.join([nothing, ran], g.node("func.return", function="begin_run"))

    # end_run(): back to the plan, the vehicles wait at the start again.
    g.function("end_run")
    entry = g.entry("end_run")
    g.chain(
        entry,
        g.lua(MODULE, "stop_run"),
        g.set("state", "build"),
        g.call("gamestate.set", key="bridge_state", value="build"),
        g.call("time.set_scale", scale=1.0),
        g.call("ui.set_visible", entity_id=find("Cursor"), visible=True),
        g.call("ui.set_visible", entity_id=widget("LoadGroup"), visible=False),
        g.call("ui.set_visible", entity_id=widget("ResultWindow"), visible=False),
        g.call("ui.set_text", entity_id=widget("RunButton"), text="TEST RUN"),
        call("refresh"),
        g.node("func.return", function="end_run"),
    )

    # run_update(dt): the load, the vehicles across, fallen or stuck.
    g.function("run_update", [("dt", "number")])
    entry = g.entry("run_update")
    stress = g.lua(
        MODULE,
        "stress",
        show=g.op("logic.not", g.call("settings.get", key="bridge_hide_stress", default=False)["value"]),
    )
    nodes = [
        g.set("clock", g.op("math.add", g.get("clock"), entry["dt"])),
        stress,
        g.call("ui.set_progress", entity_id=widget("LoadBar"), value=stress["stress"]),
        g.call(
            "ui.set_text",
            entity_id=widget("LoadText"),
            text=g.fmt("LOAD %d%%", g.node("math.floor", A=g.op("math.multiply", stress["stress"], 100))["Result"]),
        ),
        g.call("gamestate.set", key="bridge_load", value=stress["stress"]),
    ]
    g.chain(entry, *nodes)
    snapped = g.branch(
        g.op(
            "logic.and",
            g.op("logic.not", g.get("warned")),
            g.op("compare.greater", g.call("gamestate.get", key="bridge_breaks", default=0)["value"], 0),
        )
    )
    g.chain(nodes[-1], snapped)
    warned = g.chain(
        snapped["True"],
        g.set("warned", True),
        g.call("ui.show_subtitle", text="[a joint snaps]", seconds=2),
        call("say", text="A JOINT SNAPPED"),
    )
    # The press that started the run is not taken as the one ending it.
    again = g.op(
        "logic.and", g.op("compare.greater", g.get("clock"), 0.2), g.op("logic.or", pressed("run"), pressed("erase"))
    )
    back = g.join([snapped["False"], warned], g.branch(again))
    ended = g.chain(back["True"], call("end_run"), call("say", text="BACK TO THE PLAN"))
    across = g.branch(
        g.op(
            "compare.greater_equal",
            g.call("gamestate.get", key="bridge_arrived", default=0)["value"],
            g.get("vehicles"),
        )
    )
    g.chain(back["False"], across)
    won = g.chain(across["True"], call("win"))
    fell = g.branch(g.op("compare.greater", g.call("gamestate.get", key="bridge_fallen", default=0)["value"], 0))
    g.chain(across["False"], fell)
    lost = g.chain(fell["True"], call("fail", reason="A VEHICLE FELL"))
    stuck = g.branch(g.op("compare.greater", g.get("clock"), g.get("run_time")))
    g.chain(fell["False"], stuck)
    late = g.chain(stuck["True"], call("fail", reason="THE VEHICLES DID NOT MAKE IT"))
    g.join([ended, won, lost, late, stuck["False"]], g.node("func.return", function="run_update"))

    # fail(reason): back to the plan.
    g.function("fail", [("reason", "string")])
    entry = g.entry("fail")
    g.chain(
        entry,
        g.call("sound.play", asset_path="sounds/ui/game_over.wav"),
        g.call("gamestate.set", key="bridge_result", value=entry["reason"]),
        call("end_run"),
        call("say", text=entry["reason"]),
        g.node("func.return", function="fail"),
    )

    # win(): the stars, the best cost, the result window.
    g.function("win")
    entry = g.entry("win")
    cost = g.lua(MODULE, "cost", pure=True)["cost"]
    middle = g.op("math.divide", g.op("math.add", g.get("budget"), g.get("star_budget")), 2)
    stars_value = g.node(
        "logic.select",
        Condition=g.op("compare.less_equal", cost, g.get("star_budget")),
        **{
            "True": 3,
            "False": g.node(
                "logic.select",
                Condition=g.op("compare.less_equal", g.lua(MODULE, "cost", pure=True)["cost"], middle),
                **{"True": 2, "False": 1},
            )["Result"],
        },
    )["Result"]
    g.var("stars", "int", 0, exposed=False)
    score = g.op(
        "math.add",
        g.op("math.multiply", g.get("stars"), 1000),
        g.op("math.subtract", g.get("budget"), g.lua(MODULE, "cost", pure=True)["cost"]),
    )
    best_score = g.node(
        "math.max", A=score, B=g.call("settings.get", key=key("bridge_score_", g.get("level")), default=0)["value"]
    )["Result"]
    best_stars = g.node(
        "math.max",
        A=g.get("stars"),
        B=g.call("settings.get", key=key("bridge_stars_", g.get("level")), default=0)["value"],
    )["Result"]
    old_best = g.call("settings.get", key=key("bridge_best_", g.get("level")), default=999999)["value"]
    g.chain(
        entry,
        g.set("state", "won"),
        g.set("stars", stars_value),
        g.call("gamestate.set", key="bridge_state", value="won"),
        g.call("gamestate.set", key="bridge_stars", value=g.get("stars")),
        g.call("settings.set", key=key("bridge_done_", g.get("level")), value=True),
        g.call("settings.set", key=key("bridge_score_", g.get("level")), value=best_score),
        g.call("settings.set", key=key("bridge_stars_", g.get("level")), value=best_stars),
        g.call(
            "settings.set",
            key=key("bridge_best_", g.get("level")),
            value=g.node("math.min", A=old_best, B=g.lua(MODULE, "cost", pure=True)["cost"])["Result"],
        ),
        g.call("settings.save"),
        call("tally"),
        g.call("time.set_scale", scale=1.0),
        g.call("sound.play", asset_path="sounds/game/level_done.wav"),
        g.call("ui.show_subtitle", text="[fanfare]", seconds=2),
        g.call("ui.set_text", entity_id=widget("ResultStars"), text=g.fmt("STARS %d / 3", g.get("stars"))),
        g.call(
            "ui.set_text",
            entity_id=widget("ResultCost"),
            text=g.fmt("COST %d / %d", g.lua(MODULE, "cost", pure=True)["cost"], g.get("budget")),
        ),
        g.call("ui.set_visible", entity_id=widget("ResultWindow"), visible=True),
        g.call("ui.raise_window", entity_id=widget("ResultWindow")),
        g.call("ui.set_focus", entity_id=widget("NextButton")),
        g.node("func.return", function="win"),
    )

    # tally(): the score of the campaign (the best of each level) and its lines for the victory screen.
    g.function("tally")
    entry = g.entry("tally")
    loop = g.node("flow.for", First=1, Last=g.get("levels"))
    g.chain(entry, g.set("total", 0), loop)
    level_score = g.call("settings.get", key=key("bridge_score_", loop["Index"]), default=0)["value"]
    done_line = g.fmt(
        "%d  %-16s %d STARS",
        loop["Index"],
        g.node("table.get_item", List=g.get("names"), Index=loop["Index"])["Result"],
        g.call("settings.get", key=key("bridge_stars_", loop["Index"]), default=0)["value"],
    )
    g.chain(
        loop["Body"],
        g.set("total", g.op("math.add", g.get("total"), level_score)),
        g.call("gamestate.set", key=key("bridge_line_", loop["Index"]), value=done_line),
    )
    g.chain(
        loop["Completed"],
        g.call("gamestate.set", key="bridge_score", value=g.get("total")),
        g.call("gamestate.set", key="bridge_base", value=g.get("total")),
        g.node("func.return", function="tally"),
    )

    # say(text): the message line, for three seconds.
    g.function("say", [("text", "string")])
    entry = g.entry("say")
    g.chain(
        entry,
        g.call("ui.set_text", entity_id=widget("Message"), text=entry["text"]),
        g.call("ui.set_visible", entity_id=widget("Message"), visible=True),
        g.set("message_time", 3.0),
        g.call("gamestate.set", key="bridge_message", value=entry["text"]),
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
        g.call("gamestate.set", key="bridge_paused", value=entry["paused"]),
        show,
    )
    g.chain(
        show["True"],
        g.call("ui.raise_window", entity_id=widget("PauseWindow")),
        g.call("ui.set_focus", entity_id=widget("ResumeButton")),
    )

    for kind in ("road", "wood", "steel"):
        event = g.event(f"on_{kind}_clicked")
        g.chain(event, call("show_kind", label=g.lua(MODULE, "set_kind", kind=kind)["label"]))
    undo_event = g.event("on_undo_clicked")
    building = g.branch(state_is("build"))
    g.chain(undo_event, building, call("undo"))
    run_event = g.event("on_run_clicked")
    in_build = g.branch(state_is("build"))
    g.chain(run_event, in_build, call("begin_run"))
    in_run = g.branch(state_is("run"))
    g.chain(in_build["False"], in_run, call("end_run"))
    resume = g.event("on_resume_clicked")
    g.chain(resume, call("set_paused", paused=False))
    clear = g.event("on_clear_clicked")
    stop = g.branch(state_is("run"))
    g.chain(clear, call("set_paused", paused=False), stop)
    ended = g.chain(stop["True"], call("end_run"))
    cleared = g.join([stop["False"], ended], g.lua(MODULE, "clear"))
    g.chain(cleared, call("refresh"))
    edit = g.event("on_edit_clicked")
    g.chain(edit, call("end_run"))
    # The run is driven by the design alone: the same design runs the same test run again.
    replay = g.event("on_replay_clicked")
    g.chain(replay, call("end_run"), g.set("replay_wait", 0.2))
    following = g.event("on_next_clicked")
    leave = g.branch(g.get("leaving"))
    g.chain(following, leave)
    g.chain(
        leave["False"],
        g.set("leaving", True),
        g.call("time.set_scale", scale=1.0),
        g.call("scene.transition_to", scene_path=g.get("next_scene"), kind="wipe_left", duration=0.45, rgba=LOOK.sweep),
    )
    levels = g.event("on_levels_clicked")
    leave = g.branch(g.get("leaving"))
    g.chain(levels, leave)
    g.chain(
        leave["False"],
        g.set("leaving", True),
        g.call("time.set_paused", paused=False),
        g.call("time.set_scale", scale=1.0),
        g.call(
            "scene.transition_to",
            scene_path="scenes/level_select.owl",
            kind="wipe_left",
            duration=0.45,
            rgba=LOOK.sweep,
        ),
    )
    return g.text()


def vehicle_graph() -> str:
    """A vehicle: waits during the build, drives across at ``speed`` during the run, counts itself across or fallen,
    then is put back at the start when the plan comes back."""
    g = Graph(6510000000000000000)
    g.var("speed", "float", 3.2, low=0)
    g.var("finish_x", "float", 13.0)
    g.var("fall_y", "float", -7.0)
    g.var("pull", "float", 5.0, low=0)
    g.var("start", "vec3", (0, 0, 0), exposed=False)
    g.var("state", "string", "wait", exposed=False)
    create = g.event("on_create")
    g.chain(create, g.set("start", g.call("transform.get_position")["xyz"]))
    tick = g.event("on_tick")
    run = g.op("compare.equal", g.call("gamestate.get", key="bridge_state", default="build")["value"], "run")
    driving = g.branch(g.op("logic.and", run, g.op("compare.equal", g.get("state"), "drive")))
    waiting = g.branch(
        g.op(
            "logic.and",
            g.op("compare.equal", g.call("gamestate.get", key="bridge_state", default="build")["value"], "run"),
            g.op("compare.equal", g.get("state"), "wait"),
        )
    )
    g.chain(tick, waiting)
    go = g.chain(waiting["True"], g.set("state", "drive"))
    g.join([waiting["False"], go], driving)
    velocity = g.node("math.break_vector2", Value=g.call("physics.get_velocity")["velocity"])
    position = g.node("math.break_vector3", Value=g.call("transform.get_position")["xyz"])
    # The engine pushes towards the cruising speed, its force bounded: the vehicle slows on a climb.
    mass = g.call("physics.get_mass")["mass"]
    push = g.node(
        "math.clamp",
        Value=g.op(
            "math.multiply", g.op("math.multiply", g.op("math.subtract", g.get("speed"), velocity["X"]), mass), 4
        ),
        Min=g.op("math.multiply", mass, -3),
        Max=g.op("math.multiply", mass, g.get("pull")),
    )["Result"]
    drive = g.call("physics.add_force", force=g.node("math.make_vector2", X=push, Y=0)["Result"])
    where = g.call(
        "gamestate.set",
        key=g.op("text.concat", "bridge_x_", g.call("entity.get_name", entity_id=None)["name"]),
        value=position["X"],
    )
    g.chain(driving["True"], drive, where)
    across = g.branch(g.op("compare.greater", position["X"], g.get("finish_x")))
    g.chain(where, across)
    count = g.call("gamestate.get", key="bridge_arrived", default=0)["value"]
    g.chain(
        across["True"],
        g.set("state", "arrived"),
        g.call("gamestate.set", key="bridge_arrived", value=g.op("math.add", count, 1)),
        g.call("sound.play", asset_path="sounds/game/across.wav"),
    )
    fell = g.branch(
        g.op(
            "compare.less",
            g.node("math.break_vector3", Value=g.call("transform.get_position")["xyz"])["Y"],
            g.get("fall_y"),
        )
    )
    g.chain(across["False"], fell)
    fallen = g.call("gamestate.get", key="bridge_fallen", default=0)["value"]
    g.chain(
        fell["True"],
        g.set("state", "fallen"),
        g.call("gamestate.set", key="bridge_fallen", value=g.op("math.add", fallen, 1)),
        g.call("sound.play", asset_path="sounds/game/splash.wav"),
        g.call("ui.show_subtitle", text="[splash]", seconds=2),
    )
    # Back to the plan: the vehicle waits at the start again.
    reset = g.branch(
        g.op(
            "logic.and",
            g.op("compare.equal", g.call("gamestate.get", key="bridge_state", default="build")["value"], "build"),
            g.op("compare.not_equal", g.get("state"), "wait"),
        )
    )
    g.chain(driving["False"], reset)
    start = g.node("math.break_vector3", Value=g.get("start"))
    g.chain(
        reset["True"],
        g.set("state", "wait"),
        g.call(
            "physics.set_transform",
            xy=g.node("math.make_vector2", X=start["X"], Y=start["Y"])["Result"],
            rotation_or_z=0,
        ),
        g.call("physics.set_velocity", velocity=(0, 0)),
        g.call("physics.set_angular_velocity", angular_velocity=(0, 0, 0)),
    )
    return g.text()


def part_graph() -> str:
    """A part of the run: a joint snapping is counted (``bridge_breaks``) and heard."""
    g = Graph(6520000000000000000)
    broke = g.event("on_joint_break")
    count = g.call("gamestate.get", key="bridge_breaks", default=0)["value"]
    g.chain(
        broke,
        g.call("gamestate.set", key="bridge_breaks", value=g.op("math.add", count, 1)),
        g.call("sound.play", asset_path="sounds/game/snap.wav"),
    )
    return g.text()


def level_select_graph() -> str:
    """The level select: a level is open when the one before is done; each button shows the stars won."""
    g = Graph(6530000000000000000)
    g.var("levels", "int", len(lay.LEVELS), low=1)
    g.var("names", "strings", ", ".join(level.name for level in lay.LEVELS))
    g.var("sweep", "color", LOOK.sweep)
    g.var("leaving", "bool", False, exposed=False)
    create = g.event("on_create")
    after = mk._transition_in(g, LOOK, create)  # noqa: SLF001
    loop = g.node("flow.for", First=1, Last=g.get("levels"))
    g.chain(after, loop)
    index = loop["Index"]
    tag = g.op("text.concat", "LevelButton", index)
    stars = g.call("settings.get", key=g.op("text.concat", "bridge_stars_", index), default=0)["value"]
    previous = g.call(
        "settings.get", key=g.op("text.concat", "bridge_done_", g.op("math.subtract", index, 1)), default=False
    )["value"]
    open_ = g.op("logic.or", g.op("compare.equal", index, 1), previous)
    name = g.node("table.get_item", List=g.get("names"), Index=index)["Result"]
    text = g.node(
        "logic.select",
        Condition=open_,
        **{"True": g.fmt("%d. %s   %d / 3", index, name, stars), "False": g.fmt("%d. LOCKED", index)},
    )["Result"]
    g.chain(
        loop["Body"],
        g.call("ui.set_text", entity_id=g.call("scene.find_entity", name=tag)["entity_id"], text=text),
        g.call(
            "ui.set_button_enabled",
            entity_id=g.call("scene.find_entity", name=g.op("text.concat", "LevelButton", loop["Index"]))["entity_id"],
            enabled=g.op(
                "logic.or",
                g.op("compare.equal", loop["Index"], 1),
                g.call(
                    "settings.get",
                    key=g.op("text.concat", "bridge_done_", g.op("math.subtract", loop["Index"], 1)),
                    default=False,
                )["value"],
            ),
        ),
    )
    g.chain(loop["Completed"], g.call("gamestate.set", key="bridge_select", value=True))
    update = g.event("on_update")
    cancel = g.branch(
        g.op(
            "logic.and",
            g.op("logic.not", g.get("leaving")),
            g.call("input.is_action_just_pressed", action="ui_cancel")["pressed"],
        )
    )
    g.chain(update, cancel, g.node("func.call", function="go", scene_path="scenes/title.owl", jingle=False))
    for number in range(1, len(lay.LEVELS) + 1):
        event = g.event(f"on_level_{number}_clicked")
        g.chain(event, g.node("func.call", function="go", scene_path=f"scenes/level_{number}.owl", jingle=True))
    back = g.event("on_back_clicked")
    g.chain(back, g.node("func.call", function="go", scene_path="scenes/title.owl", jingle=False))
    g.function("go", [("scene_path", "string"), ("jingle", "boolean")])
    entry = g.entry("go")
    leave = g.branch(g.get("leaving"))
    jingle = g.branch(entry["jingle"])
    g.chain(entry, leave)
    g.chain(leave["False"], g.set("leaving", True), jingle)
    played = g.chain(jingle["True"], g.call("sound.play", asset_path="sounds/ui/start.wav"))
    g.join(
        [jingle["False"], played],
        g.call(
            "scene.transition_to", scene_path=entry["scene_path"], kind="wipe_left", duration=0.45, rgba=g.get("sweep")
        ),
    )
    return g.text()


# --------------------------------------------------------------------------------------------------------- project

PROJECT = """OwlProject:
  name: OwlBridgeBuilder
  firstScene: scenes/title.owl
  author: Silmaen
  description: Bridge Builder, a sample game of the Owl Engine
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
        SunDirection: [-0.4, -0.6, -0.7]
        Ambient: [0.25, 0.27, 0.3]
    - Type: Renderer2D
      Name: ui
    - Type: Renderer2D
      Name: overlay
"""
"""The project."""


def settings_file() -> str:
    """The settings and their defaults."""
    lines = [
        "GameSettings:",
        "  - {key: resolution_width, type: int, value: 1280}",
        "  - {key: resolution_height, type: int, value: 720}",
        "  - {key: fullscreen, type: bool, value: false}",
        "  - {key: resizable, type: bool, value: true}",
        "  - {key: volume_master, type: float, value: 1.0}",
        "  - {key: volume_music, type: float, value: 0.8}",
        "  - {key: volume_sfx, type: float, value: 1.0}",
        "  - {key: ui_scale, type: float, value: 1.0}",
        "  - {key: color_blind_mode, type: string, value: none}",
        "  - {key: reduced_motion, type: bool, value: false}",
        "  - {key: subtitles, type: bool, value: true}",
        "  - {key: bridge_hide_stress, type: bool, value: false}",
        "  - {key: bridge_slow_run, type: bool, value: false}",
        "  - {key: bridge_best, type: int, value: 0}",
    ]
    for number in range(1, len(lay.LEVELS) + 1):
        lines += [
            f"  - {{key: bridge_done_{number}, type: bool, value: false}}",
            f"  - {{key: bridge_stars_{number}, type: int, value: 0}}",
            f"  - {{key: bridge_score_{number}, type: int, value: 0}}",
            f"  - {{key: bridge_best_{number}, type: int, value: 999999}}",
            f"  - {{key: bridge_design_{number}, type: string, value: none}}",
        ]
    return "\n".join(lines) + "\n"


ACTIONS = """FormatVersion: 1
# Actions of Bridge Builder, on top of the built-in ones (the `ui_*` actions drive the menus). Read by
# scripts/game/level.owlvs.
actions:
  - name: cursor_left
    type: button
    bindings:
      - {key: Left}
      - {key: A}
      - {gamepad_button: DpadLeft}
  - name: cursor_right
    type: button
    bindings:
      - {key: Right}
      - {key: D}
      - {gamepad_button: DpadRight}
  - name: cursor_up
    type: button
    bindings:
      - {key: Up}
      - {key: W}
      - {gamepad_button: DpadUp}
  - name: cursor_down
    type: button
    bindings:
      - {key: Down}
      - {key: S}
      - {gamepad_button: DpadDown}
  - name: cursor_stick
    type: vector
    bindings:
      - {gamepad_stick: left}
  - name: place
    type: button
    bindings:
      - {key: Space}
      - {gamepad_button: A}
  - name: click
    type: button
    bindings:
      - {mouse_button: Left}
  - name: erase
    type: button
    bindings:
      - {key: X}
      - {key: Delete}
      - {gamepad_button: X}
  - name: click_erase
    type: button
    bindings:
      - {mouse_button: Right}
  - name: undo
    type: button
    bindings:
      - {key: Z}
      - {key: Backspace}
      - {gamepad_button: B}
  - name: kind_next
    type: button
    bindings:
      - {key: Tab}
      - {gamepad_button: LeftBumper}
      - {gamepad_button: RightBumper}
  - name: kind_road
    type: button
    bindings:
      - {key: D1}
  - name: kind_wood
    type: button
    bindings:
      - {key: D2}
  - name: kind_steel
    type: button
    bindings:
      - {key: D3}
  - name: run
    type: button
    bindings:
      - {key: R}
      - {gamepad_button: Y}
  - name: pause
    type: button
    bindings:
      - {key: Escape}
      - {key: P}
      - {gamepad_button: Start}
"""
"""The input actions."""


def main() -> None:
    """Write the project, the levels, the prefabs, the HUD and the graphs (the level select comes with the menus)."""
    for folder in ("scenes", "huds", "prefabs", "scripts/game", "scripts/ui"):
        (ROOT / folder).mkdir(parents=True, exist_ok=True)
    (ROOT / "owl_project.yml").write_text(PROJECT)
    (ROOT / "game_settings.yml").write_text(settings_file())
    (ROOT / "input_actions.yml").write_text(ACTIONS)
    for number, level in enumerate(lay.LEVELS, start=1):
        (ROOT / "scenes" / f"level_{number}.owl").write_text(level_scene(number, level))
    for name, text in prefabs().items():
        (ROOT / "prefabs" / name).write_text(text)
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    (ROOT / "scripts" / "game" / "level.owlvs").write_text(level_graph())
    (ROOT / "scripts" / "game" / "vehicle.owlvs").write_text(vehicle_graph())
    (ROOT / "scripts" / "game" / "part.owlvs").write_text(part_graph())
    (ROOT / "scripts" / "ui" / "level_select.owlvs").write_text(level_select_graph())


if __name__ == "__main__":
    main()
