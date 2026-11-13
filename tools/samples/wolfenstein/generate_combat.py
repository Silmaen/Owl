#!/usr/bin/env python3
"""
Slice the combat art and synthesise the combat sounds of the Wolfenstein mini sample game into ``samples/wolfenstein/``
(deterministic).

Art, from two CC0 sheets of OpenGameArt downloaded by hand and given on the command line:

- ``--guard``: *The Guard* (``guard_spritesheet.png``, 8 x 7 cells of 64 pixels): row 0 the guard standing in the 8
  directions, rows 1 to 4 the 4 walking frames in the 8 directions, row 5 two pain frames, three dying frames and
  the corpse, row 6 aiming and firing. Written as is to ``textures/guard/guard.png`` (the whole sheet, a frame per
  cell, the frame of a guard picked by ``guard.owlvs``), the corpse alone to ``textures/guard/corpse.png``.
- ``--weapons``: *FPS weapons overlay* (``fps_weapons.png``, 5 x 4 cells of 64 pixels on a white background): a row
  per weapon (knife, pistol, machine gun, chain gun), the weapon at rest then four frames of an attack. Its
  background is made transparent and the sheet written to ``textures/weapons/weapons.png``, shown by the HUD.

Sounds (``sounds/combat/``), mono 16 bit at 22.05 kHz: a shot per weapon (``knife.wav``, ``pistol.wav``,
``machine_gun.wav``, ``chain_gun.wav``), the guard shot (``guard_shot.wav``), the alert of a guard (``alert.wav``),
its pain and death (``guard_pain.wav``, ``guard_death.wav``), the player hurt and dead (``hurt.wav``,
``player_death.wav``), the pick-ups of ammo, health and a weapon (``ammo.wav``, ``health.wav``, ``weapon.wav``) and
the click of an empty gun (``empty.wav``).

Run from the repository root:
``./docker_run.sh poetry run python -m tools.samples.wolfenstein.generate_combat --guard <png> --weapons <png>``
(the sounds alone without the two options).
"""

from __future__ import annotations

import argparse
import math
import random
import struct
import wave
from pathlib import Path

from PIL import Image

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "wolfenstein"
"""The sample project."""
RATE = 22050
"""Samples per second."""
CELL = 64
"""Side of a frame of both sheets, in pixels."""


def guard_art(source: Path) -> None:
    """The guard sheet, and its corpse alone (row 5, column 5)."""
    sheet = Image.open(source).convert("RGBA")
    if sheet.size != (8 * CELL, 7 * CELL):
        raise SystemExit(f"{source}: expected a sheet of 8 x 7 cells of {CELL} pixels, got {sheet.size}")
    out = ROOT / "textures" / "guard"
    out.mkdir(parents=True, exist_ok=True)
    sheet.save(out / "guard.png")
    sheet.crop((5 * CELL, 5 * CELL, 6 * CELL, 6 * CELL)).save(out / "corpse.png")


def weapons_art(source: Path) -> None:
    """The weapon sheet, its background (the colour of the top left pixel) made transparent."""
    sheet = Image.open(source).convert("RGBA")
    if sheet.size != (5 * CELL, 4 * CELL):
        raise SystemExit(f"{source}: expected a sheet of 5 x 4 cells of {CELL} pixels, got {sheet.size}")
    data = list(sheet.getdata())
    background = data[0][:3]
    sheet.putdata([(0, 0, 0, 0) if pixel[:3] == background else pixel for pixel in data])
    out = ROOT / "textures" / "weapons"
    out.mkdir(parents=True, exist_ok=True)
    sheet.save(out / "weapons.png")


def _write(name: str, samples: list[float], peak: float) -> None:
    """Write the samples, scaled so that the loudest one reaches ``peak`` of the full scale."""
    path = ROOT / "sounds" / "combat" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    top = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(data)


def _noise(seconds: float, seed: int, decay: float, low_pass: float) -> list[float]:
    """A burst of noise falling off by ``decay`` per second, smoothed by a one pole low pass (0: none, near 1: dull)."""
    rng = random.Random(seed)
    out, last = [], 0.0
    for i in range(round(seconds * RATE)):
        last = last * low_pass + rng.uniform(-1.0, 1.0) * (1.0 - low_pass)
        out.append(last * math.exp(-i / RATE * decay))
    return out


