"""
Small pixel-art painting helpers on Pillow images (no anti-aliasing, ordered dithering, seeded noise), used by
``generate_assets`` to paint the backdrops, the props and the icons of The Moon Lantern.
"""

from __future__ import annotations

import math
import random
from collections.abc import Sequence
from typing import Any

from PIL import Image, ImageDraw

Colour = tuple[int, int, int] | tuple[int, int, int, int]

BAYER = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)
"""4 x 4 ordered dither thresholds (0 to 15)."""


def rgba(colour: Colour, alpha: int = 255) -> tuple[int, int, int, int]:
    """A colour with an alpha."""
    return (colour[0], colour[1], colour[2], colour[3] if len(colour) > 3 else alpha)


def mix(a: Colour, b: Colour, share: float) -> tuple[int, int, int]:
    """A colour between two."""
    return tuple(round(a[i] + (b[i] - a[i]) * share) for i in range(3))  # type: ignore[return-value]


def lighten(colour: Colour, amount: int) -> tuple[int, int, int]:
    """A colour moved towards white (or black for a negative amount)."""
    return tuple(max(0, min(255, colour[i] + amount)) for i in range(3))  # type: ignore[return-value]


class Canvas:
    """A transparent image with its pixels and an ImageDraw, painted in whole pixels."""

    def __init__(self, width: int, height: int, seed: int = 1) -> None:
        self.image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.image)
        self.pixels: Any = self.image.load()
        self.width = width
        self.height = height
        self.random = random.Random(seed)

    def put(self, x: int, y: int, colour: Colour) -> None:
        """One pixel, inside the canvas only."""
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[x, y] = rgba(colour)

    def get(self, x: int, y: int) -> tuple[int, int, int, int]:
        """One pixel."""
        return self.pixels[x, y]

    def rect(self, x0: int, y0: int, x1: int, y1: int, colour: Colour) -> None:
        """A filled rectangle, corners included."""
        self.draw.rectangle((x0, y0, x1, y1), fill=rgba(colour))

    def frame(self, x0: int, y0: int, x1: int, y1: int, colour: Colour) -> None:
        """The border of a rectangle."""
        self.draw.rectangle((x0, y0, x1, y1), outline=rgba(colour))

    def line(self, points: Sequence[tuple[float, float]], colour: Colour, width: int = 1) -> None:
        """A polyline."""
        self.draw.line(list(points), fill=rgba(colour), width=width)

    def polygon(self, points: Sequence[tuple[float, float]], colour: Colour, outline: Colour | None = None) -> None:
        """A filled polygon."""
        self.draw.polygon(list(points), fill=rgba(colour), outline=rgba(outline) if outline else None)

    def ellipse(self, x0: int, y0: int, x1: int, y1: int, colour: Colour, outline: Colour | None = None) -> None:
        """A filled ellipse."""
        self.draw.ellipse((x0, y0, x1, y1), fill=rgba(colour), outline=rgba(outline) if outline else None)

    def gradient(self, x0: int, y0: int, x1: int, y1: int, colours: Sequence[Colour]) -> None:
        """A vertical gradient through several colours, dithered between the neighbouring two."""
        bands = len(colours) - 1
        for y in range(y0, y1 + 1):
            share = (y - y0) / max(1, y1 - y0) * bands
            band = min(bands - 1, int(share))
            local = share - band
            for x in range(x0, x1 + 1):
                threshold = (BAYER[y % 4][x % 4] + 0.5) / 16
                self.put(x, y, colours[band + 1] if local > threshold else colours[band])

    def dither(self, x0: int, y0: int, x1: int, y1: int, colour: Colour, density: float) -> None:
        """Pixels of a colour over a rectangle, an ordered share of them."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if (BAYER[y % 4][x % 4] + 0.5) / 16 < density:
                    self.put(x, y, colour)

    def speckle(self, x0: int, y0: int, x1: int, y1: int, colours: Sequence[Colour], density: float) -> None:
        """Random pixels of a few colours over a rectangle (only over pixels already painted)."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                inside = 0 <= x < self.width and 0 <= y < self.height
                if inside and self.pixels[x, y][3] > 0 and self.random.random() < density:
                    self.put(x, y, self.random.choice(colours))

    def glow(self, cx: float, cy: float, radius: float, colour: Colour, strength: float, squash: float = 1.0) -> None:
        """Brighten the painted pixels towards a colour around a point, fading out smoothly to its radius."""
        for y in range(max(0, int(cy - radius * squash)), min(self.height, int(cy + radius * squash) + 1)):
            for x in range(max(0, int(cx - radius)), min(self.width, int(cx + radius) + 1)):
                distance = math.hypot(x - cx, (y - cy) / squash) / radius
                if distance >= 1 or self.pixels[x, y][3] == 0:
                    continue
                share = strength * (1 - distance) ** 2
                if share <= 0:
                    continue
                old = self.pixels[x, y]
                self.pixels[x, y] = (*mix(old, colour, min(1.0, share)), old[3])

    def paste(self, other: Canvas, x: int = 0, y: int = 0) -> None:
        """Lay another canvas over this one."""
        self.image.alpha_composite(other.image, (x, y))

    def crop(self, rect: tuple[int, int, int, int]) -> Image.Image:
        """The pixels of a rectangle, corners included."""
        x0, y0, x1, y1 = rect
        return self.image.crop((x0, y0, x1 + 1, y1 + 1))

    def outline_alpha(self, colour: Colour) -> None:
        """Draw a one-pixel outline round every painted pixel, on the transparent pixels next to it."""
        source: Any = self.image.copy().load()
        for y in range(self.height):
            for x in range(self.width):
                if source[x, y][3] > 0:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < self.width and 0 <= ny < self.height and source[nx, ny][3] > 128:
                        self.pixels[x, y] = rgba(colour)
                        break
