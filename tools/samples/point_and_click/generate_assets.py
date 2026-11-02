#!/usr/bin/env python3
"""
Paint the pictures and synthesise the sounds of The Moon Lantern, the point-and-click sample game, into
``samples/point_and_click/`` (deterministic, home-made).

- ``textures/rooms/<room>.png``: the 320 x 180 pixel-art backdrop of each room (``rooms.py``), and for each
  walk-behind mask and prop of the room its own picture (``<room>_<name>.png``), cut from the same painting so that a
  sprite laid over the backdrop at its rectangle fits exactly; a prop with several frames is a sheet of one row;
- ``textures/items.png``: the icons of the inventory (``rooms.ITEMS``, 24 x 24 each, one row), and each of them alone
  in ``textures/items/<item>.png`` (the object in hand, drawn as the cursor);
- ``textures/cursors.png``: the action cursors (walk, look, use, talk, pick, exit left, exit right, pointer; 16 x 16);
- ``textures/ui/``: the inventory slot and the title vignette.

Sounds (``sounds/``): mono 16 bit at 22.05 kHz, short and neutral: a click, a pick-up, a combination, a door, a
munch, a sparkle, the fire, the stream and the fireflies (loops), a calm music loop and the ending jingle.

The 3D actors are Quaternius characters (CC0, from poly.pizza): the adventurer (the hero), the wizard (Barnaby) and the
goblin (the bridge troll), in ``meshes/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.point_and_click.generate_assets``.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from collections.abc import Sequence
from pathlib import Path

from PIL import Image

from tools.samples.point_and_click import rooms
from tools.samples.point_and_click.pixel import Canvas, Colour, lighten

ROOT = Path(__file__).resolve().parents[3] / "samples" / "point_and_click"
"""The sample project."""
W, H = rooms.WIDTH, rooms.HEIGHT

INK = (24, 16, 22)
WOOD = [(52, 32, 24), (78, 50, 32), (104, 70, 42), (132, 92, 56), (160, 116, 72)]
STONE = [(58, 58, 70), (82, 82, 96), (110, 108, 120), (140, 136, 146), (172, 166, 170)]
NIGHT = [(10, 12, 34), (20, 26, 62), (34, 48, 92), (56, 76, 124), (84, 108, 150)]
LEAF = [(12, 30, 30), (20, 46, 38), (30, 68, 46), (48, 96, 58), (76, 128, 72), (112, 160, 90)]
FIRE = [(250, 236, 150), (248, 184, 72), (226, 112, 40), (170, 56, 28)]
GOLD = [(120, 84, 30), (176, 130, 46), (222, 180, 80), (250, 224, 140)]
MOON = [(150, 190, 230), (200, 226, 250), (240, 248, 255)]
WATER = [(14, 30, 54), (22, 46, 78), (36, 68, 104), (70, 110, 146)]


# --- shared pieces -------------------------------------------------------------------------------------------------


def stars(canvas: Canvas, x0: int, y0: int, x1: int, y1: int, count: int) -> None:
    """Twinkling dots in a sky."""
    for _ in range(count):
        x, y = canvas.random.randint(x0, x1), canvas.random.randint(y0, y1)
        bright = canvas.random.random()
        canvas.put(x, y, MOON[2] if bright > 0.8 else MOON[0])
        if bright > 0.93:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                canvas.put(x + dx, y + dy, MOON[0])


def tree_line(canvas: Canvas, base: int, height: int, colour: Colour, seed: int, spacing: int = 14) -> None:
    """A row of tree crowns and trunks standing on a line."""
    rng = random.Random(seed)
    x = -rng.randint(0, spacing)
    while x < W + spacing:
        top = base - height + rng.randint(-height // 3, height // 4)
        radius = rng.randint(spacing // 2 + 2, spacing + 4)
        if rng.random() < 0.5:
            canvas.polygon([(x, top), (x - radius, base), (x + radius, base)], colour)
        else:
            canvas.ellipse(x - radius, top, x + radius, base + radius // 2, colour)
        x += rng.randint(spacing - 4, spacing + 6)
    canvas.rect(0, base, W - 1, base + 6, colour)


def leaves(canvas: Canvas, cx: int, cy: int, rx: int, ry: int, shades: Sequence[Colour], seed: int) -> None:
    """A clump of foliage: blobs from dark to light, lit from the top left."""
    rng = random.Random(seed)
    for index, shade in enumerate(shades):
        for _ in range(16 - index * 2):
            angle = rng.random() * math.tau
            distance = rng.random() ** 0.6
            x = cx + math.cos(angle) * rx * distance * (1 - index * 0.12) - index * 2
            y = cy + math.sin(angle) * ry * distance * (1 - index * 0.12) - index * 2
            size = rng.randint(3, 6) - index // 2
            canvas.ellipse(round(x - size), round(y - size * 0.8), round(x + size), round(y + size * 0.8), shade)


def planks_vertical(canvas: Canvas, x0: int, y0: int, x1: int, y1: int, width: int, seed: int) -> None:
    """A wall of vertical planks with seams, grain and knots."""
    rng = random.Random(seed)
    x = x0
    index = 0
    while x <= x1:
        shade = WOOD[1 + (index * 7 + rng.randint(0, 1)) % 3]
        canvas.rect(x, y0, min(x1, x + width - 1), y1, shade)
        canvas.rect(x, y0, x, y1, WOOD[0])
        for _ in range(width // 2):
            gx = x + rng.randint(2, width - 2)
            gy = rng.randint(y0, y1 - 8)
            canvas.line([(gx, gy), (gx, gy + rng.randint(4, 14))], lighten(shade, -14))
        if rng.random() < 0.6:
            kx, ky = x + rng.randint(3, width - 4), rng.randint(y0 + 6, y1 - 6)
            canvas.ellipse(kx - 1, ky - 2, kx + 1, ky + 2, WOOD[0])
        x += width
        index += 1


def floor_boards(canvas: Canvas, y0: int, y1: int, seed: int) -> None:
    """Floor boards running across, wider towards the viewer, their seams staggered."""
    rng = random.Random(seed)
    rows = [y0]
    height = 4
    while rows[-1] < y1:
        rows.append(min(y1, rows[-1] + height))
        height += 1
    for index in range(len(rows) - 1):
        top, bottom = rows[index], rows[index + 1]
        shade = WOOD[2 + index % 2]
        canvas.rect(0, top, W - 1, bottom, shade)
        canvas.line([(0, top), (W - 1, top)], WOOD[0])
        x = rng.randint(0, 40)
        while x < W:
            canvas.line([(x, top), (x, bottom)], WOOD[1])
            x += rng.randint(48, 90)
        canvas.speckle(0, top + 1, W - 1, bottom - 1, [lighten(shade, 10), lighten(shade, -10)], 0.05)


def stone_blocks(
    canvas: Canvas, x0: int, y0: int, x1: int, y1: int, seed: int, shades: Sequence[Colour] = STONE
) -> None:
    """A wall of stone blocks in staggered courses."""
    rng = random.Random(seed)
    y = y0
    course = 0
    while y <= y1:
        height = rng.randint(6, 8)
        x = x0 - (course % 2) * 6
        while x <= x1:
            width = rng.randint(9, 14)
            shade = shades[rng.randint(1, 3)]
            canvas.rect(max(x0, x), y, min(x1, x + width - 2), min(y1, y + height - 2), shade)
            canvas.line([(max(x0, x), y), (min(x1, x + width - 2), y)], lighten(shade, 18))
            x += width
        y += height
        course += 1


def save(image: Image.Image, relative: str) -> None:
    """Write a picture of the project."""
    path = ROOT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)


def sheet(frames: list[Image.Image]) -> Image.Image:
    """Frames side by side."""
    width, height = frames[0].size
    out = Image.new("RGBA", (width * len(frames), height), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        out.paste(frame, (index * width, 0))
    return out


# --- the cottage ---------------------------------------------------------------------------------------------------


def paint_lantern(lit: bool) -> Canvas:
    """The Moon Lantern on its rectangle of the cottage: brass, four panes, dark or glowing with moonlight."""
    x0, y0, x1, y1 = rooms.COTTAGE.props[2].rect
    canvas = Canvas(x1 - x0 + 1, y1 - y0 + 1, 31)
    w = x1 - x0
    cx = w // 2
    # Ring and roof.
    canvas.ellipse(cx - 4, 0, cx + 4, 6, GOLD[1], INK)
    canvas.polygon([(cx, 5), (2, 20), (w - 2, 20)], GOLD[1], INK)
    canvas.line([(cx, 6), (4, 19)], GOLD[3])
    # Body: frame and panes.
    canvas.rect(4, 20, w - 4, 50, INK)
    pane = MOON[2] if lit else (36, 44, 66)
    inner = MOON[1] if lit else (52, 62, 88)
    canvas.rect(6, 22, w - 6, 48, inner)
    canvas.rect(9, 25, w - 9, 45, pane)
    if lit:
        canvas.ellipse(cx - 5, 28, cx + 5, 42, (255, 255, 255))
    else:
        canvas.rect(cx - 3, 32, cx + 3, 40, (70, 74, 92))
        canvas.put(cx, 36, (110, 116, 140))
    canvas.rect(cx - 1, 20, cx + 1, 50, GOLD[0])
    canvas.rect(4, 34, w - 4, 35, GOLD[1])
    # Base.
    canvas.polygon([(3, 50), (w - 3, 50), (w - 7, 58), (7, 58)], GOLD[1], INK)
    canvas.line([(5, 51), (w - 5, 51)], GOLD[3])
    return canvas


def paint_cottage() -> dict[str, Image.Image]:
    """Barnaby's cottage by firelight."""
    room = rooms.COTTAGE
    back = Canvas(W, H, 11)
    planks_vertical(back, 0, 0, W - 1, 117, 16, 3)
    back.rect(0, 0, W - 1, 7, WOOD[0])
    for x in (60, 160, 260):
        back.rect(x - 4, 0, x + 4, 12, WOOD[1])
        back.frame(x - 4, 0, x + 4, 12, WOOD[0])
    back.rect(0, 114, W - 1, 117, WOOD[0])
    floor_boards(back, 117, H - 1, 5)
    # Window: a night with a thin moon.
    back.rect(116, 26, 174, 84, WOOD[0])
    back.gradient(121, 31, 169, 79, [NIGHT[0], NIGHT[1], NIGHT[2], NIGHT[3]])
    stars(back, 122, 32, 168, 70, 18)
    back.ellipse(150, 38, 162, 50, MOON[1])
    back.ellipse(146, 36, 158, 48, NIGHT[1])
    back.rect(144, 31, 146, 79, WOOD[1])
    back.rect(121, 54, 169, 56, WOOD[1])
    back.rect(114, 82, 176, 86, WOOD[3])
    back.line([(114, 82), (176, 82)], WOOD[4])
    # Door to the glade.
    back.rect(6, 40, 38, 114, WOOD[0])
    back.ellipse(6, 30, 38, 52, WOOD[0])
    back.rect(9, 44, 35, 113, WOOD[2])
    back.ellipse(9, 33, 35, 54, WOOD[2])
    for x in (15, 22, 29):
        back.line([(x, 36), (x, 113)], WOOD[1])
    for y in (54, 96):
        back.rect(9, y, 35, y + 2, STONE[1])
    back.ellipse(29, 74, 32, 77, GOLD[2])
    # Shelf with books and a bottle (the jar is a prop).
    back.rect(28, 51, 92, 54, WOOD[3])
    back.line([(28, 51), (92, 51)], WOOD[4])
    back.rect(28, 55, 92, 56, WOOD[0])
    for x in (32, 86):
        back.polygon([(x, 56), (x + 3, 56), (x, 62)], WOOD[1])
    for index, x in enumerate(range(31, 55, 4)):
        colour = [(120, 40, 40), (40, 80, 120), (60, 110, 60), (140, 110, 50), (90, 50, 110), (130, 60, 40)][index]
        top = 40 + (index * 3) % 5
        back.rect(x, top, x + 3, 50, colour)
        back.line([(x, top), (x, 50)], lighten(colour, 30))
        back.rect(x, top + 3, x + 3, top + 3, GOLD[2])
    back.rect(78, 42, 84, 50, (60, 120, 90))
    back.rect(80, 38, 82, 42, (60, 120, 90))
    back.rect(80, 36, 82, 37, WOOD[1])
    # Fireplace.
    stone_blocks(back, 218, 40, 292, 116, 9)
    back.rect(214, 36, 296, 42, WOOD[1])
    back.line([(214, 36), (296, 36)], WOOD[4])
    back.rect(232, 66, 278, 116, (26, 18, 16))
    back.ellipse(232, 56, 278, 80, (26, 18, 16))
    back.rect(234, 106, 276, 116, (44, 30, 24))
    for x in range(238, 274, 8):
        back.rect(x, 108, x + 6, 112, WOOD[0])
    back.rect(246, 22, 264, 34, STONE[1])
    back.rect(250, 26, 260, 34, GOLD[1])
    back.ellipse(251, 27, 259, 33, MOON[0])
    # The lantern's pedestal.
    back.rect(292, 106, 312, 136, STONE[2])
    back.rect(290, 104, 314, 108, STONE[3])
    back.rect(290, 132, 314, 136, STONE[1])
    back.line([(296, 110), (296, 130)], STONE[3])
    # Rug.
    back.ellipse(150, 138, 252, 168, (110, 36, 40))
    back.ellipse(158, 142, 244, 164, (150, 60, 50))
    back.ellipse(176, 147, 226, 159, (110, 36, 40))
    # Warm light of the fire over the room, colder by the window.
    back.glow(255, 100, 190, (255, 170, 90), 0.45, 0.8)
    back.glow(145, 70, 60, (120, 150, 210), 0.18)
    # Table in front (a mask) with its shadow.
    table = Canvas(W, H, 13)
    back.ellipse(70, 158, 152, 170, WOOD[1])
    table.polygon([(70, 124), (150, 124), (154, 138), (66, 138)], WOOD[3], INK)
    table.line([(72, 126), (148, 126)], WOOD[4])
    table.rect(66, 138, 154, 144, WOOD[1])
    table.line([(66, 144), (154, 144)], INK)
    for x in (72, 142):
        table.rect(x, 144, x + 6, 166, WOOD[1])
        table.line([(x, 144), (x, 166)], WOOD[0])
    table.glow(130, 124, 60, (255, 170, 90), 0.3)
    back.paste(table)
    # Props.
    # The objects to pick are lighter than what is round them and outlined in ink, like the icons of the bag.
    jar = Canvas(15, 15, 17)
    jar.rect(3, 4, 11, 13, (176, 214, 224, 220))
    jar.frame(3, 4, 11, 13, (236, 246, 250))
    jar.rect(4, 1, 10, 3, GOLD[1])
    jar.line([(5, 6), (5, 11)], (255, 255, 255))
    jar.outline_alpha(INK)
    crust = Canvas(25, 13, 19)
    crust.ellipse(1, 2, 23, 11, (236, 186, 98), INK)
    crust.ellipse(4, 4, 20, 9, (252, 222, 150))
    crust.ellipse(8, 5, 16, 8, (176, 116, 54))
    crust.line([(6, 3), (12, 3)], (255, 244, 200))
    crust.outline_alpha(INK)
    fire_frames = []
    for frame in range(3):
        fire = Canvas(39, 31, 40 + frame)
        rng = random.Random(70 + frame)
        for index, shade in enumerate(FIRE[::-1]):
            for _ in range(5):
                cx = 19 + rng.randint(-10, 10) * (1 - index * 0.2)
                top = 4 + index * 5 + rng.randint(-3, 3)
                width = 6 - index
                fire.polygon([(cx, top), (cx - width, 28), (cx + width, 28)], shade)
        fire.rect(6, 27, 32, 30, FIRE[3])
        fire_frames.append(fire.image)
    pictures = {
        "cottage": back.image,
        "cottage_table": table.crop(room.masks[0].rect),
        "cottage_jar": jar.image,
        "cottage_crust": crust.image,
        "cottage_lantern": sheet([paint_lantern(False).image, paint_lantern(True).image]),
        "cottage_fire": sheet(fire_frames),
    }
    return pictures


