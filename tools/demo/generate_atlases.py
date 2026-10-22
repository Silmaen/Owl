#!/usr/bin/env python3
"""Generate the procedural tile atlases for the Engine Demo.

Produces the 128 px-tile block-face atlas of the voxel demo, three images with the same 4x4 layout:

* ``voxel_blocks.png``           - colour (RGBA, translucent blocks carry their alpha);
* ``voxel_blocks_normal.png``    - tangent-space normal map, OpenGL convention (+Y towards the top of the image),
  derived from a per-tile height map;
* ``voxel_blocks_roughness.png`` - greyscale multiplier of the block roughness (128 keeps it, darker is smoother).

Voxel faces repeat across greedy-merged quads, so every tile is self-tileable: the noises wrap, the patterns have a
period dividing 128 and the height / normal maps read their neighbours with wrap-around inside the tile.

The raycast atlases (``raycast_walls.png`` / the wolf3d sprites) are authored separately and deliberately left
untouched. Run with ``poetry run python``.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "atlases"
TILE = 128
S = TILE

VOXEL_TILES = [
    "grass_top", "grass_side", "dirt", "stone",
    "cobblestone", "sand", "log_top", "log_side",
    "planks", "leaves", "snow", "gravel",
    "brick", "water", "ice", "glass",
]

# Normal-map slope scale per tile (height 0..1 over a few pixels): bigger is more relief.
NORMAL_STRENGTH = {
    "grass_top": 5.0, "grass_side": 4.0, "dirt": 4.0, "stone": 5.0, "cobblestone": 4.5, "sand": 3.0,
    "log_top": 4.0, "log_side": 5.0, "planks": 5.0, "leaves": 4.0, "snow": 3.0, "gravel": 5.0,
    "brick": 3.5, "water": 2.0, "ice": 3.0, "glass": 4.0,
}


# --- small maths helpers ----------------------------------------------------

def _clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if v < lo else hi if v > hi else v


def _smooth(e0: float, e1: float, v: float) -> float:
    t = _clamp((v - e0) / (e1 - e0))
    return t * t * (3.0 - 2.0 * t)


def _mix(a: tuple, b: tuple, t: float) -> tuple[int, int, int]:
    t = _clamp(t)
    return (int(a[0] + (b[0] - a[0]) * t), int(a[1] + (b[1] - a[1]) * t), int(a[2] + (b[2] - a[2]) * t))


def _shade(c: tuple, f: float) -> tuple[int, int, int]:
    return (int(_clamp(c[0] * f, 0, 255)), int(_clamp(c[1] * f, 0, 255)), int(_clamp(c[2] * f, 0, 255)))


def _wrapnoise(seed: int, cx: int, cy: int | None = None) -> list[float]:
    """Smooth value noise in 0..1 whose lattice wraps, so the field tiles seamlessly."""
    cy = cx if cy is None else cy
    rng = random.Random(seed)
    grid = [[rng.random() for _ in range(cx)] for _ in range(cy)]
    out = [0.0] * (S * S)
    for y in range(S):
        gy = y * cy / S
        y0 = int(gy)
        ty = gy - y0
        ty = ty * ty * (3 - 2 * ty)
        ra, rb = grid[y0 % cy], grid[(y0 + 1) % cy]
        for x in range(S):
            gx = x * cx / S
            x0 = int(gx)
            tx = gx - x0
            tx = tx * tx * (3 - 2 * tx)
            xa, xb = x0 % cx, (x0 + 1) % cx
            top = ra[xa] + (ra[xb] - ra[xa]) * tx
            bot = rb[xa] + (rb[xb] - rb[xa]) * tx
            out[y * S + x] = top + (bot - top) * ty
    return out


def _fbm(seed: int, cells: int, octaves: int = 3, cy: int | None = None) -> list[float]:
    """Sum of wrapping noises, each octave doubling the frequency, normalised to 0..1."""
    total = [0.0] * (S * S)
    norm = 0.0
    amp = 1.0
    for o in range(octaves):
        f = _wrapnoise(seed + o * 101, cells << o, None if cy is None else cy << o)
        total = [a + b * amp for a, b in zip(total, f, strict=True)]
        norm += amp
        amp *= 0.5
    return [v / norm for v in total]


def _voronoi(seed: int, n: int, jitter: float) -> tuple[list[float], list[float], list[int]]:
    """Wrapping Worley noise on an n x n lattice: nearest distance, second distance and cell id per pixel."""
    rng = random.Random(seed)
    c = S / n
    pts = [[((i + 0.5 + (rng.random() - 0.5) * 2 * jitter) * c, (j + 0.5 + (rng.random() - 0.5) * 2 * jitter) * c)
            for i in range(n)] for j in range(n)]
    f1 = [0.0] * (S * S)
    f2 = [0.0] * (S * S)
    ids = [0] * (S * S)
    for y in range(S):
        cj = int(y / c)
        for x in range(S):
            ci = int(x / c)
            best = [1e9, 1e9]
            bid = 0
            for dj in (-1, 0, 1):
                jj = (cj + dj) % n
                sy = (cj + dj - jj) // n * S
                for di in (-1, 0, 1):
                    ii = (ci + di) % n
                    sx = (ci + di - ii) // n * S
                    px, py = pts[jj][ii]
                    d = math.hypot(x - (px + sx), y - (py + sy))
                    if d < best[0]:
                        best[1] = best[0]
                        best[0] = d
                        bid = jj * n + ii
                    elif d < best[1]:
                        best[1] = d
            f1[y * S + x] = best[0]
            f2[y * S + x] = best[1]
            ids[y * S + x] = bid
    return f1, f2, ids


class Tile:
    """One 128 px tile: colour, height (0..1) and roughness (0..255) maps."""

    def __init__(self) -> None:
        self.col: list[tuple[int, int, int, int]] = [(255, 0, 255, 255)] * (S * S)
        self.h = [0.5] * (S * S)
        self.ro = [128.0] * (S * S)

    def put(self, x: int, y: int, c: tuple, h: float | None = None, ro: float | None = None, a: int = 255) -> None:
        """Write a pixel with wrap-around coordinates."""
        i = (y % S) * S + (x % S)
        self.col[i] = (c[0], c[1], c[2], a)
        if h is not None:
            self.h[i] = h
        if ro is not None:
            self.ro[i] = ro

    def fill(self, fn) -> None:
        """Fill every pixel from fn(x, y) -> (colour, height, roughness[, alpha])."""
        for y in range(S):
            for x in range(S):
                r = fn(x, y)
                a = r[3] if len(r) > 3 else 255
                self.put(x, y, r[0], r[1], r[2], a)

    def bake_light(self, amount: float) -> None:
        """Shade the colour from the height gradient (light from the top-left), wrap-around neighbours."""
        h = self.h
        for y in range(S):
            for x in range(S):
                i = y * S + x
                d = h[((y - 1) % S) * S + (x - 1) % S] - h[((y + 1) % S) * S + (x + 1) % S]
                f = 1.0 + amount * _clamp(d * 3.0, -1.0, 1.0)
                r, g, b, a = self.col[i]
                self.col[i] = (int(_clamp(r * f, 0, 255)), int(_clamp(g * f, 0, 255)), int(_clamp(b * f, 0, 255)), a)


def _walk(rng: random.Random, x: float, y: float, ang: float, steps: int, wobble: float,
          branch: float = 0.0) -> list[tuple[int, int]]:
    """Random-walk polyline (with optional branches) as wrapped integer points."""
    pts: list[tuple[int, int]] = []
    for _ in range(steps):
        ang += rng.uniform(-wobble, wobble)
        x += math.cos(ang)
        y += math.sin(ang)
        pts.append((int(x) % S, int(y) % S))
        if branch and rng.random() < branch:
            pts += _walk(rng, x, y, ang + rng.choice((-1, 1)) * rng.uniform(0.5, 1.0), rng.randint(6, 16), wobble)
    return pts


# --- tiles ------------------------------------------------------------------

def _paint_grass(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    low = _fbm(seed, 4, 3)
    mid = _fbm(seed + 7, 12, 2)
    dark, base, light = (52, 104, 36), (86, 142, 54), (118, 172, 70)

    def base_px(x: int, y: int):
        i = y * S + x
        v = low[i] * 0.7 + mid[i] * 0.3
        c = _mix(dark, base, v * 2) if v < 0.5 else _mix(base, light, (v - 0.5) * 2)
        j = rng.randint(-6, 6)
        return ((c[0] + j, c[1] + j, c[2] + j // 2), 0.25 + 0.1 * mid[i], 150 + 20 * (mid[i] - 0.5))

    t.fill(base_px)
    blades = sorted(((rng.randrange(S), rng.randrange(S), rng.randint(6, 15), rng.choice((-1, 0, 0, 1)),
                      rng.random()) for _ in range(640)), key=lambda b: b[1])
    for bx, by, length, lean, tone in blades:
        root = _mix((46, 92, 32), (74, 128, 48), tone)
        tip = _mix((112, 168, 66), (166, 212, 96), tone)
        for k in range(length):
            f = k / length
            px = bx + (lean * k) // 3
            c = _mix(root, tip, f)
            t.put(px, by - k, c, 0.35 + 0.6 * f, 140 + 30 * (1 - f))
            if length > 8 and k < length - 3:
                t.put(px + 1, by - k, _shade(c, 0.78), 0.3 + 0.4 * f, 165)
    # Tufts: fans of long blades.
    for _ in range(26):
        cx, cy = rng.randrange(S), rng.randrange(S)
        for a in (-0.7, -0.35, 0.0, 0.35, 0.7):
            ln = rng.randint(9, 15)
            for k in range(ln):
                f = k / ln
                t.put(cx + int(math.sin(a) * k * 0.9), cy - int(math.cos(a) * k),
                      _mix((60, 116, 40), (176, 220, 104), f), 0.45 + 0.5 * f, 140)
    t.bake_light(0.35)
    return t


def _paint_dirt(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    low = _fbm(seed, 4, 3)
    mid = _fbm(seed + 5, 16, 2)
    dark, base, light = (96, 68, 40), (124, 90, 56), (150, 112, 72)

    def base_px(x: int, y: int):
        i = y * S + x
        v = low[i] * 0.6 + mid[i] * 0.4
        c = _mix(dark, base, v * 2) if v < 0.5 else _mix(base, light, (v - 0.5) * 2)
        j = rng.randint(-7, 7)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.3 + 0.2 * mid[i], 170 + 25 * (v - 0.5))

    t.fill(base_px)
    # Roots: thin, mostly horizontal, raised dark strands.
    for _ in range(3):
        pts = _walk(rng, rng.randrange(S), rng.randrange(S), rng.choice((0.0, math.pi)) + rng.uniform(-0.4, 0.4),
                    rng.randint(70, 120), 0.25, 0.015)
        for px, py in pts:
            t.put(px, py, (78, 54, 34), 0.62, 150)
            t.put(px, py - 1, (112, 82, 52), 0.7, 160)
            t.put(px, py + 1, (62, 42, 26), 0.35, 190)
    # Pebbles: few, small, warm brown-grey domes with a thin soft outline.
    for n in range(18):
        cx, cy = rng.randrange(S), rng.randrange(S)
        big = n < 3
        rx, ry = (rng.randint(4, 6), rng.randint(3, 5)) if big else (rng.randint(1, 3), rng.randint(1, 3))
        tone = rng.choice(((132, 112, 90), (112, 94, 76), (138, 118, 94), (100, 84, 68), (122, 104, 82)))
        for dy in range(-ry - 1, ry + 2):
            for dx in range(-rx - 1, rx + 2):
                e = (dx / (rx + 0.5)) ** 2 + (dy / (ry + 0.5)) ** 2
                if e <= 1.0:
                    dome = math.sqrt(1.0 - e)
                    c = _shade(tone, 0.85 + 0.2 * (-dx / rx - dy / ry) * 0.5 + 0.08 * dome)
                    t.put(cx + dx, cy + dy, c, 0.42 + 0.3 * dome, 150 + rng.randint(-8, 8))
                elif e <= 1.35:
                    i = ((cy + dy) % S) * S + (cx + dx) % S
                    t.put(cx + dx, cy + dy, _shade(t.col[i], 0.75), 0.3, 185)
    for _ in range(260):  # fine specks
        t.put(rng.randrange(S), rng.randrange(S), rng.choice(((90, 64, 38), (156, 120, 80))), None, None)
    t.bake_light(0.4)
    return t


def _tile_grass_top(seed: int) -> Tile:
    return _paint_grass(seed)


def _tile_grass_side(seed: int) -> Tile:
    rng = random.Random(seed + 3)
    t = _paint_dirt(seed + 11)
    grass = _paint_grass(seed + 5)
    n1 = _wrapnoise(seed + 1, 6, 1)[:S]
    n2 = _wrapnoise(seed + 2, 24, 1)[:S]
    depth = [int(18 + 16 * n1[x] + 7 * n2[x]) for x in range(S)]
    for _ in range(16):  # hanging blade clumps with pointed tips
        cx, w, ext = rng.randrange(S), rng.randint(2, 4), rng.randint(6, 14)
        for dx in range(-w, w + 1):
            depth[(cx + dx) % S] = max(depth[(cx + dx) % S], depth[cx % S] + int(ext * (1 - abs(dx) / (w + 1))))
    for x in range(S):
        for y in range(S):
            i = y * S + x
            edge = depth[x] - y
            if edge > 0:
                t.col[i], t.h[i], t.ro[i] = grass.col[i], grass.h[i], grass.ro[i]
                if edge <= 3:  # darker shaded underside of the overhang
                    r, g, b, a = t.col[i]
                    t.col[i] = (int(r * 0.78), int(g * 0.8), int(b * 0.78), a)
                    t.h[i] = 0.55 + 0.1 * edge
            elif edge > -7:  # shadow cast on the dirt under the grass
                r, g, b, a = t.col[i]
                f = 0.65 + 0.35 * (-edge) / 7
                t.col[i] = (int(r * f), int(g * f), int(b * f), a)
                t.h[i] = min(t.h[i], 0.25 + 0.05 * (-edge))
    return t


def _tile_dirt(seed: int) -> Tile:
    return _paint_dirt(seed)


def _tile_stone(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    low = _fbm(seed, 4, 3)
    fine = _fbm(seed + 9, 24, 2)
    warp = _fbm(seed + 4, 3, 2)
    base = (128, 128, 132)

    def px(x: int, y: int):
        i = y * S + x
        # Strata: horizontal bands whose period divides 128, wobbled by a wrapping warp.
        band = math.sin(2 * math.pi * (y + 14 * warp[i]) * 5 / S)
        v = low[i] * 0.55 + fine[i] * 0.25 + band * 0.1 + 0.1
        j = rng.randint(-5, 5)
        c = _shade(base, 0.78 + 0.45 * v)
        c = (c[0] + j, c[1] + j, c[2] + j + (2 if band > 0.4 else 0))
        return (c, 0.4 + 0.25 * fine[i] + 0.06 * band, 140 + 40 * (fine[i] - 0.5) + 14 * band)

    t.fill(px)
    for _ in range(5):  # cracks: dark core, low, lighter lip
        pts = _walk(rng, rng.randrange(S), rng.randrange(S), rng.uniform(0, 6.28), rng.randint(40, 90), 0.5, 0.03)
        for x, y in pts:
            t.put(x, y, (72, 72, 78), 0.12, 190)
            t.put(x - 1, y - 1, (146, 146, 152), 0.55, 135)
    for _ in range(90):  # flecks of mineral
        x, y = rng.randrange(S), rng.randrange(S)
        c = rng.choice(((96, 96, 104), (160, 160, 164), (112, 108, 100)))
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1))[: rng.randint(1, 4)]:
            t.put(x + dx, y + dy, c, None, 120 + rng.randint(-20, 20))
    t.bake_light(0.45)
    return t


def _stones(seed: int, n: int, jitter: float, mortar_w: float, bevel: float, palette: list[tuple],
            mortar_col: tuple, gap_rough: float, dome_pow: float = 0.6) -> Tile:
    """Voronoi stone field shared by cobblestone and gravel."""
    t = Tile()
    rng = random.Random(seed)
    f1, f2, ids = _voronoi(seed, n, jitter)
    cell_col = [rng.choice(palette) for _ in range(n * n)]
    cell_tone = [rng.uniform(0.82, 1.15) for _ in range(n * n)]
    cell_rough = [rng.uniform(-25, 22) for _ in range(n * n)]
    cell_base = [rng.uniform(0.0, 0.15) for _ in range(n * n)]
    grain = _fbm(seed + 3, 20, 2)
    low = _fbm(seed + 8, 5, 2)

    def px(x: int, y: int):
        i = y * S + x
        e = f2[i] - f1[i]
        k = ids[i]
        if e < mortar_w:
            j = rng.randint(-6, 6)
            return ((mortar_col[0] + j, mortar_col[1] + j, mortar_col[2] + j), 0.02, gap_rough + rng.randint(-10, 10))
        s = _clamp((e - mortar_w) / bevel)
        dome = (1.0 - (1.0 - s) ** 2) ** dome_pow
        ao = 0.62 + 0.38 * _smooth(0, 1, s)
        v = cell_tone[k] * (0.9 + 0.2 * grain[i]) * (0.94 + 0.12 * low[i]) * ao
        c = _shade(cell_col[k], v)
        j = rng.randint(-4, 4)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.2 + 0.6 * cell_base[k] + 0.55 * dome,
                140 + cell_rough[k] + 25 * (grain[i] - 0.5))

    t.fill(px)
    t.bake_light(0.55)
    return t


def _tile_cobblestone(seed: int) -> Tile:
    pal = [(136, 136, 140), (124, 124, 126), (146, 142, 136), (112, 114, 118), (130, 126, 120)]
    return _stones(seed, 5, 0.35, 3.0, 9.0, pal, (46, 46, 50), 180)


def _tile_gravel(seed: int) -> Tile:
    pal = [(130, 126, 122), (104, 100, 98), (152, 144, 134), (120, 110, 100), (92, 90, 92), (140, 132, 120)]
    return _stones(seed, 11, 0.45, 1.6, 4.0, pal, (58, 56, 54), 200, 0.7)


def _tile_sand(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    warp = _fbm(seed, 2, 3)
    low = _fbm(seed + 2, 5, 2)
    base, trough, crest = (214, 196, 140), (190, 168, 112), (232, 216, 162)

    def px(x: int, y: int):
        i = y * S + x
        ph = 2 * math.pi * (y + 12 * (warp[i] - 0.5) * 2 + 4 * math.sin(2 * math.pi * x * 2 / S)) * 6 / S
        r = 0.5 + 0.5 * math.sin(ph)
        c = _mix(trough, crest, r * 0.8 + low[i] * 0.2) if r < 0.5 else _mix(base, crest, (r - 0.5) * 1.6)
        j = rng.randint(-7, 7)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.3 + 0.35 * r + 0.03 * rng.random(),
                160 + 20 * (1 - r) + rng.randint(-8, 8))

    t.fill(px)
    for _ in range(70):  # darker grains and tiny pebbles
        x, y = rng.randrange(S), rng.randrange(S)
        t.put(x, y, rng.choice(((170, 148, 96), (150, 130, 92), (236, 224, 176))), 0.5 + rng.random() * 0.2, 150)
    t.bake_light(0.35)
    return t


def _tile_log_top(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    warp = _fbm(seed, 3, 2)
    fine = _fbm(seed + 6, 24, 2)
    edge_n = _fbm(seed + 1, 8, 2)
    c0 = (S - 1) / 2

    def px(x: int, y: int):
        i = y * S + x
        edge = min(x, y, S - 1 - x, S - 1 - y)
        bark_w = 8 + 4 * edge_n[i]
        j = rng.randint(-5, 5)
        if edge < bark_w:
            groove = 0.5 + 0.5 * math.sin((x + y * 0.3) * 1.4) * edge_n[i]
            c = _shade((92, 66, 42), 0.78 + 0.3 * fine[i] + 0.15 * groove)
            return ((c[0] + j, c[1] + j, c[2] + j), 0.72 + 0.2 * fine[i], 185)
        r = math.hypot(x - c0, y - c0) * (1.0 + 0.1 * (warp[i] - 0.5)) + 3 * (fine[i] - 0.5)
        band = (r / 6.4) % 1.0
        c = _mix((178, 140, 90), (146, 108, 66), band * 0.8)
        if band < 0.22:  # dark late-wood line of each ring
            c = _mix(c, (104, 72, 42), 1.0 - _smooth(0.0, 0.22, band))
        if r < 7:
            c = _mix(c, (120, 84, 50), 0.5)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.45 + 0.1 * band - (0.15 if band < 0.12 else 0.0),
                140 + (30 if band < 0.2 else 0))

    t.fill(px)
    for ang in (0.5, 2.4, 4.2):  # radial drying checks
        for k in range(14, 52):
            x, y = int(c0 + math.cos(ang) * k), int(c0 + math.sin(ang) * k)
            if k % 5 != 0:
                t.put(x, y, (72, 50, 30), 0.1, 200)
    t.bake_light(0.4)
    return t


def _tile_log_side(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    wob = _wrapnoise(seed, 1, 6)
    fine = _fbm(seed + 3, 8, 3, cy=2)
    grain = _wrapnoise(seed + 5, 16, 4)
    colw = [rng.uniform(0.15, 0.3) for _ in range(S // 16 + 1)]

    def px(x: int, y: int):
        i = y * S + x
        u = x + 5 * (wob[i] - 0.5) * 2
        p = (u % 16) / 16.0
        cw = colw[int(u // 16) % (S // 16)]
        ridge = _smooth(0.0, cw, p) * (1 - _smooth(1 - 0.12, 1.0, p))  # 0 in the groove, 1 on the ridge
        sub = 0.5 + 0.5 * math.sin(2 * math.pi * u * 4 / 16 + 6 * fine[i])
        v = 0.7 + 0.3 * ridge + 0.1 * fine[i] + 0.06 * sub + 0.08 * grain[i]
        c = _shade((112, 80, 50), v)
        j = rng.randint(-5, 5)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.1 + 0.75 * ridge + 0.1 * fine[i],
                190 - 30 * ridge + 15 * (grain[i] - 0.5))

    t.fill(px)
    for _ in range(9):  # short horizontal bark splits
        x, y = rng.randrange(S), rng.randrange(S)
        for k in range(rng.randint(5, 11)):
            t.put(x + k, y, (58, 40, 24), 0.05, 210)
            t.put(x + k, y + 1, (140, 104, 66), 0.7, 170)
    kx, ky = 38, 72  # a knot
    for dy in range(-9, 10):
        for dx in range(-7, 8):
            e = math.hypot(dx / 7, dy / 9)
            if e < 1.0:
                ring = (e * 4) % 1.0
                c = _mix((150, 112, 70), (86, 58, 34), ring if e > 0.2 else 0.9)
                t.put(kx + dx, ky + dy, c, 0.55 + 0.3 * (1 - e), 150)
            elif e < 1.25:
                t.put(kx + dx, ky + dy, (54, 36, 22), 0.05, 205)
    t.bake_light(0.4)
    return t


def _tile_planks(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    grain = _fbm(seed, 2, 3, cy=24)
    lowv = _fbm(seed + 4, 3, 2)
    board_tone = [rng.uniform(0.88, 1.1) for _ in range(4)]
    board_off = [0, 64, 0, 64]  # staggered butt joints, period 128
    base = (168, 122, 72)

    def px(x: int, y: int):
        i = y * S + x
        b = y // 32
        ry = y % 32
        bx = (x - board_off[b]) % S
        gline = 1.0 if (grain[i] * 9) % 1.0 < 0.1 else 0.0
        v = board_tone[b] * (0.92 + 0.16 * lowv[i]) * (1.0 - 0.14 * gline)
        c = _shade(base, v)
        j = rng.randint(-4, 4)
        if ry < 2 or bx < 2:  # joints
            return ((74, 48, 28), 0.03, 195)
        if ry < 4:  # lit upper bevel of the board
            return ((c[0] + 14 + j, c[1] + 12 + j, c[2] + 8 + j), 0.62, 150)
        if ry >= 30:  # board lower lip in shadow
            return ((int(c[0] * 0.8), int(c[1] * 0.78), int(c[2] * 0.76)), 0.4, 160)
        return ((c[0] + j, c[1] + j, c[2] + j), 0.55 + 0.05 * lowv[i] - 0.06 * gline,
                150 + 25 * gline + 15 * (lowv[i] - 0.5))

    t.fill(px)
    for b in range(4):  # nail heads near the butt joints
        for nx in (board_off[b] + 8, board_off[b] - 8):
            for ny in (b * 32 + 9, b * 32 + 24):
                for dy in range(-3, 4):
                    for dx in range(-3, 4):
                        e = math.hypot(dx, dy)
                        if e <= 2.6:
                            t.put(nx + dx, ny + dy, _shade((92, 90, 94), 1.4 - 0.25 * e + 0.12 * (-dx - dy)), 0.95, 95)
                        elif e <= 3.6:
                            t.put(nx + dx, ny + dy, (60, 38, 22), 0.25, 200)
    t.bake_light(0.3)
    return t


def _tile_leaves(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    layers = [(190, (28, 82, 30), (44, 104, 40), 0.25), (150, (40, 104, 38), (66, 134, 52), 0.5),
              (100, (62, 134, 50), (96, 170, 70), 0.78)]
    for i in range(S * S):
        t.col[i] = (0, 0, 0, 0)
        t.h[i] = 0.0
        t.ro[i] = 170.0
    for count, dark, light, hh in layers:
        for _ in range(count):
            cx, cy = rng.randrange(S), rng.randrange(S)
            ang = rng.uniform(0, math.pi)
            ca, sa = math.cos(ang), math.sin(ang)
            la, lb = rng.uniform(5.5, 9.0), rng.uniform(2.6, 4.2)
            tone = rng.random()
            for dy in range(-10, 11):
                for dx in range(-10, 11):
                    u, w = dx * ca + dy * sa, -dx * sa + dy * ca
                    e = (u / la) ** 2 + (w / lb) ** 2
                    if e > 1.0:
                        continue
                    c = _mix(dark, light, _clamp(0.55 + 0.3 * (-w / lb) + 0.15 * tone))
                    if abs(w) < 0.7 and abs(u) < la - 1:  # midrib
                        c = _shade(c, 0.78)
                    if e > 0.72:
                        c = _shade(c, 0.78)
                    t.put(cx + dx, cy + dy, c, hh + 0.12 * (1 - e) + 0.04 * rng.random(), 165 + rng.randint(-15, 15))
    t.bake_light(0.35)
    return t


def _tile_snow(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    big = _fbm(seed, 3, 3)
    mid = _fbm(seed + 5, 9, 2)
    fine = _fbm(seed + 8, 32, 1)
    top, shadow = (242, 245, 252), (196, 212, 236)

    def px(x: int, y: int):
        i = y * S + x
        h = big[i] * 0.65 + mid[i] * 0.25 + fine[i] * 0.1
        c = _mix(shadow, top, _smooth(0.3, 0.62, h) + 0.15 * fine[i])
        j = rng.randint(-3, 3)
        return ((c[0] + j, c[1] + j, c[2] + j), h, 120 + 20 * (1 - h) + rng.randint(-6, 6))

    t.fill(px)
    t.bake_light(0.6)
    for _ in range(80):  # sparkles
        x, y = rng.randrange(S), rng.randrange(S)
        t.put(x, y, (255, 255, 255), 0.9, 60)
        if rng.random() < 0.3:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                t.put(x + dx, y + dy, (236, 244, 255), 0.8, 80)
    return t


def _tile_brick(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    nb = [[rng.uniform(0.86, 1.12) for _ in range(4)] for _ in range(8)]
    hue = [[rng.uniform(-1, 1) for _ in range(4)] for _ in range(8)]
    grain = _fbm(seed, 14, 2)
    low = _fbm(seed + 3, 4, 2)
    chips = {(rng.randrange(8), rng.randrange(4)): rng.choice((0, 1, 2, 3)) for _ in range(7)}
    mw = 3

    def px(x: int, y: int):
        i = y * S + x
        row, ry = y // 16, y % 16
        off = 16 if row % 2 else 0
        bxs = (x + off) % S
        col, bx = bxs // 32, bxs % 32
        j = rng.randint(-4, 4)
        e = min(bx - mw, 31 - bx, ry - mw, 15 - ry) if (bx >= mw and ry >= mw) else -1
        chip = chips.get((row, col))
        if e >= 0 and chip is not None:  # chipped corner: a jagged break
            cx = bx if chip in (0, 2) else 31 - bx
            cy = ry - mw if chip in (0, 1) else 15 - ry
            if cx + cy * 1.6 < 9 + 2.5 * math.sin(cx * 1.7 + cy):
                return ((176, 104, 82), 0.3, 165)
        if e < 0:
            c = _shade((208, 198, 186), 0.92 + 0.14 * grain[i])
            return ((c[0] + j, c[1] + j, c[2] + j), 0.05, 180 + rng.randint(-8, 8))
        v = nb[row][col] * (0.9 + 0.2 * grain[i]) * (0.95 + 0.1 * low[i])
        base = (150 + 8 * hue[row][col], 60 + 6 * hue[row][col], 46)
        c = _shade(base, v * (0.82 + 0.18 * _smooth(0, 2.5, e)))
        if rng.random() < 0.02:
            c = _shade(c, 0.7)
        return ((c[0] + j, c[1] + j // 2, c[2] + j // 2), 0.4 + 0.45 * _smooth(0, 3.0, e) + 0.05 * grain[i],
                140 + 18 * (grain[i] - 0.5) * 2 + 8 * hue[row][col])

    t.fill(px)
    t.bake_light(0.5)
    return t


def _tile_water(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    ph1, ph2 = _fbm(seed, 3, 2), _fbm(seed + 4, 4, 2)
    deep, shallow, foam = (46, 96, 190), (90, 146, 228), (176, 214, 250)
    tp = 2 * math.pi

    def px(x: int, y: int):
        i = y * S + x
        w = (0.5 * math.sin(tp * (2 * x + y) / S + 5 * ph1[i]) + 0.35 * math.sin(tp * (x - 3 * y) / S + 6 * ph2[i])
             + 0.15 * math.sin(tp * (5 * x + 4 * y) / S + 3 * ph1[i]))
        h = 0.5 + 0.5 * w
        c = _mix(deep, shallow, h)
        if h > 0.82:
            c = _mix(c, foam, (h - 0.82) / 0.18 * 0.75)
        return (c, h, 100 - 14 * h + rng.randint(-4, 4), 178)

    t.fill(px)
    t.bake_light(0.25)
    return t


def _tile_ice(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    cloud = _fbm(seed, 3, 3)
    fine = _fbm(seed + 2, 14, 2)
    base, frost = (170, 204, 236), (222, 238, 250)

    def px(x: int, y: int):
        i = y * S + x
        c = _mix(base, frost, _smooth(0.45, 0.75, cloud[i]) * 0.8 + 0.1 * fine[i])
        return (c, 0.5 + 0.2 * fine[i], 100 + 25 * (cloud[i] - 0.5) + rng.randint(-5, 5), 205)

    t.fill(px)
    for _ in range(6):  # cracks: white lip, deep blue core
        pts = _walk(rng, rng.randrange(S), rng.randrange(S), rng.uniform(0, 6.28), rng.randint(50, 100), 0.45, 0.035)
        for x, y in pts:
            t.put(x, y, (96, 138, 188), 0.05, 135, 235)
            t.put(x + 1, y + 1, (240, 250, 255), 0.7, 105, 225)
    for _ in range(24):  # trapped bubbles
        cx, cy, r = rng.randrange(S), rng.randrange(S), rng.randint(1, 3)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r:
                    t.put(cx + dx, cy + dy, (232, 244, 252), 0.6, 95, 225)
    t.bake_light(0.4)
    return t


def _tile_glass(seed: int) -> Tile:
    t = Tile()
    rng = random.Random(seed)
    frame_w = 8

    def px(x: int, y: int):
        e = min(x, y, S - 1 - x, S - 1 - y)
        j = rng.randint(-3, 3)
        if e < frame_w:
            f = 0.78 if e < 1 else 1.12 if e >= frame_w - 2 else 1.0
            c = _shade((150, 182, 198), f)
            return ((c[0] + j, c[1] + j, c[2] + j), 0.55 + 0.4 * _smooth(0, frame_w - 1, e), 125, 255)
        c = (198 + j, 226 + j, 236 + j)
        a = 72 + rng.randint(0, 6)
        d = x - y
        if (14 < x + y < 118 and (abs(d) < 5 or abs(d - 22) < 2)) or (150 < x + y < 190 and abs(d + 14) < 2):
            c, a = (240, 250, 255), 175
        return (c, 0.3, 100 + rng.randint(-6, 6), a)

    t.fill(px)
    t.bake_light(0.4)
    return t


BUILDERS = {
    "grass_top": _tile_grass_top, "grass_side": _tile_grass_side, "dirt": _tile_dirt, "stone": _tile_stone,
    "cobblestone": _tile_cobblestone, "sand": _tile_sand, "log_top": _tile_log_top, "log_side": _tile_log_side,
    "planks": _tile_planks, "leaves": _tile_leaves, "snow": _tile_snow, "gravel": _tile_gravel,
    "brick": _tile_brick, "water": _tile_water, "ice": _tile_ice, "glass": _tile_glass,
}


def _normal_pixels(t: Tile, strength: float) -> list[tuple[int, int, int, int]]:
    """Tangent-space normals, OpenGL convention (G up in the image), wrap-around neighbours inside the tile."""
    h = t.h
    out = []
    for y in range(S):
        ym, yp = ((y - 1) % S) * S, ((y + 1) % S) * S
        for x in range(S):
            xm, xp = (x - 1) % S, (x + 1) % S
            dx = (h[y * S + xp] - h[y * S + xm]) * 0.5 * strength
            dy = (h[yp + x] - h[ym + x]) * 0.5 * strength  # +Y towards the top of the image
            nx, ny, nz = -dx, dy, 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            out.append((int((nx * inv * 0.5 + 0.5) * 255 + 0.5), int((ny * inv * 0.5 + 0.5) * 255 + 0.5),
                        int((nz * inv * 0.5 + 0.5) * 255 + 0.5), 255))
    return out


def make_voxel_blocks() -> None:
    size = 4 * TILE
    color = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    normal = Image.new("RGBA", (size, size), (128, 128, 255, 255))
    rough = Image.new("RGBA", (size, size), (128, 128, 128, 255))
    for i, name in enumerate(VOXEL_TILES):
        t = BUILDERS[name](1000 + i)
        pos = ((i % 4) * TILE, (i // 4) * TILE)
        tc = Image.new("RGBA", (S, S))
        tc.putdata(t.col)
        color.paste(tc, pos)
        tn = Image.new("RGBA", (S, S))
        tn.putdata(_normal_pixels(t, NORMAL_STRENGTH[name]))
        normal.paste(tn, pos)
        tr = Image.new("RGBA", (S, S))
        tr.putdata([(g, g, g, 255) for g in (int(_clamp(v, 0, 255)) for v in t.ro)])
        rough.paste(tr, pos)
    color.save(OUT / "voxel_blocks.png")
    normal.save(OUT / "voxel_blocks_normal.png")
    rough.save(OUT / "voxel_blocks_roughness.png")


if __name__ == "__main__":
    make_voxel_blocks()
    print("Generated atlases in", OUT)
