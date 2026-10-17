#!/usr/bin/env python3
"""Build the Engine Demo atlases from the Kenney CC0 packs in ``kenney/``.

* ``town_atlas.png``     16 px tiles, 12 columns: Tiny Town's packed sheet (rows 0-10), a row of extra tiles in the
  same palette (water, lava, bridge; Tiny Town has none), then the Tiny Dungeon characters (rows 12-13).
* ``platformer_atlas.png`` 360x288: Pixel Platformer's packed tiles (18 px grid, rows 0-8) and its characters
  (24 px grid, from row 9 of 24 px, i.e. y = 216). 288 is a multiple of 18 and of 24, so the one texture reads as an
  18 px tile grid (20x16) and as a 24 px sprite grid (15x12).

Run with ``poetry run python`` from anywhere; the packs are read, never modified.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
PACKS = HERE / "kenney"


def _extra_tiles() -> Image.Image:
    """Water, lava and bridge tiles drawn in Tiny Town's palette (the pack has no liquid)."""
    sheet = Image.new("RGBA", (16 * 12, 16), (0, 0, 0, 0))

    def put(index: int, rows: list[str], palette: dict[str, tuple[int, int, int, int]]) -> None:
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                sheet.putpixel((index * 16 + x, y), palette[ch])

    water = {".": (79, 164, 224, 255), "o": (122, 196, 240, 255), "d": (58, 134, 200, 255)}
    water_a = ["................", "................", "...oooo.........", "..o....o........", "................",
               "...........oooo.", "..........o....o", "................", "................", "..oooo..........",
               ".o....o.........", "................", "........oooo....", ".......o....o...", "................",
               "................"]
    water_b = ["................", ".......oooo.....", "......o....o....", "................", "................",
               "..oooo..........", ".o....o.........", "................", "..........oooo..", ".........o....o.",
               "................", "................", "...oooo.........", "..o....o........",
               "................", "................"]
    put(0, water_a, water)
    put(1, water_b, water)
    lava = {".": (214, 72, 36, 255), "o": (250, 160, 52, 255), "d": (150, 40, 28, 255)}
    put(2, water_a, lava)
    put(3, water_b, lava)
    wood = {"d": (74, 44, 40, 255), "b": (200, 126, 84, 255), "l": (226, 160, 108, 255)}
    plank = ["d" + "b" * 14 + "d", "d" + "l" * 14 + "d", "d" + "b" * 14 + "d", "d" * 16] * 4
    put(4, plank, wood)
    return sheet


def build_town() -> None:
    town = Image.open(PACKS / "tiny-town/Tilemap/tilemap_packed.png").convert("RGBA")
    dungeon = Image.open(PACKS / "tiny-dungeon/Tilemap/tilemap_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (192, 14 * 16), (0, 0, 0, 0))
    atlas.paste(town, (0, 0))
    atlas.paste(_extra_tiles(), (0, 11 * 16))
    atlas.paste(dungeon.crop((0, 7 * 16, 192, 9 * 16)), (0, 12 * 16))
    atlas.save(HERE / "town_atlas.png")


def build_platformer() -> None:
    tiles = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap_packed.png").convert("RGBA")
    chars = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap-characters_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (360, 288), (0, 0, 0, 0))
    atlas.paste(tiles, (0, 0))
    atlas.paste(chars, (0, 216))
    atlas.save(HERE / "platformer_atlas.png")


if __name__ == "__main__":
    build_town()
    build_platformer()
