#!/usr/bin/env python3
"""Draw the nine-slice textures of the HUD skin of the Engine Demo (warm wood and brass, home-made, CC0).

``skin_panel.png`` is a dark wood board in a brass frame with rivets, drawn in its own colours (the theme tints it
white). ``skin_button.png`` is a grey bevelled plaque that the button states tint (amber, brighter when hovered,
darker when pressed). Both keep their borders unstretched: the insets of ``huds/engine_demo.owlhudtheme`` match
``PANEL_INSET`` and ``BUTTON_INSET``. Run with ``poetry run python tools/demo/generate_ui_skin.py``.
"""

import hashlib
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

UI = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "ui"
PANEL_SIZE = 128
PANEL_INSET = 24
PANEL_RADIUS = 12
BUTTON_SIZE = 64
BUTTON_INSET = 16
BUTTON_RADIUS = 10
SCALE = 4


def rounded_mask(size: int, radius: int) -> Image.Image:
    """Antialiased rounded-rectangle mask, drawn at a larger scale then reduced."""
    big = Image.new("L", (size * SCALE, size * SCALE), 0)
    ImageDraw.Draw(big).rounded_rectangle(
        (0, 0, size * SCALE - 1, size * SCALE - 1), radius=radius * SCALE, fill=255
    )
    return big.resize((size, size), Image.LANCZOS)


def wood(size: int, seed: int) -> Image.Image:
    """Dark wood board: vertical grain (one tone per column, faint per-pixel noise), so the nine-slice stretches the
    middle up and down without showing it."""
    rng = random.Random(seed)
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    columns = [0.86 + 0.14 * rng.random() for _ in range(size)]
    for x in range(1, size - 1):
        columns[x] = (columns[x - 1] + 2.0 * columns[x] + columns[x + 1]) / 4.0
    for y in range(size):
        for x in range(size):
            tone = columns[x] + rng.uniform(-0.025, 0.025)
            pixels[x, y] = (int(70 * tone), int(45 * tone), int(29 * tone))
    return image.filter(ImageFilter.GaussianBlur(0.5))


def brass_frame(size: int, inset: int, radius: int) -> Image.Image:
    """Brass band along the edge with a bevel: lit at the top left, shaded at the bottom right."""
    frame = Image.new("RGBA", (size * SCALE, size * SCALE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(frame)
    band = 7 * SCALE
    full = size * SCALE - 1
    draw.rounded_rectangle((0, 0, full, full), radius=radius * SCALE, fill=(176, 132, 64, 255))
    draw.rounded_rectangle(
        (SCALE, SCALE, full - 2 * SCALE, full - 2 * SCALE), radius=(radius - 1) * SCALE, outline=(236, 198, 120, 255),
        width=SCALE,
    )
    draw.rounded_rectangle(
        (band, band, full - band, full - band), radius=max(radius - 6, 2) * SCALE, fill=(0, 0, 0, 0),
        outline=(70, 44, 20, 255), width=2 * SCALE,
    )
    # Rivets in the corners of the band.
    near = inset * 0.42
    far = size - near
    r = 2.4 * SCALE
    for cx, cy in ((near, near), (far, near), (near, far), (far, far)):
        x, y = cx * SCALE, cy * SCALE
        draw.ellipse((x - r, y - r, x + r, y + r), fill=(250, 220, 150, 255), outline=(90, 60, 25, 255),
                     width=SCALE // 2)
    return frame.resize((size, size), Image.LANCZOS)


def make_panel() -> Image.Image:
    board = wood(PANEL_SIZE, 7).convert("RGBA")
    frame = brass_frame(PANEL_SIZE, PANEL_INSET, PANEL_RADIUS)
    panel = Image.alpha_composite(board, frame)
    # The frame drew its own hole: keep the board there, and darken it near the frame (inner shadow).
    shade = Image.new("L", (PANEL_SIZE, PANEL_SIZE), 0)
    ImageDraw.Draw(shade).rounded_rectangle((7, 7, PANEL_SIZE - 8, PANEL_SIZE - 8), radius=6, outline=110, width=4)
    shade = shade.filter(ImageFilter.GaussianBlur(3))
    dark = Image.new("RGBA", panel.size, (20, 10, 4, 255))
    panel = Image.composite(dark, panel, shade.point(lambda v: v * 0.6))
    panel.putalpha(rounded_mask(PANEL_SIZE, PANEL_RADIUS))
    return panel


def make_button() -> Image.Image:
    size = BUTTON_SIZE
    image = Image.new("RGBA", (size, size))
    pixels = image.load()
    for y in range(size):
        t = y / (size - 1)
        tone = int(250 - 70 * t)
        for x in range(size):
            pixels[x, y] = (tone, tone, tone, 255)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=BUTTON_RADIUS, outline=(96, 96, 96, 255), width=2)
    draw.rounded_rectangle((2, 2, size - 3, size - 3), radius=BUTTON_RADIUS - 2, outline=(255, 255, 255, 255), width=1)
    draw.line((BUTTON_RADIUS, size - 4, size - BUTTON_RADIUS, size - 4), fill=(140, 140, 140, 255), width=1)
    image.putalpha(rounded_mask(size, BUTTON_RADIUS))
    return image


def write_meta(path: Path) -> None:
    """Asset metadata beside a new texture, its GUID taken from the file name (kept when it exists)."""
    meta = path.with_name(path.name + ".owlmeta")
    if meta.exists():
        return
    guid = hashlib.sha256(f"engine_demo/textures/ui/{path.name}".encode()).hexdigest()[:16]
    meta.write_text(
        f"FormatVersion: 1\nGuid: {guid}\nImporter: Texture\nSettings:\n  Nearest: false\n  Compression: None\n"
        "  Mips: false\n"
    )


if __name__ == "__main__":
    UI.mkdir(parents=True, exist_ok=True)
    for name, image in (("skin_panel.png", make_panel()), ("skin_button.png", make_button())):
        image.save(UI / name)
        write_meta(UI / name)
