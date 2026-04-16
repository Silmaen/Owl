#!/usr/bin/env python3
"""Generate simple procedural textures for the sample project."""

from PIL import Image, ImageDraw
from pathlib import Path
import math
import random

OUT = Path(__file__).parent
SIZE = 64  # tile size


def make_ground():
    """Green grass/dirt ground tile."""
    img = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(img)
    # Dirt base
    draw.rectangle([0, 0, SIZE, SIZE], fill=(100, 70, 40, 255))
    # Grass top
    draw.rectangle([0, 0, SIZE, SIZE // 3], fill=(60, 140, 50, 255))
    # Grass blades
    random.seed(42)
    for x in range(0, SIZE, 3):
        h = random.randint(2, SIZE // 4)
        draw.line([(x, SIZE // 3 - h), (x, SIZE // 3)], fill=(40, 120, 35, 255), width=1)
    img.save(OUT / "ground.png")


def make_brick():
    """Brick wall tile."""
    img = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, SIZE, SIZE], fill=(140, 90, 60, 255))
    # Brick lines
    mortar = (100, 65, 40, 255)
    for y in range(0, SIZE, SIZE // 4):
        draw.line([(0, y), (SIZE, y)], fill=mortar, width=1)
    for row in range(4):
        offset = (SIZE // 4) if row % 2 else 0
        for x in range(offset, SIZE + offset, SIZE // 2):
            draw.line([(x % SIZE, row * SIZE // 4), (x % SIZE, (row + 1) * SIZE // 4)],
                      fill=mortar, width=1)
    img.save(OUT / "brick.png")


def make_platform():
    """Wooden platform tile."""
    img = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, SIZE, SIZE], fill=(160, 110, 60, 255))
    # Wood grain
    for y in range(0, SIZE, 8):
        c = 140 + (y * 3) % 30
        draw.line([(0, y), (SIZE, y)], fill=(c, c - 30, c - 60, 255), width=1)
    # Edges
    draw.rectangle([0, 0, SIZE - 1, 2], fill=(120, 80, 40, 255))
    draw.rectangle([0, SIZE - 3, SIZE - 1, SIZE - 1], fill=(120, 80, 40, 255))
    img.save(OUT / "platform.png")


def make_player():
    """Simple player character sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = SIZE // 2, SIZE // 2
    # Body
    draw.rounded_rectangle([cx - 12, cy - 8, cx + 12, cy + 16], radius=4, fill=(80, 150, 255, 255))
    # Head
    draw.ellipse([cx - 8, cy - 20, cx + 8, cy - 4], fill=(255, 210, 170, 255))
    # Eyes
    draw.ellipse([cx - 5, cy - 16, cx - 2, cy - 12], fill=(40, 40, 40, 255))
    draw.ellipse([cx + 2, cy - 16, cx + 5, cy - 12], fill=(40, 40, 40, 255))
    img.save(OUT / "player.png")


def make_coin():
    """Gold coin sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = SIZE // 2, SIZE // 2
    r = SIZE // 2 - 4
    # Outer ring
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 220, 50, 255))
    # Inner ring
    draw.ellipse([cx - r + 4, cy - r + 4, cx + r - 4, cy + r - 4], fill=(255, 200, 30, 255))
    # Star/$ symbol
    draw.ellipse([cx - r + 8, cy - r + 8, cx + r - 8, cy + r - 8], fill=(255, 230, 80, 255))
    img.save(OUT / "coin.png")


def make_checkpoint():
    """Checkpoint flag sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # Pole
    draw.rectangle([SIZE // 2 - 2, 8, SIZE // 2 + 2, SIZE - 4], fill=(180, 180, 180, 255))
    # Flag
    pts = [(SIZE // 2 + 2, 8), (SIZE // 2 + 24, 18), (SIZE // 2 + 2, 28)]
    draw.polygon(pts, fill=(50, 200, 255, 255))
    # Base
    draw.rectangle([SIZE // 2 - 8, SIZE - 8, SIZE // 2 + 8, SIZE - 2], fill=(120, 120, 120, 255))
    img.save(OUT / "checkpoint.png")


def make_hazard():
    """Red hazard/spike tile."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # Spikes
    for x in range(0, SIZE, SIZE // 4):
        pts = [(x, SIZE), (x + SIZE // 8, 4), (x + SIZE // 4, SIZE)]
        draw.polygon(pts, fill=(200, 40, 40, 255))
    img.save(OUT / "hazard.png")


def make_portal():
    """Teleport portal sprite."""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = SIZE // 2, SIZE // 2
    # Outer glow
    for i in range(5):
        r = SIZE // 2 - i * 3
        alpha = 100 + i * 30
        draw.ellipse([cx - r, cy - r, cx + r, cy + r],
                     outline=(100, 50, 255, alpha), width=2)
    # Inner bright
    draw.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(180, 120, 255, 200))
    img.save(OUT / "portal.png")


def make_sky():
    """Sky gradient background."""
    img = Image.new("RGBA", (256, 256))
    for y in range(256):
        r = int(30 + (y / 256) * 40)
        g = int(80 + (y / 256) * 60)
        b = int(180 - (y / 256) * 80)
        for x in range(256):
            img.putpixel((x, y), (r, g, b, 255))
    img.save(OUT / "sky.png")


if __name__ == "__main__":
    make_ground()
    make_brick()
    make_platform()
    make_player()
    make_coin()
    make_checkpoint()
    make_hazard()
    make_portal()
    make_sky()
    print("Generated 9 textures in", OUT)
