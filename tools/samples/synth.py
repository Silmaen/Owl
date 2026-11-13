"""
Sound synthesis shared by the generators of the sample games: tones, noise, envelopes, note sequences, pads and the
writing of mono 16 bit WAV files (22.05 kHz), deterministic so that a generator run twice writes the same files.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from collections.abc import Callable, Sequence
from pathlib import Path

RATE = 22050
"""Samples per second."""

Samples = list[float]


def write(path: Path, samples: Samples, peak: float = 0.7) -> None:
    """Write the samples, scaled so that the loudest one reaches ``peak`` of the full scale."""
    path.parent.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def constant(frequency: float) -> Callable[[float], float]:
    """A frequency that does not change."""
    return lambda _t: frequency


def tone(
    seconds: float,
    frequency: Callable[[float], float],
    decay: float,
    attack: float = 0.004,
    harmonics: tuple[float, ...] = (1.0,),
) -> Samples:
    """A tone of a varying frequency with a few harmonics, a short attack and an exponential decay."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * frequency(t) / RATE
        value = sum(weight * math.sin(phase * (k + 1)) for k, weight in enumerate(harmonics))
        out.append(value * min(1.0, t / attack) * math.exp(-t * decay))
    return out


def square(seconds: float, frequency: Callable[[float], float], decay: float, duty: float = 0.5) -> Samples:
    """A soft square wave (chip sound), decaying."""
    out, phase = [], 0.0
    for i in range(round(seconds * RATE)):
        t = i / RATE
        phase = (phase + frequency(t) / RATE) % 1.0
        out.append((0.6 if phase < duty else -0.6) * min(1.0, t / 0.003) * math.exp(-t * decay))
    return lowpass(out, 0.5)


def noise(seconds: float, decay: float, seed: int, smooth: float) -> Samples:
    """White noise through a one-pole low-pass (``smooth`` near 1 is darker), decaying."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(round(seconds * RATE)):
        last = smooth * last + (1.0 - smooth) * rng.uniform(-1.0, 1.0)
        out.append(last * math.exp(-i / RATE * decay))
    return out


def lowpass(samples: Samples, smooth: float) -> Samples:
    """A one-pole low-pass."""
    out, last = [], 0.0
    for value in samples:
        last = smooth * last + (1.0 - smooth) * value
        out.append(last)
    return out


def mix(*tracks: tuple[Samples, float]) -> Samples:
    """Tracks added with their gains, as long as the longest."""
    length = max(len(t) for t, _ in tracks)
    return [sum(t[i] * gain for t, gain in tracks if i < len(t)) for i in range(length)]


def fade(samples: Samples, fade_in: float, fade_out: float) -> Samples:
    """Linear fades at both ends."""
    first, last = max(1, round(fade_in * RATE)), max(1, round(fade_out * RATE))
    count = len(samples)
    return [s * min(1.0, i / first, (count - i) / last) for i, s in enumerate(samples)]


def loop_seamless(samples: Samples, seconds: float = 0.05) -> Samples:
    """A loop whose end is cross-faded into its start, so that it repeats without a click."""
    count = round(seconds * RATE)
    head, body = samples[:count], samples[count:]
    for i in range(count):
        alpha = i / count
        body[len(body) - count + i] = body[len(body) - count + i] * (1 - alpha) + head[i] * alpha
    return body


def note(
    frequency: float, seconds: float, decay: float = 3.0, harmonics: tuple[float, ...] = (1.0, 0.25, 0.08)
) -> Samples:
    """A plucked note."""
    return tone(seconds, constant(frequency), decay, 0.01, harmonics)


def sequence(
    notes: Sequence[tuple[float, float]],
    step: float,
    decay: float = 4.0,
    harmonics: tuple[float, ...] = (1.0, 0.25, 0.08),
    tail: float = 0.4,
) -> Samples:
    """Notes (frequency, length in steps; 0 Hz is a rest) one after the other, each ringing a little past its end."""
    total = sum(length for _, length in notes) * step
    out = [0.0] * round((total + tail) * RATE)
    cursor = 0.0
    for frequency, length in notes:
        if frequency > 0:
            start = round(cursor * RATE)
            for i, value in enumerate(note(frequency, step * length + 0.3, decay, harmonics)):
                if start + i < len(out):
                    out[start + i] += value
        cursor += step * length
    return out


def pad(chords: list[list[float]], seconds: float, detune: float = 2.003) -> Samples:
    """Slow chords, each fading in and out over its length."""
    each = seconds / len(chords)
    count = round(each * RATE)
    out: Samples = []
    for chord in chords:
        for i in range(count):
            t = i / RATE
            envelope = math.sin(math.pi * i / count) ** 0.6
            value = sum(math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * f * detune * t) for f in chord)
            out.append(value * envelope / len(chord))
    return out


def pulse(roots: list[float], seconds: float, beat: float, decay: float = 9.0) -> Samples:
    """A soft pulse on a root per bar, ``len(roots)`` bars over ``seconds``."""
    out: Samples = []
    beats = round(seconds / beat)
    for k in range(beats):
        root = roots[k * len(roots) // beats]
        out += tone(beat, constant(root), decay, 0.003, (1.0, 0.5, 0.25))
    return out


def drums(seconds: float, beat: float, pattern: str, seed: int) -> Samples:
    """A drum loop: ``k`` kick, ``s`` snare, ``h`` hat, ``.`` rest, one letter per beat, repeated."""
    out = [0.0] * round(seconds * RATE)
    kick = tone(0.25, lambda t: 110 - 300 * t if t < 0.2 else 50, 16.0, 0.002)
    snare = mix((noise(0.2, 18.0, seed, 0.5), 0.8), (tone(0.15, constant(190.0), 20.0), 0.4))
    hat = noise(0.05, 60.0, seed + 1, 0.1)
    sounds = {"k": (kick, 0.9), "s": (snare, 0.5), "h": (hat, 0.25)}
    for k in range(round(seconds / beat)):
        letter = pattern[k % len(pattern)]
        if letter in sounds:
            samples, gain = sounds[letter]
            start = round(k * beat * RATE)
            for i, value in enumerate(samples):
                if start + i < len(out):
                    out[start + i] += value * gain
    return out


def jingle(notes: Sequence[tuple[float, float]], step: float = 0.1) -> Samples:
    """A short bright tune (a start or victory jingle)."""
    return mix(
        (sequence(notes, step, 3.5, (1.0, 0.35, 0.12)), 1.0),
        (sequence([(f * 2, length) for f, length in notes], step, 6.0, (1.0,)), 0.15),
    )


def hum(seconds: float, frequency: float, seed: int) -> Samples:
    """A steady engine or wind hum: low noise and a low tone, for a loop."""
    base = tone(seconds, constant(frequency), 0.0, 0.05, (1.0, 0.5, 0.3))
    rumble = noise(seconds, 0.0, seed, 0.97)
    return loop_seamless(mix((base, 0.3), (rumble, 1.0)))