# --- the glade -----------------------------------------------------------------------------------------------------


def paint_glade(title: bool = False) -> dict[str, Image.Image]:
    """The glade at night: stars, three rows of trees, the pond, the bush, the mushrooms and the old oak."""
    room = rooms.GLADE
    back = Canvas(W, H, 21)
    back.gradient(0, 0, W - 1, 110, [NIGHT[0], NIGHT[1], NIGHT[2], NIGHT[3]])
    stars(back, 0, 0, W - 1, 70, 90)
    tree_line(back, 92, 34, (24, 38, 64), 4, 16)
    tree_line(back, 104, 30, (18, 42, 46), 5, 13)
    tree_line(back, 116, 26, LEAF[0], 6, 11)
    # Ground: grass and the path from the cottage to the bridge.
    back.gradient(0, 116, W - 1, H - 1, [LEAF[1], LEAF[2], LEAF[3]])
    back.speckle(0, 118, W - 1, H - 1, [LEAF[2], LEAF[4], LEAF[1]], 0.12)
    for x in range(W):
        middle = 152 + math.sin(x / 40) * 6
        half = 7 + 4 * math.sin(x / 23 + 1) + (x / W) * 2
        for y in range(round(middle - half), round(middle + half)):
            back.put(x, y, (102, 82, 58) if (x * 7 + y * 3) % 11 else (86, 68, 48))
    # Pond with reeds.
    back.ellipse(204, 104, 302, 126, WATER[1])
    back.ellipse(212, 107, 294, 123, WATER[2])
    for y in (110, 115, 119):
        back.line([(224, y), (240, y)], WATER[3])
        back.line([(258, y + 1), (280, y + 1)], WATER[3])
    for x in (206, 214, 290, 297):
        back.line([(x, 120), (x - 1, 106)], LEAF[3])
        back.rect(x - 2, 104, x, 108, (110, 80, 50))
    # Berry bush (the berries are a prop).
    leaves(back, 56, 106, 30, 18, LEAF[1:5], 8)
    # Mushroom ring.
    for index in range(9):
        angle = index / 9 * math.tau
        x = round(165 + math.cos(angle) * 18)
        y = round(121 + math.sin(angle) * 5)
        back.rect(x, y, x + 1, y + 3, (226, 214, 190))
        back.ellipse(x - 3, y - 2, x + 4, y + 1, (190, 40, 40))
        back.put(x - 1, y - 1, (250, 240, 230))
    # Bushes closing the left and right edges.
    leaves(back, 4, 112, 18, 26, LEAF[0:4], 13)
    leaves(back, 316, 112, 18, 26, LEAF[0:4], 14)
    back.glow(241, 88, 46, (190, 230, 120), 0.22)
    # The old oak in front (a mask).
    oak = Canvas(W, H, 23)
    oak.polygon(
        [(100, 0), (126, 0), (130, 150), (138, 172), (120, 168), (112, 174), (104, 168), (88, 172), (96, 150)],
        (58, 44, 36),
    )
    for y in range(0, 168, 5):
        oak.line([(100 + (y * 7) % 9, y), (100 + (y * 7) % 9, y + 3)], (40, 30, 26))
        oak.line([(118 + (y * 5) % 7, y + 2), (118 + (y * 5) % 7, y + 6)], (80, 62, 48))
    leaves(oak, 112, 8, 48, 22, [LEAF[0], LEAF[1], LEAF[2]], 31)
    back.ellipse(90, 166, 138, 178, LEAF[0])
    back.paste(oak)
    # Props.
    berries = Canvas(45, 25, 29)
    rng = random.Random(9)
    for _ in range(16):
        x, y = rng.randint(3, 41), rng.randint(3, 21)
        berries.ellipse(x - 1, y - 1, x + 1, y + 1, (186, 56, 150))
        berries.put(x - 1, y - 1, (250, 170, 232))
    berries.outline_alpha(INK)
    firefly_frames = []
    for frame in range(4):
        flies = Canvas(59, 37, 50 + frame)
        rng = random.Random(41)
        for index in range(11):
            phase = frame / 4 * math.tau + index
            x = round(6 + rng.random() * 46 + math.cos(phase) * 3)
            y = round(5 + rng.random() * 26 + math.sin(phase * 1.3) * 3)
            flies.ellipse(x - 2, y - 2, x + 2, y + 2, (200, 240, 120, 90))
            flies.put(x, y, (250, 255, 190))
        firefly_frames.append(flies.image)
    pictures = {
        "glade": back.image,
        "glade_trunk": oak.crop(room.masks[0].rect),
        "glade_berries": berries.image,
        "glade_fireflies": sheet(firefly_frames),
    }
    if title:
        pictures = {"title": back.image}
    return pictures


