#!/usr/bin/env python3
"""
Draw the art and synthesise the sounds of the Breakout sample game into ``samples/breakout/`` (deterministic).

Textures: a grey brick tinted per row by the game, the paddle in two widths, the ball, the two power-up capsules, the
walls and a dark backdrop tile. Sounds: mono 16 bit at 22.05 kHz, sine, square and noise only; the music loops without
a click.

Run from the repository root: ``./docker_run.sh poetry run python tools/samples/breakout/generate_assets.py``.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from collections.abc import Callable
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3] / "samples" / "breakout"
"""The sample project."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
RATE = 22050
"""Samples per second."""

Rgba = tuple[int, int, int, int]


def _shade(value: int, alpha: int = 255) -> Rgba:
    level = max(0, min(255, value))
    return (level, level, level, alpha)


def brick() -> Image.Image:
    """A bevelled grey brick (96 x 32): light top and left edges, dark bottom and right ones, a soft face."""
    width, height, bevel = 96, 32, 4
    img = Image.new("RGBA", (width, height))
    draw = ImageDraw.Draw(img)
    for y in range(height):
        draw.line([(0, y), (width - 1, y)], fill=_shade(232 - y * 2))
    for i in range(bevel):
        draw.line([(i, i), (width - 1 - i, i)], fill=_shade(255))
        draw.line([(i, i), (i, height - 1 - i)], fill=_shade(246))
        draw.line([(i, height - 1 - i), (width - 1 - i, height - 1 - i)], fill=_shade(128))
        draw.line([(width - 1 - i, i), (width - 1 - i, height - 1 - i)], fill=_shade(146))
    draw.rectangle([0, 0, width - 1, height - 1], outline=_shade(70))
    draw.line([(bevel + 4, bevel + 3), (width // 2, bevel + 3)], fill=_shade(255, 200))
    return img


def paddle(width: int) -> Image.Image:
    """The paddle: a steel capsule with red caps, ``width`` x 24 pixels."""
    height = 24
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    radius = height // 2
    for y in range(height):
        glow = 1.0 - abs(y - height * 0.35) / height
        steel = (int(120 + 110 * glow), int(140 + 100 * glow), int(170 + 80 * glow), 255)
        draw.line([(radius, y), (width - 1 - radius, y)], fill=steel)
    for cx in (radius, width - 1 - radius):
        draw.ellipse([cx - radius, 0, cx + radius, height - 1], fill=(200, 50, 45, 255))
        draw.ellipse([cx - radius + 4, 3, cx + radius - 6, height // 2], fill=(250, 140, 120, 255))
    cap = radius * 2
    draw.rectangle([cap, 0, cap + 3, height - 1], fill=(60, 60, 75, 255))
    draw.rectangle([width - 1 - cap - 3, 0, width - 1 - cap, height - 1], fill=(60, 60, 75, 255))
    return img


def ball() -> Image.Image:
    """The ball: a shaded white sphere (32 x 32)."""
    size = 32
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for y in range(size):
        for x in range(size):
            dx, dy = (x + 0.5 - size / 2) / (size / 2), (y + 0.5 - size / 2) / (size / 2)
            dist = math.hypot(dx, dy)
            if dist > 1.0:
                continue
            light = max(0.0, 1.0 - math.hypot(dx + 0.35, dy + 0.35))
            level = int(150 + 105 * light)
            alpha = 255 if dist < 0.92 else int(255 * (1.0 - dist) / 0.08)
            img.putpixel((x, y), (level, level, min(255, level + 10), alpha))
    return img


def power_up(colour: tuple[int, int, int], letter: str) -> Image.Image:
    """A power-up capsule (64 x 28) of a colour, its letter in the middle."""
    width, height = 64, 28
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    outline = (20, 20, 30, 255)
    draw.rounded_rectangle([0, 0, width - 1, height - 1], radius=height // 2, fill=(*colour, 255), outline=outline)
    light = tuple(min(255, c + 90) for c in colour)
    draw.rounded_rectangle([6, 3, width - 7, height // 2 - 1], radius=6, fill=(*light, 255))
    font = ImageFont.load_default(size=20)
    box = draw.textbbox((0, 0), letter, font=font)
    position = ((width - (box[2] - box[0])) / 2 - box[0], (height - (box[3] - box[1])) / 2 - box[1])
    draw.text((position[0] + 1, position[1] + 1), letter, font=font, fill=(0, 0, 0, 200))
    draw.text(position, letter, font=font, fill=(255, 255, 255, 255))
    return img


def wall() -> Image.Image:
    """A riveted steel tile of the walls (32 x 32), repeated along them."""
    size = 32
    img = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(img)
    for y in range(size):
        draw.line([(0, y), (size - 1, y)], fill=(78 + y // 2, 86 + y // 2, 108 + y // 2, 255))
    draw.rectangle([0, 0, size - 1, size - 1], outline=(40, 44, 58, 255))
    draw.line([(1, 1), (size - 2, 1)], fill=(150, 160, 185, 255))
    for cx, cy in ((7, 7), (24, 7), (7, 24), (24, 24)):
        draw.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=(170, 178, 200, 255))
    return img


def backdrop() -> Image.Image:
    """A dark tile of the play field (128 x 128): a faint diagonal grid and specks, repeated without a seam."""
    size = 128
    rng = random.Random(7)
    img = Image.new("RGBA", (size, size))
    for y in range(size):
        for x in range(size):
            grid = 1.0 if (x + y) % 32 == 0 or (x - y) % 32 == 0 else 0.0
            img.putpixel((x, y), (int(14 + 10 * grid), int(16 + 12 * grid), int(34 + 20 * grid), 255))
    for _ in range(40):
        x, y = rng.randrange(size), rng.randrange(size)
        img.putpixel((x, y), (60, 66, 110, 255))
    return img


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


def _noise(seconds: float, decay: float, seed: int) -> list[float]:
    rng = random.Random(seed)
    return [rng.uniform(-1.0, 1.0) * math.exp(-i / RATE * decay) for i in range(round(seconds * RATE))]


def _mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(length)]


def _constant(frequency: float) -> Callable[[float], float]:
    return lambda _t: frequency


def _notes(notes: list[tuple[float, float]], step: float, decay: float) -> list[float]:
    """A sequence of (frequency, gain) notes, one every ``step`` seconds."""
    out: list[float] = []
    for frequency, gain in notes:
        out += [s * gain for s in _tone(step, _constant(frequency), decay, square=0.3)]
    return out


def sounds() -> None:
    """Every sound of the game."""
    _write("game/paddle.wav", _tone(0.09, lambda t: 420.0 + 300.0 * t, 30.0, square=0.5))
    _write("game/wall.wav", _tone(0.06, _constant(300.0), 45.0, square=0.4), peak=0.55)
    _write("game/brick.wav", _tone(0.08, _constant(640.0), 35.0, square=0.5), peak=0.7)
    _write("game/metal.wav", _mix(_tone(0.25, _constant(1180.0), 18.0), _tone(0.25, _constant(1770.0), 25.0)), peak=0.6)
    crack = _mix(_tone(0.16, lambda t: 900.0 - 1800.0 * t, 22.0, square=0.6), [s * 0.6 for s in _noise(0.16, 30.0, 1)])
    _write("game/brick_break.wav", crack)
    _write("game/power_up.wav", _notes([(523.3, 1.0), (659.3, 1.0), (784.0, 1.0), (1046.5, 1.0)], 0.07, 12.0))
    _write("game/serve.wav", _tone(0.12, lambda t: 300.0 + 2400.0 * t, 14.0, square=0.3), peak=0.6)
    _write("game/lose.wav", _tone(0.7, lambda t: 620.0 * math.exp(-t * 2.4), 3.0, square=0.4))
    _write("game/level_clear.wav", _notes([(523.3, 1.0), (659.3, 1.0), (784.0, 1.0), (1046.5, 1.2)], 0.12, 6.0))
    _write("music/loop.wav", music(), peak=0.5)


def music() -> list[float]:
    """Eight bars of a calm chip loop at 120 BPM: a bass line and an arpeggio, 16 seconds."""
    beat = 0.5
    chords = [(110.0, (220.0, 261.6, 329.6)), (87.3, (174.6, 220.0, 261.6)), (98.0, (196.0, 246.9, 293.7)),
              (82.4, (164.8, 207.7, 246.9))]  # fmt: skip
    out: list[float] = []
    for bar in range(8):
        root, triad = chords[bar % len(chords)]
        bass = _notes([(root, 0.8), (root, 0.5), (root * 1.5, 0.6), (root, 0.5)], beat, 3.0)
        arp = _notes([(f * 2.0, 0.25) for _ in range(2) for f in (*triad, triad[1])], beat / 2.0, 9.0)
        out += _mix(bass, arp)
    return out


def textures() -> None:
    """Every image of the game."""
    TEXTURES.mkdir(parents=True, exist_ok=True)
    brick().save(TEXTURES / "brick.png")
    paddle(160).save(TEXTURES / "paddle.png")
    paddle(240).save(TEXTURES / "paddle_wide.png")
    ball().save(TEXTURES / "ball.png")
    power_up((40, 170, 70), "W").save(TEXTURES / "power_wide.png")
    power_up((50, 110, 210), "S").save(TEXTURES / "power_slow.png")
    wall().save(TEXTURES / "wall.png")
    backdrop().save(TEXTURES / "backdrop.png")


if __name__ == "__main__":
    textures()
    sounds()
