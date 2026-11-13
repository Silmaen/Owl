#!/usr/bin/env python3
"""
Make the art and the sounds of the Sunken Shrine sample game in ``samples/sunken_shrine/`` (deterministic).

- ``textures/tiles/dungeon.png``: the Kenney Tiny Dungeon sheet of the Engine Demo (CC0), copied: walls, floors,
  doors, chests, the hero, the old man, the creatures, the sword;
- ``fonts/MedievalSharp.ttf``, the font of the menus, copied from the point-and-click sample;
- made here in its palette: ``textures/hud/icons.png`` (a sheet of four 16 pixel icons: a full heart, an empty heart,
  the key, the sword), ``textures/fx/hero.png`` (the hero facing down, up, left and right, the first one the Kenney
  frame), ``textures/fx/slash.png`` (the arc of a sword stroke),
  ``textures/fx/plate.png`` (the stone plate of the block puzzle),
  ``textures/ui/title.png`` and ``textures/ui/frame.png``.

Sounds: the sword, a hit, a creature gone, the hero hurt, a pick-up, the chest, a door, a brazier, the block, the
guardian's roar, a voice blip, the jingles, a dungeon loop, a guardian loop and a menu loop (square and noise, a
chiptune of its own); the victory jingle and the gamepad mappings are copied from the Engine Demo.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sunken_shrine.generate_assets``.
"""

from __future__ import annotations

import shutil
from collections.abc import Sequence
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples import art_kit as art
from tools.samples import synth as s

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "sunken_shrine"
"""The sample project."""
OUTLINE = (37, 19, 26, 255)
"""The dark outline of the Kenney packs."""
RED, GOLD, STEEL, STONE = (232, 59, 59, 255), (248, 197, 58, 255), (196, 207, 222, 255), (140, 143, 160, 255)
"""Colours of the palette."""

HEART = [
    "..LLLL...LLLL..",
    ".LXXXXL.LXXXXL.",
    "LXRWWRXLXRRRRXL",
    "LXRWRRRXRRRRRXL",
    "LXRRRRRRRRRRRXL",
    "LXRRRRRRRRRRRXL",
    ".LXRRRRRRRRRXL.",
    "..LXRRRRRRRXL..",
    "...LXRRRRRXL...",
    "....LXRRRXL....",
    ".....LXRXL.....",
    "......LXL......",
    ".......L.......",
]
"""
A heart, 15 x 13 pixels: ``L`` a light rim that keeps it readable over the dark floor, ``X`` the outline, ``R`` the
colour, ``W`` its shine.
"""
HEART_LIGHT = (255, 238, 214, 255)
"""The light rim of the hearts."""
KEY = ["..XXX....", ".XGGGX...", ".XG.GXXXX", ".XGGGXGGX", "..XXX.XGX", "......X.X"]
"""The key."""


def _pixels(rows: list[str], colours: dict[str, tuple[int, int, int, int]]) -> Image.Image:
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    top = (16 - len(rows)) // 2
    left = (16 - len(rows[0])) // 2
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c in colours:
                image.putpixel((left + x, top + y), colours[c])
    return image