# --- the bridge ----------------------------------------------------------------------------------------------------


def paint_bridge() -> dict[str, Image.Image]:
    """The troll bridge at dusk: the stream, the stone bridge, the toll sign and the troll's turnips."""
    room = rooms.BRIDGE
    back = Canvas(W, H, 31)
    back.gradient(0, 0, W - 1, 100, [(30, 26, 66), (66, 50, 98), (140, 84, 108), (210, 130, 110)])
    stars(back, 0, 0, W - 1, 30, 30)
    back.ellipse(36, 18, 96, 30, (120, 84, 120))
    back.ellipse(200, 30, 268, 40, (150, 96, 120))
    tree_line(back, 88, 24, (52, 42, 76), 7, 18)
    # Far bank: the troll's garden behind its fence.
    back.rect(0, 86, W - 1, 124, (50, 78, 52))
    back.speckle(0, 86, W - 1, 124, [(60, 92, 58), (40, 64, 44)], 0.15)
    for row in range(3):
        for x in range(250 + row * 4, 318, 10):
            y = 96 + row * 9
            back.ellipse(x, y, x + 5, y + 4, (220, 200, 210))
            back.ellipse(x + 1, y, x + 4, y + 2, (170, 90, 150))
            back.line([(x + 2, y - 4), (x + 2, y)], LEAF[4])
    back.rect(268, 70, 300, 92, WOOD[1])
    back.polygon([(264, 72), (284, 58), (304, 72)], (120, 50, 40))
    back.rect(282, 80, 288, 92, WOOD[0])
    for x in range(238, W, 7):
        back.rect(x, 108, x + 1, 124, WOOD[2])
    back.rect(238, 112, W - 1, 113, WOOD[2])
    # The stream, from under the bridge to the front right.
    back.polygon([(0, 124), (240, 120), (W - 1, 118), (W - 1, H - 1), (168, H - 1), (150, 150), (0, 150)], WATER[1])
    for index in range(26):
        x = 120 + (index * 37) % 200
        y = 126 + (index * 13) % 50
        back.line([(x, y), (x + 8, y)], WATER[3] if index % 3 else WATER[2])
    # Near bank: grass and earth.
    back.polygon([(0, 140), (104, 146), (150, 150), (168, H - 1), (0, H - 1)], LEAF[2])
    back.speckle(0, 140, 168, H - 1, [LEAF[3], LEAF[1], (96, 80, 56)], 0.14)
    back.polygon([(0, 156), (60, 154), (110, 150), (130, 156), (100, 170), (0, 172)], (104, 84, 58))
    for x in (150, 156, 161):
        back.line([(x, 166), (x - 2, 150)], LEAF[4])
    # Toll sign: a pie drawn on a board.
    back.rect(50, 120, 53, 146, WOOD[1])
    back.rect(38, 104, 66, 122, WOOD[3])
    back.frame(38, 104, 66, 122, WOOD[0])
    back.ellipse(44, 108, 60, 118, (200, 150, 80))
    back.ellipse(46, 109, 58, 114, (130, 40, 110))
    # Bridge: deck and back wall.
    back.rect(100, 112, 240, 124, STONE[1])
    stone_blocks(back, 100, 112, 240, 122, 12)
    back.polygon([(98, 122), (242, 122), (242, 138), (98, 138)], STONE[2])
    back.speckle(100, 123, 240, 137, [STONE[3], STONE[1]], 0.1)
    back.line([(98, 122), (242, 122)], STONE[4])
    # Front parapet with the arch (a mask).
    parapet = Canvas(W, H, 33)
    stone_blocks(parapet, 98, 136, 242, 151, 14)
    parapet.rect(98, 134, 242, 137, STONE[4])
    parapet.draw.ellipse((140, 142, 200, 166), fill=(0, 0, 0, 0))
    back.paste(parapet)
    back.ellipse(142, 144, 198, 164, WATER[0])
    back.paste(parapet)
    moonstone = Canvas(17, 13, 37)
    moonstone.polygon([(8, 0), (15, 5), (12, 12), (4, 12), (1, 5)], MOON[1], (60, 90, 130))
    moonstone.polygon([(8, 2), (12, 5), (8, 9), (4, 5)], MOON[2])
    moonstone.put(6, 4, (255, 255, 255))
    pictures = {
        "bridge": back.image,
        "bridge_parapet": parapet.crop(room.masks[0].rect),
        "bridge_moonstone": moonstone.image,
    }
    return pictures


