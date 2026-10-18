"""Extra tiles of the hub village, drawn in Tiny Town's palette (``generate_kenney_atlases.py`` pastes them in the atlas).

Rows 15 and up of ``town_atlas.png`` (12 tiles per row, tile ``180 + n``), see ``VILLAGE_LAYOUT`` for the names:
the plaza cobbles, the shrine floor, crops and flower beds, a hay bale and a lamp post, the fountain (2x2, two phases of
the water), the flags (two frames each), the butterflies, a sparrow, a hen and, for ten characters of Tiny Dungeon, three
extra frames each: a second idle frame (the body one pixel lower, breathing), then the two walk frames. The water of
the river has two more phases in the free tiles 137 and 138 of row 11.
"""

from __future__ import annotations

from PIL import Image

OUT = (63, 38, 49, 255)
GRASS = (132, 198, 105, 255)
GRASS_D = (71, 159, 74, 255)
DIRT = (234, 165, 108, 255)
WOOD = (189, 108, 74, 255)
WOOD_D = (118, 59, 54, 255)
YELLOW = (253, 190, 83, 255)
ORANGE = (227, 134, 40, 255)
STONE = (192, 203, 220, 255)
STONE_D = (139, 155, 180, 255)
STONE_DD = (90, 105, 136, 255)
RED = (195, 75, 53, 255)
RED_L = (242, 132, 98, 255)
WHITE = (255, 255, 255, 255)
WATER = (79, 164, 224, 255)
WATER_L = (122, 196, 240, 255)
WATER_D = (58, 134, 200, 255)
CLEAR = (0, 0, 0, 0)

FIRST = 180  # first tile of row 15

VILLAGE_LAYOUT = {
    "cobble_a": 180, "cobble_b": 181, "shrine_floor": 182, "wheat": 183, "cabbage": 184, "carrots": 185,
    "bed_red": 186, "bed_blue": 187, "soil": 188, "hay": 189, "lamp": 190, "bed_pink": 191,
    "fountain_a": 192,  # 192..195: top left, top right, bottom left, bottom right
    "fountain_b": 196,  # 196..199
    "flag_red": 200, "flag_blue": 202, "flag_yellow": 204,  # two frames each (+1)
    "butterfly_yellow": 206, "butterfly_pink": 208, "butterfly_blue": 210, "butterfly_white": 212,  # two frames each
    "sparrow": 214,  # 214, 215: wings up, wings down
    "hen": 216,  # 216, 217: standing, pecking
    "water_c": 137, "water_d": 138,
}
# Tiny Dungeon characters that get three extra frames (idle 2, walk A, walk B) from tile EXTRA_BASE + 3 * position.
EXTRA_CHARACTERS = (144, 145, 146, 147, 148, 156, 157, 158, 159, 160)
EXTRA_BASE = 218
ROWS = 21


def extra_frames(character: int) -> tuple[int, int, int]:
    """The three extra frame indices (idle 2, walk A, walk B) of a character in :data:`EXTRA_CHARACTERS`."""
    first = EXTRA_BASE + 3 * EXTRA_CHARACTERS.index(character)
    return first, first + 1, first + 2


def _canvas(width: int = 16, height: int = 16, fill: tuple[int, int, int, int] = CLEAR) -> Image.Image:
    return Image.new("RGBA", (width, height), fill)


def _px(image: Image.Image, points: list[tuple[int, int]], colour: tuple[int, int, int, int]) -> None:
    for x, y in points:
        if 0 <= x < image.width and 0 <= y < image.height:
            image.putpixel((x, y), colour)


def _rect(image: Image.Image, x0: int, y0: int, x1: int, y1: int, colour: tuple[int, int, int, int]) -> None:
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            _px(image, [(x, y)], colour)


def _cobble(variant: int) -> Image.Image:
    tile = _canvas(fill=STONE)
    for row in range(4):
        y = row * 4 + 3
        _rect(tile, 0, y, 15, y, STONE_D)
        offset = 0 if (row + variant) % 2 == 0 else 4
        for x in range(offset, 16, 8):
            _rect(tile, x, row * 4, x, row * 4 + 2, STONE_D)
    _px(tile, [(2, 1), (11, 5), (6, 9), (13, 13)] if variant == 0 else [(9, 1), (3, 6), (12, 10), (5, 14)], STONE_DD)
    return tile


