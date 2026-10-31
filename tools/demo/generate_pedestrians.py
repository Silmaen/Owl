#!/usr/bin/env python3
"""
Draw the pedestrians of the isometric town plaza into ``textures/iso/pedestrians.png`` (Pillow, deterministic).

One sheet of 4 columns by 2 rows of 24 x 40 pixel frames: a small walker seen from the isometric view, its four walking
steps heading to the right of the screen on the first row and to the left on the second. The sprite is white and grey
so that the tint of each `AnimatedSpriteRenderer` gives every walker its own clothes.

Run from anywhere: ``python3 tools/demo/generate_pedestrians.py``.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

FRAME = (24, 40)
"""Size of a frame, in pixels."""
COLUMNS = 4
"""Steps of the walk."""
OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "iso" / "pedestrians.png"
"""Written sheet."""

SKIN = (236, 196, 160, 255)
HAIR = (90, 62, 40, 255)
CLOTH = (250, 250, 250, 255)
SHADE = (190, 190, 196, 255)
LEGS = (70, 70, 90, 255)
OUTLINE = (40, 32, 36, 255)


def _walker(step: int, facing_left: bool) -> Image.Image:
    """One frame: legs and arms swing with the step, the head turned the way of the walk."""
    width, height = FRAME
    image = Image.new("RGBA", FRAME, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    swing = (0, 2, 0, -2)[step]
    centre = width // 2
    # Shadow on the ground.
    draw.ellipse((centre - 7, height - 5, centre + 7, height - 1), fill=(0, 0, 0, 70))
    # Legs.
    for side, offset in ((-1, swing), (1, -swing)):
        x = centre + side * 2
        draw.line((x, height - 16, x + offset, height - 3), fill=OUTLINE, width=4)
        draw.line((x, height - 16, x + offset, height - 4), fill=LEGS, width=2)
    # Body.
    draw.rounded_rectangle((centre - 5, height - 29, centre + 5, height - 14), radius=3, fill=CLOTH, outline=OUTLINE)
    draw.rectangle((centre + 1, height - 27, centre + 4, height - 15), fill=SHADE)
    # Arms.
    for side, offset in ((-1, -swing), (1, swing)):
        x = centre + side * 6
        draw.line((x, height - 27, x + offset, height - 18), fill=OUTLINE, width=3)
    # Head, the hair on the back of it.
    draw.ellipse((centre - 4, height - 38, centre + 4, height - 29), fill=SKIN, outline=OUTLINE)
    back = 1 if facing_left else -1
    draw.pieslice((centre - 4, height - 38, centre + 4, height - 30), 180 + (40 if facing_left else -40),
                  360 + (40 if facing_left else -40), fill=HAIR)
    draw.point((centre - 2 * back, height - 33), fill=OUTLINE)
    return image


def main() -> None:
    """Write the sheet."""
    width, height = FRAME
    sheet = Image.new("RGBA", (width * COLUMNS, height * 2), (0, 0, 0, 0))
    for row, facing_left in enumerate((False, True)):
        for step in range(COLUMNS):
            sheet.paste(_walker(step, facing_left), (step * width, row * height))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(OUT, optimize=True)


if __name__ == "__main__":
    main()
