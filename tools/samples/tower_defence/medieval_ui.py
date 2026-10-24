#!/usr/bin/env python3
"""
The medieval-fantasy look of the Tower defence menus and HUD, drawn from scratch (deterministic): parchment, dark
wood, iron rivets and gold trim, at the size each widget is shown so that nothing is stretched.

- ``textures/ui/panel_*.png``: parchment panels framed in wood and gold (``panel``);
- ``textures/ui/banner.png``: a wooden plank button with iron bands, see-through enough for the hover colour;
- ``textures/ui/frame.png``: the wood and iron frame of the minimap, empty inside;
- ``textures/ui/wheel.png``: the ring of the build wheel, ``textures/ui/plaque.png``: its parchment scroll;
- ``textures/ui/title.png``: TOWER DEFENCE in gold capitals of the ``Cinzel`` font;
- the world map: ``world_map.png`` (the parchment plan), ``sites.png`` (locked, open, done, capital), ``stars.png``
  and ``path_dot.png`` (the road traced between the sites).
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


SITES = ((0.12, 0.62), (0.3, 0.42), (0.47, 0.6), (0.66, 0.36), (0.84, 0.55))
"""Places of the five level sites on the world map, in shares of its width and height (from the top left)."""
CAPITAL = (0.05, 0.82)
"""Place of the human capital the road starts from."""


def _ink_tree(draw: ImageDraw.ImageDraw, x: float, y: float, size: float) -> None:
    draw.polygon([(x, y - size), (x - size * 0.6, y + size * 0.4), (x + size * 0.6, y + size * 0.4)], fill=(70, 96, 52))
    draw.line([(x, y + size * 0.4), (x, y + size * 0.7)], fill=(*INK, 255), width=2)
    draw.polygon(
        [(x, y - size), (x - size * 0.6, y + size * 0.4), (x + size * 0.6, y + size * 0.4)], outline=(*INK, 255)
    )


def _ink_mountain(draw: ImageDraw.ImageDraw, x: float, y: float, size: float) -> None:
    draw.polygon([(x - size, y), (x, y - size * 1.1), (x + size, y)], fill=(150, 128, 96), outline=(*INK, 255))
    draw.polygon([(x, y - size * 1.1), (x + size, y), (x + size * 0.15, y)], fill=(120, 100, 74))
    draw.polygon(
        [(x - size * 0.25, y - size * 0.8), (x, y - size * 1.1), (x + size * 0.25, y - size * 0.8)],
        fill=(240, 236, 226),
    )


def world_map(size: tuple[int, int] = (1920, 1080)) -> Image.Image:
    """The parchment map of the campaign, drawn by hand: a coast, the sea, forests, mountains and a river, a compass;
    the sites and the road between them are sprites of the world map scene over it."""
    width, height = size
    image = parchment(size, 77)
    draw = ImageDraw.Draw(image)
    rng = random.Random(9)
    sea: list[tuple[float, float]] = [(0.0, 0.0), (width * 0.55, 0.0)]
    for k in range(13):
        x = width * (0.55 - 0.04 * math.sin(k * 0.9)) + k * width * 0.035
        sea.append((x, height * (0.02 + 0.017 * k) + rng.uniform(-12, 12)))
    sea += [(float(width), height * 0.25), (float(width), 0.0)]
    draw.polygon(sea, fill=(168, 186, 182))
    for _ in range(40):
        x, y = rng.uniform(width * 0.62, width), rng.uniform(0, height * 0.18)
        draw.arc((x - 18, y - 6, x + 18, y + 6), 200, 340, fill=(100, 120, 120), width=2)
    draw.line(sea[1:-2], fill=(*INK, 255), width=3)
    river: list[tuple[float, float]] = [(width * 0.38, -10.0)]
    for k in range(1, 12):
        river.append((width * (0.38 + 0.03 * math.sin(k * 0.8)) + k * 6, height * k / 11))
    draw.line(river, fill=(110, 140, 160), width=10)
    draw.line(river, fill=(70, 100, 120), width=2)
    for _ in range(260):
        x, y = rng.uniform(40, width - 40), rng.uniform(height * 0.25, height - 30)
        if any(abs(x - width * sx) < 90 and abs(y - height * sy) < 80 for sx, sy in SITES):
            continue
        if rng.random() < 0.7:
            _ink_tree(draw, x, y, rng.uniform(9, 15))
    for _ in range(22):
        x, y = rng.uniform(width * 0.55, width * 0.98), rng.uniform(height * 0.62, height * 0.95)
        _ink_mountain(draw, x, y, rng.uniform(26, 44))
    cx, cy, r = width * 0.93, height * 0.84, 70
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=(*INK, 255), width=3)
    for k in range(8):
        a = k * math.pi / 4
        length = r * (1.0 if k % 2 == 0 else 0.55)
        draw.polygon(
            [
                (cx, cy),
                (cx + math.cos(a + 0.18) * r * 0.25, cy + math.sin(a + 0.18) * r * 0.25),
                (cx + math.cos(a) * length, cy + math.sin(a) * length),
            ],
            fill=(*INK, 255),
        )
    frame = Image.new("RGBA", size, (0, 0, 0, 0))
    fd = ImageDraw.Draw(frame)
    fd.rectangle((6, 6, width - 7, height - 7), outline=(*WOOD_DARK, 255), width=12)
    fd.rectangle((22, 22, width - 23, height - 23), outline=(*GOLD, 255), width=4)
    return Image.alpha_composite(image, frame)


def site_sheet(cell: int = 128) -> Image.Image:
    """The sites of the world map, 4 frames: locked (a grey ruin), open (a camp under a red banner), done (a human
    keep under a blue banner) and the capital."""
    sheet = Image.new("RGBA", (cell * 4, cell), (0, 0, 0, 0))
    for frame in range(4):
        image = Image.new("RGBA", (cell, cell), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        stone = (128, 126, 122) if frame == 0 else (206, 190, 160)
        roof = (90, 90, 92) if frame == 0 else ((150, 40, 32) if frame == 1 else (60, 84, 160))
        draw.ellipse((14, cell - 34, cell - 14, cell - 6), fill=(0, 0, 0, 70))
        draw.rectangle((30, 52, cell - 30, cell - 18), fill=stone, outline=(*INK, 255), width=3)
        for x in range(30, cell - 30, 14):
            draw.rectangle((x, 44, x + 8, 52), fill=stone, outline=(*INK, 255), width=2)
        draw.rectangle((cell // 2 - 12, 26, cell // 2 + 12, cell - 18), fill=stone, outline=(*INK, 255), width=3)
        draw.polygon([(cell // 2 - 16, 28), (cell // 2, 6), (cell // 2 + 16, 28)], fill=roof, outline=(*INK, 255))
        draw.rectangle((cell // 2 - 6, cell - 40, cell // 2 + 6, cell - 18), fill=(*INK, 255))
        if frame in (1, 2, 3):
            draw.line([(cell - 26, 20), (cell - 26, 58)], fill=(*INK, 255), width=3)
            banner = (180, 30, 24) if frame == 1 else ((50, 80, 170) if frame == 2 else (214, 168, 64))
            draw.polygon([(cell - 26, 20), (cell - 4, 27), (cell - 26, 34)], fill=banner, outline=(*INK, 255))
        if frame == 0:
            draw.line([(36, 60), (60, 90)], fill=(*INK, 255), width=3)
            draw.line([(80, 56), (70, 84)], fill=(*INK, 255), width=3)
        sheet.paste(image, (frame * cell, 0))
    return sheet


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


def path_dot(size: int = 24) -> Image.Image:
    """A dot of the road drawn on the map between two sites."""
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(image).ellipse((3, 3, size - 4, size - 4), fill=(140, 30, 24, 255), outline=(*INK, 255), width=2)
    return image


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
    world_map().save(folder / "world_map.png")
    site_sheet().save(folder / "sites.png")
    star_sheet().save(folder / "stars.png")
    path_dot().save(folder / "path_dot.png")
