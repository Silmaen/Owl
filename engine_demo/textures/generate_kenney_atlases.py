#!/usr/bin/env python3
"""Build the Engine Demo atlases from the Kenney CC0 packs in ``kenney/``.

* ``town_atlas.png``     16 px tiles, 12 columns: Tiny Town's packed sheet (rows 0-10), a row of extra tiles in the
  same palette (water, lava, bridge; Tiny Town has none), then the Tiny Dungeon characters (rows 12-13) and the four
  walk frames of the hero (row 14, tiles 168-171: the pack has none, they are drawn from the idle sprite).
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


HERO_TILE = (4, 12)  # column, row of the hero in the atlas
WALK_ROW = 14


def _hero_walk_frames(hero: Image.Image) -> list[Image.Image]:
    """Four walk frames drawn from the idle hero: contact (body one pixel lower, feet apart), passing with the left
    foot lifted, contact again, passing with the right foot lifted."""
    width, height = hero.size

    def blank() -> Image.Image:
        return Image.new("RGBA", hero.size, (0, 0, 0, 0))

    def contact() -> Image.Image:
        frame = blank()
        frame.paste(hero.crop((0, 0, width, 14)), (0, 1))  # the body without its legs, one pixel lower
        for foot_x, shift in ((5, -1), (9, 1)):  # the feet, pushed apart
            frame.paste(hero.crop((foot_x, 14, foot_x + 2, 16)), (foot_x + shift, 14))
        return frame

    def passing(lifted_x: int) -> Image.Image:
        frame = hero.copy()
        for x in range(lifted_x, lifted_x + 2):  # the lifted foot leaves the ground: its last row goes
            frame.putpixel((x, 15), (0, 0, 0, 0))
        return frame

    return [contact(), passing(5), contact(), passing(9)]


def build_town() -> None:
    town = Image.open(PACKS / "tiny-town/Tilemap/tilemap_packed.png").convert("RGBA")
    dungeon = Image.open(PACKS / "tiny-dungeon/Tilemap/tilemap_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (192, 15 * 16), (0, 0, 0, 0))
    atlas.paste(town, (0, 0))
    atlas.paste(_extra_tiles(), (0, 11 * 16))
    atlas.paste(dungeon.crop((0, 7 * 16, 192, 9 * 16)), (0, 12 * 16))
    hero_x, hero_y = HERO_TILE[0] * 16, HERO_TILE[1] * 16
    for index, frame in enumerate(_hero_walk_frames(atlas.crop((hero_x, hero_y, hero_x + 16, hero_y + 16)))):
        atlas.paste(frame, (index * 16, WALK_ROW * 16))
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