def _shrine_floor() -> Image.Image:
    tile = _canvas(fill=STONE_D)
    _rect(tile, 0, 0, 15, 0, STONE_DD)
    _rect(tile, 0, 15, 15, 15, STONE_DD)
    _rect(tile, 0, 0, 0, 15, STONE_DD)
    _rect(tile, 15, 0, 15, 15, STONE_DD)
    for x, y in ((7, 4), (8, 4), (4, 7), (4, 8), (11, 7), (11, 8), (7, 11), (8, 11)):
        _px(tile, [(x, y)], YELLOW)
    _px(tile, [(7, 7), (8, 7), (7, 8), (8, 8)], ORANGE)
    return tile


def _soil() -> Image.Image:
    tile = _canvas(fill=DIRT)
    for y in (3, 7, 11, 15):
        _rect(tile, 0, y, 15, y, WOOD)
    return tile


def _crop(kind: str) -> Image.Image:
    tile = _soil()
    for row, y in enumerate((2, 6, 10, 14)):
        for x in range(1, 16, 5):
            x += row % 2 * 2
            if kind == "wheat":
                _px(tile, [(x, y), (x, y - 1), (x, y - 2)], YELLOW)
                _px(tile, [(x, y - 2)], ORANGE)
                _px(tile, [(x + 1, y - 1)], YELLOW)
            elif kind == "cabbage":
                _rect(tile, x - 1, y - 2, x + 1, y, GRASS_D)
                _px(tile, [(x, y - 1)], GRASS)
                _px(tile, [(x - 1, y - 2), (x + 1, y)], OUT)
            else:
                _px(tile, [(x, y - 2), (x - 1, y - 1), (x + 1, y - 1)], GRASS_D)
                _px(tile, [(x, y - 1), (x, y)], ORANGE)
    return tile


def _bed(flower: tuple[int, int, int, int], centre: tuple[int, int, int, int]) -> Image.Image:
    tile = _canvas(fill=GRASS)
    _rect(tile, 1, 2, 14, 13, DIRT)
    _rect(tile, 1, 13, 14, 13, WOOD)
    for index, (x, y) in enumerate(((3, 4), (7, 3), (11, 5), (4, 8), (9, 8), (12, 10), (6, 11), (2, 11))):
        _px(tile, [(x, y + 1)], GRASS_D)
        _px(tile, [(x - 1, y), (x + 1, y), (x, y - 1), (x, y)], flower if index % 3 else centre)
        _px(tile, [(x, y)], centre)
    return tile


def _hay() -> Image.Image:
    tile = _canvas()
    _rect(tile, 2, 4, 13, 14, OUT)
    _rect(tile, 3, 5, 12, 13, YELLOW)
    for y in (7, 10):
        _rect(tile, 3, y, 12, y, ORANGE)
    _rect(tile, 3, 5, 12, 5, (255, 228, 140, 255))
    _px(tile, [(5, 3), (8, 3), (11, 3), (6, 2)], YELLOW)
    return tile


def _lamp() -> Image.Image:
    tile = _canvas()
    _rect(tile, 7, 5, 8, 14, OUT)
    _rect(tile, 7, 6, 7, 13, STONE_D)
    _rect(tile, 5, 14, 10, 15, OUT)
    _rect(tile, 6, 14, 9, 14, STONE)
    _rect(tile, 5, 1, 10, 5, OUT)
    _rect(tile, 6, 2, 9, 4, YELLOW)
    _px(tile, [(7, 3), (8, 3)], WHITE)
    _rect(tile, 6, 0, 9, 0, OUT)
    return tile