# --- icons ---------------------------------------------------------------------------------------------------------


def paint_items() -> list[Image.Image]:
    """The inventory icons, 24 x 24 each: jar, jar of fireflies, berries, pie crust, berry pie, moonstone."""
    icons = []
    for item in rooms.ITEMS:
        icon = Canvas(24, 24, 61)
        if item in ("jar", "firefly_jar"):
            icon.rect(6, 6, 17, 21, (150, 190, 200, 180))
            icon.frame(6, 6, 17, 21, (220, 236, 240))
            icon.rect(7, 2, 16, 5, WOOD[2])
            icon.line([(8, 8), (8, 18)], (240, 250, 255))
            if item == "firefly_jar":
                for x, y in ((10, 10), (14, 13), (11, 17), (15, 9)):
                    icon.ellipse(x - 2, y - 2, x + 2, y + 2, (220, 250, 140, 160))
                    icon.put(x, y, (255, 255, 210))
        elif item == "berries":
            for x, y in ((8, 10), (13, 8), (16, 13), (10, 15), (14, 17), (7, 18)):
                icon.ellipse(x - 3, y - 3, x + 3, y + 3, (130, 40, 110), INK)
                icon.put(x - 1, y - 1, (230, 140, 210))
            icon.line([(12, 4), (13, 8)], LEAF[4])
        elif item in ("crust", "pie"):
            icon.ellipse(1, 7, 22, 20, (200, 150, 80), INK)
            icon.ellipse(3, 9, 20, 17, (226, 186, 110))
            if item == "pie":
                icon.ellipse(5, 9, 18, 15, (130, 40, 110))
                for x in (6, 11, 16):
                    icon.line([(x, 9), (x + 2, 15)], (226, 186, 110))
            else:
                icon.ellipse(6, 10, 17, 15, (150, 104, 56))
        elif item == "moonstone":
            icon.polygon([(12, 2), (21, 9), (17, 21), (7, 21), (3, 9)], MOON[1], (60, 90, 130))
            icon.polygon([(12, 5), (17, 9), (12, 15), (7, 9)], MOON[2])
            icon.put(9, 7, (255, 255, 255))
        icon.outline_alpha(INK)
        icons.append(icon.image)
    return icons


