#!/usr/bin/env python3
"""
Draw the art and synthesise the sounds of the Bridge builder sample game into ``samples/bridge_builder/``
(deterministic, home-made).

- ``textures/ui/title.png``: BRIDGE BUILDER in warm capitals over a truss; ``textures/ui/frame.png``, the riveted frame
  of the buttons; ``textures/ui/icon_<tool>.png``, the icons of the tool buttons (road, wood, steel, undo, test run,
  grid); ``textures/ui/cursor.png`` is not needed, the cursor is a model of the scene.

Sounds: a part laid in wood, in steel and as road, a part removed, a refusal, the start of a test run, a creak of a
loaded joint, a joint snapping, a vehicle falling in the river, a vehicle across, the start and the level jingles, a
menu loop and a build loop; the victory jingle and the gamepad mappings are copied from the Engine Demo.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.bridge_builder.generate_assets``.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples import art_kit as art
from tools.samples import synth as s

ROOT = Path(__file__).resolve().parents[3] / "samples" / "bridge_builder"
"""The sample project."""
CREAM = (255, 236, 196)
"""Top colour of the title."""
RUST = (214, 120, 52)
"""Bottom colour of the title."""
STEEL = (120, 140, 165)
"""Colour of the truss behind the title."""


def _truss(image: Image.Image, draw: ImageDraw.ImageDraw, width: int, height: int) -> None:
    """A Warren truss under the letters."""
    k = art.SS
    top, bottom = height * 0.78, height * 0.95
    draw.line([(40 * k, top * k), ((width - 40) * k, top * k)], fill=(*STEEL, 255), width=8 * k)
    draw.line([(40 * k, bottom * k), ((width - 40) * k, bottom * k)], fill=(*STEEL, 255), width=8 * k)
    step = (width - 80) / 12
    for i in range(12):
        x0, x1 = 40 + i * step, 40 + (i + 1) * step
        if i % 2 == 0:
            draw.line([(x0 * k, bottom * k), (x1 * k, top * k)], fill=(*STEEL, 255), width=6 * k)
        else:
            draw.line([(x0 * k, top * k), (x1 * k, bottom * k)], fill=(*STEEL, 255), width=6 * k)
    for i in range(13):
        x = 40 + i * step
        for y in (top, bottom):
            draw.ellipse([(x - 7) * k, (y - 7) * k, (x + 7) * k, (y + 7) * k], fill=(200, 60, 40, 255))


def title() -> Image.Image:
    """The title art."""
    return art.title_art("BRIDGE BUILDER", "OpenSans-ExtraBold", CREAM, RUST, (60, 30, 15), 140, _truss, 2.5)


def frame() -> Image.Image:
    """A frame with a rivet in each corner, for the buttons."""
    width, height = 512, 96
    image, draw = art.canvas(width, height)
    k = art.SS
    draw.rectangle([4 * k, 4 * k, (width - 4) * k, (height - 4) * k], outline=(*STEEL, 255), width=4 * k)
    for x, y in ((14, 14), (width - 14, 14), (14, height - 14), (width - 14, height - 14)):
        draw.ellipse([(x - 5) * k, (y - 5) * k, (x + 5) * k, (y + 5) * k], fill=(200, 60, 40, 255))
    return art.finish(image, width, height, 0.6)


def _icon_road(draw: ImageDraw.ImageDraw, size: int) -> None:
    """A deck seen from the side: asphalt with its yellow line over a wooden frame."""
    u = size / 64
    draw.rectangle([4 * u, 34 * u, 60 * u, 44 * u], fill=(150, 104, 56, 255))
    draw.rectangle([4 * u, 22 * u, 60 * u, 34 * u], fill=(66, 66, 72, 255))
    for x in (8, 26, 44):
        draw.rectangle([x * u, 27 * u, (x + 12) * u, 29 * u], fill=(242, 216, 76, 255))
    for x in (10, 32, 54):
        draw.rectangle([(x - 2) * u, 44 * u, (x + 2) * u, 50 * u], fill=(120, 82, 44, 255))


def _icon_beams(colour: tuple[int, int, int], dark: tuple[int, int, int], thickness: float):
    """A truss of beams: two chords and their diagonals."""

    def paint(draw: ImageDraw.ImageDraw, size: int) -> None:
        u = size / 64
        w = int(thickness * u)
        points = [(6, 48), (19, 16), (32, 48), (45, 16), (58, 48)]
        draw.line([(x * u, y * u) for x, y in points], fill=(*colour, 255), width=w, joint="curve")
        draw.line([(6 * u, 48 * u), (58 * u, 48 * u)], fill=(*colour, 255), width=w)
        draw.line([(19 * u, 16 * u), (45 * u, 16 * u)], fill=(*colour, 255), width=w)
        for x, y in points:
            r = thickness * 0.45 * u
            draw.ellipse([x * u - r, y * u - r, x * u + r, y * u + r], fill=(*dark, 255))

    return paint


def _icon_undo(draw: ImageDraw.ImageDraw, size: int) -> None:
    """An arrow turning back."""
    u = size / 64
    draw.arc([14 * u, 14 * u, 54 * u, 54 * u], 200, 90, fill=(*CREAM, 255), width=int(7 * u))
    draw.polygon([(6 * u, 30 * u), (24 * u, 30 * u), (14 * u, 14 * u)], fill=(*CREAM, 255))


def _icon_run(draw: ImageDraw.ImageDraw, size: int) -> None:
    """A truck driving to the right."""
    u = size / 64
    draw.rectangle([6 * u, 20 * u, 40 * u, 42 * u], fill=(214, 70, 52, 255))
    draw.polygon([(40 * u, 26 * u), (52 * u, 26 * u), (58 * u, 34 * u), (58 * u, 42 * u), (40 * u, 42 * u)],
                 fill=(*CREAM, 255))
    for x in (16, 48):
        draw.ellipse([(x - 6) * u, 38 * u, (x + 6) * u, 50 * u], fill=(30, 28, 30, 255))


def _icon_grid(draw: ImageDraw.ImageDraw, size: int) -> None:
    """A grid of three by three cells."""
    u = size / 64
    for k in range(4):
        v = (8 + k * 16) * u
        draw.line([(v, 8 * u), (v, 56 * u)], fill=(190, 215, 255, 255), width=int(3 * u))
        draw.line([(8 * u, v), (56 * u, v)], fill=(190, 215, 255, 255), width=int(3 * u))


ICONS = {
    "road": _icon_road,
    "wood": _icon_beams((186, 128, 70), (110, 70, 36), 7.0),
    "steel": _icon_beams((140, 162, 192), (70, 84, 104), 5.5),
    "undo": _icon_undo,
    "run": _icon_run,
    "grid": _icon_grid,
}
"""The icons of the tool buttons of the build."""


def textures() -> None:
    """Write every image."""
    (ROOT / "textures" / "ui").mkdir(parents=True, exist_ok=True)
    title().save(ROOT / "textures" / "ui" / "title.png")
    frame().save(ROOT / "textures" / "ui" / "frame.png")
    icons()


def icons() -> None:
    """Write the icons of the tool buttons into ``textures/ui/icon_<tool>.png``."""
    (ROOT / "textures" / "ui").mkdir(parents=True, exist_ok=True)
    for name, paint in ICONS.items():
        art.icon(64, paint, 0.6).save(ROOT / "textures" / "ui" / f"icon_{name}.png")


def music_build() -> s.Samples:
    """The build loop, 16 s: a light plucked tune over soft chords in G, ending where it starts."""
    seconds = 16.0
    g, c, d, e = 196.0, 261.6, 293.7, 329.6
    pad = s.pad([[g, 2 * d, 2 * g], [c, 2 * e, 2 * g], [e / 2 * 1.5, 2 * g, 2 * e], [d, 2 * 185.0, 2 * d]], seconds)
    tune = [
        (392.0, 2), (440.0, 1), (493.9, 1), (587.3, 2), (493.9, 2),
        (523.3, 2), (493.9, 1), (440.0, 1), (392.0, 4),
        (329.6, 2), (392.0, 2), (440.0, 2), (493.9, 2),
        (440.0, 2), (392.0, 2), (370.0, 2), (293.7, 2),
    ]  # fmt: skip
    melody = s.sequence([(f, n) for f, n in tune], 0.25, 4.5)
    bass = s.pulse([g / 2, c / 2, e / 2, d / 2], seconds, 0.5, 5.0)
    return s.mix((pad, 0.45), (melody[: len(pad)], 0.18), (bass[: len(pad)], 0.3))


def music_menu() -> s.Samples:
    """The menu loop, 16 s: warm open chords and a slow bell."""
    seconds = 16.0
    pad = s.pad([[98.0, 147.0, 196.0], [110.0, 165.0, 220.0], [87.3, 130.8, 174.6], [98.0, 147.0, 246.9]], seconds)
    bells = s.sequence(
        [(587.3, 8), (0.0, 8), (659.3, 8), (0.0, 8), (523.3, 8), (0.0, 8), (587.3, 8), (0.0, 8)], 0.25, 1.5
    )
    return s.mix((pad, 0.55), (bells[: len(pad)], 0.12))


def sounds() -> None:
    """Write every sound."""
    out = ROOT / "sounds"
    knock = s.mix(
        (s.tone(0.18, lambda t: 220 - 300 * t, 28.0, 0.001, (1.0, 0.5)), 1.0), (s.noise(0.06, 50.0, 1, 0.6), 0.4)
    )
    s.write(out / "game" / "place_wood.wav", knock, 0.5)
    clank = s.mix(
        (s.tone(0.35, s.constant(620.0), 12.0, 0.001, (1.0, 0.0, 0.6, 0.0, 0.3)), 1.0),
        (s.noise(0.05, 60.0, 2, 0.3), 0.4),
    )
    s.write(out / "game" / "place_steel.wav", clank, 0.45)
    thud = s.mix((s.tone(0.22, lambda t: 140 - 120 * t, 18.0, 0.001), 1.0), (s.noise(0.12, 30.0, 3, 0.85), 0.6))
    s.write(out / "game" / "place_road.wav", thud, 0.55)
    s.write(out / "game" / "remove.wav", s.tone(0.15, lambda t: 500 - 1800 * t, 20.0, 0.002, (1.0, 0.3)), 0.4)
    s.write(out / "game" / "refuse.wav", s.square(0.22, s.constant(140.0), 8.0), 0.35)
    s.write(out / "game" / "run.wav", s.tone(0.6, lambda t: 900 + 500 * t, 3.0, 0.02, (1.0, 0.2)), 0.4)
    creak = s.mix(
        (s.tone(0.5, lambda t: 180 + 40 * (t * 7 % 1), 4.0, 0.05, (1.0, 0.7, 0.4)), 0.7),
        (s.noise(0.5, 4.0, 4, 0.97), 0.5),
    )
    s.write(out / "game" / "creak.wav", creak, 0.4)
    crack = s.mix((s.noise(0.35, 14.0, 5, 0.4), 1.0), (s.tone(0.2, lambda t: 1200 - 3000 * t, 20.0, 0.001), 0.6))
    s.write(out / "game" / "snap.wav", crack, 0.6)
    splash = s.fade(s.mix((s.noise(1.1, 3.0, 6, 0.75), 1.0), (s.noise(0.4, 8.0, 7, 0.93), 0.7)), 0.01, 0.4)
    s.write(out / "game" / "splash.wav", splash, 0.6)
    s.write(out / "game" / "across.wav", s.jingle([(659.3, 1), (784.0, 2)], 0.08), 0.45)
    s.write(out / "game" / "level_done.wav", s.jingle([(392.0, 1), (493.9, 1), (587.3, 1), (784.0, 3)], 0.1), 0.55)
    s.write(out / "ui" / "start.wav", s.jingle([(293.7, 1), (392.0, 1), (493.9, 1), (587.3, 3)], 0.1), 0.55)
    s.write(out / "ui" / "game_over.wav", s.jingle([(392.0, 2), (349.2, 2), (293.7, 4)], 0.14), 0.5)
    s.write(out / "ui" / "select.wav", s.tone(0.07, s.constant(980.0), 34.0, 0.002, (1.0, 0.3)), 0.3)
    s.write(out / "music" / "build.wav", music_build(), 0.45)
    s.write(out / "music" / "menu.wav", music_menu(), 0.45)
    art.copy_common(ROOT)


def main() -> None:
    """Write the images and the sounds."""
    textures()
    sounds()


if __name__ == "__main__":
    main()