def _fountain(phase: int) -> list[Image.Image]:
    """The fountain, 2x2 tiles: a round stone basin, water with ripples that move with ``phase``, a spout."""
    big = _canvas(32, 32)
    cx, cy, radius = 15.5, 16.5, 14.5
    for y in range(32):
        for x in range(32):
            dist = ((x - cx) ** 2 + ((y - cy) * 1.1) ** 2) ** 0.5
            if dist <= radius:
                colour = OUT if dist > radius - 1.2 else STONE if dist > radius - 3.2 else STONE_D if dist > radius - 4.2 else WATER
                big.putpixel((x, y), colour)
    rng_points = [(9, 14), (10, 14), (19, 20), (20, 20), (12, 22), (13, 22), (21, 12), (22, 12)]
    for index, (x, y) in enumerate(rng_points):
        shift = (phase + index // 2) % 2
        _px(big, [(x + shift, y + shift)], WATER_L if (index + phase) % 2 == 0 else WATER_D)
    _rect(big, 13, 14, 18, 19, OUT)
    _rect(big, 14, 15, 17, 18, STONE)
    _rect(big, 14, 18, 17, 18, STONE_D)
    jet = (15, 16, 17)[phase % 3 if phase < 3 else 0]
    _px(big, [(15, 13), (16, 13), (15, 12 - phase), (16, 11 - phase)], WATER_L)
    _px(big, [(13 + phase, 13), (18 - phase, 13), (12 + phase, 14), (19 - phase, 14)], WATER_L)
    _px(big, [(jet, 12)], WHITE)
    return [big.crop((0, 0, 16, 16)), big.crop((16, 0, 32, 16)), big.crop((0, 16, 16, 32)), big.crop((16, 16, 32, 32))]


def _flag(cloth: tuple[int, int, int, int], dark: tuple[int, int, int, int], wave: int) -> Image.Image:
    tile = _canvas()
    _rect(tile, 4, 2, 4, 15, OUT)
    _rect(tile, 5, 2, 5, 15, WOOD)
    _px(tile, [(4, 1), (5, 1)], YELLOW)
    for step in range(9):
        x = 6 + step
        y = 3 + (1 if (step + wave) % 4 in (1, 2) else 0)
        height = 5 - step // 3
        _rect(tile, x, y, x, y + height - 1, cloth if step % 4 < 2 else dark)
        _px(tile, [(x, y - 1)], OUT)
        _px(tile, [(x, y + height)], OUT)
    return tile


def _butterfly(wing: tuple[int, int, int, int], dark: tuple[int, int, int, int], open_wings: bool) -> Image.Image:
    tile = _canvas()
    body = [(8, 7), (8, 8), (8, 9)]
    if open_wings:
        upper = [(6, 6), (5, 5), (6, 5), (7, 6), (9, 6), (10, 5), (11, 5), (10, 6)]
        lower = [(6, 8), (7, 8), (6, 9), (9, 8), (10, 8), (10, 9)]
    else:
        upper = [(7, 6), (7, 7), (6, 7), (9, 6), (9, 7), (10, 7)]
        lower = [(7, 8), (9, 8), (7, 9), (9, 9)]
    _px(tile, upper + lower, wing)
    _px(tile, [(6, 5), (10, 5)] if open_wings else [(7, 7), (9, 7)], dark)
    _px(tile, body, OUT)
    _px(tile, [(7, 5), (9, 5)], OUT)
    return tile


def _sparrow(wings_up: bool) -> Image.Image:
    tile = _canvas()
    brown, belly = (150, 92, 70, 255), (230, 200, 170, 255)
    _px(tile, [(6, 8), (7, 8), (8, 8), (9, 8), (10, 8), (7, 9), (8, 9), (9, 9)], brown)
    _px(tile, [(11, 7), (12, 7), (11, 8)], brown)
    _px(tile, [(12, 7)], OUT)
    _px(tile, [(13, 8)], ORANGE)
    _px(tile, [(7, 10), (8, 10), (9, 10)], belly)
    _px(tile, [(5, 9), (4, 9)], OUT)
    if wings_up:
        _px(tile, [(7, 7), (7, 6), (8, 5), (8, 6), (9, 7)], OUT)
    else:
        _px(tile, [(7, 10), (6, 11), (7, 11), (9, 11), (10, 11)], OUT)
    return tile


def _hen(pecking: bool) -> Image.Image:
    tile = _canvas()
    _rect(tile, 4, 7, 11, 12, OUT)
    _rect(tile, 5, 8, 10, 11, WHITE)
    _px(tile, [(5, 11), (6, 11)], STONE)
    head_y = 8 if pecking else 5
    _rect(tile, 10, head_y, 13, head_y + 2, OUT)
    _rect(tile, 11, head_y + 1, 12, head_y + 1, WHITE)
    _px(tile, [(11, head_y)], RED)
    _px(tile, [(14, head_y + 1)], ORANGE)
    _px(tile, [(6, 13), (9, 13), (6, 14), (9, 14)], ORANGE)
    _px(tile, [(3, 8), (3, 9)], OUT)
    return tile


def _character_frames(hero: Image.Image) -> tuple[Image.Image, Image.Image, Image.Image]:
    """Idle 2 (breathing: the body one pixel lower), walk A (feet apart, or the same breath for a robe) and walk B."""
    width, height = hero.size
    idle = _canvas()
    idle.paste(hero.crop((0, 0, width, 14)), (0, 1))
    idle.alpha_composite(hero.crop((0, 14, width, height)), (0, 14))
    columns = [x for x in range(width) if hero.getpixel((x, height - 1))[3] > 0]
    groups: list[list[int]] = []
    for x in columns:
        if groups and x == groups[-1][-1] + 1:
            groups[-1].append(x)
        else:
            groups.append([x])
    if len(groups) < 2:
        return idle, idle, hero.copy()
    left, right = groups[0][0], groups[-1][0]
    contact = _canvas()
    contact.paste(hero.crop((0, 0, width, 14)), (0, 1))
    for foot_x, shift in ((left, -1), (right, 1)):
        contact.alpha_composite(hero.crop((foot_x, 14, foot_x + 2, 16)), (foot_x + shift, 14))
    passing = hero.copy()
    for x in range(right, min(right + 2, width)):
        passing.putpixel((x, 15), CLEAR)
    return idle, contact, passing


def build_village_tiles(atlas: Image.Image) -> None:
    """Paste the village tiles into an atlas of 12 columns (tall enough for :data:`ROWS` rows)."""

    def put(index: int, tile: Image.Image) -> None:
        atlas.paste(tile, ((index % 12) * 16, (index // 12) * 16))

    layout = VILLAGE_LAYOUT
    put(layout["cobble_a"], _cobble(0))
    put(layout["cobble_b"], _cobble(1))
    put(layout["shrine_floor"], _shrine_floor())
    put(layout["wheat"], _crop("wheat"))
    put(layout["cabbage"], _crop("cabbage"))
    put(layout["carrots"], _crop("carrots"))
    put(layout["bed_red"], _bed(RED, YELLOW))
    put(layout["bed_blue"], _bed((90, 130, 235, 255), WHITE))
    put(layout["bed_pink"], _bed((235, 120, 190, 255), YELLOW))
    put(layout["soil"], _soil())
    put(layout["hay"], _hay())
    put(layout["lamp"], _lamp())
    for name, phase in (("fountain_a", 0), ("fountain_b", 1)):
        for offset, tile in enumerate(_fountain(phase)):
            put(layout[name] + offset, tile)
    for name, cloth, dark in (("flag_red", RED, RED_L), ("flag_blue", (70, 110, 220, 255), (130, 170, 245, 255)),
                              ("flag_yellow", YELLOW, ORANGE)):
        put(layout[name], _flag(cloth, dark, 0))
        put(layout[name] + 1, _flag(cloth, dark, 2))
    for name, wing, dark in (("butterfly_yellow", YELLOW, ORANGE), ("butterfly_pink", (240, 130, 190, 255), RED),
                             ("butterfly_blue", (110, 160, 245, 255), (60, 90, 190, 255)), ("butterfly_white", WHITE, STONE)):
        put(layout[name], _butterfly(wing, dark, True))
        put(layout[name] + 1, _butterfly(wing, dark, False))
    put(layout["sparrow"], _sparrow(True))
    put(layout["sparrow"] + 1, _sparrow(False))
    put(layout["hen"], _hen(False))
    put(layout["hen"] + 1, _hen(True))
    for position, character in enumerate(EXTRA_CHARACTERS):
        sprite = atlas.crop(((character % 12) * 16, (character // 12) * 16, (character % 12) * 16 + 16, (character // 12) * 16 + 16))
        for offset, frame in enumerate(_character_frames(sprite)):
            put(EXTRA_BASE + 3 * position + offset, frame)
