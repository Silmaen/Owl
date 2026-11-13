#!/usr/bin/env python3
"""
Make the models, the art and the sounds of the Asteroid field sample game in ``samples/asteroid_field/``
(deterministic).

- Copied from the Rail shooter sample (each sample is self-contained): the ships and the rocks of the Kenney Space Kit
  (CC0) it converted, its lasers, its starfield and the target brackets of its HUD;
- made here: ``models/freighter.glb`` (a long hauler with containers), ``models/gate.glb`` (the jump gate, a ring of
  segments around a glowing core), ``textures/ui/title.png`` and ``textures/ui/frame.png``, ``textures/hud/aim.png``
  (the heading cross), ``textures/hud/aim_circle.png`` (the mouse aim) and ``textures/hud/lead.png`` (the X of the aim
  assist).

Sounds: lasers, hits, explosions, the alarm of a wave, the jump of the convoy, the jingles, a menu loop and a combat
loop; the victory jingle and the gamepad mappings come from the Engine Demo.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.asteroid_field.generate_assets``.
"""

from __future__ import annotations

import math
import shutil
from pathlib import Path

from PIL import Image, ImageDraw

from tools.samples import art_kit as art
from tools.samples import mesh_kit as mk
from tools.samples import synth as s

REPOSITORY = Path(__file__).resolve().parents[3]
"""The repository root."""
ROOT = REPOSITORY / "samples" / "asteroid_field"
"""The sample project."""
RAIL = REPOSITORY / "samples" / "rail_shooter"
"""The sample the space models are copied from."""
GREEN = (120, 255, 190)
"""Colour of the cockpit lines."""
VIOLET = (190, 140, 255)
"""Colour of the highlights."""

COPIED = [
    "models/player.glb",
    "models/fighter.glb",
    "models/interceptor.glb",
    "models/asteroid_a.glb",
    "models/asteroid_b.glb",
    "models/asteroid_c.glb",
    "models/bolt.glb",
    "models/enemy_bolt.glb",
    "textures/sky/starfield.hdr",
    "textures/hud/target.png",
]
"""Files of the Rail shooter sample used as they are."""


def freighter() -> mk.Model:
    """A hauler 14 long, nose along -Z: a spine, the cabin at the front, containers along it, engines at the back."""
    spine = mk.Primitive((0.55, 0.58, 0.62))
    mk.box(spine, (0.0, 0.0, 0.0), (1.2, 1.2, 14.0))
    cabin = mk.Primitive((0.85, 0.85, 0.82))
    mk.box(cabin, (0.0, 0.5, -6.8), (2.6, 1.8, 2.2))
    glass = mk.Primitive((0.15, 0.25, 0.35), (0.05, 0.15, 0.25))
    mk.box(glass, (0.0, 0.8, -7.95), (2.0, 0.6, 0.1))
    containers = [mk.Primitive(colour) for colour in ((0.75, 0.35, 0.2), (0.25, 0.45, 0.7), (0.8, 0.65, 0.25))]
    for k, z in enumerate((-4.0, -1.5, 1.0, 3.5)):
        for side in (-1, 1):
            mk.box(containers[(k + side) % 3], (side * 1.35, 0.0, z), (1.4, 2.0, 2.2))
    engines = mk.Primitive((0.3, 0.3, 0.34))
    glow = mk.Primitive((0.5, 0.8, 1.0), (0.4, 0.7, 1.0))
    for side in (-1, 1):
        mk.cylinder(engines, (side * 0.9, 0.0, 6.0), (side * 0.9, 0.0, 7.6), 0.7, 10)
        mk.cylinder(glow, (side * 0.9, 0.0, 7.6), (side * 0.9, 0.0, 7.65), 0.55, 10)
    return mk.Model("freighter", [spine, cabin, glass, *containers, engines, glow])


def gate() -> mk.Model:
    """The jump gate: a ring 30 across in the XY plane, twelve segments with lamps, a glowing disc inside."""
    frame = mk.Primitive((0.45, 0.48, 0.55))
    lamps = mk.Primitive((0.7, 0.5, 1.0), (0.6, 0.4, 1.0))
    for k in range(12):
        angle = 2 * math.pi * k / 12
        x, y = 15 * math.cos(angle), 15 * math.sin(angle)
        mk.box(frame, (x, y, 0.0), (3.2, 3.2, 3.0))
        mk.sphere(lamps, (x * 1.08, y * 1.08, 0.0), 0.6, 4, 6)
    field = mk.Primitive((0.4, 0.3, 0.9), (0.35, 0.25, 0.9))
    sides = 36
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        p0, p1 = (13 * math.cos(a0), 13 * math.sin(a0), 0.0), (13 * math.cos(a1), 13 * math.sin(a1), 0.0)
        mk.add_triangle(field, (0.0, 0.0, 0.0), p0, p1, ((0.0, 0.0, 1.0),) * 3)
        mk.add_triangle(field, (0.0, 0.0, 0.0), p1, p0, ((0.0, 0.0, -1.0),) * 3)
    return mk.Model("gate", [frame, lamps, field])


