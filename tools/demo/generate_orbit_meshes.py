#!/usr/bin/env python3
"""Generate the procedural meshes of the Engine Demo orbit (``engine_demo/meshes/orbit/*.glb``: a station with its
solar wings and lit windows, a cratered moon, faceted rocks for the debris, a relay mast and the beads of the precision
ruler), in metres, with plain coloured materials."""

import math
from pathlib import Path

from generate_meshes import MeshBuilder, box, cross, fbm, normalize, smooth_normals, transformed, write_glb

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


def rock(radius: float, seed: int, stretch: tuple[float, float, float]) -> MeshBuilder:
    """A faceted rock: a coarse sphere pushed in and out by noise, stretched, one flat normal per triangle."""
    smooth = sphere(radius, 14, 8, bumps=0.32, seed=seed)
    m = MeshBuilder()
    for t in range(0, len(smooth.indices), 3):
        corners = [smooth.vertices[smooth.indices[t + k]][0] for k in range(3)]
        corners = [(p[0] * stretch[0], p[1] * stretch[1], p[2] * stretch[2]) for p in corners]
        a, b, c = corners
        n = cross(tuple(b[i] - a[i] for i in range(3)), tuple(c[i] - a[i] for i in range(3)))
        if sum(x * x for x in n) < 1e-12:
            continue
        n = normalize(n)
        first = len(m.vertices)
        for p in corners:
            m.add(p, n, (p[0] / radius, p[2] / radius))
        m.indices += [first, first + 1, first + 2]
    return m


def station_panels() -> MeshBuilder:
    """Two solar wings on a truss along x, 70 m from tip to tip, out of the ring."""
    m = box((0.0, 0.0, 0.0), (44.0, 0.5, 0.5))
    for side in (-1.0, 1.0):
        for k in range(3):
            x = side * (22.0 + 4.6 * k + 2.2)
            m.extend(box((x, 0.0, 3.4), (4.2, 0.08, 6.0)))
            m.extend(box((x, 0.0, -3.4), (4.2, 0.08, 6.0)))
        m.extend(box((side * 22.0, 0.0, 0.0), (0.6, 0.6, 13.6)))
    return m


def station_windows() -> MeshBuilder:
    """Rows of windows on the outer faces of the eight ring modules and a docking light ring on the hub."""
    m = MeshBuilder()
    for k in range(8):
        angle = 2.0 * math.pi * k / 8
        strip = MeshBuilder()
        for w in range(4):
            strip.extend(box((2.03, 0.35, -2.0 + 1.3 * w), (0.06, 0.45, 0.8)))
        m.extend(transformed(strip, yaw=angle, move=(13.0 * math.cos(angle), 0.0, -13.0 * math.sin(angle))))
    for k in range(12):
        angle = 2.0 * math.pi * k / 12
        m.extend(box((2.4 * math.cos(angle), 2.4 * math.sin(angle), 3.03), (0.35, 0.35, 0.08)))
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
    write_glb(OUT / "station.glb", station(), "Station", (0.78, 0.8, 0.84, 1.0), metallic=0.6, roughness=0.45)
    write_glb(OUT / "station_panels.glb", station_panels(), "StationPanels", (0.12, 0.16, 0.32, 1.0), metallic=0.3,
              roughness=0.25)
    write_glb(OUT / "station_windows.glb", station_windows(), "StationWindows", (1.0, 0.86, 0.6, 1.0))
    write_glb(OUT / "rock_a.glb", rock(1.0, 11, (1.0, 0.7, 0.85)), "RockA", (0.42, 0.38, 0.35, 1.0), roughness=0.95)
    write_glb(OUT / "rock_b.glb", rock(1.0, 23, (1.2, 0.8, 0.75)), "RockB", (0.36, 0.33, 0.31, 1.0), roughness=0.95)
    write_glb(OUT / "moon.glb", sphere(60.0, 64, 32, bumps=0.06, seed=7), "Moon", (0.55, 0.52, 0.5, 1.0))
    write_glb(OUT / "relay.glb", relay(), "Relay", (0.85, 0.42, 0.2, 1.0))
    write_glb(OUT / "bead.glb", sphere(0.05, 12, 6), "Bead", (1.0, 0.82, 0.25, 1.0))


if __name__ == "__main__":
    main()
