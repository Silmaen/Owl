#!/usr/bin/env python3
"""Generate the cockpit overlay of the Engine Demo far orbit (``engine_demo/textures/ui/``): ``cockpit_frame.png``, the
canopy bow, its two struts and the dashboard cowl with two screens around a transparent view, 1280x720, and
``cockpit_reticle.png``, the white heading reticle tinted by the HUD, 128x128. Drawn at four times the size, then
reduced, for smooth edges."""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "ui"
WIDTH = 1280
HEIGHT = 720
SS = 4

HULL = (26, 29, 35, 255)
HULL_LIGHT = (58, 63, 72, 255)
SCREEN = (8, 14, 18, 235)
SCREEN_EDGE = (70, 110, 120, 255)


def pts(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """Points of the 1280x720 layout at the supersampled size."""
    return [(x * SS, y * SS) for x, y in points]


def arc(x0: float, x1: float, y_edge: float, y_mid: float, steps: int = 48) -> list[tuple[float, float]]:
    """A parabola from (x0, y_edge) through (mid, y_mid) to (x1, y_edge)."""
    mid = (x0 + x1) / 2
    half = (x1 - x0) / 2
    return [(x0 + (x1 - x0) * k / steps,
             y_mid + (y_edge - y_mid) * ((x0 + (x1 - x0) * k / steps - mid) / half) ** 2) for k in range(steps + 1)]


def frame() -> Image.Image:
    """The canopy frame and the dashboard."""
    image = Image.new("RGBA", (WIDTH * SS, HEIGHT * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    # Canopy bow across the top.
    bow_low = arc(0, WIDTH, 92, 26)
    draw.polygon(pts([(0, 0), (WIDTH, 0)] + list(reversed(bow_low))), fill=HULL)
    draw.line(pts(bow_low), fill=HULL_LIGHT, width=2 * SS)
    # Two struts from the dashboard to the bow.
    for side in (1, -1):
        def x(v: float, s: int = side) -> float:
            return v if s == 1 else WIDTH - v

        strut = [(x(0), 96), (x(34), 84), (x(212), 590), (x(170), 604)]
        draw.polygon(pts(strut), fill=HULL)
        draw.line(pts([(x(34), 84), (x(212), 590)]), fill=HULL_LIGHT, width=2 * SS)
        for k in range(1, 6):
            t = k / 6
            cx, cy = x(17 + (191 - 17) * t), 90 + (597 - 90) * t
            r = 2.2 * SS
            draw.ellipse((cx * SS - r, cy * SS - r, cx * SS + r, cy * SS + r), fill=HULL_LIGHT)
    # Dashboard cowl, higher at the sides.
    cowl_top = arc(0, WIDTH, 588, 606)
    draw.polygon(pts([(0, HEIGHT)] + cowl_top + [(WIDTH, HEIGHT)]), fill=HULL)
    draw.line(pts(cowl_top), fill=HULL_LIGHT, width=3 * SS)
    # Two screens of the readouts and a slot of the throttle between them.
    for left in (352, 728):
        draw.rounded_rectangle(pts([(left, 624), (left + 200, 704)]), radius=8 * SS, fill=SCREEN,
                               outline=SCREEN_EDGE, width=2 * SS)
    draw.rounded_rectangle(pts([(586, 626), (694, 640)]), radius=4 * SS, fill=SCREEN, outline=SCREEN_EDGE,
                           width=SS)
    image = image.filter(ImageFilter.GaussianBlur(SS * 0.4))
    return image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)


def reticle(size: int = 128) -> Image.Image:
    """A ring broken in four, four ticks and a centre dot, white on transparent."""
    big = size * SS
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    centre = big / 2
    radius = big * 0.3
    width = int(2.5 * SS)
    for k in range(4):
        start = 90 * k + 18
        draw.arc((centre - radius, centre - radius, centre + radius, centre + radius), start, start + 54,
                 fill=(255, 255, 255, 230), width=width)
        angle = math.radians(90 * k)
        inner, outer = radius * 1.12, radius * 1.45
        draw.line([(centre + inner * math.cos(angle), centre + inner * math.sin(angle)),
                   (centre + outer * math.cos(angle), centre + outer * math.sin(angle))],
                  fill=(255, 255, 255, 230), width=width)
    dot = big * 0.025
    draw.ellipse((centre - dot, centre - dot, centre + dot, centre + dot), fill=(255, 255, 255, 255))
    return image.resize((size, size), Image.Resampling.LANCZOS)


def main() -> None:
    """Write the cockpit textures."""
    OUT.mkdir(parents=True, exist_ok=True)
    frame().save(OUT / "cockpit_frame.png", optimize=True)
    reticle().save(OUT / "cockpit_reticle.png", optimize=True)


if __name__ == "__main__":
    main()
