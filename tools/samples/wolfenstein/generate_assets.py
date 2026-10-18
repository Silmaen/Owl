#!/usr/bin/env python3
"""
Draw the menu art and synthesise the menu sounds of the Wolfenstein mini sample game into ``samples/wolfenstein/``
(deterministic). The walls, the props and the game sounds come from the Engine Demo (``textures/wolf3d/``,
``sounds/raycast/``) and are not written here.

Menus (``textures/ui/``): the title, engraved in a riveted steel plate with a red band, and the steel plate of a
button, a little see-through so that the button colour (red when hovered) shows in it. Sounds (``sounds/ui/``), mono
16 bit at 22.05 kHz: ``start.wav``, a short low brass call over a snare roll, and the three sounds of the tally
(``tally_tick.wav`` while a ratio counts up, ``tally_line.wav`` when a line is done, ``tally_total.wav`` at the end).

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.wolfenstein.generate_assets``.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "wolfenstein"
"""The sample project."""
FONTS = REPOSITORY / "engine_assets" / "fonts"
"""The fonts of the engine, for the title art."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
RATE = 22050
"""Samples per second."""

STEEL = (88, 92, 96)
"""Base colour of the plates."""
RED = (150, 26, 20)
"""The red band of the title."""


def _steel(size: tuple[int, int], seed: int, alpha: int = 255) -> Image.Image:
    """Brushed steel: a base grey with fine horizontal streaks and a soft vertical light, deterministic."""
    width, height = size
    rng = random.Random(seed)
    img = Image.new("RGBA", size, (*STEEL, alpha))
    draw = ImageDraw.Draw(img)
    for y in range(height):
        light = int(18 * math.sin(math.pi * y / height)) + rng.randint(-7, 7)
        draw.line([(0, y), (width - 1, y)], fill=(*(max(0, min(255, c + light)) for c in STEEL), alpha))
    for _ in range(width // 4):
        x, y, length = rng.randrange(width), rng.randrange(height), rng.randint(20, 120)
        tone = rng.randint(-20, 20)
        draw.line([(x, y), (x + length, y)], fill=(*(max(0, min(255, c + tone)) for c in STEEL), alpha))
    return img


def _rivets(draw: ImageDraw.ImageDraw, points: list[tuple[int, int]], radius: int) -> None:
    for x, y in points:
        draw.ellipse([x - radius, y - radius, x + radius, y + radius], fill=(40, 42, 45, 255))
        draw.ellipse([x - radius + 2, y - radius + 1, x + radius - 2, y + radius - 3], fill=(160, 164, 168, 255))


def title_art() -> Image.Image:
    """The title (1400 x 300): WOLFENSTEIN engraved in a riveted steel plate, MINI on a red band under it."""
    size = (1400, 300)
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rectangle([24, 24, size[0] - 16, 236], fill=(0, 0, 0, 170))
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(9)))
    plate = _steel((size[0] - 40, 210), 5)
    draw = ImageDraw.Draw(plate)
    draw.rectangle([0, 0, plate.width - 1, plate.height - 1], outline=(30, 32, 34, 255), width=6)
    draw.line([(6, 7), (plate.width - 7, 7)], fill=(150, 154, 158, 255), width=2)
    _rivets(draw, [(22, 22), (plate.width - 23, 22), (22, plate.height - 23), (plate.width - 23, plate.height - 23)], 9)
    font = ImageFont.truetype(str(FONTS / "OpenSans-ExtraBold.ttf"), 150)
    box = draw.textbbox((0, 0), "WOLFENSTEIN", font=font)
    x = (plate.width - (box[2] - box[0])) / 2 - box[0]
    y = (plate.height - (box[3] - box[1])) / 2 - box[1]
    draw.text((x + 3, y + 4), "WOLFENSTEIN", font=font, fill=(150, 154, 158, 255))
    draw.text((x - 2, y - 2), "WOLFENSTEIN", font=font, fill=(18, 19, 20, 255))
    draw.text((x, y), "WOLFENSTEIN", font=font, fill=(52, 54, 57, 255))
    img.paste(plate, (20, 14), plate)
    band = Image.new("RGBA", (380, 70), (*RED, 255))
    band_draw = ImageDraw.Draw(band)
    band_draw.rectangle([0, 0, 379, 69], outline=(70, 10, 8, 255), width=4)
    small = ImageFont.truetype(str(FONTS / "OpenSans-ExtraBold.ttf"), 50)
    box = band_draw.textbbox((0, 0), "M I N I", font=small)
    band_draw.text(((380 - (box[2] - box[0])) / 2 - box[0], (70 - (box[3] - box[1])) / 2 - box[1]), "M I N I",
                   font=small, fill=(235, 225, 210, 255))  # fmt: skip
    band = band.rotate(-2, expand=True, resample=Image.Resampling.BICUBIC)
    img.paste(band, ((size[0] - band.width) // 2, 212), band)
    return img


def plate() -> Image.Image:
    """A menu button (640 x 100): a steel plate with a dark rim and four rivets, a quarter see-through."""
    size = (640, 100)
    img = _steel(size, 9, 185)
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size[0] - 1, size[1] - 1], outline=(22, 23, 25, 235), width=5)
    draw.line([(5, 6), (size[0] - 6, 6)], fill=(170, 174, 178, 190), width=2)
    _rivets(draw, [(18, 18), (size[0] - 19, 18), (18, size[1] - 19), (size[0] - 19, size[1] - 19)], 6)
    return img


