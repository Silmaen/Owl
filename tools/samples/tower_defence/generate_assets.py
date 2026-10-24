#!/usr/bin/env python3
"""
Draw the sprites and synthesise the sounds of the Tower defence sample game into ``samples/tower_defence/``
(deterministic, home-made).

- ``textures/enemies/<kind>.png``: the Horde in 8 directions, the walk, the blow and the fall (``creatures.py``);
- ``textures/fx/``: ``projectiles.png`` (arrow, cannon ball, fire shell, frost shard), ``fire.png`` (4 flame frames),
  ``ring.png`` (range ring) and ``cursor.png`` (pad frame) laid flat on the ground, ``shadow.png``;
- ``textures/icons.png``: the badges of the build ring and of the info panel (``ICONS``, 6 by row);
- ``textures/puff.png``, ``textures/bar.png`` (health bar), tinted by the script;
- menus and HUD: parchment, wood, iron and gold of ``medieval_ui.py`` in ``textures/ui/``, and the parade under them
  (the Horde sheets and the keep, tower and road of ``scenery.py`` in ``textures/menu/``).

Sounds: mono 16 bit at 22.05 kHz, short and neutral (build, sell, shots, hits, horn, coins, keep hit, start jingle);
the victory jingle is copied from the Engine Demo.

The 3D terrain, the towers and the keep are models of the Kenney Tower Defense Kit and Castle Kit (CC0), copied into
``models/td/`` and ``models/castle/``.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_assets``.
"""

from __future__ import annotations

import math
import random
import shutil
import struct
import wave
from collections.abc import Callable
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples.tower_defence import creatures, medieval_ui, scenery, world_map_art

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "tower_defence"
"""The sample project."""
DEMO = REPOSITORY / "engine_demo"
"""The Engine Demo, source of the terrain sheet and of the victory jingle."""
FONTS = REPOSITORY / "engine_assets" / "fonts"
"""The fonts of the engine, for the title art."""
TEXTURES = ROOT / "textures"
"""Folder of the written images."""
SOUNDS = ROOT / "sounds"
"""Folder of the written sounds."""
RATE = 22050
"""Samples per second."""
SS = 4
"""Supersampling of the drawings."""
RGB = tuple[int, int, int]

TOWERS = ("archer", "cannon", "frost")
"""Tower kinds, also the first three frames of ``icons.png``."""


def _shade(colour: RGB, amount: float) -> tuple[int, int, int, int]:
    """A colour moved towards white (positive) or black (negative), opaque."""
    if amount >= 0:
        r, g, b = (round(c + (255 - c) * amount) for c in colour)
    else:
        r, g, b = (round(c * (1 + amount)) for c in colour)
    return r, g, b, 255


class Canvas:
    """A supersampled RGBA drawing, in output pixels."""

    def __init__(self, width: int, height: int) -> None:
        self.size = (width, height)
        self.image = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.image)

    def poly(
        self, points: list[tuple[float, float]], colour: tuple[int, ...], outline: tuple[int, ...] | None = None
    ) -> None:
        """A filled polygon."""
        scaled = [(x * SS, y * SS) for x, y in points]
        self.draw.polygon(scaled, fill=colour, outline=outline, width=SS if outline else 0)

    def ellipse(self, box: tuple[float, float, float, float], colour: tuple[int, ...], width: float = 0) -> None:
        """A filled ellipse, or its outline ``width`` pixels thick."""
        scaled = [v * SS for v in box]
        if width:
            self.draw.ellipse(scaled, outline=colour, width=round(width * SS))
        else:
            self.draw.ellipse(scaled, fill=colour)

    def line(self, points: list[tuple[float, float]], colour: tuple[int, ...], width: float) -> None:
        """A polyline."""
        self.draw.line([(x * SS, y * SS) for x, y in points], fill=colour, width=round(width * SS))

    def done(self) -> Image.Image:
        """The image at its output size."""
        return self.image.resize(self.size, Image.Resampling.LANCZOS)


ICONS = (
    "archer",
    "cannon",
    "frost",
    "sell",
    "upgrade",
    "locked",
    "pierce",
    "double",
    "fire",
    "shock",
    "freeze",
    "aura",
)
"""Badges of ``icons.png``, 6 per row: the towers, sell, the next level, a locked power-up, then the power-ups."""