def paint_cursors() -> Image.Image:
    """The action cursors, 16 x 16: walk, look, use, talk, pick, exit left, exit right, pointer."""
    light, dark = (250, 240, 214), INK
    icons = []
    for kind in ("walk", "look", "use", "talk", "pick", "exit_left", "exit_right", "pointer"):
        icon = Canvas(16, 16, 71)
        if kind == "walk":
            for x, y in ((4, 9), (10, 4)):
                icon.ellipse(x, y, x + 3, y + 5, light)
                icon.rect(x, y + 6, x + 3, y + 7, light)
        elif kind == "look":
            icon.ellipse(1, 4, 14, 11, light)
            icon.ellipse(5, 5, 10, 10, (70, 120, 180))
            icon.ellipse(6, 6, 9, 9, dark)
        elif kind in ("use", "pick"):
            icon.rect(4, 6, 12, 13, light)
            for x in (4, 6, 8, 10):
                icon.rect(x, 2 if kind == "use" and x == 6 else 4, x + 1, 7, light)
            if kind == "use":
                icon.rect(12, 7, 14, 9, light)
            else:
                icon.rect(2, 8, 4, 10, light)
        elif kind == "talk":
            icon.ellipse(1, 2, 14, 11, light)
            icon.polygon([(4, 9), (3, 14), (8, 10)], light)
            for x in (5, 8, 11):
                icon.put(x, 6, dark)
        elif kind in ("exit_left", "exit_right"):
            points = [(2, 8), (8, 2), (8, 5), (14, 5), (14, 11), (8, 11), (8, 14)]
            if kind == "exit_right":
                points = [(15 - x, y) for x, y in points]
            icon.polygon(points, light)
        else:
            icon.polygon([(1, 1), (1, 13), (4, 10), (7, 15), (9, 14), (6, 9), (11, 9)], light)
        icon.outline_alpha(dark)
        icons.append(icon.image)
    return sheet(icons)