def _write(name: str, samples: list[float], peak: float) -> None:
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


def _mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(length)]


def _brass(seconds: float, frequency: float, start: float = 0.0) -> list[float]:
    """A soft low horn: six harmonics falling off, a slow attack and release, after ``start`` seconds of silence."""
    out = [0.0] * round(start * RATE)
    count = round(seconds * RATE)
    for i in range(count):
        t = i / RATE
        envelope = min(1.0, t / 0.06) * min(1.0, (seconds - t) / 0.25)
        value = sum(math.sin(2 * math.pi * frequency * n * t) / (n * n) for n in range(1, 7))
        out.append(envelope * value)
    return out


def _snare(seconds: float, start: float, seed: int, period: float = 0.06) -> list[float]:
    """A soft snare roll: short noise bursts every ``period`` seconds, growing then fading."""
    rng = random.Random(seed)
    out = [0.0] * round(start * RATE)
    for i in range(round(seconds * RATE)):
        t = i / RATE
        burst = math.exp(-(t % period) * 60.0)
        swell = math.sin(math.pi * t / seconds)
        out.append(rng.uniform(-1.0, 1.0) * burst * swell * 0.35)
    return out


def jingle() -> list[float]:
    """The start jingle, 1.4 seconds: a low call D, A, D over a short snare roll."""
    call = _mix(_brass(0.32, 146.8), _brass(0.32, 220.0, 0.3), _brass(0.8, 293.7, 0.6), _brass(0.8, 146.8, 0.6))
    return _mix(call, _snare(0.6, 0.0, 4))


def tick() -> list[float]:
    """A dry click, 25 ms: the tally counting up."""
    return [math.sin(2 * math.pi * 1400.0 * i / RATE) * math.exp(-i / RATE * 260.0) for i in range(round(0.025 * RATE))]


def line_done() -> list[float]:
    """A muffled thump with a little ring, 0.3 second: a line of the tally is done."""
    rng = random.Random(8)
    out = []
    for i in range(round(0.3 * RATE)):
        t = i / RATE
        thump = math.sin(2 * math.pi * (95.0 - 40.0 * t) * t) * math.exp(-t * 16.0)
        ring = 0.25 * math.sin(2 * math.pi * 660.0 * t) * math.exp(-t * 12.0)
        out.append(thump + ring + rng.uniform(-1, 1) * 0.15 * math.exp(-t * 50.0))
    return out


def total() -> list[float]:
    """The end of the tally, 0.9 second: the call of the jingle, shorter, an octave up."""
    return _mix(_brass(0.25, 293.7), _brass(0.7, 440.0, 0.2), _brass(0.7, 587.3, 0.2))


def sounds() -> None:
    """Every sound written by this script."""
    _write("ui/start.wav", jingle(), 0.5)
    _write("ui/tally_tick.wav", tick(), 0.3)
    _write("ui/tally_line.wav", line_done(), 0.55)
    _write("ui/tally_total.wav", total(), 0.45)


def textures() -> None:
    """Every image written by this script."""
    (TEXTURES / "ui").mkdir(parents=True, exist_ok=True)
    title_art().save(TEXTURES / "ui" / "title.png")
    plate().save(TEXTURES / "ui" / "plate.png")


if __name__ == "__main__":
    textures()
    sounds()