def _rings(image: Image.Image, draw: ImageDraw.ImageDraw, width: int, height: int) -> None:
    k = art.SS
    for radius, alpha in ((150, 200), (120, 120)):
        draw.ellipse(
            [
                (width / 2 - radius * 2.6) * k,
                (height / 2 - radius * 0.55) * k,
                (width / 2 + radius * 2.6) * k,
                (height / 2 + radius * 0.55) * k,
            ],
            outline=(*VIOLET, alpha),
            width=4 * k,
        )


def title() -> Image.Image:
    """ASTEROID FIELD in italic capitals, green to white, orbits of violet behind."""
    return art.title_art(
        "ASTEROID FIELD", "OpenSans-BoldItalic", (230, 255, 240), GREEN, (10, 30, 30), 140, _rings, 5.0
    )


def frame() -> Image.Image:
    """A frame with a notch in each corner, for the buttons."""
    width, height = 512, 96
    image, draw = art.canvas(width, height)
    k = art.SS
    cut = 14
    points = [(cut, 3), (width - cut, 3), (width - 3, cut), (width - 3, height - cut), (width - cut, height - 3),
              (cut, height - 3), (3, height - cut), (3, cut), (cut, 3)]  # fmt: skip
    draw.line([(x * k, y * k) for x, y in points], fill=(*GREEN, 220), width=3 * k)
    return art.finish(image, width, height, 1.0)