def paint_ui() -> dict[str, Image.Image]:
    """The inventory slot frame, the vignette of the menus and the glow of the lit lantern."""
    slot = Canvas(32, 32, 81)
    slot.rect(0, 0, 31, 31, (40, 28, 30, 220))
    slot.frame(0, 0, 31, 31, GOLD[2])
    slot.frame(1, 1, 30, 30, GOLD[0])
    vignette = Canvas(160, 90, 83)
    for y in range(90):
        for x in range(160):
            distance = math.hypot((x - 80) / 80, (y - 45) / 45)
            alpha = max(0, min(255, round((distance - 0.6) * 380)))
            if alpha:
                vignette.put(x, y, (6, 6, 18, alpha))
    glow = Canvas(64, 64, 85)
    for y in range(64):
        for x in range(64):
            distance = math.hypot(x - 31.5, y - 31.5) / 32
            if distance < 1:
                glow.put(x, y, (255, 255, 255, round(255 * (1 - distance) ** 2)))
    return {"ui/slot": slot.image, "ui/vignette": vignette.image, "ui/glow": glow.image}


# --- sounds --------------------------------------------------------------------------------------------------------

RATE = 22050
"""Samples per second."""


def write_wave(name: str, samples: list[float], peak: float = 0.8) -> None:
    """Write a mono 16 bit sound, scaled so that its loudest sample reaches ``peak``."""
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    path = ROOT / "sounds" / f"{name}.wav"
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def tone(
    frequency: float, seconds: float, decay: float = 6.0, harmonics: tuple[float, ...] = (1.0, 0.3)
) -> list[float]:
    """A plucked note with a few harmonics."""
    out = []
    for i in range(round(seconds * RATE)):
        t = i / RATE
        envelope = min(1.0, t / 0.005) * math.exp(-t * decay)
        out.append(sum(a * math.sin(math.tau * frequency * (k + 1) * t) for k, a in enumerate(harmonics)) * envelope)
    return out


