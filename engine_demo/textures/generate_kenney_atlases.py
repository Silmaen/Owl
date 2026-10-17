#!/usr/bin/env python3
"""Build the Engine Demo atlases from the Kenney CC0 packs in ``kenney/``.

* ``town_atlas.png``     16 px tiles, 12 columns: Tiny Town's packed sheet (rows 0-10), a row of extra tiles in the
  same palette (four phases of water, lava, bridge; Tiny Town has none), then the Tiny Dungeon characters (rows 12-13)
  and the four walk frames of the hero (row 14, tiles 168-171: the pack has none, they are drawn from the idle sprite),
  then the village tiles of ``generate_village_tiles.py`` (rows 15-20: plaza, crops, fountain, flags, butterflies, a
  sparrow, a hen, idle and walk frames of ten characters).
* ``platformer_atlas.png`` 360x288: Pixel Platformer's packed tiles (18 px grid, rows 0-8) and its characters
  (24 px grid, from row 9 of 24 px, i.e. y = 216). 288 is a multiple of 18 and of 24, so the one texture reads as an
  18 px tile grid (20x16) and as a 24 px sprite grid (15x12).

* ``platformer_ext_atlas.png`` 288x288, 18 px tiles, 16 columns: the Pixel Platformer farm expansion (rows 0-6, tile
  ``i``), the industrial expansion (rows 7-13, tile ``112 + i``), then row 14: the six phases of the water surface
  (tiles 224-229, the wave of the base tile 33 rolled one pixel at a time), non-collidable copies of dirt tiles
  (230-235) that hide secret rooms, the water body (236), the darkened dirt of a cave (237-239) and row 15: the
  checkpoint flag, idle (240) then raised and waving (241-243).
* ``platformer_backdrop.png`` 192x120, 24 px tiles, 8 columns: the Pixel Platformer backgrounds (rows 0-2) and a
  keyed copy of row 1 (row 3, the sky colour made transparent) so the horizon bands can be stacked as parallax layers.

Run with ``poetry run python`` from anywhere; the packs are read, never modified.
"""

from __future__ import annotations

from pathlib import Path

from generate_village_tiles import ROWS, build_village_tiles
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
    # Two more phases of the water (the ripples of the two first, rolled), so that the river runs on four frames.
    put(5, [row[5:] + row[:5] for row in water_a[8:] + water_a[:8]], water)
    put(6, [row[11:] + row[:11] for row in water_b[4:] + water_b[:4]], water)
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
    atlas = Image.new("RGBA", (192, ROWS * 16), (0, 0, 0, 0))
    atlas.paste(town, (0, 0))
    atlas.paste(_extra_tiles(), (0, 11 * 16))
    atlas.paste(dungeon.crop((0, 7 * 16, 192, 9 * 16)), (0, 12 * 16))
    hero_x, hero_y = HERO_TILE[0] * 16, HERO_TILE[1] * 16
    for index, frame in enumerate(_hero_walk_frames(atlas.crop((hero_x, hero_y, hero_x + 16, hero_y + 16)))):
        atlas.paste(frame, (index * 16, WALK_ROW * 16))
    build_village_tiles(atlas)
    atlas.save(HERE / "town_atlas.png")


def build_platformer() -> None:
    tiles = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap_packed.png").convert("RGBA")
    chars = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap-characters_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (360, 288), (0, 0, 0, 0))
    atlas.paste(tiles, (0, 0))
    atlas.paste(chars, (0, 216))
    atlas.save(HERE / "platformer_atlas.png")


FARM_DIRT_COPIES = (33, 34, 35, 49, 50, 51)
CAVE_DIRT = (34, 33, 35)
CHECKPOINT_OFF = ((150, 150, 160, 255), (110, 110, 125, 255))
CHECKPOINT_ON = ((235, 70, 60, 255), (170, 40, 40, 255))


