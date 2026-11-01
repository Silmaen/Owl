#!/usr/bin/env python3
"""
Draw the art and synthesise the sounds of the Rail shooter sample game into ``samples/rail_shooter/``
(deterministic, home-made).

- ``textures/sky/starfield.hdr``: the stars and a faint nebula of the sky, an equirectangular Radiance image (run
  length encoded) lighting the scenes as their ``Environment``;
- ``textures/hud/``: the far and near reticles, the target brackets of the enemies and of the boss (HUD markers),
  the medals (a sheet of four frames: none, bronze, silver, gold);
- ``textures/ui/``: the title art and the frame of a button, in the cyan and amber of the game's cockpit look.

Sounds: mono 16 bit at 22.05 kHz, soft and short (lasers, hits, explosions, rocks, barrel roll, a two-tone alarm, the
start jingle), two music loops (menus, flight) that loop without a click; the victory jingle is copied from the
Engine Demo.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.rail_shooter.generate_assets``.
"""

from __future__ import annotations

import math
import random
import shutil
import struct
import wave
from collections.abc import Callable
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "rail_shooter"
"""The sample project."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
FONTS = REPOSITORY / "engine_assets" / "fonts"
"""The fonts of the engine, for the title art."""
RATE = 22050
"""Samples per second."""
CYAN = (110, 230, 255)
"""Colour of the cockpit lines."""
AMBER = (255, 190, 70)
"""Colour of the highlights."""
SS = 4
"""Supersampling of the drawings."""


# ---------------------------------------------------------------------------------------------------------------- sky


def _rgbe(r: float, g: float, b: float) -> bytes:
    """A linear colour as the four bytes of a Radiance pixel."""
    top = max(r, g, b)
    if top < 1e-32:
        return b"\0\0\0\0"
    mantissa, exponent = math.frexp(top)
    scale = mantissa * 256.0 / top
    return bytes((int(r * scale), int(g * scale), int(b * scale), exponent + 128))


def _rle(channel: bytes) -> bytes:
    """One channel of a scanline in the run length encoding of the Radiance format."""
    out = bytearray()
    i = 0
    while i < len(channel):
        run = 1
        while i + run < len(channel) and run < 127 and channel[i + run] == channel[i]:
            run += 1
        if run > 2:
            out += bytes((128 + run, channel[i]))
            i += run
            continue
        start = i
        while i < len(channel) and i - start < 128:
            if i + 2 < len(channel) and channel[i] == channel[i + 1] == channel[i + 2]:
                break
            i += 1
        out += bytes((i - start,)) + channel[start:i]
    return bytes(out)


def starfield(width: int = 2048, height: int = 1024) -> bytes:
    """The sky: thousands of stars of a few colours and a faint band of nebula, as a Radiance file."""
    rng = random.Random(20261031)
    pixels = [[(0.0, 0.0, 0.0)] * width for _ in range(height)]
    # The nebula: a soft band across the sky, blue and violet, faint enough to stay a background.
    for y in range(height):
        latitude = (y + 0.5) / height * math.pi - math.pi / 2
        for x in range(0, width):
            longitude = (x + 0.5) / width * 2 * math.pi
            band = latitude - 0.35 * math.sin(longitude * 1.0 + 0.6)
            density = math.exp(-(band * band) / 0.02)
            ripple = 0.6 + 0.4 * math.sin(longitude * 5.0 + math.sin(longitude * 3.0) * 2.0)
            level = 0.012 * density * ripple
            if level > 0.0004:
                pixels[y][x] = (level * 0.55, level * 0.45, level * 1.0)
    tints = [(1.0, 1.0, 1.0), (0.8, 0.88, 1.0), (1.0, 0.92, 0.78), (1.0, 0.8, 0.7)]
    for _ in range(9000):
        z = rng.uniform(-1.0, 1.0)
        longitude = rng.uniform(0.0, 2.0 * math.pi)
        y = min(height - 1, int((math.acos(-z)) / math.pi * height))
        x = min(width - 1, int(longitude / (2 * math.pi) * width))
        brightness = 0.03 * rng.random() ** 8 + 0.002
        tint = tints[rng.randrange(len(tints))]
        r, g, b = pixels[y][x]
        pixels[y][x] = (r + tint[0] * brightness, g + tint[1] * brightness, b + tint[2] * brightness)
    out = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n" + f"-Y {height} +X {width}\n".encode())
    for row in pixels:
        encoded = [_rgbe(*p) for p in row]
        out += bytes((2, 2, width >> 8, width & 0xFF))
        for channel in range(4):
            out += _rle(bytes(e[channel] for e in encoded))
    return bytes(out)


# ------------------------------------------------------------------------------------------------------------ images


def _canvas(width: int, height: int) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def _finish(image: Image.Image, width: int, height: int, glow: float = 0.0) -> Image.Image:
    """Downsample a supersampled drawing, with a halo of its own colours when ``glow`` is above 0."""
    small = image.resize((width, height), Image.Resampling.LANCZOS)
    if glow <= 0.0:
        return small
    halo = small.filter(ImageFilter.GaussianBlur(glow))
    return Image.alpha_composite(halo, small)


def reticle() -> Image.Image:
    """The far reticle: four corner brackets around a small diamond, cyan."""
    size = 128
    image, draw = _canvas(size, size)
    s = SS
    w = 5 * s
    for cx, cy in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
        x0 = size * s / 2 + cx * 52 * s
        y0 = size * s / 2 + cy * 52 * s
        draw.line([(x0, y0 - cy * 22 * s), (x0, y0), (x0 - cx * 22 * s, y0)], fill=(*CYAN, 255), width=w)
    c = size * s / 2
    draw.polygon([(c, c - 9 * s), (c + 9 * s, c), (c, c + 9 * s), (c - 9 * s, c)], outline=(*CYAN, 255), width=3 * s)
    return _finish(image, size, size, 2.0)


def reticle_near() -> Image.Image:
    """The near reticle: a thin open ring with four ticks."""
    size = 96
    image, draw = _canvas(size, size)
    s = SS
    c = size * s / 2
    for start in (20, 110, 200, 290):
        draw.arc([c - 34 * s, c - 34 * s, c + 34 * s, c + 34 * s], start, start + 50, fill=(*CYAN, 220), width=4 * s)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        draw.line(
            [(c + dx * 40 * s, c + dy * 40 * s), (c + dx * 46 * s, c + dy * 46 * s)], fill=(*CYAN, 220), width=4 * s
        )
    return _finish(image, size, size, 1.5)


def target(colour: tuple[int, int, int], size: int, gap: float) -> Image.Image:
    """Target brackets around an enemy, opened by ``gap`` (a share of the side)."""
    image, draw = _canvas(size, size)
    s = SS
    m = 6 * s
    full = size * s
    arm = full * gap
    for x0, y0, dx, dy in ((m, m, 1, 1), (full - m, m, -1, 1), (m, full - m, 1, -1), (full - m, full - m, -1, -1)):
        draw.line([(x0, y0 + dy * arm), (x0, y0), (x0 + dx * arm, y0)], fill=(*colour, 255), width=4 * s)
    return _finish(image, size, size, 1.5)


def medals() -> Image.Image:
    """Four medals side by side: none (an empty ring), bronze, silver, gold, each a star on a disc with a ribbon."""
    cell = 128
    sheet = Image.new("RGBA", (cell * 4, cell), (0, 0, 0, 0))
    metals = [
        None,
        ((205, 127, 50), (120, 70, 30)),
        ((215, 220, 230), (120, 125, 140)),
        ((255, 205, 60), (170, 110, 20)),
    ]
    for index, metal in enumerate(metals):
        image, draw = _canvas(cell, cell)
        s = SS
        c = cell * s / 2
        if metal is None:
            draw.ellipse([c - 40 * s, c - 34 * s, c + 40 * s, c + 46 * s], outline=(140, 150, 170, 160), width=4 * s)
        else:
            face, rim = metal
            draw.polygon(
                [(c - 30 * s, 0), (c - 8 * s, 0), (c + 6 * s, c - 20 * s), (c - 14 * s, c - 20 * s)], fill=(*CYAN, 255)
            )
            draw.polygon(
                [(c + 30 * s, 0), (c + 8 * s, 0), (c - 6 * s, c - 20 * s), (c + 14 * s, c - 20 * s)],
                fill=(60, 120, 200, 255),
            )
            draw.ellipse([c - 42 * s, c - 34 * s, c + 42 * s, c + 50 * s], fill=(*rim, 255))
            draw.ellipse([c - 35 * s, c - 27 * s, c + 35 * s, c + 43 * s], fill=(*face, 255))
            points = []
            for k in range(10):
                radius = (26 if k % 2 == 0 else 11) * s
                angle = -math.pi / 2 + k * math.pi / 5
                points.append((c + radius * math.cos(angle), c + 8 * s + radius * math.sin(angle)))
            draw.polygon(points, fill=(*rim, 255))
        sheet.alpha_composite(_finish(image, cell, cell), (index * cell, 0))
    return sheet


def title_art() -> Image.Image:
    """RING RUNNER in wide italic capitals, white to cyan, crossed by an amber orbit."""
    width, height = 1400, 340
    image, draw = _canvas(width, height)
    s = SS
    font = ImageFont.truetype(str(FONTS / "OpenSans-ExtraBoldItalic.ttf"), 150 * s)
    text = "RING RUNNER"
    box = draw.textbbox((0, 0), text, font=font)
    x = (width * s - (box[2] - box[0])) / 2 - box[0]
    y = (height * s - (box[3] - box[1])) / 2 - box[1]
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).text((x, y), text, font=font, fill=255)
    gradient = Image.new("RGBA", image.size)
    for row in range(image.size[1]):
        t = row / image.size[1]
        colour = tuple(int(255 + (c - 255) * t) for c in CYAN)
        ImageDraw.Draw(gradient).line([(0, row), (image.size[0], row)], fill=(*colour, 255))
    letters = Image.new("RGBA", image.size, (0, 0, 0, 0))
    letters.paste(gradient, (0, 0), mask)
    outline = Image.new("RGBA", image.size, (0, 0, 0, 0))
    outline_mask = mask.filter(ImageFilter.MaxFilter(4 * s + 1))
    outline.paste(Image.new("RGBA", image.size, (10, 30, 70, 255)), (0, 0), outline_mask)
    image = Image.alpha_composite(outline, letters)
    orbit = Image.new("RGBA", image.size, (0, 0, 0, 0))
    ring = ImageDraw.Draw(orbit)
    ring.arc([40 * s, 80 * s, (width - 40) * s, (height - 20) * s], 200, 340, fill=(*AMBER, 255), width=7 * s)
    ring.arc([40 * s, 80 * s, (width - 40) * s, (height - 20) * s], 20, 160, fill=(*AMBER, 140), width=4 * s)
    image = Image.alpha_composite(image, orbit)
    return _finish(image, width, height, 6.0)


def button_frame() -> Image.Image:
    """A frame with cut corners, cyan, for the buttons."""
    width, height = 512, 96
    image, draw = _canvas(width, height)
    s = SS
    cut = 18 * s
    w, h = width * s - 3 * s, height * s - 3 * s
    o = 3 * s
    points = [(o + cut, o), (w, o), (w, h - cut), (w - cut, h), (o, h), (o, o + cut), (o + cut, o)]
    draw.line(points, fill=(*CYAN, 230), width=3 * s)
    draw.line([(w - 90 * s, h), (w - cut, h)], fill=(*AMBER, 255), width=5 * s)
    return _finish(image, width, height, 1.0)


def textures() -> None:
    """Write every image."""
    for folder in ("sky", "hud", "ui"):
        (TEXTURES / folder).mkdir(parents=True, exist_ok=True)
    (TEXTURES / "sky" / "starfield.hdr").write_bytes(starfield())
    reticle().save(TEXTURES / "hud" / "reticle.png")
    reticle_near().save(TEXTURES / "hud" / "reticle_near.png")
    target((255, 90, 70), 64, 0.3).save(TEXTURES / "hud" / "target.png")
    target((255, 60, 120), 96, 0.22).save(TEXTURES / "hud" / "boss_target.png")
    medals().save(TEXTURES / "hud" / "medals.png")
    title_art().save(TEXTURES / "ui" / "title.png")
    button_frame().save(TEXTURES / "ui" / "button_frame.png")


# ------------------------------------------------------------------------------------------------------------ sounds


def _write(name: str, samples: list[float], peak: float = 0.7) -> None:
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


def _tone(
    seconds: float,
    frequency: Callable[[float], float],
    decay: float,
    attack: float = 0.004,
    harmonics: tuple[float, ...] = (1.0,),
) -> list[float]:
    """A tone of a varying frequency with a few harmonics, a short attack and an exponential decay."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * frequency(t) / RATE
        value = sum(weight * math.sin(phase * (k + 1)) for k, weight in enumerate(harmonics))
        out.append(value * min(1.0, t / attack) * math.exp(-t * decay))
    return out


