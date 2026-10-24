#!/usr/bin/env python3
"""
Draw the sprites and synthesise the sounds of the Tower defence sample game into ``samples/tower_defence/``
(deterministic, home-made).

- ``textures/enemies/{goblin,grunt,ogre}.png``: the Horde in 8 directions x 4 walk frames (``creatures.py``);
- ``textures/fx/``: ``projectiles.png`` (arrow, cannon ball, fire shell, frost shard), ``fire.png`` (4 flame frames),
  ``ring.png`` (range ring) and ``cursor.png`` (pad frame) laid flat on the ground, ``shadow.png``;
- ``textures/icons.png``: the badges of the build ring and of the info panel (``ICONS``, 6 by row);
- ``textures/puff.png``, ``textures/bar.png`` (health bar), tinted by the script;
- menus: ``textures/ui/title.png``, ``textures/ui/banner.png``, and the parade under them (``textures/enemies.png``
  in 4 headings, ``textures/towers/archer.png``, ``textures/keep.png``).

Sounds: mono 16 bit at 22.05 kHz, short and neutral (build, sell, shots, hits, horn, coins, keep hit, start jingle);
the victory jingle is copied from the Engine Demo.

The 3D terrain, the towers and the keep are models of the Kenney Tower Defense Kit and Castle Kit (CC0), copied into
``models/td/`` and ``models/castle/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_assets``.
"""

from __future__ import annotations

import math
import random
import shutil
import struct
import wave
from collections.abc import Callable
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