def _aim(draw: ImageDraw.ImageDraw, size: int) -> None:
    c, r, g = size / 2, size * 0.32, size * 0.12
    w = max(2, size // 22)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        draw.line([(c + dx * g, c + dy * g), (c + dx * r, c + dy * r)], fill=(*GREEN, 255), width=w)
    draw.ellipse([c - w, c - w, c + w, c + w], fill=(*GREEN, 255))


def _aim_circle(draw: ImageDraw.ImageDraw, size: int) -> None:
    c, r = size / 2, size * 0.4
    w = max(2, size // 20)
    draw.ellipse([c - r, c - r, c + r, c + r], outline=(*GREEN, 255), width=w)
    draw.ellipse([c - w, c - w, c + w, c + w], fill=(*GREEN, 255))


def _lead(draw: ImageDraw.ImageDraw, size: int) -> None:
    c, r = size / 2, size * 0.34
    w = max(3, size // 14)
    for dx, dy in ((1, 1), (1, -1)):
        draw.line([(c - dx * r, c - dy * r), (c + dx * r, c + dy * r)], fill=(*VIOLET, 255), width=w)


def hud_marks() -> None:
    """Write the heading cross, the aim circle and the X of the aim assist into ``textures/hud/``."""
    (ROOT / "textures" / "hud").mkdir(parents=True, exist_ok=True)
    art.icon(64, _aim, 1.0).save(ROOT / "textures" / "hud" / "aim.png")
    art.icon(64, _aim_circle, 1.0).save(ROOT / "textures" / "hud" / "aim_circle.png")
    art.icon(64, _lead, 1.0).save(ROOT / "textures" / "hud" / "lead.png")


def models_and_textures() -> None:
    """Copy the Rail shooter files, write the home-made models and images."""
    for relative in COPIED:
        target = ROOT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(RAIL / relative, target)
    mk.write_glb(ROOT / "models" / "freighter.glb", freighter())
    mk.write_glb(ROOT / "models" / "gate.glb", gate())
    (ROOT / "textures" / "ui").mkdir(parents=True, exist_ok=True)
    title().save(ROOT / "textures" / "ui" / "title.png")
    frame().save(ROOT / "textures" / "ui" / "frame.png")
    hud_marks()


def music_combat() -> s.Samples:
    """The combat loop, 16 s: a driving minor pulse, drums and a pad."""
    seconds = 16.0
    e, g, a, b, c, d = 82.41, 98.0, 110.0, 123.47, 130.81, 146.83
    pad = s.pad([[e, b, 2 * g], [c, g * 2, 2 * e], [a, 2 * e, 2 * c], [b, 2 * d, 2 * 185.0]], seconds)
    bass = s.pulse([e / 2, c / 2, a / 2, b / 2], seconds, 0.25, 12.0)
    drums = s.drums(seconds, 0.25, "k.h.s.h.k.k.s.h.", 7)
    return s.mix((pad, 0.4), (bass[: len(pad)], 0.35), (drums[: len(pad)], 0.35))


def music_menu() -> s.Samples:
    """The menu loop, 16 s: slow chords and a far bell."""
    seconds = 16.0
    pad = s.pad([[82.4, 123.5, 164.8], [73.4, 110.0, 146.8], [87.3, 130.8, 174.6], [82.4, 123.5, 196.0]], seconds)
    bells = s.sequence(
        [(659.3, 8), (0.0, 8), (493.9, 8), (0.0, 8), (587.3, 8), (0.0, 8), (659.3, 8), (0.0, 8)], 0.25, 1.5
    )
    return s.mix((pad, 0.55), (bells[: len(pad)], 0.1))


def sounds() -> None:
    """Write every sound."""
    out = ROOT / "sounds"
    s.write(
        out / "game" / "laser.wav",
        s.mix(
            (s.tone(0.14, lambda t: 1600 - 5600 * t, 24.0, 0.002, (1.0, 0.3)), 1.0), (s.noise(0.04, 60.0, 1, 0.6), 0.15)
        ),
        0.4,
    )
    s.write(out / "game" / "enemy_laser.wav", s.tone(0.2, lambda t: 520 - 1300 * t, 16.0, 0.003, (1.0, 0.5, 0.2)), 0.35)
    s.write(
        out / "game" / "hit.wav",
        s.mix((s.noise(0.1, 34.0, 2, 0.7), 1.0), (s.tone(0.08, s.constant(260.0), 34.0), 0.5)),
        0.45,
    )
    s.write(
        out / "game" / "explosion.wav",
        s.fade(s.mix((s.noise(0.9, 4.5, 3, 0.93), 1.0), (s.tone(0.6, lambda t: 90 - 50 * t, 5.0), 0.6)), 0.0, 0.2),
        0.7,
    )
    s.write(
        out / "game" / "hull_hit.wav",
        s.mix((s.tone(0.3, lambda t: 120 - 60 * t, 10.0, 0.002, (1.0, 0.6)), 1.0), (s.noise(0.22, 15.0, 4, 0.9), 0.6)),
        0.6,
    )
    s.write(
        out / "game" / "rock_hit.wav",
        s.mix(
            (s.noise(0.14, 26.0, 5, 0.82), 1.0), (s.tone(0.12, lambda t: 160 - 90 * t, 24.0, 0.001, (1.0, 0.4)), 0.8)
        ),
        0.5,
    )
    s.write(out / "game" / "alarm.wav", s.sequence([(523.3, 2), (392.0, 2), (523.3, 2), (392.0, 2)], 0.16, 5.0), 0.45)
    s.write(
        out / "game" / "jump.wav",
        s.fade(
            s.mix(
                (s.tone(1.6, lambda t: 80 + 900 * t * t, 1.2, 0.05, (1.0, 0.5)), 1.0), (s.noise(1.6, 1.5, 6, 0.9), 0.4)
            ),
            0.1,
            0.4,
        ),
        0.6,
    )
    s.write(out / "game" / "engine_hum.wav", s.hum(1.5, 92.0, 8), 0.4)
    hiss = s.mix((s.noise(0.8, 0.0, 9, 0.4), 0.6), (s.tone(0.8, s.constant(880.0), 0.0, 0.01), 0.3))
    s.write(out / "game" / "bolt_hiss.wav", s.loop_seamless(hiss), 0.3)
    s.write(out / "game" / "lock.wav", s.sequence([(1318.5, 1), (1760.0, 1)], 0.05, 20.0), 0.3)
    s.write(out / "game" / "order.wav", s.sequence([(880.0, 1), (1174.7, 1)], 0.07, 14.0), 0.3)
    s.write(out / "ui" / "start.wav", s.jingle([(329.6, 1), (493.9, 1), (659.3, 1), (987.8, 3)], 0.1), 0.55)
    s.write(out / "ui" / "game_over.wav", s.jingle([(329.6, 2), (311.1, 2), (277.2, 2), (220.0, 5)], 0.15), 0.5)
    s.write(out / "ui" / "select.wav", s.tone(0.08, s.constant(1046.5), 30.0, 0.002, (1.0, 0.2)), 0.3)
    s.write(out / "music" / "combat.wav", music_combat(), 0.45)
    s.write(out / "music" / "menu.wav", music_menu(), 0.45)
    art.copy_common(ROOT)


def main() -> None:
    """Write everything."""
    models_and_textures()
    sounds()


if __name__ == "__main__":
    main()