def _noise(seconds: float, decay: float, seed: int, smooth: float) -> list[float]:
    """White noise through a one-pole low-pass (``smooth`` near 1 is darker), decaying."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(round(seconds * RATE)):
        last = smooth * last + (1.0 - smooth) * rng.uniform(-1.0, 1.0)
        out.append(last * math.exp(-i / RATE * decay))
    return out


def _constant(frequency: float) -> Callable[[float], float]:
    """A frequency that does not change."""
    return lambda _t: frequency


def _mix(*tracks: tuple[list[float], float]) -> list[float]:
    length = max(len(t) for t, _ in tracks)
    return [sum(t[i] * gain for t, gain in tracks if i < len(t)) for i in range(length)]


def _fade_out(samples: list[float], seconds: float) -> list[float]:
    count = round(seconds * RATE)
    return [s * min(1.0, (len(samples) - i) / count) for i, s in enumerate(samples)]


def _note(frequency: float, seconds: float, decay: float = 3.0) -> list[float]:
    return _tone(seconds, _constant(frequency), decay, 0.01, (1.0, 0.25, 0.08))


def _sequence(notes: list[tuple[float, float]], step: float, decay: float = 4.0) -> list[float]:
    """Notes (frequency, length in steps; 0 Hz is a rest) one after the other, each ringing a little past its end."""
    total = sum(length for _, length in notes) * step
    out = [0.0] * round((total + 0.4) * RATE)
    cursor = 0.0
    for frequency, length in notes:
        if frequency > 0:
            start = round(cursor * RATE)
            for i, value in enumerate(_note(frequency, step * length + 0.3, decay)):
                if start + i < len(out):
                    out[start + i] += value
        cursor += step * length
    return out


def _pad(chords: list[list[float]], seconds: float) -> list[float]:
    """Slow chords, each fading in and out over its length, the loop's last chord running into its first."""
    each = seconds / len(chords)
    count = round(each * RATE)
    out: list[float] = []
    for chord in chords:
        for i in range(count):
            t = i / RATE
            envelope = math.sin(math.pi * i / count) ** 0.6
            value = sum(math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * f * 2.003 * t) for f in chord)
            out.append(value * envelope / len(chord))
    return out


