#!/usr/bin/env python3
"""Crop the inventory item icons of the Engine Demo out of the Wolfenstein sprites.

The sprites sit at the bottom of a 128 px canvas; every icon is cropped to the ink, centred in a square and scaled
to 64 px (nearest) so that it fills a world sprite and an inventory slot. Run with ``poetry run python``.
"""

from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
ITEMS = ["key_yellow", "key_blue", "treasure", "treasure_crown", "health", "food"]
SIZE = 64
MARGIN = 2


def make_icon(name: str) -> None:
    """Crop one sprite to its ink bounding box, pad it to a square and scale it to the icon size."""
    sprite = Image.open(HERE / "wolf3d_sprites" / f"{name}.png").convert("RGBA")
    # Ignore the faint floor shadow: only opaque pixels count as ink.
    alpha = sprite.getchannel("A").point(lambda a: 255 if a > 200 else 0)
    box = alpha.getbbox()
    ink = sprite.crop(box)
    side = max(ink.size) + 2 * MARGIN
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(ink, ((side - ink.width) // 2, (side - ink.height) // 2), ink)
    out = HERE / "items"
    out.mkdir(exist_ok=True)
    square.resize((SIZE, SIZE), Image.NEAREST).save(out / f"{name}.png")


if __name__ == "__main__":
    for item in ITEMS:
        make_icon(item)
