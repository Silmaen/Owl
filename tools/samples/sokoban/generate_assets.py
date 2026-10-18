#!/usr/bin/env python3
"""
Draw the tile sheet and synthesise the sounds of the Sokoban sample game into ``samples/sokoban/`` (deterministic).

Tile sheet (``textures/sokoban_tiles.png``, 8 x 2 tiles of 16 pixels): the floor, the walls and the hero come from the
Kenney Tiny Dungeon sheet of the Engine Demo (CC0), the goal mark and the crates are drawn here in its palette. The
order of the tiles is the one ``sokoban.lua`` and ``tilesets/sokoban.owltileset`` expect (``TILES``).

Sounds: mono 16 bit at 22.05 kHz, sine, square and noise only; the music loops without a click. The two menu jingles
are copied from the Engine Demo (``sounds/ui/``).

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.sokoban.generate_assets``.
"""

from __future__ import annotations

import math
import random
import shutil
import struct
import wave
from collections.abc import Callable
from pathlib import Path

from PIL import Image

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "sokoban"
"""The sample project."""
DEMO = REPOSITORY / "engine_demo"
"""The Engine Demo, source of the Kenney sheet and of the menu jingles."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
RATE = 22050
"""Samples per second."""
TILE = 16
"""Side of a tile, in pixels."""
COLUMNS, ROWS = 8, 2
"""Tiles of the sheet."""

TILES = {
    "floor": 0,
    "floor_worn": 1,
    "wall": 2,
    "wall_mossy": 3,
    "goal": 4,
    "box": 5,
    "box_on_goal": 6,
    "start": 7,
    "hero_idle": 8,
    "hero_walk": 9,
    "hero_push": 13,
}
"""Index of each tile of the sheet (``hero_walk`` is the first of four frames)."""

DUNGEON_TILES = {"floor": 48, "floor_worn": 49, "wall": 40, "hero": 85}
"""The tiles taken from ``tiny_dungeon.png`` (12 columns)."""

Rgba = tuple[int, int, int, int]

OUTLINE: Rgba = (45, 40, 55, 255)
"""The dark outline of the Kenney packs."""


def _dungeon_tile(sheet: Image.Image, index: int) -> Image.Image:
    x, y = index % 12 * TILE, index // 12 * TILE
    return sheet.crop((x, y, x + TILE, y + TILE))


def goal() -> Image.Image:
    """The goal mark: a red diamond outline on transparent ground, the floor showing through."""
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    red, light = (200, 60, 50, 255), (240, 120, 90, 255)
    for i in range(6):
        for x, y in ((8 + i, 2 + i), (7 - i, 2 + i), (8 + i, 13 - i), (7 - i, 13 - i)):
            img.putpixel((x, y), red)
    for i in range(3):
        for x, y in ((8 + i, 5 + i), (7 - i, 5 + i), (8 + i, 10 - i), (7 - i, 10 - i)):
            img.putpixel((x, y), light)
    return img


def crate(on_goal: bool) -> Image.Image:
    """A wooden crate with a cross brace; on a goal its planks turn golden and a green band lights its rim."""
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    if on_goal:
        dark, plank, light = (150, 104, 40, 255), (232, 178, 74, 255), (252, 222, 130, 255)
        rim: Rgba = (90, 200, 90, 255)
    else:
        dark, plank, light = (122, 70, 44, 255), (196, 124, 74, 255), (226, 162, 108, 255)
        rim = dark
    for y in range(1, 15):
        for x in range(1, 15):
            img.putpixel((x, y), plank)
    for i in range(1, 15):
        img.putpixel((i, 1), rim)
        img.putpixel((i, 14), rim)
        img.putpixel((1, i), rim)
        img.putpixel((14, i), rim)
    for i in range(3, 13):
        img.putpixel((i, i), dark)
        img.putpixel((i, 15 - i), dark)
    for x in range(3, 13):
        img.putpixel((x, 2), light)
    for i in range(16):
        for x, y in ((i, 0), (i, 15), (0, i), (15, i)):
            img.putpixel((x, y), OUTLINE)
    for x, y in ((0, 0), (15, 0), (0, 15), (15, 15)):
        img.putpixel((x, y), (0, 0, 0, 0))
    return img


def mossy(wall: Image.Image) -> Image.Image:
    """The wall with moss in the joints of its lower bricks, to break the repetition of long walls."""
    img = wall.copy()
    green, light = (70, 130, 60, 255), (110, 170, 80, 255)
    for x, y in ((3, 9), (4, 9), (5, 10), (10, 13), (11, 13), (12, 12), (2, 13), (7, 6), (8, 6)):
        img.putpixel((x, y), green)
    for x, y in ((4, 10), (11, 12), (8, 7)):
        img.putpixel((x, y), light)
    return img


def hero_frames(hero: Image.Image) -> list[Image.Image]:
    """Idle, four walk frames (contact, left foot lifted, contact, right foot lifted) and a push frame (leaning)."""

    def contact() -> Image.Image:
        frame = Image.new("RGBA", hero.size, (0, 0, 0, 0))
        frame.paste(hero.crop((0, 0, TILE, 14)), (0, 1))
        for foot_x, shift in ((5, -1), (9, 1)):
            frame.paste(hero.crop((foot_x, 14, foot_x + 2, 16)), (foot_x + shift, 14))
        return frame

    def passing(lifted_x: int) -> Image.Image:
        frame = hero.copy()
        for x in range(lifted_x, lifted_x + 2):
            frame.putpixel((x, 15), (0, 0, 0, 0))
        return frame

    push = Image.new("RGBA", hero.size, (0, 0, 0, 0))
    push.paste(hero.crop((0, 0, TILE, 12)), (1, 1))
    push.paste(hero.crop((0, 12, TILE, 16)), (0, 12))
    return [hero, contact(), passing(5), contact(), passing(9), push]


def tile_sheet() -> Image.Image:
    """The tile sheet of the game, in the order of ``TILES``."""
    dungeon = Image.open(DEMO / "textures" / "tilemaps" / "tiny_dungeon.png").convert("RGBA")
    sheet = Image.new("RGBA", (COLUMNS * TILE, ROWS * TILE), (0, 0, 0, 0))

    def put(index: int, img: Image.Image) -> None:
        sheet.paste(img, (index % COLUMNS * TILE, index // COLUMNS * TILE))

    for name in ("floor", "floor_worn", "wall"):
        put(TILES[name], _dungeon_tile(dungeon, DUNGEON_TILES[name]))
    put(TILES["wall_mossy"], mossy(_dungeon_tile(dungeon, DUNGEON_TILES["wall"])))
    put(TILES["goal"], goal())
    put(TILES["box"], crate(on_goal=False))
    put(TILES["box_on_goal"], crate(on_goal=True))
    frames = hero_frames(_dungeon_tile(dungeon, DUNGEON_TILES["hero"]))
    put(TILES["start"], frames[0])
    put(TILES["hero_idle"], frames[0])
    for k, frame in enumerate(frames[1:5]):
        put(TILES["hero_walk"] + k, frame)
    put(TILES["hero_push"], frames[5])
    return sheet


def _write(name: str, samples: list[float], peak: float = 0.8) -> None:
    """Write the samples, scaled so that the loudest one reaches ``peak`` of the full scale."""
    path = SOUNDS / name
    path.parent.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def _tone(seconds: float, frequency: Callable[[float], float], decay: float, square: float = 0.0) -> list[float]:
    """A tone of a varying frequency, an exponential decay and a share of square wave for a chip sound."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * frequency(t) / RATE
        sine = math.sin(phase)
        wave_value = (1.0 - square) * sine + square * (1.0 if sine >= 0.0 else -1.0)
        attack = min(1.0, t / 0.004)
        out.append(wave_value * attack * math.exp(-t * decay))
    return out