def music_flight() -> list[float]:
    """The flight loop, 16 s: a minor pad, a soft pulse on the root and a sparse arpeggio, ending where it starts."""
    seconds = 16.0
    a, c, d, e, f, g = 110.0, 130.81, 146.83, 164.81, 174.61, 196.0
    pad = _pad([[a, e, 2 * c], [f, c * 2, 2 * a], [d, a, 2 * f], [e, g, 2 * e]], seconds)
    pulse: list[float] = []
    beat = 0.25
    for k in range(round(seconds / beat)):
        half_root = [a, f, d, e][k * 4 // round(seconds / beat)] / 2
        pulse += _tone(beat, _constant(half_root), 9.0, 0.003, (1.0, 0.5, 0.25))
    arpeggio_notes = []
    for root in (a, f, d, e):
        for ratio in (2.0, 3.0, 4.0, 3.0, 2.5, 3.0, 4.0, 5.0):
            arpeggio_notes.append((root * ratio, 1.0))
        arpeggio_notes += [(0.0, 8.0)]
    arpeggio = _sequence(arpeggio_notes, 0.25, 6.0)[: len(pad)]
    return _mix((pad, 0.5), (pulse[: len(pad)], 0.35), (arpeggio, 0.12))


def music_menu() -> list[float]:
    """The menu loop, 16 s: open fifths and a slow bell now and then."""
    seconds = 16.0
    pad = _pad([[98.0, 147.0, 196.0], [87.3, 130.8, 174.6], [110.0, 165.0, 220.0], [98.0, 147.0, 246.9]], seconds)
    bells = _sequence(
        [(587.3, 8), (0.0, 8), (493.9, 8), (0.0, 8), (659.3, 8), (0.0, 8), (587.3, 8), (0.0, 8)], 0.25, 1.5
    )[: len(pad)]
    return _mix((pad, 0.55), (bells, 0.12))


def sounds() -> None:
    """Write every sound."""
    _write(
        "game/laser.wav",
        _mix(
            (_tone(0.16, lambda t: 1400 - 5000 * t, 22.0, 0.002, (1.0, 0.3)), 1.0), (_noise(0.05, 60.0, 1, 0.6), 0.15)
        ),
        0.45,
    )
    _write("game/enemy_laser.wav", _tone(0.2, lambda t: 620 - 1600 * t, 16.0, 0.003, (1.0, 0.5, 0.2)), 0.4)
    _write("game/hit.wav", _mix((_noise(0.12, 30.0, 2, 0.75), 1.0), (_tone(0.1, _constant(220.0), 30.0), 0.5)), 0.5)
    _write(
        "game/explosion.wav",
        _fade_out(_mix((_noise(0.9, 4.5, 3, 0.93), 1.0), (_tone(0.6, lambda t: 90 - 50 * t, 5.0), 0.6)), 0.2),
        0.7,
    )
    _write(
        "game/pop.wav",
        _mix((_noise(0.4, 9.0, 10, 0.9), 1.0), (_tone(0.3, lambda t: 140 - 80 * t, 12.0, 0.002), 0.7)),
        0.55,
    )
    _write(
        "game/rock_hit.wav",
        _mix((_noise(0.14, 26.0, 11, 0.82), 1.0), (_tone(0.12, lambda t: 160 - 90 * t, 24.0, 0.001, (1.0, 0.4)), 0.8)),
        0.5,
    )
    _write("game/rock_break.wav", _mix((_noise(0.35, 11.0, 4, 0.88), 1.0), (_noise(0.08, 40.0, 5, 0.4), 0.4)), 0.55)
    _write(
        "game/hull_hit.wav",
        _mix((_tone(0.35, lambda t: 120 - 60 * t, 9.0, 0.002, (1.0, 0.6)), 1.0), (_noise(0.25, 14.0, 6, 0.9), 0.6)),
        0.65,
    )
    _write(
        "game/roll.wav",
        [v * math.sin(math.pi * i / round(0.5 * RATE)) for i, v in enumerate(_noise(0.5, 0.0, 7, 0.96))],
        0.4,
    )
    alarm = _sequence([(440.0, 2), (330.0, 2), (440.0, 2), (330.0, 2)], 0.18, 5.0)
    _write("game/alarm.wav", alarm, 0.45)
    _write(
        "game/boss_explosion.wav",
        _fade_out(
            _mix(
                (_noise(2.2, 1.8, 8, 0.95), 1.0),
                (_tone(1.8, lambda t: 70 - 25 * t, 2.0), 0.7),
                (_noise(0.4, 8.0, 9, 0.85), 0.5),
            ),
            0.5,
        ),
        0.75,
    )
    _write("ui/start.wav", _sequence([(392.0, 1), (523.3, 1), (659.3, 1), (784.0, 3)], 0.1, 3.5), 0.55)
    _write("ui/game_over.wav", _sequence([(392.0, 2), (349.2, 2), (311.1, 2), (261.6, 5)], 0.16, 2.5), 0.5)
    _write("ui/select.wav", _tone(0.08, _constant(880.0), 30.0, 0.002, (1.0, 0.2)), 0.3)
    _write("music/flight.wav", music_flight(), 0.5)
    _write("music/menu.wav", music_menu(), 0.45)
    shutil.copyfile(REPOSITORY / "engine_demo" / "sounds" / "ui" / "victory.wav", SOUNDS / "ui" / "victory.wav")


def main() -> None:
    """Write every texture and sound."""
    textures()
    sounds()


if __name__ == "__main__":
    main()
