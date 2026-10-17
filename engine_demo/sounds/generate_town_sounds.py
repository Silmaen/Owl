#!/usr/bin/env python3
"""
Synthesise the sounds of the isometric town into ``sounds/town/`` (standard library only, deterministic).

The Kenney audio packs could not be downloaded from the build machine (kenney.nl answers 403), so the town's sounds are
drawn here: sine and noise, mono 16 bit at 22.05 kHz (mono, so that a spatial source is positioned by the sound
system). The loops (engine, ambience, waves, yard) repeat without a click.

Run from anywhere: ``python3 engine_demo/sounds/generate_town_sounds.py``.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
"""Samples per second."""
OUT = Path(__file__).resolve().parent / "town"
"""Folder of the written files."""


def _write(name: str, samples: list[float], peak: float = 0.85) -> None:
    """Write the samples, scaled so that the loudest one reaches ``peak`` of the full scale."""
    OUT.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(OUT / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def _count(seconds: float) -> int:
    return round(seconds * RATE)


def _noise(rng: random.Random, count: int, smooth: float) -> list[float]:
    """White noise through a one-pole low-pass (``smooth`` close to 1 is darker)."""
    out, last = [], 0.0
    for _ in range(count):
        last = smooth * last + (1.0 - smooth) * rng.uniform(-1.0, 1.0)
        out.append(last)
    return out


def _loopable(samples: list[float], overlap: int) -> list[float]:
    """Make a seamless loop: the last ``overlap`` samples are faded into the first ones and dropped."""
    body, tail = samples[:-overlap], samples[-overlap:]
    for i in range(overlap):
        fade = i / overlap
        body[i] = body[i] * fade + tail[i] * (1.0 - fade)
    return body


def engine() -> list[float]:
    """One second of a low engine: a fundamental of 55 Hz, its harmonics and a 11 Hz putter."""
    out = []
    for i in range(_count(1.0)):
        t = i / RATE
        tone = sum(math.sin(2 * math.pi * 55 * k * t + k) / k for k in range(1, 7))
        putter = 0.65 + 0.35 * math.sin(2 * math.pi * 11 * t)
        out.append(tone * putter)
    return out


def ambience() -> list[float]:
    """Eight seconds of town air: wind, a distant hum and a slow swell."""
    rng = random.Random(1)
    count = _count(8.0) + _count(0.6)
    wind = _noise(rng, count, 0.96)
    out = []
    for i in range(count):
        t = i / RATE
        swell = 0.6 + 0.4 * math.sin(2 * math.pi * t / 8.0)
        hum = 0.05 * math.sin(2 * math.pi * 110 * t) + 0.03 * math.sin(2 * math.pi * 165 * t + 1.0)
        out.append(wind[i] * 4.0 * swell + hum)
    return _loopable(out, _count(0.6))


def waves() -> list[float]:
    """Ten seconds of surf: noise swelling twice."""
    rng = random.Random(2)
    count = _count(10.0) + _count(0.8)
    body = _noise(rng, count, 0.9)
    out = []
    for i in range(count):
        t = i / RATE
        swell = math.sin(math.pi * ((t / 5.0) % 1.0)) ** 2
        out.append(body[i] * (0.15 + swell))
    return _loopable(out, _count(0.8))


def yard() -> list[float]:
    """Six seconds of an industrial yard: a machine hum and clanks."""
    rng = random.Random(3)
    count = _count(6.0) + _count(0.4)
    rumble = _noise(rng, count, 0.97)
    clanks = [(0.9, 760.0), (2.4, 540.0), (3.1, 880.0), (4.8, 620.0)]
    out = []
    for i in range(count):
        t = i / RATE
        hum = 0.25 * math.sin(2 * math.pi * 80 * t) * (0.7 + 0.3 * math.sin(2 * math.pi * 3 * t))
        clank = sum(
            math.exp(-(t - at) * 14.0) * math.sin(2 * math.pi * freq * (t - at)) for at, freq in clanks if t >= at
        )
        out.append(rumble[i] * 2.5 + hum + 0.5 * clank)
    return _loopable(out, _count(0.4))


def horn() -> list[float]:
    """A short two-tone car horn."""
    out = []
    for i in range(_count(0.45)):
        t = i / RATE
        env = min(1.0, t / 0.02) * min(1.0, (0.45 - t) / 0.08)
        tone = sum(math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * 2 * f * t) for f in (349.0, 440.0))
        out.append(tone * env)
    return out


def cash() -> list[float]:
    """A coin: two quick rising bell tones."""
    out = []
    for i in range(_count(0.45)):
        t = i / RATE
        first = math.exp(-t * 14.0) * math.sin(2 * math.pi * 1318.5 * t)
        later = max(0.0, t - 0.09)
        second = math.exp(-later * 11.0) * math.sin(2 * math.pi * 1975.5 * later) if t >= 0.09 else 0.0
        out.append(first + second + 0.2 * math.sin(2 * math.pi * 3951 * t) * math.exp(-t * 30.0))
    return out


def click_build() -> list[float]:
    """A short thump with a click: a road laid."""
    rng = random.Random(4)
    out = []
    for i in range(_count(0.12)):
        t = i / RATE
        freq = 420.0 - 2200.0 * t
        out.append(
            math.exp(-t * 34.0) * math.sin(2 * math.pi * freq * t) + 0.25 * math.exp(-t * 90.0) * rng.uniform(-1, 1)
        )
    return out


def click_bulldoze() -> list[float]:
    """A crunch: a low thump and a burst of dark noise."""
    rng = random.Random(5)
    grit = _noise(rng, _count(0.3), 0.55)
    out = []
    for i in range(_count(0.3)):
        t = i / RATE
        out.append(math.exp(-t * 11.0) * grit[i] * 2.2 + math.exp(-t * 22.0) * math.sin(2 * math.pi * 95 * t))
    return out


def error() -> list[float]:
    """A low buzz: the tool refused."""
    out = []
    for i in range(_count(0.22)):
        t = i / RATE
        square = 1.0 if math.sin(2 * math.pi * 140 * t) >= 0 else -1.0
        out.append(square * min(1.0, t / 0.01) * math.exp(-t * 9.0))
    return out


def bell() -> list[float]:
    """A church bell: inharmonic partials ringing out."""
    partials = [
        (0.5, 1.0, 1.1),
        (1.0, 0.7, 1.5),
        (1.19, 0.5, 1.8),
        (1.56, 0.35, 2.4),
        (2.0, 0.3, 2.8),
        (2.74, 0.2, 3.5),
    ]
    base = 392.0
    out = []
    for i in range(_count(3.2)):
        t = i / RATE
        out.append(sum(a * math.exp(-t * d) * math.sin(2 * math.pi * base * r * t) for r, a, d in partials))
    return out


def chirp() -> list[float]:
    """Two quick bird chirps."""
    out = []
    for i in range(_count(0.35)):
        t = i / RATE
        local = t % 0.14 if t < 0.28 else 0.2
        freq = 3000.0 + 7000.0 * local
        env = math.sin(math.pi * min(1.0, local / 0.1)) if t < 0.28 else 0.0
        out.append(env * math.sin(2 * math.pi * freq * local))
    return out


def main() -> None:
    """Write every sound of the town."""
    _write("engine.wav", engine(), 0.7)
    _write("ambience.wav", ambience(), 0.7)
    _write("waves.wav", waves(), 0.7)
    _write("yard.wav", yard(), 0.7)
    _write("horn.wav", horn(), 0.7)
    _write("cash.wav", cash(), 0.7)
    _write("click_build.wav", click_build(), 0.8)
    _write("click_bulldoze.wav", click_bulldoze(), 0.8)
    _write("error.wav", error(), 0.6)
    _write("bell.wav", bell(), 0.7)
    _write("chirp.wav", chirp(), 0.5)
    for path in sorted(OUT.glob("*.wav")):
        print(path)


if __name__ == "__main__":
    main()
