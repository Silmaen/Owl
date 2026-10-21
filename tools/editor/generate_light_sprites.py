#!/usr/bin/env python3
"""Generate the Owl Nest light bulb sprites (``source/owlnest/assets/icons/lights/bulb_on.png`` and ``bulb_off.png``),
drawn facing the camera at each point light of the viewport: a glass bulb on a screw base, lit (warm glass, glow and
rays) or switched off (grey glass). 256x256 RGBA, drawn 4x larger then downsampled for smooth edges."""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parents[2] / "source" / "owlnest" / "assets" / "icons" / "lights"
SIZE = 256
SCALE = 4


def draw_bulb(lit: bool) -> Image.Image:
    big = SIZE * SCALE
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))

    def px(value: float) -> int:
        return int(value * big)

    centre = (px(0.5), px(0.40))
    radius = px(0.24)
    if lit:
        glow = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        ImageDraw.Draw(glow).ellipse(
            (centre[0] - radius * 1.6, centre[1] - radius * 1.6, centre[0] + radius * 1.6, centre[1] + radius * 1.6),
            fill=(255, 215, 94, 120),
        )
        image = Image.alpha_composite(image, glow.filter(ImageFilter.GaussianBlur(px(0.06))))
    draw = ImageDraw.Draw(image)
    outline = (90, 70, 20, 255) if lit else (60, 60, 60, 255)
    glass = (255, 225, 120, 255) if lit else (170, 170, 170, 230)
    width = px(0.02)
    # Rays around the lit glass.
    if lit:
        for index in range(8):
            angle = math.pi * 2.0 * index / 8.0 - math.pi / 2.0
            inner, outer = radius * 1.35, radius * 1.75
            draw.line(
                (
                    centre[0] + math.cos(angle) * inner,
                    centre[1] + math.sin(angle) * inner,
                    centre[0] + math.cos(angle) * outer,
                    centre[1] + math.sin(angle) * outer,
                ),
                fill=(255, 200, 60, 255),
                width=width,
            )
    # Glass: a disc narrowing into the neck, the neck drawn over the disc with its two sides only.
    neck_top = centre[1] + radius * 0.55
    neck_bottom = px(0.72)
    neck_half = radius * 0.42
    draw.ellipse(
        (centre[0] - radius, centre[1] - radius, centre[0] + radius, centre[1] + radius),
        fill=glass,
        outline=outline,
        width=width,
    )
    top_left = (centre[0] - radius * 0.83, neck_top)
    top_right = (centre[0] + radius * 0.83, neck_top)
    bottom_left = (centre[0] - neck_half, neck_bottom)
    bottom_right = (centre[0] + neck_half, neck_bottom)
    draw.polygon([top_left, top_right, bottom_right, bottom_left], fill=glass)
    draw.line((top_left, bottom_left), fill=outline, width=width)
    draw.line((top_right, bottom_right), fill=outline, width=width)
    # Filament.
    filament = (255, 130, 30, 255) if lit else (100, 100, 100, 255)
    loop = radius * 0.28
    draw.arc(
        (centre[0] - loop, centre[1] - loop * 1.2, centre[0] + loop, centre[1] + loop * 0.8),
        start=180,
        end=360,
        fill=filament,
        width=width,
    )
    for side in (-1, 1):
        draw.line(
            (centre[0] + side * loop, centre[1] - loop * 0.2, centre[0] + side * neck_half * 0.5, neck_bottom),
            fill=filament,
            width=width // 2,
        )
    # Screw base: three metal bands and the contact.
    band_height = px(0.045)
    for band in range(3):
        top = neck_bottom + band * band_height
        draw.rounded_rectangle(
            (centre[0] - neck_half, top, centre[0] + neck_half, top + band_height - width // 2),
            radius=band_height // 3,
            fill=(150, 150, 160, 255) if band % 2 == 0 else (120, 120, 130, 255),
            outline=(60, 60, 70, 255),
            width=width // 2,
        )
    contact_top = neck_bottom + 3 * band_height
    draw.rounded_rectangle(
        (centre[0] - neck_half * 0.5, contact_top, centre[0] + neck_half * 0.5, contact_top + band_height),
        radius=band_height // 2,
        fill=(70, 70, 80, 255),
    )
    return image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, lit in (("bulb_on", True), ("bulb_off", False)):
        path = OUT / f"{name}.png"
        draw_bulb(lit).save(path)
        print(f"  {path.relative_to(OUT.parents[3])}")


if __name__ == "__main__":
    main()
