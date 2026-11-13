#!/usr/bin/env python3
"""
Write the status bar art and its graph of the Wolfenstein mini sample game into ``samples/wolfenstein/``
(deterministic).

The bar along the bottom of the screen is laid out in ``huds/play.owlhud`` by ``generate_menus.py``; this script draws
what it shows and writes ``scripts/game/status.owlvs``, the graph that drives it:

- ``textures/ui/face.png``: the face of the player, 5 x 6 frames of 32 pixels drawn at 4 screen pixels each. A row
  per health bracket (100 to 81 %, 80 to 61 %... 20 to 1 %), each bloodier than the one above; the columns look left,
  ahead, right, wince (hit) and grin (a new gun); the last row is the dead face.
- ``textures/ui/weapon_icons.png``: the knife, the pistol, the machine gun and the chain gun seen from the side,
  4 x 1 frames.
- ``textures/ui/hurt_flash.png``: a red veil in 8 strengths (8 x 1 frames, the first clear), flashed over the view on
  a hit.
- ``textures/ui/hurt_side.png``: a red glow along one edge of the view, a row per side the shot came from (ahead, right,
  behind, left) and a column per strength (the first clear).
- ``textures/ui/low_health.png``: a red vignette in 8 strengths (4 x 2 frames, the first clear), shown while the
  health is low.

The overlays are sprite sheets shown one frame at a time (``ui.set_image_frame``), drawn with the nearest texel so
that no frame bleeds into the next one.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.wolfenstein.generate_status``,
then ``generate_menus.py`` for the HUD.
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples.vs_graph import Graph, Node, Pin

ROOT = Path(__file__).resolve().parents[3] / "samples" / "wolfenstein"
"""The sample project."""

FACE = 32
"""Side of a face frame, in face pixels."""
FACE_ZOOM = 4
"""Texture pixels per face pixel."""
FACE_COLUMNS, FACE_ROWS = 5, 6
"""Looks (left, ahead, right, wince, grin) by health brackets (and the dead row)."""
DEAD_FRAME = 25
"""Frame of the dead face."""
ICON_W, ICON_H, ICON_ZOOM = 64, 24, 3
"""Size of a weapon icon in icon pixels, and texture pixels per icon pixel."""
OVERLAY_W, OVERLAY_H = 320, 180
"""Size of a frame of the screen overlays, in texture pixels."""
FLASH_FRAMES = 8
"""Strengths of the hit flash (the first clear)."""
SIDE_STRENGTHS = 4
"""Strengths of the side glow (the first clear)."""
TINT_FRAMES = 8
"""Strengths of the low health vignette (the first clear), in 4 x 2 frames."""

SKIN, SKIN_SHADE, SKIN_DARK = (226, 172, 128), (190, 132, 94), (140, 92, 62)
HAIR, HAIR_DARK = (184, 132, 56), (124, 82, 32)
WHITE, IRIS, BROW = (240, 238, 232), (52, 96, 170), (112, 72, 32)
MOUTH, TEETH = (122, 34, 30), (248, 246, 236)
BLOOD, BLOOD_DARK, BRUISE = (176, 8, 6), (110, 0, 0), (118, 70, 104)
UNIFORM, COLLAR, INK = (92, 100, 62), (62, 68, 40), (34, 22, 16)
DEAD_SKIN, DEAD_SHADE = (156, 156, 146), (112, 112, 106)
RGB = tuple[int, int, int]


# ----------------------------------------------------------------------------------------------------------- face


def _face(bracket: int, column: int, dead: bool = False) -> Image.Image:
    """One face: ``bracket`` 0 (healthy) to 4 (near death), ``column`` the look (0 left, 1 ahead, 2 right, 3 wince,
    4 grin)."""
    img = Image.new("RGBA", (FACE, FACE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    skin, shade = (DEAD_SKIN, DEAD_SHADE) if dead else (SKIN, SKIN_SHADE)

    def px(x: int, y: int, colour: RGB) -> None:
        d.point((x, y), fill=colour)

    def row(x0: int, x1: int, y: int, colour: RGB) -> None:
        d.line([(x0, y), (x1, y)], fill=colour)

    # Shoulders, collar and neck.
    d.rectangle([2, 28, 29, 31], fill=UNIFORM)
    d.polygon([(11, 28), (16, 31), (20, 28)], fill=COLLAR)
    d.rectangle([12, 24, 19, 28], fill=shade)
    # Hair behind, head, ears.
    d.ellipse([6, 2, 25, 18], fill=HAIR_DARK)
    d.ellipse([7, 6, 24, 27], fill=INK)
    d.ellipse([8, 7, 23, 26], fill=skin)
    d.rectangle([6, 13, 7, 18], fill=shade)
    d.rectangle([24, 13, 25, 18], fill=shade)
    d.ellipse([7, 2, 24, 12], fill=HAIR)
    row(9, 22, 10, HAIR_DARK)
    row(10, 21, 11, skin)
    # Chin and cheek shade.
    row(12, 19, 25, shade)
    px(9, 20, shade)
    px(22, 20, shade)

    wince = column == 3
    grin = column == 4
    look = column - 1 if column <= 2 else 0
    # Brows: lowered at the middle when wincing, raised when grinning.
    brow_y = 11 if grin else 12
    if wince:
        row(10, 12, 12, BROW)
        px(13, 13, BROW)
        row(19, 21, 12, BROW)
        px(18, 13, BROW)
    else:
        row(10, 13, brow_y, BROW)
        row(18, 21, brow_y, BROW)
    # Eyes.
    lids = 2 if bracket >= 4 else (1 if bracket == 3 else 0)
    for left in (10, 18):
        if dead or wince:
            row(left, left + 3, 15, INK)
            continue
        if lids == 2:
            row(left, left + 3, 14, SKIN_DARK)
            row(left, left + 3, 15, WHITE)
            px(left + 1 + look, 15, IRIS)
            px(left + 2 + look, 15, IRIS)
        else:
            row(left, left + 3, 14, WHITE)
            row(left, left + 3, 15, WHITE)
            d.rectangle([left + 1 + look, 14, left + 2 + look, 15], fill=IRIS)
            if lids == 1 and left == 10:
                row(left, left + 3, 14, BRUISE)
    # Nose.
    d.rectangle([15, 16, 16, 19], fill=shade)
    px(14, 19, SKIN_DARK)
    px(17, 19, SKIN_DARK)
    # Mouth.
    if grin:
        row(11, 20, 22, TEETH)
        row(12, 19, 23, MOUTH)
        px(11, 21, MOUTH)
        px(20, 21, MOUTH)
    elif wince or dead or bracket >= 4:
        d.rectangle([13, 21, 18, 23], fill=MOUTH)
        row(13, 18, 21, TEETH)
    else:
        row(13, 18, 22, MOUTH)

    # The wounds, worse at every bracket.
    if bracket >= 1 or dead:
        for x, y in ((20, 8), (21, 9), (21, 10)):
            px(x, y, BLOOD)
    if bracket >= 2 or dead:
        row(10, 12, 16, BRUISE)
        for y in (20, 21):
            px(16, y, BLOOD)
    if bracket >= 3 or dead:
        for y in range(17, 23):
            px(21, y, BLOOD if y % 2 else BLOOD_DARK)
        px(19, 23, BLOOD)
        for x, y in ((11, 7), (12, 8), (13, 8), (12, 9)):
            px(x, y, BLOOD_DARK)
    if bracket >= 4 or dead:
        for x, y in ((14, 8), (15, 9), (16, 8), (17, 9), (18, 8), (10, 12), (9, 17), (9, 18), (10, 19), (13, 24)):
            px(x, y, BLOOD)
        for x in (19, 20):
            px(x, 26, BLOOD_DARK)
    return img


def face_sheet() -> Image.Image:
    """Every face: a row per health bracket, then the dead face."""
    sheet = Image.new("RGBA", (FACE * FACE_COLUMNS, FACE * FACE_ROWS), (0, 0, 0, 0))
    for bracket in range(FACE_ROWS - 1):
        for column in range(FACE_COLUMNS):
            sheet.paste(_face(bracket, column), (column * FACE, bracket * FACE))
    for column in range(FACE_COLUMNS):
        sheet.paste(_face(4, 1, dead=True), (column * FACE, (FACE_ROWS - 1) * FACE))
    return sheet.resize((sheet.width * FACE_ZOOM, sheet.height * FACE_ZOOM), Image.Resampling.NEAREST)


# ---------------------------------------------------------------------------------------------------------- icons

STEEL, STEEL_DARK, STEEL_LIGHT = (164, 166, 174), (84, 86, 96), (222, 224, 232)
GUNMETAL, WOOD, WOOD_DARK = (52, 54, 62), (132, 80, 40), (86, 50, 24)


def _knife(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(22, 9), (50, 9), (58, 12), (50, 14), (22, 14)], fill=STEEL)
    d.line([(22, 9), (50, 9)], fill=STEEL_LIGHT)
    d.line([(22, 14), (50, 14)], fill=STEEL_DARK)
    d.rectangle([18, 6, 21, 17], fill=GUNMETAL)
    d.rectangle([5, 9, 17, 14], fill=WOOD)
    d.line([(5, 14), (17, 14)], fill=WOOD_DARK)
    for x in (8, 13):
        d.point((x, 11), fill=STEEL_LIGHT)


def _pistol(d: ImageDraw.ImageDraw) -> None:
    d.rectangle([26, 8, 56, 11], fill=STEEL)
    d.line([(26, 8), (56, 8)], fill=STEEL_LIGHT)
    d.rectangle([14, 6, 34, 13], fill=STEEL_DARK)
    d.line([(14, 6), (34, 6)], fill=STEEL)
    d.polygon([(14, 13), (24, 13), (20, 22), (9, 22)], fill=WOOD)
    d.line([(9, 22), (20, 22)], fill=WOOD_DARK)
    d.arc([22, 11, 31, 19], 0, 180, fill=GUNMETAL, width=2)
    d.point((26, 14), fill=GUNMETAL)


def _machine_gun(d: ImageDraw.ImageDraw) -> None:
    d.rectangle([40, 9, 61, 11], fill=GUNMETAL)
    d.rectangle([56, 7, 58, 9], fill=GUNMETAL)
    d.rectangle([16, 6, 42, 13], fill=STEEL_DARK)
    d.line([(16, 6), (42, 6)], fill=STEEL)
    d.rectangle([31, 13, 35, 23], fill=GUNMETAL)
    d.rectangle([22, 13, 26, 20], fill=GUNMETAL)
    d.line([(2, 8), (16, 8)], fill=GUNMETAL, width=2)
    d.line([(2, 12), (16, 12)], fill=GUNMETAL, width=2)
    d.rectangle([1, 7, 3, 14], fill=GUNMETAL)


def _chain_gun(d: ImageDraw.ImageDraw) -> None:
    for y in (5, 9, 13):
        d.rectangle([30, y, 62, y + 2], fill=STEEL)
        d.line([(30, y), (62, y)], fill=STEEL_LIGHT)
    d.rectangle([58, 4, 61, 16], fill=STEEL_DARK)
    d.rectangle([18, 3, 31, 17], fill=GUNMETAL)
    d.line([(18, 3), (31, 3)], fill=STEEL_DARK)
    d.rectangle([14, 0, 26, 2], fill=STEEL_DARK)
    d.polygon([(6, 8), (18, 8), (18, 15), (12, 21), (6, 21)], fill=WOOD)
    d.line([(6, 21), (12, 21)], fill=WOOD_DARK)


def weapon_icons() -> Image.Image:
    """The four weapons from the side, left to right in the order of the keys 1 to 4."""
    sheet = Image.new("RGBA", (ICON_W * 4, ICON_H), (0, 0, 0, 0))
    for index, draw in enumerate((_knife, _pistol, _machine_gun, _chain_gun)):
        icon = Image.new("RGBA", (ICON_W, ICON_H), (0, 0, 0, 0))
        draw(ImageDraw.Draw(icon))
        sheet.paste(icon, (index * ICON_W, 0))
    return sheet.resize((sheet.width * ICON_ZOOM, sheet.height * ICON_ZOOM), Image.Resampling.NEAREST)


# ------------------------------------------------------------------------------------------------------- overlays


def _mask(strength: float, alpha_of: object) -> Image.Image:
    """A red frame whose alpha at (u, v), both 0 to 1, is ``strength * alpha_of(u, v)``."""
    frame = Image.new("RGBA", (OVERLAY_W, OVERLAY_H), (0, 0, 0, 0))
    if strength <= 0:
        return frame
    pixels = frame.load()
    assert pixels is not None
    for y in range(OVERLAY_H):
        v = (y + 0.5) / OVERLAY_H
        for x in range(OVERLAY_W):
            u = (x + 0.5) / OVERLAY_W
            alpha = max(0.0, min(1.0, strength * alpha_of(u, v)))  # type: ignore[operator]
            pixels[x, y] = (196, 8, 4, round(alpha * 255))
    return frame


def hurt_flash() -> Image.Image:
    """The flash of a hit: an even red veil, up to half opaque."""
    sheet = Image.new("RGBA", (8 * FLASH_FRAMES, 8), (0, 0, 0, 0))
    for index in range(FLASH_FRAMES):
        alpha = round(255 * 0.5 * index / (FLASH_FRAMES - 1))
        sheet.paste(Image.new("RGBA", (8, 8), (200, 12, 6, alpha)), (index * 8, 0))
    return sheet


def _edge(distance: float, along: float) -> float:
    """Glow at ``distance`` (0 to 1) from an edge, ``along`` it (0 to 1): strongest at the middle of the edge."""
    glow = max(0.0, 1.0 - distance / 0.42) ** 2
    return glow * (0.55 + 0.45 * math.sin(math.pi * along))


def hurt_side() -> Image.Image:
    """The glow of the side a shot came from: rows ahead (top), right, behind (bottom), left."""
    sides = (
        lambda u, v: _edge(v, u),
        lambda u, v: _edge(1 - u, v),
        lambda u, v: _edge(1 - v, u),
        lambda u, v: _edge(u, v),
    )
    sheet = Image.new("RGBA", (OVERLAY_W * SIDE_STRENGTHS, OVERLAY_H * len(sides)), (0, 0, 0, 0))
    for row, side in enumerate(sides):
        for column in range(SIDE_STRENGTHS):
            strength = 0.95 * column / (SIDE_STRENGTHS - 1)
            sheet.paste(_mask(strength, side), (column * OVERLAY_W, row * OVERLAY_H))
    return sheet


def low_health() -> Image.Image:
    """The vignette of low health: a light red veil, darker towards the corners."""

    def vignette(u: float, v: float) -> float:
        r = math.hypot(2 * u - 1, 2 * v - 1) / math.sqrt(2)
        edge = max(0.0, min(1.0, (r - 0.35) / 0.65))
        return 0.08 + 0.56 * edge * edge * (3 - 2 * edge)

    sheet = Image.new("RGBA", (OVERLAY_W * 4, OVERLAY_H * 2), (0, 0, 0, 0))
    for index in range(TINT_FRAMES):
        frame = _mask(index / (TINT_FRAMES - 1), vignette)
        sheet.paste(frame, ((index % 4) * OVERLAY_W, (index // 4) * OVERLAY_H))
    return sheet


# ---------------------------------------------------------------------------------------------------------- graph

LOOKS = "1, 0, 1, 2, 1, 1, 0, 2, 1, 1"
"""The look of the face over time (0 left, 1 ahead, 2 right), one step every ``look_period``."""


def status_graph() -> str:
    """``scripts/game/status.owlvs``: the face, the weapon icon and the red of the hits and of low health."""
    g = Graph(7108000000)
    g.var("low_health", "int", 25, low=1)
    g.var("flash_time", "float", 0.35, low=0.01)
    g.var("side_time", "float", 0.9, low=0.01)
    g.var("wince_time", "float", 0.6, low=0)
    g.var("grin_time", "float", 1.5, low=0)
    g.var("shake_time", "float", 0.3, low=0.01)
    g.var("shake_angle", "float", 0.035, low=0)
    g.var("look_period", "float", 1.1, low=0.1)
    for name in ("flash_id", "side_id", "tint_id", "face_id", "weapon_id", "player_id"):
        g.var(name, "entity", 0, exposed=False)
    for name in (
        "hurts",
        "owned",
        "flash",
        "side_clock",
        "side",
        "wince",
        "grin",
        "shake",
        "clock",
        "look_clock",
        "look_step",
    ):
        g.var(name, "float", 0, exposed=False)
    g.var("health", "float", 100, exposed=False)
    g.var("face", "int", 0, exposed=False)
    g.var("tinted", "bool", False, exposed=False)

    def owned_guns() -> Pin:
        return g.op(
            "math.add",
            g.call("gamestate.get", key="wolf_has_3", default=0)["value"],
            g.call("gamestate.get", key="wolf_has_4", default=0)["value"],
        )

    def still() -> Pin:
        """Whether the player asked for reduced motion."""
        return g.call("settings.get", key="reduced_motion", default=False)["value"]

    def frame(variable: str, frames: int) -> Pin:
        """The frame of a clear-to-full sheet for a variable going from 1 down to 0."""
        return g.op("math.ceil", g.op("math.multiply", g.get(variable), frames - 1))

    # The widgets, and what the player has when the floor starts.
    create = g.event("on_create")
    ids = [
        g.set(f"{name}_id", g.call("ui.find", name=tag)["entity_id"])
        for name, tag in (
            ("flash", "HurtFlash"),
            ("side", "HurtSide"),
            ("tint", "LowHealthTint"),
            ("face", "FaceImage"),
            ("weapon", "WeaponIcon"),
        )
    ]
    g.chain(
        create,
        *ids,
        g.set("player_id", g.call("scene.find_entity", name="Player")["entity_id"]),
        g.set("hurts", g.call("gamestate.get", key="wolf_hurts", default=0)["value"]),
        g.set("owned", owned_guns()),
        g.call(
            "gamestate.set",
            key="wolf_flashes",
            value=g.call("gamestate.get", key="wolf_flashes", default=0)["value"],
        ),
    )

    update = g.event("on_update")
    dt = update["dt"]
    health = g.set("health", g.call("gamestate.get", key="wolf_health", default=100)["value"])
    hurts = g.call("gamestate.get", key="wolf_hurts", default=0)
    hit = g.branch(g.op("compare.greater", hurts["value"], g.get("hurts")))
    g.chain(update, health, hurts, hit)
    hit_done = g.chain(hit["True"], g.node("func.call", function="hit"))
    synced = g.join([hit_done, hit["False"]], g.set("hurts", hurts["value"]))
    gained = g.branch(g.op("compare.greater", owned_guns(), g.get("owned")))
    g.chain(synced, gained)
    grinned = g.chain(gained["True"], g.set("grin", g.get("grin_time")))
    counted = g.join([grinned, gained["False"]], g.set("owned", owned_guns()))

    def decay(variable: str, rate: Pin | float) -> Node:
        return g.set(
            variable, g.op("math.max", 0, g.op("math.subtract", g.get(variable), g.op("math.multiply", dt, rate)))
        )

    decayed = g.chain(
        counted,
        decay("flash", g.op("math.divide", 1, g.get("flash_time"))),
        decay("side_clock", g.op("math.divide", 1, g.get("side_time"))),
        decay("wince", 1),
        decay("grin", 1),
        decay("shake", 1),
        g.set("clock", g.op("math.add", g.get("clock"), dt)),
        g.set("look_clock", g.op("math.add", g.get("look_clock"), dt)),
    )
    step = g.branch(g.op("compare.greater", g.get("look_clock"), g.get("look_period")))
    g.chain(decayed, step)
    stepped = g.chain(
        step["True"],
        g.set("look_clock", 0),
        g.set("look_step", g.op("math.modulo", g.op("math.add", g.get("look_step"), 1), 10)),
    )
    draw = g.node("func.call", function="draw")
    g.join([stepped, step["False"]], draw)

    # A hit: the flash, the glow of its side, the wince and the shake (none with reduced motion).
    g.function("hit")
    entry = g.entry("hit")
    player = g.get("player_id")
    position = g.call("transform.get_position", entity_id=player, split__xyz=True)
    yaw = g.call("transform.get_rotation", entity_id=g.get("player_id"), split__rotation=True)["rz"]
    dx = g.op("math.subtract", g.call("gamestate.get", key="wolf_hit_x", default=position["x"])["value"], position["x"])
    dy = g.op("math.subtract", g.call("gamestate.get", key="wolf_hit_y", default=position["y"])["value"], position["y"])
    # Forward is (-sin yaw, cos yaw), right (cos yaw, sin yaw).
    ahead = g.op(
        "math.add",
        g.op("math.multiply", dx, g.op("math.negate", g.op("math.sin", yaw))),
        g.op("math.multiply", dy, g.op("math.cos", yaw)),
    )
    right = g.op(
        "math.add",
        g.op("math.multiply", dx, g.op("math.cos", yaw)),
        g.op("math.multiply", dy, g.op("math.sin", yaw)),
    )
    lengthwise = g.op("compare.greater_equal", g.op("math.abs", ahead), g.op("math.abs", right))
    side = g.node(
        "logic.select",
        Condition=lengthwise,
        **{
            "True": g.node(
                "logic.select", Condition=g.op("compare.greater_equal", ahead, 0), **{"True": 0, "False": 2}
            )["Result"],
            "False": g.node(
                "logic.select", Condition=g.op("compare.greater_equal", right, 0), **{"True": 1, "False": 3}
            )["Result"],
        },
    )["Result"]
    g.chain(
        entry,
        g.set("side", side),
        g.set("flash", 1),
        g.set("side_clock", 1),
        g.set("wince", g.get("wince_time")),
        g.set(
            "shake", g.node("logic.select", Condition=still(), **{"True": 0, "False": g.get("shake_time")})["Result"]
        ),
        g.call("gamestate.set", key="wolf_hit_side", value=g.get("side")),
        g.call(
            "gamestate.set",
            key="wolf_flashes",
            value=g.op("math.add", g.call("gamestate.get", key="wolf_flashes", default=0)["value"], 1),
        ),
        g.node("func.return", function="hit"),
    )

    # Every frame: the sheets, the face, the weapon, the shake read by player.owlvs.
    g.function("draw")
    entry = g.entry("draw")
    alive = g.op("compare.greater", g.get("health"), 0)
    low = g.op("logic.and", alive, g.op("compare.less", g.get("health"), g.get("low_health")))
    strength = g.op("math.divide", g.op("math.subtract", g.get("low_health"), g.get("health")), g.get("low_health"))
    pulse = g.node(
        "logic.select",
        Condition=still(),
        **{"True": 0, "False": g.op("math.multiply", g.op("math.sin", g.op("math.multiply", g.get("clock"), 5)), 1.2)},
    )["Result"]
    level = g.node(
        "math.clamp",
        Value=g.op("math.round", g.op("math.add", g.op("math.add", 2, g.op("math.multiply", strength, 4)), pulse)),
        Min=1,
        Max=TINT_FRAMES - 1,
    )["Result"]
    tint = g.node("logic.select", Condition=g.get("tinted"), **{"True": level, "False": 0})["Result"]
    bracket = g.node(
        "math.clamp",
        Value=g.op("math.floor", g.op("math.divide", g.op("math.subtract", 100, g.get("health")), 20)),
        Min=0,
        Max=4,
    )["Result"]
    look = g.node(
        "table.get_item",
        List=g.const("table", LOOKS),
        Index=g.op("math.floor", g.op("math.add", g.get("look_step"), 1)),
    )["Result"]
    column = g.node(
        "logic.select",
        Condition=g.op("compare.greater", g.get("wince"), 0),
        **{
            "True": 3,
            "False": g.node(
                "logic.select", Condition=g.op("compare.greater", g.get("grin"), 0), **{"True": 4, "False": look}
            )["Result"],
        },
    )["Result"]
    face = g.node(
        "logic.select",
        Condition=alive,
        **{
            "True": g.op("math.floor", g.op("math.add", g.op("math.multiply", bracket, FACE_COLUMNS), column)),
            "False": DEAD_FRAME,
        },
    )["Result"]
    wobble = g.op(
        "math.multiply",
        g.op("math.multiply", g.op("math.divide", g.get("shake"), g.get("shake_time")), g.get("shake_angle")),
        g.op("math.sin", g.op("math.multiply", g.get("clock"), 70)),
    )
    side_frame = g.op(
        "math.floor",
        g.op("math.add", g.op("math.multiply", g.get("side"), SIDE_STRENGTHS), frame("side_clock", SIDE_STRENGTHS)),
    )
    g.chain(
        entry,
        g.set("tinted", low),
        g.call("ui.set_image_frame", entity_id=g.get("flash_id"), frame=frame("flash", FLASH_FRAMES)),
        g.call("ui.set_image_frame", entity_id=g.get("side_id"), frame=side_frame),
        g.call("ui.set_image_frame", entity_id=g.get("tint_id"), frame=tint),
        g.set("face", face),
        g.call("ui.set_image_frame", entity_id=g.get("face_id"), frame=g.get("face")),
        g.call(
            "ui.set_image_frame",
            entity_id=g.get("weapon_id"),
            frame=g.op(
                "math.floor", g.op("math.subtract", g.call("gamestate.get", key="wolf_weapon", default=2)["value"], 1)
            ),
        ),
        g.call("gamestate.set", key="wolf_shake_yaw", value=wobble),
        g.call(
            "gamestate.set",
            key="wolf_low_health",
            value=g.node("logic.select", Condition=g.get("tinted"), **{"True": 1, "False": 0})["Result"],
        ),
        g.call("gamestate.set", key="wolf_face", value=g.get("face")),
        g.node("func.return", function="draw"),
    )
    return g.text()


def main() -> None:
    """Write the art and the graph of the status bar."""
    ui = ROOT / "textures" / "ui"
    face_sheet().save(ui / "face.png")
    weapon_icons().save(ui / "weapon_icons.png")
    hurt_flash().save(ui / "hurt_flash.png")
    hurt_side().save(ui / "hurt_side.png")
    low_health().save(ui / "low_health.png")
    (ROOT / "scripts" / "game" / "status.owlvs").write_text(status_graph())


if __name__ == "__main__":
    main()
