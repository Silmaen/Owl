#!/usr/bin/env python3
"""Generate the procedural textures still used by the sample project.

Only the textures referenced by the current scene flow are generated here.
Older sprites (brick / ground / coin / checkpoint / sky) were retired
together with the legacy gameplay scenes; their generators are gone too.
"""

from PIL import Image, ImageDraw
from pathlib import Path

OUT = Path(__file__).parent
SIZE = 64  # tile size


def make_platform():
    """Wooden platform tile (used by static and moving platforms in the platformer)."""
    img = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, SIZE, SIZE], fill=(160, 110, 60, 255))
    for y in range(0, SIZE, 8):
        c = 140 + (y * 3) % 30
        draw.line([(0, y), (SIZE, y)], fill=(c, c - 30, c - 60, 255), width=1)
    draw.rectangle([0, 0, SIZE - 1, 2], fill=(120, 80, 40, 255))
    draw.rectangle([0, SIZE - 3, SIZE - 1, SIZE - 1], fill=(120, 80, 40, 255))
    img.save(OUT / "platform.png")


def make_player():
    """Simple player character sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = SIZE // 2, SIZE // 2
    draw.rounded_rectangle([cx - 12, cy - 8, cx + 12, cy + 16], radius=4, fill=(80, 150, 255, 255))
    draw.ellipse([cx - 8, cy - 20, cx + 8, cy - 4], fill=(255, 210, 170, 255))
    draw.ellipse([cx - 5, cy - 16, cx - 2, cy - 12], fill=(40, 40, 40, 255))
    draw.ellipse([cx + 2, cy - 16, cx + 5, cy - 12], fill=(40, 40, 40, 255))
    img.save(OUT / "player.png")


def make_hazard():
    """Red spike hazard tile (used as the spike trap sprite in the platformer)."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    for x in range(0, SIZE, SIZE // 4):
        pts = [(x, SIZE), (x + SIZE // 8, 4), (x + SIZE // 4, SIZE)]
        draw.polygon(pts, fill=(200, 40, 40, 255))
    img.save(OUT / "hazard.png")


def make_portal():
    """Teleport / victory portal sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = SIZE // 2, SIZE // 2
    for i in range(5):
        r = SIZE // 2 - i * 3
        alpha = 100 + i * 30
        draw.ellipse([cx - r, cy - r, cx + r, cy + r],
                     outline=(100, 50, 255, alpha), width=2)
    draw.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(180, 120, 255, 200))
    img.save(OUT / "portal.png")


if __name__ == "__main__":
    make_platform()
    make_player()
    make_hazard()
    make_portal()
    print("Generated 4 textures in", OUT)