def icon(kind: str) -> Image.Image:
    """A round badge of the build ring and of the HUD."""
    c = Canvas(64, 64)
    rims = {
        "archer": (176, 52, 44),
        "cannon": (90, 90, 100),
        "frost": (60, 140, 210),
        "sell": (200, 160, 40),
        "upgrade": (60, 150, 70),
        "locked": (70, 70, 74),
        "pierce": (176, 52, 44),
        "double": (176, 52, 44),
        "fire": (210, 100, 30),
        "shock": (120, 100, 70),
        "freeze": (90, 180, 230),
        "aura": (130, 80, 190),
    }
    rim = rims[kind]
    c.ellipse((2, 2, 62, 62), _shade(rim, -0.4))
    c.ellipse((5, 5, 59, 59), _shade(rim, 0.0))
    c.ellipse((10, 10, 54, 54), (245, 236, 214, 255) if kind != "locked" else (150, 148, 144, 255))
    ink = (50, 36, 28, 255)
    if kind in ("archer", "pierce", "double"):
        c.draw.arc([18 * SS, 14 * SS, 40 * SS, 50 * SS], 270, 90, fill=ink, width=3 * SS)
        c.line([(29, 15), (29, 49)], ink, 1.2)
        for lift in (-6, 6) if kind == "double" else (0,):
            c.line([(18, 32 + lift), (46, 32 + lift)], ink, 2.5)
            c.poly([(46, 32 + lift), (40, 28 + lift), (40, 36 + lift)], ink)
        if kind == "pierce":
            for x in (36, 42):
                c.ellipse((x - 3, 29, x + 3, 35), (176, 52, 44, 255))
    elif kind in ("cannon", "fire", "shock"):
        c.ellipse((20, 22, 44, 46), ink)
        c.ellipse((24, 26, 30, 32), (150, 150, 160, 255))
        if kind == "fire":
            c.poly([(32, 12), (40, 26), (34, 24), (32, 30), (28, 22), (24, 26)], (230, 110, 30, 255))
        if kind == "shock":
            for r in (20, 26):
                c.ellipse((32 - r, 34 - r, 32 + r, 34 + r), (120, 90, 50, 255), width=2)
    elif kind in ("frost", "freeze", "aura"):
        colour = (40, 110, 190, 255) if kind != "aura" else (120, 60, 180, 255)
        for degrees in (0, 60, 120):
            ray = math.radians(degrees)
            dx, dy = 15 * math.cos(ray), 15 * math.sin(ray)
            c.line([(32 - dx, 32 - dy), (32 + dx, 32 + dy)], colour, 3)
        if kind == "freeze":
            c.ellipse((24, 24, 40, 40), (200, 240, 255, 255))
        if kind == "aura":
            c.ellipse((13, 13, 51, 51), colour, width=2)
    elif kind == "upgrade":
        c.poly([(32, 14), (48, 32), (38, 32), (38, 48), (26, 48), (26, 32), (16, 32)], (60, 150, 70, 255))
    elif kind == "locked":
        c.ellipse((22, 14, 42, 38), (60, 60, 64, 255), width=4)
        c.poly([(18, 30), (46, 30), (46, 50), (18, 50)], (60, 60, 64, 255))
    else:
        c.ellipse((18, 18, 46, 46), (220, 176, 40, 255))
        c.ellipse((22, 22, 42, 42), (245, 205, 80, 255))
        c.line([(32, 24), (32, 40)], (150, 110, 20, 255), 3)
    return c.done()