def icons() -> Image.Image:
    """Full heart, empty heart, key, sword (the sword cut from the Kenney sheet)."""
    sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    sheet.paste(
        _pixels(HEART, {"L": HEART_LIGHT, "X": OUTLINE, "R": (240, 40, 48, 255), "W": (255, 214, 214, 255)}), (0, 0)
    )
    # The empty heart: the same rim around a hollow, so the hearts lost still read as slots.
    sheet.paste(
        _pixels(HEART, {"L": (150, 140, 150, 255), "X": OUTLINE, "R": (52, 30, 40, 255), "W": (52, 30, 40, 255)}),
        (16, 0),
    )
    sheet.paste(_pixels(KEY, {"X": OUTLINE, "G": GOLD}), (32, 0))
    tiles = Image.open(REPOSITORY / "engine_demo" / "textures" / "tilemaps" / "tiny_dungeon.png").convert("RGBA")
    sword = tiles.crop((104 % 12 * 16, 104 // 12 * 16, 104 % 12 * 16 + 16, 104 // 12 * 16 + 16))
    sheet.paste(sword, (48, 0), sword)
    return sheet


HERO_SHEET = 85
"""The frame of the hero in the Kenney Tiny Dungeon sheet, facing the viewer."""
HERO_PALETTE = "abcdefgh"
"""
The colours of the hero, in the order they appear in its frame: ``a`` outline, ``b`` dark hair, ``c`` hair and
leather, ``d`` skin, ``e`` eyes, ``f`` shaded skin, ``g`` tunic, ``h`` shaded tunic.
"""
HERO_BACK = [
    "................",
    "....aaaaaaaa....",
    "...aaaaaaaaaa...",
    "..aaabccccbaaa..",
    "..aabccccccbaa..",
    "..aaccccccccaa..",
    "..aaccccccccaa..",
    "..aabccccccbaa..",
    "..aafbccccbfaa..",
    "..aaaffbbffaaa..",
    ".aaaghggggghaaa.",
    "aaaggggggggggaaa",
    "aaddhgggggghddaa",
    "aaddaccccccaddaa",
    "aaaaabbaabbaaaaa",
    ".aaaaccaaccaaaa.",
]
"""The hero seen from behind: its hair where its face was, the back of its tunic."""
HERO_RIGHT = [
    "................",
    "....aaaaaaaa....",
    "...aaaaaaaaaa...",
    "..aaabccccbaaa..",
    "..aabcccccccaa..",
    "..aacccccccdaa..",
    "..aacccccddddaa.",
    "..aabccccdddeda.",
    "..aabbcccfdddaa.",
    "..aaabbcffffaa..",
    "...aaghcccgaaa..",
    "...aagggggggaa..",
    "...aahggggddaa..",
    "...aacccccddaa..",
    "...aabbaabbaa...",
    "...aaccaaccaa...",
]
"""The hero facing right: the hair behind, one eye, the nose and the hand in front."""


def hero() -> Image.Image:
    """
    The hero in the four ways it faces, 64 x 16: down (the Kenney frame), up, left, right (drawn here in its colours).
    """
    tiles = Image.open(ROOT / "textures" / "tiles" / "dungeon.png").convert("RGBA")
    x, y = HERO_SHEET % 12 * 16, HERO_SHEET // 12 * 16
    front = tiles.crop((x, y, x + 16, y + 16))
    colours: list[tuple[int, int, int, int]] = []
    for py in range(16):
        for px in range(16):
            pixel = front.getpixel((px, py))
            if isinstance(pixel, tuple) and pixel[3] > 0 and pixel not in colours:
                colours.append(pixel)  # type: ignore[arg-type]
    palette = dict(zip(HERO_PALETTE, colours, strict=False))
    sheet = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    sheet.paste(front, (0, 0))
    back = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    right = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for image, rows in ((back, HERO_BACK), (right, HERO_RIGHT)):
        for py, row in enumerate(rows):
            for px, c in enumerate(row):
                if c in palette:
                    image.putpixel((px, py), palette[c])
    sheet.paste(back, (16, 0))
    sheet.paste(right.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (32, 0))
    sheet.paste(right, (48, 0))
    return sheet


def slash() -> Image.Image:
    """The white arc of a stroke, 32 x 32, to the right."""
    image, draw = art.canvas(32, 32)
    k = art.SS
    draw.arc([2 * k, 2 * k, 30 * k, 30 * k], -70, 70, fill=(255, 255, 255, 230), width=4 * k)
    draw.arc([6 * k, 6 * k, 26 * k, 26 * k], -50, 50, fill=(255, 240, 200, 160), width=2 * k)
    return art.finish(image, 32, 32)


def plate() -> Image.Image:
    """A sunken stone plate, 16 x 16."""
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle([1, 1, 14, 14], fill=OUTLINE)
    draw.rectangle([2, 2, 13, 13], fill=STONE)
    draw.rectangle([4, 4, 11, 11], outline=(90, 92, 110, 255))
    draw.point([(7, 7), (8, 8)], fill=GOLD)
    return image


def _crest(image: Image.Image, draw: ImageDraw.ImageDraw, width: int, height: int) -> None:
    """A sword under the letters."""
    k = art.SS
    y = height * 0.86
    draw.rectangle([(width * 0.2) * k, (y - 5) * k, (width * 0.78) * k, (y + 5) * k], fill=STEEL)
    draw.polygon(
        [((width * 0.78) * k, (y - 9) * k), ((width * 0.82) * k, y * k), ((width * 0.78) * k, (y + 9) * k)], fill=STEEL
    )
    draw.rectangle([(width * 0.17) * k, (y - 16) * k, (width * 0.2) * k, (y + 16) * k], fill=GOLD)
    draw.rectangle([(width * 0.12) * k, (y - 5) * k, (width * 0.17) * k, (y + 5) * k], fill=(120, 70, 40, 255))


def title() -> Image.Image:
    """THE SUNKEN SHRINE in golden capitals."""
    return art.title_art(
        "THE SUNKEN SHRINE", "OpenSans-ExtraBold", (255, 236, 150), (220, 130, 40), (37, 19, 26), 120, _crest, 2.0
    )


def frame() -> Image.Image:
    """A stone frame with gold corners, for the buttons."""
    width, height = 512, 96
    image, draw = art.canvas(width, height)
    k = art.SS
    draw.rectangle([4 * k, 4 * k, (width - 4) * k, (height - 4) * k], outline=STONE, width=5 * k)
    for x, y in ((4, 4), (width - 16, 4), (4, height - 16), (width - 16, height - 16)):
        draw.rectangle([x * k, y * k, (x + 12) * k, (y + 12) * k], fill=GOLD)
    return art.finish(image, width, height)


def textures() -> None:
    """Copy the sheet, write the home-made images."""
    for folder in ("tiles", "hud", "fx", "ui"):
        (ROOT / "textures" / folder).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        REPOSITORY / "engine_demo" / "textures" / "tilemaps" / "tiny_dungeon.png",
        ROOT / "textures" / "tiles" / "dungeon.png",
    )
    # The storybook letters of the menus, the font of the point-and-click sample.
    (ROOT / "fonts").mkdir(exist_ok=True)
    shutil.copyfile(
        REPOSITORY / "samples" / "point_and_click" / "fonts" / "MedievalSharp.ttf", ROOT / "fonts" / "MedievalSharp.ttf"
    )
    icons().save(ROOT / "textures" / "hud" / "icons.png")
    hero().save(ROOT / "textures" / "fx" / "hero.png")
    slash().save(ROOT / "textures" / "fx" / "slash.png")
    plate().save(ROOT / "textures" / "fx" / "plate.png")
    title().save(ROOT / "textures" / "ui" / "title.png")
    frame().save(ROOT / "textures" / "ui" / "frame.png")


def _chip(notes: Sequence[tuple[float, float]], step: float, duty: float = 0.5, decay: float = 4.0) -> s.Samples:
    """A square-wave melody."""
    out: s.Samples = []
    for frequency, length in notes:
        seconds = step * length
        out += (
            s.square(seconds, s.constant(frequency), decay, duty) if frequency > 0 else [0.0] * round(seconds * s.RATE)
        )
    return out


def music_shrine() -> s.Samples:
    """The dungeon loop, 16 s: a minor chiptune over a soft bass."""
    a, c, d, e, f, g = 220.0, 261.6, 293.7, 329.6, 349.2, 392.0
    melody = [(e, 2), (0, 1), (e, 1), (d, 2), (c, 2), (d, 2), (e, 2), (a, 4),
              (f, 2), (0, 1), (f, 1), (e, 2), (d, 2), (c, 2), (d, 2), (e, 4),
              (e, 2), (0, 1), (g, 1), (f, 2), (e, 2), (d, 2), (c, 2), (d, 4),
              (c, 2), (d, 2), (e, 2), (c, 2), (a, 4), (0, 4)]  # fmt: skip
    lead = _chip(melody, 0.125, 0.25, 3.0)
    bass = _chip([(a / 2, 8), (f / 2, 8), (g / 2, 8), (e / 2, 8)] * 2, 0.25, 0.5, 1.0)
    seconds = 16.0
    count = round(seconds * s.RATE)
    lead = (lead * 2)[:count]
    return s.mix((lead + [0.0] * (count - len(lead)), 0.35), (bass[:count], 0.3))


def music_guardian() -> s.Samples:
    """The guardian loop, 8 s: a fast low ostinato and drums."""
    seconds = 8.0
    notes = [(110.0, 1), (110.0, 1), (130.8, 1), (110.0, 1), (146.8, 1), (110.0, 1), (130.8, 1), (123.5, 1)] * 4
    ostinato = _chip(notes, 0.25, 0.5, 6.0)
    drums = s.drums(seconds, 0.25, "k.s.kks.", 3)
    count = round(seconds * s.RATE)
    return s.mix((ostinato[:count], 0.35), (drums[:count], 0.4))


def music_menu() -> s.Samples:
    """The menu loop, 16 s: a slow chiptune hymn."""
    notes = [(329.6, 4), (293.7, 4), (261.6, 4), (293.7, 4), (329.6, 4), (392.0, 4), (329.6, 8)] * 1
    lead = _chip(notes, 0.25, 0.5, 1.5)
    pad = s.pad([[110.0, 164.8, 220.0], [98.0, 146.8, 196.0], [87.3, 130.8, 174.6], [98.0, 146.8, 196.0]], 16.0)
    return s.mix((pad, 0.5), (lead[: len(pad)], 0.2))


def sounds() -> None:
    """Write every sound."""
    out = ROOT / "sounds"
    s.write(out / "game" / "sword.wav", s.fade(s.noise(0.16, 18.0, 1, 0.3), 0.01, 0.06), 0.4)
    s.write(out / "game" / "hit.wav", s.square(0.1, lambda t: 300 - 1500 * t, 20.0), 0.45)
    s.write(
        out / "game" / "defeat.wav",
        s.mix((s.square(0.3, lambda t: 500 - 1400 * t, 8.0), 0.8), (s.noise(0.25, 14.0, 2, 0.6), 0.5)),
        0.5,
    )
    # The hero hurt: a thump under a falling buzz, loud enough to read over the music.
    s.write(
        out / "game" / "hurt.wav",
        s.mix(
            (s.square(0.32, lambda t: 330 - 700 * t, 6.0, 0.25), 0.9),
            (s.fade(s.noise(0.18, 10.0, 7, 0.5), 0.0, 0.12), 0.8),
        ),
        0.7,
    )
    s.write(out / "game" / "pickup.wav", _chip([(659.3, 1), (784.0, 1), (1046.5, 2)], 0.06, 0.5, 6.0), 0.4)
    s.write(
        out / "game" / "chest.wav",
        _chip([(392.0, 1), (523.3, 1), (659.3, 1), (784.0, 1), (1046.5, 3)], 0.08, 0.5, 5.0),
        0.45,
    )
    s.write(
        out / "game" / "door.wav",
        s.mix((s.noise(0.6, 4.0, 3, 0.92), 1.0), (s.square(0.5, lambda t: 70 + 20 * t, 3.0), 0.4)),
        0.55,
    )
    s.write(out / "game" / "brazier.wav", s.fade(s.noise(0.7, 3.0, 4, 0.8), 0.05, 0.3), 0.45)
    s.write(out / "game" / "push.wav", s.noise(0.3, 9.0, 5, 0.95), 0.55)
    s.write(
        out / "game" / "roar.wav",
        s.fade(
            s.mix((s.square(1.0, lambda t: 90 + 30 * (t * 9 % 1), 2.0), 0.8), (s.noise(1.0, 2.0, 6, 0.9), 0.6)),
            0.05,
            0.3,
        ),
        0.6,
    )
    s.write(out / "game" / "voice.wav", s.square(0.05, s.constant(660.0), 20.0, 0.25), 0.25)
    s.write(
        out / "game" / "heart.wav",
        _chip([(523.3, 1), (659.3, 1), (784.0, 1), (1046.5, 1), (1318.5, 4)], 0.09, 0.5, 4.0),
        0.5,
    )
    s.write(out / "ui" / "start.wav", _chip([(392.0, 1), (523.3, 1), (659.3, 1), (784.0, 3)], 0.1, 0.5, 4.0), 0.5)
    s.write(out / "ui" / "game_over.wav", _chip([(392.0, 2), (349.2, 2), (293.7, 4)], 0.14, 0.5, 3.0), 0.5)
    s.write(out / "ui" / "select.wav", s.square(0.06, s.constant(988.0), 30.0), 0.3)
    s.write(out / "music" / "shrine.wav", music_shrine(), 0.45)
    s.write(out / "music" / "guardian.wav", music_guardian(), 0.45)
    s.write(out / "music" / "menu.wav", music_menu(), 0.45)
    art.copy_common(ROOT)


def main() -> None:
    """Write the images and the sounds."""
    textures()
    sounds()


if __name__ == "__main__":
    main()
