#!/usr/bin/env python3
"""
The medieval-fantasy look of the Tower defence menus and HUD, drawn from scratch (deterministic): parchment, dark
wood, iron rivets and gold trim, at the size each widget is shown so that nothing is stretched.

- ``textures/ui/panel_*.png``: parchment panels framed in wood and gold (``panel``);
- ``textures/ui/banner.png``: a wooden plank button with iron bands, see-through enough for the hover colour;
- ``textures/ui/frame.png``: the wood and iron frame of the minimap, empty inside;
- ``textures/ui/wheel.png``: the ring of the build wheel, ``textures/ui/plaque.png``: its parchment scroll;
- ``textures/ui/title.png``: TOWER DEFENCE in gold capitals of the ``Cinzel`` font;
- ``textures/ui/stars.png``: the stars of the world map and of the tally (the world map itself is
  ``world_map_art.py``).
"""

from __future__ import annotations

import contextlib
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

RGBA = tuple[int, int, int, int]
PARCHMENT = (232, 212, 168)
INK = (58, 36, 20)
WOOD = (96, 58, 30)
WOOD_DARK = (58, 34, 18)
IRON = (70, 72, 78)
GOLD = (214, 168, 64)


def _noise(size: tuple[int, int], seed: int, scale: int) -> Image.Image:
    """Smooth grey noise: a small random image blown up and blurred."""
    rng = random.Random(seed)
    small = Image.new("L", (max(2, size[0] // scale), max(2, size[1] // scale)))
    small.putdata([rng.randrange(256) for _ in range(small.width * small.height)])
    return small.resize(size, Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(scale / 3))


def parchment(size: tuple[int, int], seed: int = 1) -> Image.Image:
    """Mottled parchment, darker and burnt toward its edges."""
    width, height = size
    coarse = _noise(size, seed, 48)
    fine = _noise(size, seed + 7, 6)
    image = Image.new("RGBA", size)
    pixels = image.load()
    assert pixels is not None
    coarse_px, fine_px = list(coarse.getdata()), list(fine.getdata())
    for y in range(height):
        for x in range(width):
            edge = min(x, y, width - 1 - x, height - 1 - y) / max(1.0, min(width, height) * 0.18)
            burn = max(0.0, 1.0 - min(1.0, edge)) ** 2
            shade = 0.86 + 0.14 * (coarse_px[y * width + x] / 255.0) + 0.05 * (fine_px[y * width + x] / 255.0 - 0.5)
            shade *= 1.0 - 0.45 * burn
            r, g, b = (min(255, int(c * shade)) for c in PARCHMENT)
            pixels[x, y] = (r, g - int(12 * burn), b - int(25 * burn), 255)
    return image


def _rivet(draw: ImageDraw.ImageDraw, x: float, y: float, radius: float) -> None:
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=(*IRON, 255), outline=(30, 30, 34, 255))
    draw.ellipse((x - radius * 0.45, y - radius * 0.55, x, y - radius * 0.1), fill=(150, 150, 158, 255))


def _wood(size: tuple[int, int], seed: int) -> Image.Image:
    """Dark wood with its grain along the width."""
    width, height = size
    grain = _noise((width, height), seed, 3).resize((width, height))
    stretched = _noise((max(2, width // 24), height), seed + 3, 2).resize((width, height), Image.Resampling.BICUBIC)
    image = Image.new("RGBA", size)
    pixels = image.load()
    g1, g2 = list(grain.getdata()), list(stretched.getdata())
    assert pixels is not None
    for y in range(height):
        for x in range(width):
            shade = 0.75 + 0.35 * (g2[y * width + x] / 255.0) + 0.08 * (g1[y * width + x] / 255.0 - 0.5)
            pixels[x, y] = tuple(min(255, int(c * shade)) for c in WOOD) + (255,)  # type: ignore[assignment]
    return image


def panel(size: tuple[int, int], seed: int = 1, border: int = 0) -> Image.Image:
    """A parchment panel in a frame of dark wood with a gold inner line and iron rivets in the corners."""
    width, height = size
    border = border or max(6, min(width, height) // 9)
    image = _wood(size, seed + 11)
    inner = parchment((width - 2 * border, height - 2 * border), seed)
    image.paste(inner, (border, border))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, width - 1, height - 1), outline=(*WOOD_DARK, 255), width=max(2, border // 4))
    draw.rectangle(
        (border - 3, border - 3, width - border + 2, height - border + 2),
        outline=(*GOLD, 255),
        width=max(2, border // 5),
    )
    for x in (border / 2, width - border / 2):
        for y in (border / 2, height - border / 2):
            _rivet(draw, x, y, max(3.0, border * 0.32))
    return image


def banner(size: tuple[int, int] = (640, 100)) -> Image.Image:
    """A plank button: wood in a gold rim, iron bands at both ends; its middle lets the button colour through."""
    width, height = size
    wood = _wood(size, 5)
    alpha = Image.new("L", size, 175)
    image = wood.copy()
    image.putalpha(alpha)
    draw = ImageDraw.Draw(image)
    for x in (0, width - 46):
        draw.rectangle((x, 0, x + 45, height - 1), fill=(*IRON, 255))
        for y in (height * 0.28, height * 0.72):
            _rivet(draw, x + 23, y, 7)
    draw.rectangle((0, 0, width - 1, height - 1), outline=(*GOLD, 255), width=4)
    draw.rectangle((8, 8, width - 9, height - 9), outline=(*WOOD_DARK, 220), width=2)
    return image


def frame(size: tuple[int, int]) -> Image.Image:
    """A wood and iron frame, empty inside (the minimap shows through)."""
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    wood = _wood(size, 9)
    border = max(10, min(width, height) // 14)
    mask = Image.new("L", size, 255)
    ImageDraw.Draw(mask).rectangle((border, border, width - border - 1, height - border - 1), fill=0)
    image.paste(wood, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    draw.rectangle((border - 2, border - 2, width - border + 1, height - border + 1), outline=(*GOLD, 255), width=3)
    draw.rectangle((0, 0, width - 1, height - 1), outline=(*WOOD_DARK, 255), width=3)
    for x in (border / 2, width / 2, width - border / 2):
        for y in (border / 2, height - border / 2):
            _rivet(draw, x, y, border * 0.33)
    return image


def wheel(size: int = 256) -> Image.Image:
    """The ring of the build wheel: a band of parchment between an iron and a gold circle, empty in the middle."""
    supersample = 2
    big = size * supersample
    paper = parchment((big, big), 21)
    mask = Image.new("L", (big, big), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((4, 4, big - 5, big - 5), fill=230)
    hole = big * 0.24
    draw.ellipse((hole, hole, big - hole, big - hole), fill=70)
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    image.paste(paper, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    draw.ellipse((4, 4, big - 5, big - 5), outline=(*IRON, 255), width=14)
    draw.ellipse((18, 18, big - 19, big - 19), outline=(*GOLD, 255), width=5)
    draw.ellipse((hole, hole, big - hole, big - hole), outline=(*GOLD, 255), width=6)
    for k in range(8):
        a = k * math.pi / 4 + math.pi / 8
        _rivet(draw, big / 2 + math.cos(a) * (big / 2 - 11), big / 2 + math.sin(a) * (big / 2 - 11), 9)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def plaque(size: tuple[int, int] = (512, 176)) -> Image.Image:
    """A parchment scroll, rolled at both ends, for the info text of the build wheel."""
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    body = parchment((width - 60, height - 24), 33)
    image.paste(body, (30, 12))
    draw = ImageDraw.Draw(image)
    for x in (0, width - 40):
        draw.rounded_rectangle(
            (x, 2, x + 39, height - 3), radius=18, fill=(196, 170, 120, 255), outline=(*INK, 255), width=3
        )
        draw.line((x + 20, 8, x + 20, height - 9), fill=(150, 120, 80, 255), width=3)
    draw.rectangle((30, 12, width - 31, height - 13), outline=(*INK, 200), width=2)
    return image


def title(fonts: Path, size: tuple[int, int] = (1400, 260)) -> Image.Image:
    """TOWER DEFENCE in gold Roman capitals with a dark outline and a soft shadow."""
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    font = ImageFont.truetype(str(fonts / "Cinzel.ttf"), 140)
    with contextlib.suppress(OSError, ValueError):
        font.set_variation_by_name("Black")
    text = "TOWER DEFENCE"
    draw = ImageDraw.Draw(image)
    box = draw.textbbox((0, 0), text, font=font)
    x = (width - (box[2] - box[0])) / 2 - box[0]
    y = (height - (box[3] - box[1])) / 2 - box[1]
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((x + 8, y + 12), text, font=font, fill=(0, 0, 0, 190))
    image = Image.alpha_composite(image, shadow.filter(ImageFilter.GaussianBlur(8)))
    draw = ImageDraw.Draw(image)
    draw.text((x, y), text, font=font, fill=(*GOLD, 255), stroke_width=8, stroke_fill=(*WOOD_DARK, 255))
    draw.text((x, y - 3), text, font=font, fill=(250, 214, 120, 255))
    gloss = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(gloss).text((x, y), text, font=font, fill=(255, 245, 200, 110))
    upper = Image.new("L", size, 0)
    ImageDraw.Draw(upper).rectangle((0, 0, width, int(y + (box[3] - box[1]) * 0.45)), fill=255)
    image.paste(gloss, (0, 0), Image.composite(gloss.getchannel("A"), Image.new("L", size, 0), upper))
    return image


def star_sheet(cell: int = 48) -> Image.Image:
    """A star of the world map and of the tally, 2 frames: empty, gold."""
    sheet = Image.new("RGBA", (cell * 2, cell), (0, 0, 0, 0))
    for frame in range(2):
        image = Image.new("RGBA", (cell * 2, cell * 2), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        points = []
        for k in range(10):
            r = cell * (0.92 if k % 2 == 0 else 0.4)
            a = -math.pi / 2 + k * math.pi / 5
            points.append((cell + math.cos(a) * r, cell + math.sin(a) * r))
        fill = (*GOLD, 255) if frame == 1 else (120, 104, 80, 160)
        draw.polygon(points, fill=fill, outline=(*WOOD_DARK, 255), width=5)
        sheet.paste(image.resize((cell, cell), Image.Resampling.LANCZOS), (frame * cell, 0))
    return sheet


def day_night_icons(cell: int = 64) -> Image.Image:
    """The phase of a level on the HUD, 2 frames: a gold sun with its rays, a pale moon with stars, on a dark disc."""
    sheet = Image.new("RGBA", (cell * 2, cell), (0, 0, 0, 0))
    for frame in range(2):
        big = cell * 4
        image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        c = big / 2
        disc = (40, 52, 96) if frame else (70, 110, 170)
        draw.ellipse((8, 8, big - 8, big - 8), fill=(*disc, 255), outline=(*GOLD, 255), width=14)
        if frame == 0:
            for k in range(12):
                a = k * math.pi / 6
                draw.line(
                    [(c + math.cos(a) * 52, c + math.sin(a) * 52), (c + math.cos(a) * 96, c + math.sin(a) * 96)],
                    fill=(250, 210, 90, 255),
                    width=14,
                )
            draw.ellipse((c - 44, c - 44, c + 44, c + 44), fill=(252, 220, 100, 255), outline=(*INK, 255), width=6)
        else:
            draw.ellipse((c - 58, c - 58, c + 58, c + 58), fill=(236, 236, 214, 255), outline=(*INK, 255), width=6)
            draw.ellipse((c - 26, c - 76, c + 84, c + 34), fill=(*disc, 255))
            for x, y in ((c + 50, c + 40), (c - 60, c - 70), (c + 70, c - 64)):
                draw.polygon([(x, y - 14), (x + 5, y), (x, y + 14), (x - 5, y)], fill=(240, 240, 220, 255))
        sheet.paste(image.resize((cell, cell), Image.Resampling.LANCZOS), (frame * cell, 0))
    return sheet


def hint_arrow(size: int = 96) -> Image.Image:
    """The arrow of the tutorial hints: gold with an ink outline, pointing right."""
    big = size * 4
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    c = big / 2
    shape = [
        (20, c - 34),
        (c + 20, c - 34),
        (c + 20, c - 90),
        (big - 20, c),
        (c + 20, c + 90),
        (c + 20, c + 34),
        (20, c + 34),
    ]
    draw.polygon(shape, fill=(*GOLD, 255), outline=(*INK, 255), width=14)
    draw.line([(40, c - 12), (c + 30, c - 12)], fill=(250, 226, 150, 255), width=10)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def _disc(size: int, colour: RGBA) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((4, 4, size - 5, size - 5), fill=colour, outline=(*GOLD, 255), width=max(2, size // 24))
    return image, draw


def modifier_icon(key: str, size: int = 96) -> Image.Image:
    """The icon of a level modifier on a disc: fog, blizzard, blood moon, drought, gale, plunder or long night."""
    big = size * 4
    background = {
        "blood_moon": (70, 20, 24, 255),
        "drought": (200, 150, 70, 255),
        "long_night": (20, 22, 48, 255),
        "plunder": (90, 60, 30, 255),
    }.get(key, (70, 96, 130, 255))
    image, draw = _disc(big, background)
    c = big / 2
    ink = (*INK, 255)
    if key == "fog":
        for k in range(4):
            y = c - 90 + k * 60
            draw.arc((c - 130, y - 30, c, y + 30), 200, 340, fill=(230, 230, 230, 255), width=22)
            draw.arc((c - 10, y - 30, c + 120, y + 30), 20, 160, fill=(230, 230, 230, 255), width=22)
    elif key == "blizzard":
        for k in range(6):
            a = k * math.pi / 3
            x, y = c + math.cos(a) * 120, c + math.sin(a) * 120
            draw.line([(c, c), (x, y)], fill=(240, 250, 255, 255), width=22)
            for side in (-1, 1):
                b = a + side * 0.6
                draw.line(
                    [
                        (c + math.cos(a) * 70, c + math.sin(a) * 70),
                        (c + math.cos(a) * 70 + math.cos(b) * 40, c + math.sin(a) * 70 + math.sin(b) * 40),
                    ],
                    fill=(240, 250, 255, 255),
                    width=16,
                )
    elif key == "blood_moon":
        draw.ellipse((c - 100, c - 100, c + 100, c + 100), fill=(200, 40, 30, 255), outline=ink, width=12)
        draw.ellipse((c - 40, c - 60, c, c - 20), fill=(160, 26, 20, 255))
    elif key == "drought":
        draw.ellipse((c - 60, c - 120, c + 60, c), fill=(252, 220, 100, 255), outline=ink, width=10)
        draw.rectangle((c - 140, c + 20, c + 140, c + 130), fill=(160, 110, 60, 255))
        for x in (-90, -20, 50):
            draw.line([(c + x, c + 30), (c + x + 30, c + 80), (c + x + 10, c + 125)], fill=ink, width=10)
    elif key == "gale":
        for k in range(3):
            y = c - 80 + k * 80
            draw.arc((c - 110, y - 40, c + 70, y + 40), 180, 360, fill=(235, 240, 245, 255), width=20)
            draw.line([(c - 140, y), (c - 100, y)], fill=(235, 240, 245, 255), width=20)
    elif key == "plunder":
        for k, (x, y) in enumerate(((c - 50, c + 40), (c + 40, c + 50), (c - 5, c - 20), (c + 60, c - 50))):
            draw.ellipse((x - 50, y - 30, x + 50, y + 30), fill=(*GOLD, 255), outline=ink, width=10)
            draw.ellipse((x - 26, y - 14, x + 26, y + 14), outline=(150, 110, 40, 255), width=8)
            del k
    elif key == "long_night":
        draw.ellipse((c - 90, c - 90, c + 90, c + 90), fill=(220, 220, 200, 255), outline=ink, width=10)
        draw.ellipse((c - 40, c - 120, c + 140, c + 60), fill=background)
        for x, y in ((c + 70, c + 80), (c - 110, c + 60), (c + 100, c - 30)):
            draw.polygon([(x, y - 24), (x + 8, y), (x, y + 24), (x - 8, y)], fill=(240, 240, 220, 255))
    return image.resize((size, size), Image.Resampling.LANCZOS)


MODIFIER_KEYS = ("fog", "blizzard", "blood_moon", "drought", "gale", "plunder", "long_night")
"""The level modifiers in the order of their icons in ``briefing_icons.png``."""


PANELS: dict[str, tuple[int, int]] = {
    "panel_stats": (420, 70),
    "panel_wave": (540, 70),
    "panel_window": (920, 640),
    "panel_board": (620, 520),
    "panel_tally": (900, 560),
}
"""The parchment panels, by file name, at the size (in reference pixels) they are shown."""


def write_all(textures: Path, fonts: Path) -> None:
    """Every image of the menus and the HUD."""
    folder = textures / "ui"
    folder.mkdir(parents=True, exist_ok=True)
    for index, (name, size) in enumerate(PANELS.items()):
        panel(size, seed=index + 1).save(folder / f"{name}.png")
    banner().save(folder / "banner.png")
    frame((360, 288)).save(folder / "frame.png")
    wheel().save(folder / "wheel.png")
    plaque().save(folder / "plaque.png")
    title(fonts).save(folder / "title.png")
    star_sheet().save(folder / "stars.png")
    day_night_icons().save(folder / "daynight.png")
    hint_arrow().save(folder / "hint_arrow.png")
