#!/usr/bin/env python3
"""Synthesise the hub and raycast sound effects of the Engine Demo (no download: the Kenney audio packs were not reachable).

Everything is built from sine / noise generators with the standard library only (``wave`` + ``math`` + ``random``),
mono 16-bit 22 050 Hz, seeded so that the files are reproducible. Run with ``python3 generate_sounds.py`` from anywhere;
the files are written next to the script.

Hub: ``hub_ambience`` (wind and birds, loops), ``hub_water`` (river / fountain, loops), ``hub_step_1`` / ``hub_step_2``,
``hub_door``, ``hub_board``, ``hub_lever``, ``hub_gate``, ``hub_portal_hum`` (retriggered while the portal is awake),
``hub_portal_wake``, ``hub_bell``.
Raycast: ``raycast_ambience`` (loops), ``raycast_door``, ``raycast_pushwall``, ``raycast_treasure``, ``raycast_food``,
``raycast_ammo``, ``raycast_elevator``.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

HERE = Path(__file__).parent
RATE = 22050
TAU = 2 * math.pi


def seconds(duration: float) -> int:
    return int(duration * RATE)


def noise(rng: random.Random, count: int) -> list[float]:
    return [rng.uniform(-1.0, 1.0) for _ in range(count)]


def lowpass(samples: list[float], cutoff: float) -> list[float]:
    """One-pole low-pass filter."""
    alpha = 1 - math.exp(-TAU * cutoff / RATE)
    out, state = [], 0.0
    for value in samples:
        state += alpha * (value - state)
        out.append(state)
    return out


def highpass(samples: list[float], cutoff: float) -> list[float]:
    return [a - b for a, b in zip(samples, lowpass(samples, cutoff))]


def bandpass(samples: list[float], low: float, high: float) -> list[float]:
    return lowpass(highpass(samples, low), high)


def envelope(count: int, attack: float, release: float) -> list[float]:
    """Linear attack, exponential release (both in seconds)."""
    out = []
    for i in range(count):
        t = i / RATE
        gain = min(1.0, t / attack) if attack > 0 else 1.0
        out.append(gain * math.exp(-t / release) if release > 0 else gain)
    return out


def tone(freq: float, count: int, harmonics: tuple[float, ...] = (1.0,), start: float = 0.0) -> list[float]:
    return [sum(amp * math.sin(TAU * freq * (k + 1) * (i / RATE) + start) for k, amp in enumerate(harmonics))
            for i in range(count)]


def mix(*tracks: list[float]) -> list[float]:
    length = max(len(track) for track in tracks)
    return [sum(track[i] for track in tracks if i < len(track)) for i in range(length)]


def scale(samples: list[float], gain: float) -> list[float]:
    return [value * gain for value in samples]


def shaped(samples: list[float], env: list[float]) -> list[float]:
    return [a * b for a, b in zip(samples, env)]


def place(track: list[float], at: float, clip: list[float]) -> None:
    """Add ``clip`` into ``track`` from ``at`` seconds on (wrapping, so that loops stay seamless)."""
    start = int(at * RATE)
    for i, value in enumerate(clip):
        track[(start + i) % len(track)] += value


def loopable(samples: list[float], fade: float = 0.25) -> list[float]:
    """Cross-fade the end into the beginning so that the file loops without a click."""
    n = int(fade * RATE)
    body = samples[: len(samples) - n]
    for i in range(n):
        mixing = i / n
        body[i] = body[i] * mixing + samples[len(samples) - n + i] * (1 - mixing)
    return body


def save(name: str, samples: list[float], peak: float = 0.8) -> None:
    top = max(abs(value) for value in samples) or 1.0
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, value / top * peak)) * 32767)) for value in samples)
    with wave.open(str(HERE / f"{name}.wav"), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(frames)


def chirp(rng: random.Random, base: float) -> list[float]:
    """A bird call: two or three rapid rising or falling notes."""
    out: list[float] = []
    for _ in range(rng.choice((2, 3, 3, 4))):
        length = seconds(rng.uniform(0.05, 0.1))
        start, end = base * rng.uniform(0.9, 1.1), base * rng.uniform(1.25, 1.7) * rng.choice((1, 1, 0.7))
        phase, note = 0.0, []
        for i in range(length):
            freq = start + (end - start) * (i / length)
            phase += TAU * freq / RATE
            note.append(math.sin(phase) * math.sin(math.pi * i / length) ** 1.5)
        out += note + [0.0] * seconds(0.04)
    return out


def hub_ambience() -> None:
    rng = random.Random(11)
    count = seconds(10.0)
    wind = lowpass(noise(rng, count), 380)
    wind = [w * (0.55 + 0.45 * math.sin(TAU * 2 * i / count + 1.0) * math.sin(TAU * 3 * i / count)) for i, w in enumerate(wind)]
    track = scale(wind, 2.4)
    for at, base in ((0.6, 3300), (1.9, 2700), (3.0, 3900), (4.4, 3100), (5.1, 3500), (6.8, 2900), (7.9, 3700), (8.8, 3200)):
        place(track, at, scale(chirp(rng, base), 0.28 if base < 3600 else 0.2))
    save("hub_ambience", loopable(track), 0.55)


def hub_water() -> None:
    rng = random.Random(21)
    count = seconds(5.0)
    flow = bandpass(noise(rng, count), 300, 2600)
    burble = [f * (0.6 + 0.4 * math.sin(TAU * 7 * i / count) * math.sin(TAU * 11 * i / count + 2)) for i, f in enumerate(flow)]
    drops = [0.0] * count
    for _ in range(26):
        freq = rng.uniform(900, 2200)
        place(drops, rng.uniform(0, 5.0), scale(shaped(tone(freq, seconds(0.07)), envelope(seconds(0.07), 0.002, 0.02)), 0.35))
    save("hub_water", loopable(mix(scale(burble, 1.0), drops), 0.3), 0.5)


def step(seed: int, pitch: float) -> None:
    rng = random.Random(seed)
    count = seconds(0.11)
    scuff = shaped(lowpass(noise(rng, count), 1600 * pitch), envelope(count, 0.003, 0.025))
    thud = shaped(tone(95 * pitch, count), envelope(count, 0.002, 0.03))
    save(f"hub_step_{seed}", mix(scuff, scale(thud, 0.9)), 0.5)


def hub_door() -> None:
    rng = random.Random(31)
    creak_len = seconds(0.35)
    creak = []
    phase = 0.0
    for i in range(creak_len):
        freq = 180 + 140 * math.sin(math.pi * i / creak_len) + 25 * math.sin(TAU * 40 * i / RATE)
        phase += TAU * freq / RATE
        creak.append((math.sin(phase) + 0.4 * math.sin(2.02 * phase)) * math.sin(math.pi * i / creak_len))
    thud_len = seconds(0.3)
    thud = shaped(mix(tone(70, thud_len), lowpass(noise(rng, thud_len), 500)), envelope(thud_len, 0.002, 0.06))
    track = [0.0] * seconds(0.7)
    place(track, 0.0, scale(creak, 0.35))
    place(track, 0.32, scale(thud, 1.0))
    save("hub_door", track, 0.7)


def hub_board() -> None:
    rng = random.Random(41)
    track = [0.0] * seconds(0.4)
    place(track, 0.0, shaped(mix(tone(220, seconds(0.08)), lowpass(noise(rng, seconds(0.08)), 2500)), envelope(seconds(0.08), 0.001, 0.02)))
    place(track, 0.06, scale(shaped(highpass(noise(rng, seconds(0.3)), 1500), envelope(seconds(0.3), 0.04, 0.08)), 0.35))
    save("hub_board", track, 0.6)


def hub_lever() -> None:
    rng = random.Random(51)
    track = [0.0] * seconds(0.6)
    ratchet = [0.0] * seconds(0.3)
    for k in range(5):
        place(ratchet, k * 0.05, shaped(highpass(noise(rng, seconds(0.03)), 2200), envelope(seconds(0.03), 0.001, 0.008)))
    place(track, 0.0, scale(ratchet, 0.6))
    clunk = shaped(mix(tone(140, seconds(0.3), (1.0, 0.5, 0.3)), tone(910, seconds(0.3))), envelope(seconds(0.3), 0.001, 0.07))
    place(track, 0.3, clunk)
    save("hub_lever", track, 0.7)


def hub_gate() -> None:
    rng = random.Random(61)
    count = seconds(1.4)
    rumble = shaped(lowpass(noise(rng, count), 260), [math.sin(math.pi * i / count) ** 0.7 for i in range(count)])
    grind = shaped(bandpass(noise(rng, count), 400, 1400), [0.35 * math.sin(math.pi * i / count) * (0.6 + 0.4 * math.sin(TAU * 18 * i / RATE)) for i in range(count)])
    save("hub_gate", mix(scale(rumble, 3.0), grind), 0.7)


def hub_portal_hum() -> None:
    count = seconds(2.4)
    base = 110.0
    hum = [0.0] * count
    for k, amp in enumerate((1.0, 0.55, 0.35, 0.2, 0.12)):
        for i in range(count):
            wobble = 1 + 0.004 * math.sin(TAU * 5 * i / count)
            hum[i] += amp * math.sin(TAU * base * (k + 1) * wobble * (i / RATE))
    shimmer = [0.18 * math.sin(TAU * (880 + 220 * math.sin(TAU * 2 * i / count)) * i / RATE) * math.sin(TAU * 3 * i / count) ** 2 for i in range(count)]
    track = mix(hum, shimmer)
    save("hub_portal_hum", loopable(track, 0.2), 0.45)


def hub_portal_wake() -> None:
    count = seconds(1.6)
    rise = []
    phase = 0.0
    for i in range(count):
        phase += TAU * (180 + 900 * (i / count) ** 2) / RATE
        rise.append(math.sin(phase) * math.sin(math.pi * i / count) ** 0.6)
    sparkle = [0.0] * count
    for k in range(10):
        place(sparkle, 0.5 + k * 0.08, scale(shaped(tone(1600 + 260 * k, seconds(0.2)), envelope(seconds(0.2), 0.002, 0.06)), 0.3))
    save("hub_portal_wake", mix(rise, sparkle), 0.7)


def hub_bell() -> None:
    count = seconds(3.2)
    track = [0.0] * count
    for freq, amp, decay in ((392, 1.0, 1.1), (784, 0.55, 0.8), (1176, 0.35, 0.5), (1570, 0.2, 0.35), (980, 0.25, 0.9)):
        track = mix(track, scale(shaped(tone(freq, count), envelope(count, 0.002, decay)), amp))
    save("hub_bell", track, 0.55)


def raycast_ambience() -> None:
    rng = random.Random(71)
    count = seconds(12.0)
    drone = [0.0] * count
    for freq, amp in ((55, 1.0), (82.4, 0.5), (110.3, 0.3)):
        for i in range(count):
            drone[i] += amp * math.sin(TAU * round(freq * 12) / 12 * (i / RATE))
    draft = lowpass(noise(rng, count), 220)
    draft = [d * (0.5 + 0.5 * math.sin(TAU * 3 * i / count)) for i, d in enumerate(draft)]
    track = mix(scale(drone, 0.12), scale(draft, 2.0))
    for at in (1.7, 4.2, 6.9, 9.5, 11.0):
        freq = rng.uniform(1100, 1700)
        place(track, at, scale(shaped(tone(freq, seconds(0.12)), envelope(seconds(0.12), 0.002, 0.03)), 0.12))
    save("raycast_ambience", loopable(track, 0.5), 0.5)


def raycast_door() -> None:
    rng = random.Random(81)
    count = seconds(0.9)
    slide = shaped(bandpass(noise(rng, count), 250, 1800), [math.sin(math.pi * i / count) ** 0.8 * 0.5 for i in range(count)])
    motor = []
    phase = 0.0
    for i in range(count):
        phase += TAU * (95 + 35 * math.sin(math.pi * i / count)) / RATE
        motor.append(math.sin(phase) * math.sin(math.pi * i / count) * 0.4)
    clunk = shaped(mix(tone(90, seconds(0.15)), lowpass(noise(rng, seconds(0.15)), 600)), envelope(seconds(0.15), 0.001, 0.04))
    track = mix(slide, motor)
    place(track, 0.0, scale(clunk, 0.8))
    save("raycast_door", track, 0.7)


def raycast_pushwall() -> None:
    rng = random.Random(91)
    count = seconds(2.4)
    grind = shaped(lowpass(noise(rng, count), 340), [(0.3 + 0.7 * math.sin(math.pi * i / count)) * (0.7 + 0.3 * math.sin(TAU * 9 * i / RATE)) for i in range(count)])
    gravel = shaped(bandpass(noise(rng, count), 500, 2200), [0.3 * math.sin(math.pi * i / count) * (rng.random() < 0.5) for i in range(count)])
    low = [0.0] * count
    for i in range(count):
        low[i] = 0.5 * math.sin(TAU * 48 * i / RATE) * math.sin(math.pi * i / count)
    save("raycast_pushwall", mix(scale(grind, 3.0), gravel, low), 0.75)


def raycast_treasure() -> None:
    track = [0.0] * seconds(0.7)
    for k, freq in enumerate((1318.5, 1568.0, 2093.0, 2637.0)):
        n = seconds(0.4)
        note = shaped(tone(freq, n, (1.0, 0.3)), envelope(n, 0.002, 0.12))
        place(track, k * 0.07, scale(note, 0.5))
    save("raycast_treasure", track, 0.7)


def raycast_food() -> None:
    rng = random.Random(101)
    track = [0.0] * seconds(0.5)
    for k in range(3):
        n = seconds(0.09)
        bite = shaped(mix(lowpass(noise(rng, n), 1800), tone(260 - 30 * k, n)), envelope(n, 0.002, 0.025))
        place(track, k * 0.13, bite)
    heal = shaped(tone(660, seconds(0.2)), envelope(seconds(0.2), 0.003, 0.07))
    place(track, 0.3, scale(heal, 0.4))
    save("raycast_food", track, 0.6)


def raycast_ammo() -> None:
    rng = random.Random(111)
    track = [0.0] * seconds(0.3)
    for at, freq in ((0.0, 1800), (0.11, 1200)):
        n = seconds(0.06)
        click = shaped(mix(highpass(noise(rng, n), 1800), tone(freq, n)), envelope(n, 0.0005, 0.012))
        place(track, at, click)
    place(track, 0.16, scale(shaped(tone(2400, seconds(0.1)), envelope(seconds(0.1), 0.001, 0.03)), 0.3))
    save("raycast_ammo", track, 0.7)


def raycast_elevator() -> None:
    count = seconds(2.2)
    hum = [0.0] * count
    phase = 0.0
    for i in range(count):
        phase += TAU * (70 + 50 * (i / count)) / RATE
        hum[i] = (math.sin(phase) + 0.5 * math.sin(2 * phase)) * min(1.0, i / seconds(0.2)) * min(1.0, (count - i) / seconds(0.6))
    rng = random.Random(121)
    rattle = shaped(lowpass(noise(rng, count), 300), [0.3 * min(1.0, i / seconds(0.3)) for i in range(count)])
    track = mix(hum, rattle)
    ding = shaped(mix(tone(1568, seconds(1.0)), scale(tone(2352, seconds(1.0)), 0.4)), envelope(seconds(1.0), 0.002, 0.3))
    place(track, 1.6, scale(ding, 0.5))
    track += [0.0] * seconds(0.4)
    save("raycast_elevator", track, 0.7)


def main() -> None:
    hub_ambience()
    hub_water()
    step(1, 1.0)
    step(2, 0.88)
    hub_door()
    hub_board()
    hub_lever()
    hub_gate()
    hub_portal_hum()
    hub_portal_wake()
    hub_bell()
    raycast_ambience()
    raycast_door()
    raycast_pushwall()
    raycast_treasure()
    raycast_food()
    raycast_ammo()
    raycast_elevator()


if __name__ == "__main__":
    main()