def _flag(cloth: tuple[tuple[int, ...], tuple[int, ...]], wave: int) -> Image.Image:
    """A checkpoint flag on its pole in the packs' palette: grey when idle, red and waving once reached."""
    tile = Image.new("RGBA", (18, 18), (0, 0, 0, 0))
    pixels = tile.load()
    outline = (45, 40, 55, 255)
    pole = (205, 170, 120, 255)
    for y in range(2, 17):
        for x in (4, 5):
            pixels[x, y] = pole
        pixels[3, y] = outline
        pixels[6, y] = outline
    for x in range(2, 8):
        pixels[x, 16] = outline
        pixels[x, 17] = outline
    pixels[4, 1] = pixels[5, 1] = (240, 205, 90, 255)
    light, dark = cloth
    for row in range(6):
        reach = 8 - row if row < 3 else 3 + row - 2
        for step in range(max(reach, 1)):
            x = 7 + step
            y = 2 + row + (wave if step >= 4 else 0) % 3 - (1 if step >= 5 and wave == 2 else 0)
            if 0 <= x < 18 and 0 <= y < 18:
                pixels[x, y] = light if row < 3 else dark
    return tile


def build_platformer_ext() -> None:
    farm = Image.open(PACKS / "pixel-platformer-farm-expansion/Tilemap/tilemap_packed.png").convert("RGBA")
    industrial = Image.open(PACKS / "pixel-platformer-industrial-expansion/Tilemap/tilemap_packed.png").convert("RGBA")
    base = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (288, 288), (0, 0, 0, 0))
    atlas.paste(farm, (0, 0))
    atlas.paste(industrial, (0, 126))
    water = base.crop((13 * 18, 18, 14 * 18, 36))  # tile 33 of the base sheet: the wave repeats every 6 px
    for phase in range(6):
        frame = water.copy()
        wave = water.crop((0, 0, 18, 8))
        for y in range(8):
            row = wave.crop((0, y, 18, y + 1))
            frame.paste(row.crop((18 - phase, 0, 18, 1)), (0, y))
            frame.paste(row.crop((0, 0, 18 - phase, 1)), (phase, y))
        atlas.paste(frame, (phase * 18, 14 * 18))
    for slot, tile in enumerate(FARM_DIRT_COPIES):
        copy = farm.crop(((tile % 16) * 18, (tile // 16) * 18, (tile % 16) * 18 + 18, (tile // 16) * 18 + 18))
        atlas.paste(copy, ((6 + slot) * 18, 14 * 18))
    atlas.paste(base.crop((13 * 18, 54, 14 * 18, 72)), (12 * 18, 14 * 18))  # tile 53 of the base sheet: the water body
    for slot, tile in enumerate(CAVE_DIRT):
        copy = farm.crop(((tile % 16) * 18, (tile // 16) * 18, (tile % 16) * 18 + 18, (tile // 16) * 18 + 18))
        dark = copy.point(lambda value: int(value * 0.55) if value < 256 else value)
        dark.putalpha(copy.getchannel("A"))
        atlas.paste(dark, ((13 + slot) * 18, 14 * 18))
    for slot, (cloth, wave) in enumerate(((CHECKPOINT_OFF, 0), (CHECKPOINT_ON, 0), (CHECKPOINT_ON, 1), (CHECKPOINT_ON, 2))):
        atlas.paste(_flag(cloth, wave), (slot * 18, 15 * 18))
    atlas.save(HERE / "platformer_ext_atlas.png")


def build_platformer_backdrop() -> None:
    sheet = Image.open(PACKS / "pixel-platformer/Tilemap/tilemap-backgrounds_packed.png").convert("RGBA")
    atlas = Image.new("RGBA", (192, 120), (0, 0, 0, 0))
    atlas.paste(sheet, (0, 0))
    for column in range(8):
        tile = sheet.crop((column * 24, 24, column * 24 + 24, 48)).copy()
        sky = tile.getpixel((0, 0))
        pixels = tile.load()
        for y in range(24):
            for x in range(24):
                if pixels[x, y] == sky:
                    pixels[x, y] = (0, 0, 0, 0)
        atlas.paste(tile, (column * 24, 72))
    atlas.save(HERE / "platformer_backdrop.png")


if __name__ == "__main__":
    build_town()
    build_platformer()
    build_platformer_ext()
    build_platformer_backdrop()