def icon_sheet() -> Image.Image:
    """Every badge, 6 by row, in the order of ``ICONS``."""
    sheet = Image.new("RGBA", (64 * 6, 64 * 2), (0, 0, 0, 0))
    for index, kind in enumerate(ICONS):
        sheet.paste(icon(kind), ((index % 6) * 64, (index // 6) * 64))
    return sheet


def ring() -> Image.Image:
    """The range ring, 256 x 256: a ground circle laid flat by the ``ground`` layer, a thick white rim and a light
    fill."""
    c = Canvas(256, 256)
    c.ellipse((3, 3, 253, 253), (255, 255, 255, 40))
    c.ellipse((3, 3, 253, 253), (255, 255, 255, 235), width=5)
    return c.done()


def cursor() -> Image.Image:
    """The frame of the pad under the cursor, laid flat on the ground (64 x 64)."""
    c = Canvas(64, 64)
    c.ellipse((4, 4, 60, 60), (255, 230, 120, 255), width=4)
    for angle in range(0, 360, 90):
        a = math.radians(angle + 45)
        x, y = 32 + 30 * math.cos(a), 32 + 30 * math.sin(a)
        c.ellipse((x - 4, y - 4, x + 4, y + 4), (255, 230, 120, 255))
    return c.done()


def shadow() -> Image.Image:
    """The soft shadow under an enemy, laid flat on the ground."""
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    assert pixels is not None
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - 31.5, y - 31.5) / 32.0
            if d < 1.0:
                pixels[x, y] = (0, 0, 0, round(200 * min(1.0, (1.0 - d) * 2.0)))
    return img


def fx_projectiles() -> Image.Image:
    """Arrow (pointing right: the script turns it along its flight), cannon ball, fire shell, frost shard; 32 x 32."""
    sheet = Image.new("RGBA", (128, 32), (0, 0, 0, 0))
    arrow = Canvas(32, 32)
    arrow.line([(4, 16), (26, 16)], (130, 88, 44, 255), 2)
    arrow.poly([(30, 16), (24, 12), (24, 20)], (225, 225, 232, 255))
    arrow.poly([(4, 16), (1, 12), (8, 16), (1, 20)], (235, 235, 235, 255))
    ball = Canvas(32, 32)
    ball.ellipse((8, 8, 24, 24), (30, 30, 36, 255))
    ball.ellipse((11, 11, 16, 16), (130, 130, 140, 255))
    fire = Canvas(32, 32)
    fire.ellipse((3, 3, 29, 29), (255, 140, 40, 120))
    fire.ellipse((8, 8, 24, 24), (255, 190, 60, 255))
    fire.ellipse((12, 11, 20, 19), (255, 250, 200, 255))
    shard = Canvas(32, 32)
    shard.poly([(16, 2), (25, 16), (16, 30), (7, 16)], (150, 220, 255, 255))
    shard.poly([(16, 6), (20, 16), (16, 22)], (240, 252, 255, 255))
    for index, image in enumerate((arrow, ball, fire, shard)):
        sheet.paste(image.done(), (index * 32, 0))
    return sheet


def fx_fire() -> Image.Image:
    """Four frames of a flickering flame (torches, burning ground), 32 x 40 each."""
    sheet = Image.new("RGBA", (128, 40), (0, 0, 0, 0))
    rng = random.Random(5)
    for frame in range(4):
        c = Canvas(32, 40)
        for layer, (colour, width) in enumerate(
            (((230, 80, 20, 220), 12), ((255, 160, 40, 240), 8), ((255, 240, 170, 255), 4))
        ):
            sway = rng.uniform(-3, 3)
            top = 6 + layer * 7 + rng.uniform(-2, 2)
            c.poly([(16 - width, 38), (16 + sway, top), (16 + width, 38)], colour)
            c.ellipse((16 - width, 30, 16 + width, 39), colour)
        sheet.paste(c.done(), (frame * 32, 0))
    return sheet


def puff() -> Image.Image:
    """A soft white disc fading to the edge, tinted and scaled by the scripts (splash, death, frost)."""
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    assert pixels is not None
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - 31.5, y - 31.5) / 32.0
            if d < 1.0:
                pixels[x, y] = (255, 255, 255, round(255 * (1.0 - d) ** 1.5))
    return img


def bar() -> Image.Image:
    """The health bar: a white strip with a dark rim, tinted by the scripts."""
    img = Image.new("RGBA", (32, 6), (255, 255, 255, 255))
    ImageDraw.Draw(img).rectangle([0, 0, 31, 5], outline=(40, 40, 40, 255))
    return img


def textures() -> None:
    """Every image of the game."""
    for folder in ("ui", "fx", "enemies"):
        (TEXTURES / folder).mkdir(parents=True, exist_ok=True)
    for kind, model in creatures.CREATURES.items():
        creatures.sheet(model()).save(TEXTURES / "enemies" / f"{kind}.png")
    scenery.write_all(TEXTURES / "menu")
    icon_sheet().save(TEXTURES / "icons.png")
    ring().save(TEXTURES / "fx" / "ring.png")
    cursor().save(TEXTURES / "fx" / "cursor.png")
    shadow().save(TEXTURES / "fx" / "shadow.png")
    fx_projectiles().save(TEXTURES / "fx" / "projectiles.png")
    fx_fire().save(TEXTURES / "fx" / "fire.png")
    puff().save(TEXTURES / "puff.png")
    bar().save(TEXTURES / "bar.png")
    medieval_ui.write_all(TEXTURES, ROOT / "fonts")
    world_map_art.write_all(TEXTURES / "ui", ROOT / "fonts")


# Sounds ---------------------------------------------------------------------------------------------------------------


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
    """A tone of a varying frequency, an exponential decay and a share of square wave."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * frequency(t) / RATE
        sine = math.sin(phase)
        value = (1.0 - square) * sine + square * (1.0 if sine >= 0.0 else -1.0)
        out.append(value * min(1.0, t / 0.004) * math.exp(-t * decay))
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


def _at(seconds: float, track: list[float]) -> list[float]:
    """A track delayed by ``seconds``."""
    return [0.0] * round(seconds * RATE) + track


def _constant(frequency: float) -> Callable[[float], float]:
    return lambda _t: frequency


def _horn() -> list[float]:
    """The wave horn: a low brass-like tone swelling then fading, 0.9 s."""
    out = []
    for i in range(round(0.9 * RATE)):
        t = i / RATE
        envelope = min(1.0, t / 0.15) * min(1.0, (0.9 - t) / 0.3)
        f = 147.0 * (1.0 + 0.01 * math.sin(2 * math.pi * 5 * t))
        value = sum(math.sin(2 * math.pi * f * k * t) / k for k in (1, 2, 3, 4))
        out.append(value * envelope)
    return out


def _bell(seconds: float, frequency: float) -> list[float]:
    """A soft bell: a sine and a quiet overtone."""
    out = []
    for i in range(round(seconds * RATE)):
        t = i / RATE
        value = math.sin(2 * math.pi * frequency * t) + 0.3 * math.sin(2 * math.pi * frequency * 2.76 * t)
        out.append(value * min(1.0, t / 0.003) * math.exp(-t * 5.0))
    return out


def sounds() -> None:
    """Every sound of the game."""
    knock = _mix(_noise(0.12, 30.0, 1, smooth=0.9), _tone(0.12, lambda t: 120.0 - 200.0 * t, 25.0))
    _write("game/build.wav", _mix(knock, _at(0.1, knock)), peak=0.6)
    coins = _mix(*(_at(0.05 * i, _tone(0.12, _constant(1800.0 + 300.0 * i), 30.0)) for i in range(4)))
    _write("game/sell.wav", coins, peak=0.4)
    _write("game/coins.wav", _mix(coins, _at(0.12, coins)), peak=0.45)
    twang = _mix(_tone(0.12, lambda t: 520.0 - 900.0 * t, 28.0, square=0.15), _noise(0.05, 80.0, 2, smooth=0.3))
    _write("game/arrow.wav", twang, peak=0.35)
    _write("game/cannon.wav", _mix(_noise(0.35, 12.0, 3, smooth=0.93), _tone(0.3, lambda t: 70.0 - 40.0 * t, 10.0)))
    _write("game/boom.wav", _noise(0.4, 9.0, 4, smooth=0.88), peak=0.6)
    shimmer = _mix(*(_at(0.03 * i, _tone(0.2, _constant(1200.0 + 400.0 * i), 14.0)) for i in range(4)))
    _write("game/frost.wav", shimmer, peak=0.3)
    _write("game/hit.wav", _noise(0.05, 70.0, 5, smooth=0.7), peak=0.3)
    _write("game/pop.wav", _tone(0.1, lambda t: 300.0 + 1200.0 * t, 25.0, square=0.2), peak=0.35)
    _write("game/leak.wav", _mix(_noise(0.3, 14.0, 6, smooth=0.95), _tone(0.3, lambda t: 90.0 - 50.0 * t, 9.0)))
    _write("game/horn.wav", _horn(), peak=0.5)
    _write("game/defeat.wav", _mix(_bell(1.2, 220.0), _at(0.3, _bell(1.2, 185.0)), _at(0.6, _bell(1.4, 147.0))))
    _write("ui/click.wav", _tone(0.05, _constant(900.0), 60.0, square=0.3), peak=0.3)
    _write("ui/start.wav", _mix(_bell(0.6, 392.0), _at(0.15, _bell(0.6, 494.0)), _at(0.3, _bell(1.0, 587.0))))
    shutil.copyfile(DEMO / "sounds" / "ui" / "victory.wav", SOUNDS / "ui" / "victory.wav")


if __name__ == "__main__":
    textures()
    sounds()
