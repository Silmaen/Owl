#!/usr/bin/env python3
"""
Draw the art and synthesise the sounds of the Lunar lander sample game into ``samples/lunar_lander/``
(deterministic, home-made).

- ``textures/ui/title.png``: LUNAR LANDER in silver capitals under an orbit; ``textures/ui/frame.png``,
  the thin frame of the buttons;
- ``textures/hud/pad.png``, ``textures/hud/impact.png``: the HUD marker of the target pad and of the predicted
  impact;
- ``textures/sky/starfield.hdr``: the starfield of the Rail shooter sample, copied.

Sounds: the rumble of the engine (a grain played over and over while it burns), the puff of the side thrusters, the
touchdown, a leg snapping, the crash, the low fuel beep, the start and game over jingles, a menu loop and a flight
loop; the victory jingle and the gamepad mappings are copied from the Engine Demo.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.lunar_lander.generate_assets``.
"""

from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples import art_kit as art
from tools.samples import synth as s

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "lunar_lander"
"""The sample project."""
SILVER = (235, 238, 245)
"""Top colour of the title."""
STEEL = (150, 165, 190)
"""Bottom colour of the title."""
AMBER = (255, 184, 64)
"""Colour of the highlights."""


def _orbit(image: Image.Image, draw: ImageDraw.ImageDraw, width: int, height: int) -> None:
    """A thin orbit over the letters, the lander a bright point coming down from it."""
    k = art.SS
    draw.arc([60 * k, 30 * k, (width - 60) * k, (height + 120) * k], 192, 348, fill=(*AMBER, 230), width=5 * k)
    x, y = width * 0.8, height * 0.2
    draw.ellipse([(x - 9) * k, (y - 9) * k, (x + 9) * k, (y + 9) * k], fill=(255, 245, 220, 255))
    for step in range(1, 6):
        r = 4 - step * 0.5
        cx, cy = x + step * 14, y - step * 7
        draw.ellipse([(cx - r) * k, (cy - r) * k, (cx + r) * k, (cy + r) * k], fill=(*AMBER, 200))


def title() -> Image.Image:
    """The title art."""
    return art.title_art("LUNAR LANDER", "Inter-SemiBold", SILVER, STEEL, (20, 24, 40), 150, _orbit, 3.0)


def frame() -> Image.Image:
    """A console row, for the buttons: an amber bar on the left, a hairline under the row, ticks on the right."""
    width, height = 512, 96
    image, draw = art.canvas(width, height)
    k = art.SS
    draw.rectangle([0, 8 * k, 7 * k, (height - 8) * k], fill=(*AMBER, 255))
    draw.line([(14 * k, (height - 3) * k), ((width - 2) * k, (height - 3) * k)], fill=(200, 210, 230, 150), width=2 * k)
    for index in range(4):
        x = width - 12 - index * 12
        draw.line([(x * k, (height - 14) * k), (x * k, (height - 4) * k)], fill=(*AMBER, 200), width=2 * k)
    return art.finish(image, width, height, 0.8)