def noise(seconds: float, seed: int, smooth: float = 0.0) -> list[float]:
    """White noise, low-passed by ``smooth`` (0 to 1)."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for _ in range(round(seconds * RATE)):
        last = last * smooth + (rng.random() * 2 - 1) * (1 - smooth)
        out.append(last)
    return out


def loop_fade(samples: list[float], seconds: float = 0.25) -> list[float]:
    """Cross-fade the end of a loop into its start, so that it repeats without a click."""
    count = round(seconds * RATE)
    head, body = samples[:count], samples[count:]
    for i in range(count):
        share = i / count
        body[-count + i] = body[-count + i] * (1 - share) + head[i] * share
    return body


def melody(
    notes: list[tuple[float | None, float]], beat: float, decay: float, harmonics: tuple[float, ...]
) -> list[float]:
    """Notes (semitones from A4, or None for a rest, and beats) one after the other, their tails overlapping."""
    total = round(sum(length for _, length in notes) * beat * RATE + RATE)
    out = [0.0] * total
    start = 0
    for semitone, length in notes:
        if semitone is not None:
            note = tone(440 * 2 ** (semitone / 12), length * beat + 0.6, decay, harmonics)
            for i, s in enumerate(note):
                if start + i < total:
                    out[start + i] += s
        start += round(length * beat * RATE)
    return out


def write_sounds() -> None:
    """Every sound of the game."""
    write_wave("click", tone(880, 0.08, 40))
    sparkle = [0.0] * round(0.5 * RATE)
    for index, semitone in enumerate((12, 16, 19, 24)):
        note = tone(440 * 2 ** (semitone / 12), 0.3, 12)
        for i, s in enumerate(note):
            j = i + index * round(0.06 * RATE)
            if j < len(sparkle):
                sparkle[j] += s
    write_wave("pickup", sparkle)
    pop = [math.sin(math.tau * (300 + 900 * i / RATE) * i / RATE) * math.exp(-i / RATE * 30) for i in range(3000)]
    write_wave("combine", pop + tone(660, 0.25, 14))
    thud = noise(0.4, 3, 0.96)
    write_wave("door", [s * math.exp(-i / RATE * 9) for i, s in enumerate(thud)])
    munch = []
    for bite in range(4):
        chunk = noise(0.12, 10 + bite, 0.7)
        munch += [s * math.exp(-i / RATE * 25) for i, s in enumerate(chunk)] + [0.0] * round(0.1 * RATE)
    write_wave("munch", munch, 0.6)
    fire = noise(4.0, 21, 0.85)
    rng = random.Random(5)
    for _ in range(60):
        at = rng.randint(0, len(fire) - 400)
        for i in range(300):
            fire[at + i] += (rng.random() * 2 - 1) * 3 * math.exp(-i / 40)
    write_wave("fire", loop_fade(fire), 0.5)
    stream = noise(4.0, 31, 0.6)
    stream = [s * (0.8 + 0.2 * math.sin(i / RATE * 3.1)) for i, s in enumerate(stream)]
    write_wave("stream", loop_fade(stream), 0.45)
    hum = []
    for i in range(round(3.0 * RATE)):
        t = i / RATE
        hum.append(
            math.sin(math.tau * 520 * t) * (0.5 + 0.5 * math.sin(math.tau * 7 * t)) * 0.3
            + math.sin(math.tau * 780 * t) * (0.5 + 0.5 * math.sin(math.tau * 5 * t + 1)) * 0.2
        )
    write_wave("fireflies", loop_fade(hum), 0.3)
    # Music: a calm loop in A minor pentatonic, plucked, over a soft drone.
    phrase: list[tuple[float | None, float]] = [
        (0, 1),
        (3, 1),
        (5, 1),
        (7, 2),
        (5, 1),
        (3, 1),
        (0, 2),
        (-2, 1),
        (0, 1),
        (3, 2),
        (None, 2),
        (7, 1),
        (10, 1),
        (12, 2),
        (10, 1),
        (7, 1),
        (5, 2),
        (3, 1),
        (5, 1),
        (0, 2),
        (None, 2),
    ]
    tune = melody(phrase, 0.42, 3.0, (1.0, 0.25, 0.1))
    length = round(sum(beats for _, beats in phrase) * 0.42 * RATE)
    drone = [
        math.sin(math.tau * 110 * i / RATE) * 0.12 + math.sin(math.tau * 164.8 * i / RATE) * 0.06
        for i in range(len(tune))
    ]
    music = [a + b for a, b in zip(tune, drone, strict=True)]
    for i in range(len(tune) - length):
        music[i] += music[length + i]
    write_wave("music", music[:length], 0.5)
    jingle = melody([(0, 1), (4, 1), (7, 1), (12, 2), (11, 1), (12, 3)], 0.22, 2.5, (1.0, 0.4, 0.2))
    write_wave("jingle", jingle, 0.7)


def main() -> None:
    """Paint every picture and write every sound."""
    for pictures in (paint_cottage(), paint_glade(), paint_glade(title=True), paint_bridge(), paint_ui()):
        for name, picture in pictures.items():
            save(picture, f"textures/{name if name.startswith('ui/') else 'rooms/' + name}.png")
    icons = paint_items()
    save(sheet(icons), "textures/items.png")
    for item, icon in zip(rooms.ITEMS, icons, strict=True):
        save(icon, f"textures/items/{item}.png")
    save(paint_cursors(), "textures/cursors.png")
    write_sounds()


if __name__ == "__main__":
    main()
