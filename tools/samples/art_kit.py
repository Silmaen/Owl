"""
Drawings shared by the generators of the v0.7.0 sample games: the title art of a game (a name in an engine font
filled with a vertical gradient, outlined, with a decoration of its own), icons drawn supersampled, and the copy of
the Engine Demo victory jingle and gamepad mappings every sample carries.
"""

from __future__ import annotations

import shutil
from collections.abc import Callable
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPOSITORY = Path(__file__).resolve().parents[2]
"""The repository root."""
FONTS = REPOSITORY / "engine_assets" / "fonts"
"""The fonts of the engine."""
SS = 4
"""Supersampling of the drawings."""

RGB = tuple[int, int, int]


def canvas(width: int, height: int) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    """A transparent supersampled canvas."""
    image = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def finish(image: Image.Image, width: int, height: int, glow: float = 0.0) -> Image.Image:
    """Downsample a supersampled drawing, with a halo of its own colours when ``glow`` is above 0."""
    small = image.resize((width, height), Image.Resampling.LANCZOS)
    if glow <= 0.0:
        return small
    return Image.alpha_composite(small.filter(ImageFilter.GaussianBlur(glow)), small)


def title_art(
    text: str,
    font: str,
    top: RGB,
    bottom: RGB,
    outline: RGB,
    size: int = 150,
    decorate: Callable[[Image.Image, ImageDraw.ImageDraw, int, int], None] | None = None,
    glow: float = 4.0,
    width: int = 1400,
    height: int = 340,
) -> Image.Image:
    """A game name centred in ``width`` x ``height``, a gradient from ``top`` to ``bottom``, outlined; ``decorate``
    draws behind the letters on the supersampled canvas (its size given in output pixels)."""
    image, draw = canvas(width, height)
    if decorate is not None:
        decorate(image, draw, width, height)
    face = ImageFont.truetype(str(FONTS / f"{font}.ttf"), size * SS)
    box = draw.textbbox((0, 0), text, font=face)
    x = (width * SS - (box[2] - box[0])) / 2 - box[0]
    y = (height * SS - (box[3] - box[1])) / 2 - box[1]
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).text((x, y), text, font=face, fill=255)
    gradient = Image.new("RGBA", image.size)
    painter = ImageDraw.Draw(gradient)
    for row in range(image.size[1]):
        t = row / image.size[1]
        colour = tuple(int(a + (b - a) * t) for a, b in zip(top, bottom, strict=True))
        painter.line([(0, row), (image.size[0], row)], fill=(*colour, 255))
    letters = Image.new("RGBA", image.size, (0, 0, 0, 0))
    letters.paste(gradient, (0, 0), mask)
    rim = Image.new("RGBA", image.size, (0, 0, 0, 0))
    rim.paste(Image.new("RGBA", image.size, (*outline, 255)), (0, 0), mask.filter(ImageFilter.MaxFilter(4 * SS + 1)))
    image = Image.alpha_composite(image, rim)
    image = Image.alpha_composite(image, letters)
    return finish(image, width, height, glow)


def icon(size: int, draw: Callable[[ImageDraw.ImageDraw, int], None], glow: float = 0.0) -> Image.Image:
    """A square icon drawn by ``draw`` on a supersampled canvas of ``size * SS`` pixels."""
    image, painter = canvas(size, size)
    draw(painter, size * SS)
    return finish(image, size, size, glow)


def copy_common(root: Path) -> None:
    """Copy the victory jingle and the gamepad mappings of the Engine Demo into a sample project."""
    (root / "sounds" / "ui").mkdir(parents=True, exist_ok=True)
    (root / "input").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        REPOSITORY / "engine_demo" / "sounds" / "ui" / "victory.wav", root / "sounds" / "ui" / "victory.wav"
    )
    shutil.copyfile(
        REPOSITORY / "engine_demo" / "input" / "gamecontrollerdb.txt", root / "input" / "gamecontrollerdb.txt"
    )