def _pad_icon(draw: ImageDraw.ImageDraw, size: int) -> None:
    m = size // 8
    draw.ellipse([m, m, size - m, size - m], outline=(255, 200, 90, 255), width=size // 14)
    draw.line([(size // 2, m * 2), (size // 2, size - m * 2)], fill=(255, 200, 90, 255), width=size // 16)
    draw.line([(m * 2, size // 2), (size - m * 2, size // 2)], fill=(255, 200, 90, 255), width=size // 16)


def _impact_icon(draw: ImageDraw.ImageDraw, size: int) -> None:
    m = size // 6
    draw.line([(m, m), (size - m, size - m)], fill=(255, 90, 70, 255), width=size // 10)
    draw.line([(size - m, m), (m, size - m)], fill=(255, 90, 70, 255), width=size // 10)


def textures() -> None:
    """Write every image."""
    for folder in ("ui", "hud", "sky"):
        (ROOT / "textures" / folder).mkdir(parents=True, exist_ok=True)
    title().save(ROOT / "textures" / "ui" / "title.png")
    frame().save(ROOT / "textures" / "ui" / "frame.png")
    art.icon(64, _pad_icon, 1.0).save(ROOT / "textures" / "hud" / "pad.png")
    art.icon(48, _impact_icon, 1.0).save(ROOT / "textures" / "hud" / "impact.png")
    shutil.copyfile(
        REPOSITORY / "samples" / "rail_shooter" / "textures" / "sky" / "starfield.hdr",
        ROOT / "textures" / "sky" / "starfield.hdr",
    )


def music_flight() -> s.Samples:
    """The flight loop, 24 s: a wide suspended pad over a slow pulse, a bell now and then, ending where it starts."""
    seconds = 24.0
    d, e, g, a, b = 73.42, 82.41, 98.0, 110.0, 123.47
    pad = s.pad([[d, a, 2 * e], [g, 2 * d, 2 * a], [e, b, 2 * g], [a, 2 * e, 2 * b]], seconds, 1.501)
    pulse = s.pulse([d / 2, g / 2, e / 2, a / 2], seconds, 1.0, 3.0)
    bells = s.sequence(
        [(587.3, 6), (0.0, 10), (880.0, 6), (0.0, 10), (659.3, 6), (0.0, 10), (784.0, 6), (0.0, 42)], 0.25, 1.2
    )
    return s.mix((pad, 0.6), (pulse[: len(pad)], 0.25), (bells[: len(pad)], 0.08))


def music_menu() -> s.Samples:
    """The menu loop, 16 s: open fifths in a low register and a soft arpeggio."""
    seconds = 16.0
    pad = s.pad([[65.4, 98.0, 130.8], [73.4, 110.0, 146.8], [61.7, 92.5, 123.5], [69.3, 103.8, 138.6]], seconds)
    notes = []
    for root in (261.6, 293.7, 246.9, 277.2):
        notes += [(root, 2), (root * 1.5, 2), (root * 2, 2), (root * 1.5, 2), (0.0, 8)]
    arpeggio = s.sequence(notes, 0.25, 3.0)[: len(pad)]
    return s.mix((pad, 0.6), (arpeggio, 0.1))


def sounds() -> None:
    """Write every sound."""
    out = ROOT / "sounds"
    s.write(
        out / "game" / "engine.wav",
        s.fade(
            s.mix((s.noise(0.42, 0.0, 1, 0.93), 1.0), (s.tone(0.42, s.constant(55.0), 0.0, 0.02, (1.0, 0.5)), 0.4)),
            0.06,
            0.1,
        ),
        0.5,
    )
    s.write(out / "game" / "rcs.wav", s.fade(s.noise(0.16, 18.0, 2, 0.35), 0.005, 0.05), 0.35)
    s.write(
        out / "game" / "touchdown.wav",
        s.mix((s.noise(0.3, 14.0, 3, 0.9), 1.0), (s.tone(0.25, lambda t: 90 - 40 * t, 12.0, 0.002), 0.8)),
        0.6,
    )
    s.write(out / "game" / "landed.wav", s.jingle([(523.3, 1), (659.3, 1), (784.0, 1), (1046.5, 4)], 0.09), 0.5)
    s.write(
        out / "game" / "leg_break.wav",
        s.mix(
            (s.tone(0.3, lambda t: 900 - 2500 * t, 14.0, 0.001, (1.0, 0.6, 0.3)), 0.7),
            (s.noise(0.25, 20.0, 4, 0.5), 0.8),
        ),
        0.6,
    )
    s.write(
        out / "game" / "crash.wav",
        s.fade(
            s.mix(
                (s.noise(1.2, 3.5, 5, 0.94), 1.0),
                (s.tone(0.9, lambda t: 80 - 40 * t, 4.0), 0.7),
                (s.noise(0.3, 12.0, 6, 0.6), 0.5),
            ),
            0.0,
            0.3,
        ),
        0.75,
    )
    s.write(out / "game" / "low_fuel.wav", s.sequence([(988.0, 1), (0.0, 1), (988.0, 1)], 0.09, 12.0), 0.35)
    s.write(out / "ui" / "start.wav", s.jingle([(392.0, 1), (587.3, 1), (784.0, 1), (1174.7, 3)], 0.11), 0.55)
    s.write(out / "ui" / "game_over.wav", s.jingle([(392.0, 2), (370.0, 2), (329.6, 2), (246.9, 5)], 0.15), 0.5)
    s.write(out / "ui" / "select.wav", s.tone(0.08, s.constant(1320.0), 30.0, 0.002, (1.0, 0.2)), 0.3)
    s.write(out / "music" / "flight.wav", music_flight(), 0.45)
    s.write(out / "music" / "menu.wav", music_menu(), 0.45)
    art.copy_common(ROOT)


def main() -> None:
    """Write the images and the sounds."""
    textures()
    sounds()


if __name__ == "__main__":
    main()
