#!/usr/bin/env python3
"""Generate the star field sky of the Engine Demo far orbit (``engine_demo/textures/hdri/starfield.hdr``): an
equirectangular Radiance RGBE image (run-length encoded), black space with thousands of stars of a power-law
brightness tinted by their temperature and a faint band of the galaxy, 1024x512: the widest image the sky draws
unscaled (`g_maxEnvironmentWidth`), so a star stays one texel wide."""

import math
import random
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "engine_demo" / "textures" / "hdri" / "starfield.hdr"
WIDTH = 1024
HEIGHT = 512
STARS = 5000
BLOCK = 8
SEED = 1977
# Normal of the plane of the galaxy band, tilted against the station's horizon.
BAND = (0.35, 0.82, -0.45)

Color = tuple[float, float, float]


def star_tint(rng: random.Random) -> Color:
    """Colour of a star: mostly white, some blue-white, some yellow and orange."""
    roll = rng.random()
    if roll < 0.15:
        return (0.7, 0.8, 1.0)
    if roll < 0.7:
        return (1.0, 0.98, 0.95)
    if roll < 0.9:
        return (1.0, 0.9, 0.7)
    return (1.0, 0.72, 0.5)


def direction(u: float, v: float) -> tuple[float, float, float]:
    """World direction of an image coordinate (u to the right, v down, 0 to 1)."""
    phi = (u - 0.5) * 2.0 * math.pi
    theta = v * math.pi
    return (math.sin(theta) * math.sin(phi), math.cos(theta), -math.sin(theta) * math.cos(phi))


def band(u: float, v: float) -> Color:
    """Faint glow of the galaxy band around the great circle normal to ``BAND``."""
    d = direction(u, v)
    norm = math.sqrt(sum(c * c for c in BAND))
    distance = abs(sum(d[i] * BAND[i] for i in range(3)) / norm)
    glow = math.exp(-(distance / 0.12) ** 2) * 0.006
    if glow < 0.0004:
        return (0.0, 0.0, 0.0)
    return (glow * 0.8, glow * 0.85, glow)


def render() -> list[list[Color]]:
    """The image, row by row from the top."""
    # The glow is sampled once per block of a row: long runs of equal pixels keep the encoded file small.
    image = [[band((x // BLOCK * BLOCK + BLOCK / 2) / WIDTH, (y + 0.5) / HEIGHT) for x in range(WIDTH)]
             for y in range(HEIGHT)]
    rng = random.Random(SEED)
    for _ in range(STARS):
        # Uniform on the sphere: uniform longitude, cosine of the colatitude uniform.
        u = rng.random()
        v = math.acos(1.0 - 2.0 * rng.random()) / math.pi
        brightness = min(0.01 * (1.0 - rng.random()) ** -1.4, 1.2)
        tint = star_tint(rng)
        cx, cy = u * WIDTH, v * HEIGHT
        # Near the poles a pixel covers less of the sky: widen the star so it keeps its size on screen.
        stretch = 1.0 / max(math.sin(v * math.pi), 0.05)
        radius = 0.45
        for dy in range(-1, 2):
            for dx in range(-int(stretch) - 1, int(stretch) + 2):
                px, py = int(cx) + dx, int(cy) + dy
                if not 0 <= py < HEIGHT:
                    continue
                ox, oy = (px + 0.5 - cx) / stretch, py + 0.5 - cy
                weight = math.exp(-(ox * ox + oy * oy) / (2.0 * radius * radius))
                if weight < 0.01:
                    continue
                row = image[py]
                old = row[px % WIDTH]
                row[px % WIDTH] = tuple(old[i] + tint[i] * brightness * weight for i in range(3))
    return image


def rgbe(color: Color) -> tuple[int, int, int, int]:
    """Shared-exponent encoding of one pixel."""
    peak = max(color)
    if peak < 1e-32:
        return (0, 0, 0, 0)
    mantissa, exponent = math.frexp(peak)
    scale = mantissa * 256.0 / peak
    return (int(color[0] * scale), int(color[1] * scale), int(color[2] * scale), exponent + 128)


def encode_channel(values: list[int]) -> bytes:
    """Run-length encoding of one channel of a scanline (runs of 3 or more, literal dumps otherwise)."""
    out = bytearray()
    i = 0
    count = len(values)
    while i < count:
        run = 1
        while i + run < count and run < 127 and values[i + run] == values[i]:
            run += 1
        if run >= 3:
            out += bytes((128 + run, values[i]))
            i += run
            continue
        start = i
        while i < count and i - start < 128:
            if i + 2 < count and values[i] == values[i + 1] == values[i + 2]:
                break
            i += 1
        out += bytes((i - start,)) + bytes(values[start:i])
    return bytes(out)


def main() -> None:
    """Write the star field."""
    OUT.parent.mkdir(parents=True, exist_ok=True)
    image = render()
    data = bytearray(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {HEIGHT} +X {WIDTH}\n".encode())
    for row in image:
        pixels = [rgbe(c) for c in row]
        data += bytes((2, 2, WIDTH >> 8, WIDTH & 0xFF))
        for channel in range(4):
            data += encode_channel([p[channel] for p in pixels])
    OUT.write_bytes(bytes(data))


if __name__ == "__main__":
    main()