def _noise(seconds: float, decay: float, seed: int, smooth: float = 0.0) -> list[float]:
    """White noise with an exponential decay, low-passed by ``smooth`` (0 none, close to 1 a dull thud)."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(round(seconds * RATE)):
        last = smooth * last + (1.0 - smooth) * rng.uniform(-1.0, 1.0)
        out.append(last * math.exp(-i / RATE * decay))
    return out


def _mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(length)]


def _constant(frequency: float) -> Callable[[float], float]:
    return lambda _t: frequency


def _notes(notes: list[tuple[float, float]], step: float, decay: float, square: float = 0.3) -> list[float]:
    """A sequence of (frequency, gain) notes, one every ``step`` seconds; a gain of 0 is a rest."""
    out: list[float] = []
    for frequency, gain in notes:
        out += [s * gain for s in _tone(step, _constant(frequency), decay, square=square)]
    return out


def music() -> list[float]:
    """Eight bars of a quiet puzzle loop at 96 BPM: a plucked bass and a slow bell melody, 20 seconds."""
    beat = 60.0 / 96.0
    chords = [
        (98.0, (392.0, 493.9, 587.3)),
        (82.4, (329.6, 392.0, 493.9)),
        (65.4, (261.6, 329.6, 392.0)),
        (73.4, (293.7, 370.0, 440.0)),
    ]
    melody = [(0, 2), (1, 1), (2, 1), (1, 2), (0, 2), (2, 1), (1, 1), (0, 3), (-1, 1)]
    out: list[float] = []
    for bar in range(8):
        root, triad = chords[bar % len(chords)]
        bass = _notes([(root, 0.7), (root * 1.5, 0.4), (root * 2.0, 0.5), (root * 1.5, 0.4)], beat, 4.0, square=0.2)
        bell: list[float] = []
        index, length = melody[bar % len(melody)]
        for k in range(4):
            note = triad[(index + k) % 3] * (2.0 if k == 3 and length > 1 else 1.0)
            bell += [s * 0.22 for s in _tone(beat, _constant(note), 3.5)]
        out += _mix(bass, bell)
    return out


def sounds() -> None:
    """Every sound of the game."""
    _write("game/step.wav", _mix(_noise(0.07, 60.0, 1, smooth=0.85), _tone(0.07, _constant(140.0), 55.0)), peak=0.45)
    scrape = _mix(_noise(0.16, 18.0, 2, smooth=0.6), _tone(0.16, lambda t: 110.0 - 120.0 * t, 16.0, square=0.4))
    _write("game/push.wav", scrape, peak=0.6)
    _write("game/goal.wav", _notes([(659.3, 1.0), (987.8, 1.0)], 0.08, 14.0), peak=0.6)
    _write("game/bump.wav", _tone(0.09, lambda t: 160.0 - 300.0 * t, 30.0, square=0.6), peak=0.45)
    _write("game/undo.wav", _tone(0.1, lambda t: 700.0 - 2500.0 * t, 20.0, square=0.2), peak=0.45)
    _write("game/restart.wav", _tone(0.3, lambda t: 300.0 + 900.0 * t, 8.0, square=0.3), peak=0.5)
    clear = _notes([(523.3, 1.0), (659.3, 1.0), (784.0, 1.0), (1046.5, 1.0), (0.0, 0.0), (1046.5, 1.2)], 0.1, 6.0)
    _write("game/level_clear.wav", clear)
    _write("music/loop.wav", music(), peak=0.45)
    for name in ("start.wav", "victory.wav"):
        (SOUNDS / "ui").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(DEMO / "sounds" / "ui" / name, SOUNDS / "ui" / name)


def textures() -> None:
    """Every image of the game."""
    TEXTURES.mkdir(parents=True, exist_ok=True)
    tile_sheet().save(TEXTURES / "sokoban_tiles.png")


if __name__ == "__main__":
    textures()
    sounds()