from tools.samples.tower_defence import creatures

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "tower_defence"
"""The sample project."""
DEMO = REPOSITORY / "engine_demo"
"""The Engine Demo, source of the terrain sheet and of the victory jingle."""
FONTS = REPOSITORY / "engine_assets" / "fonts"
"""The fonts of the engine, for the title art."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
RATE = 22050
"""Samples per second."""
SS = 4
"""Supersampling of the drawings."""
RGB = tuple[int, int, int]

ENEMIES = ("runner", "soldier", "armoured")
"""Rows of ``enemies.png``."""
HEADINGS = 4
"""Columns of ``enemies.png``: +x, +y, -x, -y."""
TOWERS = ("archer", "cannon", "frost")
"""Tower kinds, also the first three frames of ``icons.png``."""


def _shade(colour: RGB, amount: float) -> tuple[int, int, int, int]:
    """A colour moved towards white (positive) or black (negative), opaque."""
    if amount >= 0:
        r, g, b = (round(c + (255 - c) * amount) for c in colour)
    else:
        r, g, b = (round(c * (1 + amount)) for c in colour)
    return r, g, b, 255


class Canvas:
    """A supersampled RGBA drawing, in output pixels."""

    def __init__(self, width: int, height: int) -> None:
        self.size = (width, height)
        self.image = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.image)

    def poly(
        self, points: list[tuple[float, float]], colour: tuple[int, ...], outline: tuple[int, ...] | None = None
    ) -> None:
        """A filled polygon."""
        scaled = [(x * SS, y * SS) for x, y in points]
        self.draw.polygon(scaled, fill=colour, outline=outline, width=SS if outline else 0)

    def ellipse(self, box: tuple[float, float, float, float], colour: tuple[int, ...], width: float = 0) -> None:
        """A filled ellipse, or its outline ``width`` pixels thick."""
        scaled = [v * SS for v in box]
        if width:
            self.draw.ellipse(scaled, outline=colour, width=round(width * SS))
        else:
            self.draw.ellipse(scaled, fill=colour)

    def line(self, points: list[tuple[float, float]], colour: tuple[int, ...], width: float) -> None:
        """A polyline."""
        self.draw.line([(x * SS, y * SS) for x, y in points], fill=colour, width=round(width * SS))

    def done(self) -> Image.Image:
        """The image at its output size."""
        return self.image.resize(self.size, Image.Resampling.LANCZOS)


def prism(
    c: Canvas, cx: float, cy: float, half_w: float, height: float, colour: RGB
) -> tuple[list[tuple[float, float]], float]:
    """An isometric box on a diamond centred on ``(cx, cy)`` of half width ``half_w`` (half height ``half_w / 2``):
    left face dark, right face mid, top light. Returns the top diamond and its centre height."""
    hh = half_w / 2
    left, front, right, back = (cx - half_w, cy), (cx, cy + hh), (cx + half_w, cy), (cx, cy - hh)

    def up(p: tuple[float, float]) -> tuple[float, float]:
        return p[0], p[1] - height

    c.poly([left, front, up(front), up(left)], _shade(colour, -0.35))
    c.poly([front, right, up(right), up(front)], _shade(colour, -0.12))
    top = [up(left), up(front), up(right), up(back)]
    c.poly(top, _shade(colour, 0.15))
    return top, cy - height


def ground_shadow(c: Canvas, cx: float, cy: float, half_w: float) -> None:
    """A soft dark ellipse on the ground."""
    c.ellipse((cx - half_w, cy - half_w / 2, cx + half_w, cy + half_w / 2), (0, 0, 0, 70))


def tower(kind: str) -> Image.Image:
    """A tower on its pad: archer (wooden, pointed roof), cannon (squat stone, barrel), frost (blue crystal)."""
    c = Canvas(64, 112)
    cx, cy = 32, 112 - 16
    ground_shadow(c, cx, cy + 2, 24)
    if kind == "archer":
        prism(c, cx, cy, 18, 46, (150, 104, 62))
        top, ty = prism(c, cx, cy - 46, 22, 8, (120, 82, 48))
        for x in (cx - 10, cx + 10):
            c.line([(x, cy - 10), (x, cy - 40)], _shade((90, 60, 34), 0), 2)
        apex = (cx, ty - 30)
        c.poly([top[0], top[1], apex], _shade((176, 52, 44), -0.25))
        c.poly([top[1], top[2], apex], _shade((176, 52, 44), 0.0))
        c.line([apex, (apex[0], apex[1] - 10)], (60, 40, 30, 255), 1.5)
        c.poly([(apex[0], apex[1] - 10), (apex[0] + 9, apex[1] - 7), (apex[0], apex[1] - 4)], (250, 210, 70, 255))
    elif kind == "cannon":
        prism(c, cx, cy, 24, 26, (128, 128, 134))
        top, ty = prism(c, cx, cy - 26, 26, 6, (110, 110, 118))
        for i in range(4):
            x = cx - 22 + i * 12
            c.poly([(x, ty + 2), (x + 6, ty + 5), (x + 6, ty - 3), (x, ty - 6)], _shade((120, 120, 128), 0.1))
        c.ellipse((cx - 11, ty - 16, cx + 11, ty + 4), (40, 40, 46, 255))
        c.line([(cx, ty - 6), (cx + 16, ty - 14)], (30, 30, 34, 255), 8)
        c.ellipse((cx + 12, ty - 19, cx + 21, ty - 10), (15, 15, 18, 255))
    else:
        prism(c, cx, cy, 20, 12, (120, 132, 150))
        base = cy - 12
        crystal = (120, 200, 245)
        tip = (cx, base - 72)
        c.poly([(cx - 13, base), (cx, base + 5), tip], _shade(crystal, -0.25))
        c.poly([(cx, base + 5), (cx + 13, base), tip], _shade(crystal, 0.1))
        c.poly([(cx - 4, base - 20), (cx + 3, base - 40), tip], (235, 250, 255, 220))
        for side in (-1, 1):
            small = (cx + side * 16, base - 34)
            c.poly([(cx + side * 10, base - 2), (cx + side * 20, base - 4), small], _shade(crystal, -0.05))
    return c.done()


def keep() -> Image.Image:
    """The keep, on 2 x 2 cells: a stone hall, four corner turrets, a gate and a banner."""
    c = Canvas(128, 176)
    cx, cy = 64, 176 - 32
    ground_shadow(c, cx, cy + 3, 56)
    stone = (170, 164, 150)
    for dx, dy in ((0, -24), (-40, 0), (40, 0)):
        prism(c, cx + dx, cy + dy, 14, 62, stone)
    prism(c, cx, cy, 40, 44, stone)
    c.poly([(cx - 22, cy + 9), (cx - 8, cy + 16), (cx - 8, cy - 8), (cx - 22, cy - 15)], (60, 42, 30, 255))
    for dx, dy in ((0, 24),):
        prism(c, cx + dx, cy + dy, 14, 62, stone)
    pole = (cx, cy - 44 - 20)
    c.line([pole, (pole[0], pole[1] - 46)], (70, 50, 36, 255), 2)
    c.poly([(pole[0], pole[1] - 46), (pole[0] + 26, pole[1] - 40), (pole[0], pole[1] - 30)], (60, 110, 200, 255))
    return c.done()


def enemy(kind: str, heading: int) -> Image.Image:
    """An enemy facing a heading (0 +x lower right, 1 +y lower left, 2 -x upper left, 3 -y upper right): a body, a
    head, and the face (eyes) only when it walks towards the viewer."""
    c = Canvas(64, 64)
    cx, ground = 32, 64 - 16
    side = 1 if heading in (0, 3) else -1
    front = heading in (0, 1)
    if kind == "runner":
        body, head, size, height = (200, 190, 70), (230, 200, 150), 7, 18
    elif kind == "soldier":
        body, head, size, height = (180, 50, 50), (230, 200, 150), 9, 22
    else:
        body, head, size, height = (120, 124, 136), (150, 154, 166), 12, 26
    ground_shadow(c, cx, ground, size + 3)
    foot = 2 * side
    c.line([(cx - 3 + foot, ground), (cx - 3, ground - 6)], _shade(body, -0.5), 3)
    c.line([(cx + 3 + foot, ground - 1), (cx + 3, ground - 6)], _shade(body, -0.5), 3)
    c.ellipse((cx - size, ground - height, cx + size, ground - 4), _shade(body, -0.05))
    c.ellipse((cx - size + 2 + side, ground - height + 2, cx + side * 2, ground - 8), _shade(body, 0.12))
    radius = size * 0.6 + 2
    hy = ground - height - radius + 3
    c.ellipse((cx - radius, hy - radius, cx + radius, hy + radius), head + (255,))
    if kind == "soldier":
        c.ellipse((cx - radius - 1, hy - radius - 2, cx + radius + 1, hy + 1), (110, 110, 118, 255))
    if kind == "armoured":
        c.line([(cx - size + 2, ground - height / 2), (cx + size - 2, ground - height / 2)], _shade(body, -0.4), 2)
        c.ellipse((cx - radius - 1, hy - radius - 1, cx + radius + 1, hy + radius), (100, 102, 112, 255))
        c.line([(cx - radius + 2, hy), (cx + radius - 2, hy)], (20, 20, 24, 255), 2)
    if front and kind != "armoured":
        for dx in (-2.5, 2.5):
            ex = cx + side * 2.5 + dx
            c.ellipse((ex - 1.3, hy - 1.5, ex + 1.3, hy + 1.2), (20, 20, 24, 255))
    if not front:
        c.ellipse((cx - side * 4 - 4, ground - height + 2, cx - side * 4 + 4, ground - height + 12), _shade(body, -0.3))
    return c.done()


def enemy_sheet() -> Image.Image:
    """Every enemy, a row each, a heading per column."""
    sheet = Image.new("RGBA", (64 * HEADINGS, 64 * len(ENEMIES)), (0, 0, 0, 0))
    for row, kind in enumerate(ENEMIES):
        for heading in range(HEADINGS):
            sheet.paste(enemy(kind, heading), (heading * 64, row * 64))
    return sheet


ICONS = (
    "archer",
    "cannon",
    "frost",
    "sell",
    "upgrade",
    "locked",
    "pierce",
    "double",
    "fire",
    "shock",
    "freeze",
    "aura",
)
"""Badges of ``icons.png``, 6 per row: the towers, sell, the next level, a locked power-up, then the power-ups."""


def icon(kind: str) -> Image.Image:
    """A round badge of the build ring and of the HUD."""
    c = Canvas(64, 64)
    rims = {
        "archer": (176, 52, 44),
        "cannon": (90, 90, 100),
        "frost": (60, 140, 210),
        "sell": (200, 160, 40),
        "upgrade": (60, 150, 70),
        "locked": (70, 70, 74),
        "pierce": (176, 52, 44),
        "double": (176, 52, 44),
        "fire": (210, 100, 30),
        "shock": (120, 100, 70),
        "freeze": (90, 180, 230),
        "aura": (130, 80, 190),
    }
    rim = rims[kind]
    c.ellipse((2, 2, 62, 62), _shade(rim, -0.4))
    c.ellipse((5, 5, 59, 59), _shade(rim, 0.0))
    c.ellipse((10, 10, 54, 54), (245, 236, 214, 255) if kind != "locked" else (150, 148, 144, 255))
    ink = (50, 36, 28, 255)
    if kind in ("archer", "pierce", "double"):
        c.draw.arc([18 * SS, 14 * SS, 40 * SS, 50 * SS], 270, 90, fill=ink, width=3 * SS)
        c.line([(29, 15), (29, 49)], ink, 1.2)
        for lift in (-6, 6) if kind == "double" else (0,):
            c.line([(18, 32 + lift), (46, 32 + lift)], ink, 2.5)
            c.poly([(46, 32 + lift), (40, 28 + lift), (40, 36 + lift)], ink)
        if kind == "pierce":
            for x in (36, 42):
                c.ellipse((x - 3, 29, x + 3, 35), (176, 52, 44, 255))
    elif kind in ("cannon", "fire", "shock"):
        c.ellipse((20, 22, 44, 46), ink)
        c.ellipse((24, 26, 30, 32), (150, 150, 160, 255))
        if kind == "fire":
            c.poly([(32, 12), (40, 26), (34, 24), (32, 30), (28, 22), (24, 26)], (230, 110, 30, 255))
        if kind == "shock":
            for r in (20, 26):
                c.ellipse((32 - r, 34 - r, 32 + r, 34 + r), (120, 90, 50, 255), width=2)
    elif kind in ("frost", "freeze", "aura"):
        colour = (40, 110, 190, 255) if kind != "aura" else (120, 60, 180, 255)
        for degrees in (0, 60, 120):
            ray = math.radians(degrees)
            dx, dy = 15 * math.cos(ray), 15 * math.sin(ray)
            c.line([(32 - dx, 32 - dy), (32 + dx, 32 + dy)], colour, 3)
        if kind == "freeze":
            c.ellipse((24, 24, 40, 40), (200, 240, 255, 255))
        if kind == "aura":
            c.ellipse((13, 13, 51, 51), colour, width=2)
    elif kind == "upgrade":
        c.poly([(32, 14), (48, 32), (38, 32), (38, 48), (26, 48), (26, 32), (16, 32)], (60, 150, 70, 255))
    elif kind == "locked":
        c.ellipse((22, 14, 42, 38), (60, 60, 64, 255), width=4)
        c.poly([(18, 30), (46, 30), (46, 50), (18, 50)], (60, 60, 64, 255))
    else:
        c.ellipse((18, 18, 46, 46), (220, 176, 40, 255))
        c.ellipse((22, 22, 42, 42), (245, 205, 80, 255))
        c.line([(32, 24), (32, 40)], (150, 110, 20, 255), 3)
    return c.done()


def icon_sheet() -> Image.Image:
    """Every badge, 6 by row, in the order of ``ICONS``."""
    sheet = Image.new("RGBA", (64 * 6, 64 * 2), (0, 0, 0, 0))
    for index, kind in enumerate(ICONS):
        sheet.paste(icon(kind), ((index % 6) * 64, (index // 6) * 64))
    return sheet


def ring() -> Image.Image:
    """The range ring, 256 x 256: a ground circle laid flat by the ``ground`` layer, a thick white rim and a light
    fill."""
    c = Canvas(256, 256)
    c.ellipse((3, 3, 253, 253), (255, 255, 255, 40))
    c.ellipse((3, 3, 253, 253), (255, 255, 255, 235), width=5)
    return c.done()


def cursor() -> Image.Image:
    """The frame of the pad under the cursor, laid flat on the ground (64 x 64)."""
    c = Canvas(64, 64)
    c.ellipse((4, 4, 60, 60), (255, 230, 120, 255), width=4)
    for angle in range(0, 360, 90):
        a = math.radians(angle + 45)
        x, y = 32 + 30 * math.cos(a), 32 + 30 * math.sin(a)
        c.ellipse((x - 4, y - 4, x + 4, y + 4), (255, 230, 120, 255))
    return c.done()


def shadow() -> Image.Image:
    """The soft shadow under an enemy, laid flat on the ground."""
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    assert pixels is not None
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - 31.5, y - 31.5) / 32.0
            if d < 1.0:
                pixels[x, y] = (0, 0, 0, round(200 * min(1.0, (1.0 - d) * 2.0)))
    return img


def fx_projectiles() -> Image.Image:
    """Arrow (pointing right: the script turns it along its flight), cannon ball, fire shell, frost shard; 32 x 32."""
    sheet = Image.new("RGBA", (128, 32), (0, 0, 0, 0))
    arrow = Canvas(32, 32)
    arrow.line([(4, 16), (26, 16)], (130, 88, 44, 255), 2)
    arrow.poly([(30, 16), (24, 12), (24, 20)], (225, 225, 232, 255))
    arrow.poly([(4, 16), (1, 12), (8, 16), (1, 20)], (235, 235, 235, 255))
    ball = Canvas(32, 32)
    ball.ellipse((8, 8, 24, 24), (30, 30, 36, 255))
    ball.ellipse((11, 11, 16, 16), (130, 130, 140, 255))
    fire = Canvas(32, 32)
    fire.ellipse((3, 3, 29, 29), (255, 140, 40, 120))
    fire.ellipse((8, 8, 24, 24), (255, 190, 60, 255))
    fire.ellipse((12, 11, 20, 19), (255, 250, 200, 255))
    shard = Canvas(32, 32)
    shard.poly([(16, 2), (25, 16), (16, 30), (7, 16)], (150, 220, 255, 255))
    shard.poly([(16, 6), (20, 16), (16, 22)], (240, 252, 255, 255))
    for index, image in enumerate((arrow, ball, fire, shard)):
        sheet.paste(image.done(), (index * 32, 0))
    return sheet


def fx_fire() -> Image.Image:
    """Four frames of a flickering flame (torches, burning ground), 32 x 40 each."""
    sheet = Image.new("RGBA", (128, 40), (0, 0, 0, 0))
    rng = random.Random(5)
    for frame in range(4):
        c = Canvas(32, 40)
        for layer, (colour, width) in enumerate(
            (((230, 80, 20, 220), 12), ((255, 160, 40, 240), 8), ((255, 240, 170, 255), 4))
        ):
            sway = rng.uniform(-3, 3)
            top = 6 + layer * 7 + rng.uniform(-2, 2)
            c.poly([(16 - width, 38), (16 + sway, top), (16 + width, 38)], colour)
            c.ellipse((16 - width, 30, 16 + width, 39), colour)
        sheet.paste(c.done(), (frame * 32, 0))
    return sheet


def puff() -> Image.Image:
    """A soft white disc fading to the edge, tinted and scaled by the scripts (splash, death, frost)."""
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    assert pixels is not None
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - 31.5, y - 31.5) / 32.0
            if d < 1.0:
                pixels[x, y] = (255, 255, 255, round(255 * (1.0 - d) ** 1.5))
    return img


def bar() -> Image.Image:
    """The health bar: a white strip with a dark rim, tinted by the scripts."""
    img = Image.new("RGBA", (32, 6), (255, 255, 255, 255))
    ImageDraw.Draw(img).rectangle([0, 0, 31, 5], outline=(40, 40, 40, 255))
    return img


def title_art() -> Image.Image:
    """The title (1400 x 260): TOWER DEFENCE in stone letters with a gold rim, a small banner at each end."""
    width, height = 1400, 260
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    font = ImageFont.truetype(str(FONTS / "OpenSans-ExtraBold.ttf"), 150)
    text = "TOWER DEFENCE"
    draw = ImageDraw.Draw(img)
    box = draw.textbbox((0, 0), text, font=font)
    x = (width - (box[2] - box[0])) / 2 - box[0]
    y = (height - (box[3] - box[1])) / 2 - box[1]
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((x + 8, y + 10), text, font=font, fill=(0, 0, 0, 170))
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(7)))
    draw = ImageDraw.Draw(img)
    draw.text((x, y), text, font=font, fill=(196, 188, 170, 255), stroke_width=7, stroke_fill=(214, 168, 60, 255))
    draw.text((x, y + 4), text, font=font, fill=(150, 142, 128, 255))
    draw.text((x, y), text, font=font, fill=(186, 178, 162, 255))
    for bx in (34, width - 70):
        draw.rectangle([bx, 40, bx + 36, 200], fill=(150, 34, 34, 255), outline=(214, 168, 60, 255), width=4)
        draw.polygon([(bx, 200), (bx + 18, 180), (bx + 36, 200), (bx + 36, 220), (bx, 220)], fill=(0, 0, 0, 0))
    return img


def banner() -> Image.Image:
    """A menu button (640 x 100): a gold rim with stone corners, see-through inside so that the button colour shows."""
    size = (640, 100)
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size[0] - 1, size[1] - 1], fill=(255, 255, 255, 18), outline=(214, 168, 60, 255), width=5)
    draw.rectangle([9, 9, size[0] - 10, size[1] - 10], outline=(120, 90, 30, 200), width=2)
    for x in (0, size[0] - 24):
        for y in (0, size[1] - 24):
            draw.rectangle([x, y, x + 23, y + 23], fill=(150, 144, 132, 255), outline=(90, 86, 80, 255), width=2)
    return img


def textures() -> None:
    """Every image of the game."""
    for folder in ("towers", "ui", "fx", "enemies"):
        (TEXTURES / folder).mkdir(parents=True, exist_ok=True)
    for kind, model in creatures.CREATURES.items():
        creatures.sheet(model()).save(TEXTURES / "enemies" / f"{kind}.png")
    tower("archer").save(TEXTURES / "towers" / "archer.png")
    keep().save(TEXTURES / "keep.png")
    enemy_sheet().save(TEXTURES / "enemies.png")
    icon_sheet().save(TEXTURES / "icons.png")
    ring().save(TEXTURES / "fx" / "ring.png")
    cursor().save(TEXTURES / "fx" / "cursor.png")
    shadow().save(TEXTURES / "fx" / "shadow.png")
    fx_projectiles().save(TEXTURES / "fx" / "projectiles.png")
    fx_fire().save(TEXTURES / "fx" / "fire.png")
    puff().save(TEXTURES / "puff.png")
    bar().save(TEXTURES / "bar.png")
    title_art().save(TEXTURES / "ui" / "title.png")
    banner().save(TEXTURES / "ui" / "banner.png")


# Sounds ---------------------------------------------------------------------------------------------------------------


def _write(name: str, samples: list[float], peak: float = 0.8) -> None:
    """Write the samples, scaled so that the loudest one reaches ``peak`` of the full scale."""
    path = SOUNDS / name
    path.parent.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def _tone(seconds: float, frequency: Callable[[float], float], decay: float, square: float = 0.0) -> list[float]:
    """A tone of a varying frequency, an exponential decay and a share of square wave."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * frequency(t) / RATE
        sine = math.sin(phase)
        value = (1.0 - square) * sine + square * (1.0 if sine >= 0.0 else -1.0)
        out.append(value * min(1.0, t / 0.004) * math.exp(-t * decay))
    return out


