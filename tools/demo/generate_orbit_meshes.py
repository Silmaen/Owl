#!/usr/bin/env python3
"""Generate the procedural meshes of the Engine Demo orbit (``engine_demo/meshes/orbit/*.glb``: a station, a cratered
moon, a relay mast and the one-centimetre beads of the precision gauge), in metres, with plain coloured materials."""

import math
from pathlib import Path

from generate_meshes import MeshBuilder, box, fbm, smooth_normals, transformed, write_glb

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "meshes" / "orbit"


def sphere(radius: float, segments: int = 32, rings: int = 16, bumps: float = 0.0, seed: int = 0) -> MeshBuilder:
    """UV sphere around the origin; ``bumps`` displaces it by a fraction of the radius with fractal noise."""
    m = MeshBuilder()
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2.0 * math.pi * (s % segments) / segments
            n = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            scale = radius * (1.0 + bumps * fbm(n, seed, 3.0, 4))
            m.add((n[0] * scale, n[1] * scale, n[2] * scale), n, (s / segments, r / rings))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            m.quad(a, a + 1, b + 1, b)
    return smooth_normals(m) if bumps > 0.0 else m


def station() -> MeshBuilder:
    """A hub with a ring of eight modules on four spokes, 30 m across."""
    m = box((0.0, 0.0, 0.0), (6.0, 6.0, 6.0))
    m.extend(box((0.0, 0.0, 0.0), (1.0, 1.0, 26.0)))
    m.extend(box((0.0, 0.0, 0.0), (26.0, 1.0, 1.0)))
    m.extend(box((0.0, 4.5, 0.0), (1.2, 3.0, 1.2)))
    for k in range(8):
        angle = 2.0 * math.pi * k / 8
        module = box((0.0, 0.0, 0.0), (4.0, 2.5, 5.5))
        m.extend(transformed(module, yaw=angle, move=(13.0 * math.cos(angle), 0.0, -13.0 * math.sin(angle))))
    return m


def relay() -> MeshBuilder:
    """A mast with a flattened dish on top, 20 m tall."""
    m = box((0.0, 0.0, 0.0), (0.8, 20.0, 0.8))
    m.extend(box((0.0, -10.0, 0.0), (5.0, 1.0, 5.0)))
    dish = sphere(4.0, 24, 12)
    dish.vertices = [((p[0], 10.0 + p[1] * 0.25, p[2]), n, uv) for p, n, uv in dish.vertices]
    return m.extend(dish)


def main() -> None:
    """Write every procedural mesh of the Engine Demo orbit."""
    OUT.mkdir(parents=True, exist_ok=True)
    write_glb(OUT / "station.glb", station(), "Station", (0.78, 0.8, 0.84, 1.0))
    write_glb(OUT / "moon.glb", sphere(60.0, 64, 32, bumps=0.06, seed=7), "Moon", (0.55, 0.52, 0.5, 1.0))
    write_glb(OUT / "relay.glb", relay(), "Relay", (0.85, 0.42, 0.2, 1.0))
    write_glb(OUT / "bead.glb", box((0.0, 0.0, 0.0), (0.006, 0.006, 0.006)), "Bead", (1.0, 0.82, 0.25, 1.0))


if __name__ == "__main__":
    main()