def _tone(seconds: float, start: float, end: float, decay: float, harmonics: int = 3) -> list[float]:
    """A tone gliding from ``start`` to ``end`` hertz, falling off by ``decay`` per second."""
    out, phase = [], 0.0
    count = round(seconds * RATE)
    for i in range(count):
        frequency = start + (end - start) * i / max(1, count - 1)
        phase += 2 * math.pi * frequency / RATE
        value = sum(math.sin(phase * n) / n for n in range(1, harmonics + 1))
        out.append(value * math.exp(-i / RATE * decay) * min(1.0, i / (0.004 * RATE)))
    return out


def _mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(length)]


def _gunshot(seed: int, seconds: float, body: float, dull: float) -> list[float]:
    """A shot: a sharp crack of noise over a low thump of ``body`` hertz."""
    return _mix(_noise(seconds, seed, 18.0, dull), [0.8 * v for v in _tone(seconds, body, body * 0.4, 22.0, 2)])


def sounds() -> None:
    """Every sound written by this script."""
    _write("knife.wav", [0.6 * v for v in _noise(0.18, 1, 14.0, 0.2)], 0.4)
    _write("pistol.wav", _gunshot(2, 0.35, 140.0, 0.35), 0.75)
    _write("machine_gun.wav", _gunshot(3, 0.16, 170.0, 0.3), 0.7)
    _write("chain_gun.wav", _gunshot(4, 0.11, 200.0, 0.25), 0.7)
    _write("guard_shot.wav", _gunshot(5, 0.4, 110.0, 0.55), 0.6)
    _write("empty.wav", _tone(0.05, 2200.0, 1800.0, 80.0, 1), 0.35)
    # A short barked call: two falling notes, rough with noise.
    bark = _mix(_tone(0.18, 420.0, 340.0, 6.0, 5), [0.3 * v for v in _noise(0.18, 6, 6.0, 0.6)])
    _write("alert.wav", bark + _mix(_tone(0.22, 360.0, 260.0, 7.0, 5), [0.3 * v for v in _noise(0.22, 7, 7.0, 0.6)]),
           0.6)
    _write("guard_pain.wav", _mix(_tone(0.2, 300.0, 220.0, 9.0, 4), [0.4 * v for v in _noise(0.2, 8, 9.0, 0.5)]), 0.55)
    _write("guard_death.wav", _mix(_tone(0.7, 330.0, 120.0, 3.5, 4), [0.3 * v for v in _noise(0.7, 9, 3.0, 0.7)]), 0.6)
    _write("hurt.wav", _mix(_tone(0.16, 200.0, 140.0, 12.0, 3), [0.5 * v for v in _noise(0.16, 10, 12.0, 0.6)]), 0.6)
    _write("player_death.wav", _mix(_tone(1.2, 220.0, 70.0, 2.0, 4), [0.3 * v for v in _noise(1.2, 11, 2.0, 0.8)]),
           0.65)
    _write("ammo.wav", _mix(_tone(0.06, 900.0, 900.0, 40.0, 2), [0.0] * 600 + _tone(0.08, 700.0, 700.0, 35.0, 2)), 0.45)
    _write("health.wav", _tone(0.12, 520.0, 520.0, 10.0, 2) + _tone(0.2, 780.0, 780.0, 8.0, 2), 0.45)
    _write("weapon.wav", _mix(_noise(0.25, 12, 10.0, 0.3), [0.7 * v for v in _tone(0.25, 260.0, 260.0, 9.0, 4)]), 0.6)


def main() -> None:
    """Write the art given on the command line, then every sound."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--guard", type=Path, help="guard_spritesheet.png of The Guard (OpenGameArt, CC0)")
    parser.add_argument("--weapons", type=Path, help="fps_weapons.png of FPS weapons overlay (OpenGameArt, CC0)")
    args = parser.parse_args()
    if args.guard is not None:
        guard_art(args.guard)
    if args.weapons is not None:
        weapons_art(args.weapons)
    sounds()


if __name__ == "__main__":
    main()