def _noise(seconds: float, decay: float, seed: int, smooth: float = 0.0) -> list[float]:
    """White noise with an exponential decay, low-passed by ``smooth`` (0 none, close to 1 a dull thud)."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(round(seconds * RATE)):
        last = smooth * last + (1.0 - smooth) * rng.uniform(-1.0, 1.0)
        out.append(last * math.exp(-i / RATE * decay))
    return out


def _mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(length)]


def _at(seconds: float, track: list[float]) -> list[float]:
    """A track delayed by ``seconds``."""
    return [0.0] * round(seconds * RATE) + track


def _constant(frequency: float) -> Callable[[float], float]:
    return lambda _t: frequency


def _horn() -> list[float]:
    """The wave horn: a low brass-like tone swelling then fading, 0.9 s."""
    out = []
    for i in range(round(0.9 * RATE)):
        t = i / RATE
        envelope = min(1.0, t / 0.15) * min(1.0, (0.9 - t) / 0.3)
        f = 147.0 * (1.0 + 0.01 * math.sin(2 * math.pi * 5 * t))
        value = sum(math.sin(2 * math.pi * f * k * t) / k for k in (1, 2, 3, 4))
        out.append(value * envelope)
    return out


def _bell(seconds: float, frequency: float) -> list[float]:
    """A soft bell: a sine and a quiet overtone."""
    out = []
    for i in range(round(seconds * RATE)):
        t = i / RATE
        value = math.sin(2 * math.pi * frequency * t) + 0.3 * math.sin(2 * math.pi * frequency * 2.76 * t)
        out.append(value * min(1.0, t / 0.003) * math.exp(-t * 5.0))
    return out


def sounds() -> None:
    """Every sound of the game."""
    knock = _mix(_noise(0.12, 30.0, 1, smooth=0.9), _tone(0.12, lambda t: 120.0 - 200.0 * t, 25.0))
    _write("game/build.wav", _mix(knock, _at(0.1, knock)), peak=0.6)
    coins = _mix(*(_at(0.05 * i, _tone(0.12, _constant(1800.0 + 300.0 * i), 30.0)) for i in range(4)))
    _write("game/sell.wav", coins, peak=0.4)
    _write("game/coins.wav", _mix(coins, _at(0.12, coins)), peak=0.45)
    twang = _mix(_tone(0.12, lambda t: 520.0 - 900.0 * t, 28.0, square=0.15), _noise(0.05, 80.0, 2, smooth=0.3))
    _write("game/arrow.wav", twang, peak=0.35)
    _write("game/cannon.wav", _mix(_noise(0.35, 12.0, 3, smooth=0.93), _tone(0.3, lambda t: 70.0 - 40.0 * t, 10.0)))
    _write("game/boom.wav", _noise(0.4, 9.0, 4, smooth=0.88), peak=0.6)
    shimmer = _mix(*(_at(0.03 * i, _tone(0.2, _constant(1200.0 + 400.0 * i), 14.0)) for i in range(4)))
    _write("game/frost.wav", shimmer, peak=0.3)
    _write("game/hit.wav", _noise(0.05, 70.0, 5, smooth=0.7), peak=0.3)
    _write("game/pop.wav", _tone(0.1, lambda t: 300.0 + 1200.0 * t, 25.0, square=0.2), peak=0.35)
    _write("game/leak.wav", _mix(_noise(0.3, 14.0, 6, smooth=0.95), _tone(0.3, lambda t: 90.0 - 50.0 * t, 9.0)))
    _write("game/horn.wav", _horn(), peak=0.5)
    _write("game/defeat.wav", _mix(_bell(1.2, 220.0), _at(0.3, _bell(1.2, 185.0)), _at(0.6, _bell(1.4, 147.0))))
    _write("ui/click.wav", _tone(0.05, _constant(900.0), 60.0, square=0.3), peak=0.3)
    _write("ui/start.wav", _mix(_bell(0.6, 392.0), _at(0.15, _bell(0.6, 494.0)), _at(0.3, _bell(1.0, 587.0))))
    shutil.copyfile(DEMO / "sounds" / "ui" / "victory.wav", SOUNDS / "ui" / "victory.wav")


if __name__ == "__main__":
    textures()
    sounds()
