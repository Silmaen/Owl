#!/usr/bin/env python3
"""
Synthesise the sounds of the Engine Demo sound yard into ``sounds/yard/`` (standard library only, deterministic).

Three music layers of the same length (pads, pulse, drums) so that they loop in sync, two stingers, and the mono
loops of the yard: drips in the cave, bubbles in the pool, the cart engine (Doppler) and the radio behind the wall.
Mono 16 bit at 22.05 kHz, so that a spatial source is positioned by the sound system.

Run from anywhere: ``python3 tools/demo/generate_yard_sounds.py``.
"""

from __future__ import annotations

import hashlib
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
"""Samples per second."""
ROOT = Path(__file__).resolve().parents[2] / "engine_demo"
"""Engine Demo project."""
OUT = ROOT / "sounds" / "yard"
"""Folder of the written files."""
BAR = 8.0
"""Length of the music layers, in seconds (four bars at 120 bpm)."""


def _write(name: str, samples: list[float], peak: float = 0.8) -> None:
    """Write the samples, scaled so that the loudest reaches ``peak``, and the sidecar with a stable GUID."""
    OUT.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(OUT / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)
    meta = OUT / f"{name}.owlmeta"
    if not meta.exists():
        guid = hashlib.sha1(f"sounds/yard/{name}".encode()).hexdigest()[:16]
        meta.write_text(f"FormatVersion: 1\nGuid: {guid}\nImporter: Sound\n", encoding="utf-8")


def _count(seconds: float) -> int:
    return round(seconds * RATE)


def _note(frequency: float, seconds: float, attack: float = 0.02, release: float = 0.2) -> list[float]:
    """A soft sine note with a short attack and release."""
    count = _count(seconds)
    out = []
    for i in range(count):
        t = i / RATE
        env = min(1.0, t / attack) * min(1.0, (seconds - t) / release)
        out.append(math.sin(2 * math.pi * frequency * t) * max(0.0, env))
    return out


def _mix_into(target: list[float], source: list[float], start: int, gain: float = 1.0) -> None:
    for i, value in enumerate(source):
        if start + i < len(target):
            target[(start + i) % len(target)] += value * gain


def pads() -> list[float]:
    """Calm: two slow chords (A minor, F major), each held two bars."""
    out = [0.0] * _count(BAR)
    for index, chord in enumerate(([220.0, 261.63, 329.63], [174.61, 220.0, 261.63])):
        for frequency in chord:
            _mix_into(out, _note(frequency, BAR / 2, attack=0.8, release=0.8), _count(index * BAR / 2), 0.3)
    return out


def pulse() -> list[float]:
    """Danger: a bass pulse in eighth notes on A, then F."""
    out = [0.0] * _count(BAR)
    for step in range(32):
        frequency = 110.0 if step < 16 else 87.31
        note = [math.tanh(2.5 * s) for s in _note(frequency, 0.2, attack=0.005, release=0.1)]
        _mix_into(out, note, _count(step * BAR / 32), 0.6)
    return out


def drums() -> list[float]:
    """Combat: kick on the beats, noise snare on two and four, hats in eighths."""
    rng = random.Random(7)
    out = [0.0] * _count(BAR)
    beat = BAR / 16
    for step in range(16):
        start = _count(step * beat)
        kick = [math.sin(2 * math.pi * (60 + 90 * math.exp(-i / 400)) * i / RATE) * math.exp(-i / 2500)
                for i in range(_count(0.25))]
        _mix_into(out, kick, start, 0.9)
        if step % 2 == 1:
            snare = [rng.uniform(-1, 1) * math.exp(-i / 1500) for i in range(_count(0.2))]
            _mix_into(out, snare, start, 0.5)
        for half in (0, 1):
            hat = [rng.uniform(-1, 1) * math.exp(-i / 200) for i in range(_count(0.05))]
            _mix_into(out, hat, start + _count(half * beat / 2), 0.2)
    return out


def stinger(frequencies: list[float]) -> list[float]:
    """A rising arpeggio ending on a held note."""
    out = [0.0] * _count(1.6)
    for index, frequency in enumerate(frequencies):
        _mix_into(out, _note(frequency, 1.6 - index * 0.12, attack=0.01, release=0.5), _count(index * 0.12), 0.4)
    return out


def drips() -> list[float]:
    """Water drops falling at random, short and bright: the cave reverb makes them ring."""
    rng = random.Random(11)
    out = [0.0] * _count(4.0)
    for _ in range(9):
        frequency = rng.uniform(900, 1600)
        drop = [math.sin(2 * math.pi * frequency * (1 + 0.6 * i / 800) * i / RATE) * math.exp(-i / 350)
                for i in range(_count(0.08))]
        _mix_into(out, drop, rng.randrange(len(out) - len(drop)), rng.uniform(0.5, 1.0))
    return out


def bubbles() -> list[float]:
    """Bubbles rising: short upward chirps."""
    rng = random.Random(13)
    out = [0.0] * _count(4.0)
    for _ in range(26):
        base = rng.uniform(300, 700)
        chirp = [math.sin(2 * math.pi * base * (1 + i / 600) * i / RATE) * math.sin(math.pi * i / _count(0.06))
                 for i in range(_count(0.06))]
        _mix_into(out, chirp, rng.randrange(len(out) - len(chirp)), rng.uniform(0.3, 0.8))
    return out


def engine() -> list[float]:
    """The cart engine: a buzzing tone, rich in harmonics so the Doppler shift is easy to hear."""
    out = []
    for i in range(_count(2.0)):
        t = i / RATE
        value = sum(math.sin(2 * math.pi * 120.0 * k * t) / k for k in range(1, 7))
        out.append(value * (0.8 + 0.2 * math.sin(2 * math.pi * 8.0 * t)))
    return out


def radio() -> list[float]:
    """A tune on the radio: a square-ish melody, bright, so the wall filter muffles it clearly."""
    melody = [523.25, 659.25, 783.99, 659.25, 587.33, 698.46, 880.0, 698.46]
    out = []
    for frequency in melody:
        for i in range(_count(0.5)):
            t = i / RATE
            value = math.tanh(4 * math.sin(2 * math.pi * frequency * t)) * min(1.0, (0.5 - t) / 0.05)
            out.append(value)
    return out


def main() -> None:
    _write("music_pads.wav", pads())
    _write("music_pulse.wav", pulse())
    _write("music_drums.wav", drums())
    _write("stinger_danger.wav", stinger([220.0, 261.63, 311.13, 369.99]))
    _write("stinger_combat.wav", stinger([220.0, 329.63, 440.0, 659.25]))
    _write("drips.wav", drips())
    _write("bubbles.wav", bubbles())
    _write("cart_engine.wav", engine())
    _write("radio.wav", radio())


if __name__ == "__main__":
    main()
