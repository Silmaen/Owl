#!/usr/bin/env python3
"""Generate the small procedural environment of the image tests
(``test/unit/test_helper/assets/render_tests/hdri/sky_gradient.hdr``): an equirectangular Radiance RGBE image, a blue sky
fading to a warm horizon above a grey ground, with a bright sun disc, 256x128."""
import math
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "test" / "unit" / "test_helper" / "assets" / "render_tests" / "hdri"
WIDTH = 256
HEIGHT = 128
SUN = (0.6, 0.55, -0.58)


def radiance(u: float, v: float) -> tuple[float, float, float]:
    phi = (u - 0.5) * 2.0 * math.pi
    theta = v * math.pi
    x, y, z = math.sin(theta) * math.sin(phi), math.cos(theta), -math.sin(theta) * math.cos(phi)
    if y >= 0.0:
        t = y ** 0.5
        color = (0.9 * (1 - t) + 0.25 * t, 0.75 * (1 - t) + 0.45 * t, 0.6 * (1 - t) + 0.9 * t)
    else:
        color = (0.18, 0.17, 0.16)
    norm = math.sqrt(sum(c * c for c in SUN))
    cos_sun = (x * SUN[0] + y * SUN[1] + z * SUN[2]) / norm
    if cos_sun > 0.995:
        color = (40.0, 36.0, 30.0)
    return color


def rgbe(color: tuple[float, float, float]) -> bytes:
    peak = max(color)
    if peak < 1e-32:
        return bytes((0, 0, 0, 0))
    mantissa, exponent = math.frexp(peak)
    scale = mantissa * 256.0 / peak
    return bytes((int(color[0] * scale), int(color[1] * scale), int(color[2] * scale), exponent + 128))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    data = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n" + f"-Y {HEIGHT} +X {WIDTH}\n".encode())
    for row in range(HEIGHT):
        for column in range(WIDTH):
            data += rgbe(radiance((column + 0.5) / WIDTH, (row + 0.5) / HEIGHT))
    (OUT / "sky_gradient.hdr").write_bytes(bytes(data))
    print(f"  {OUT / 'sky_gradient.hdr'}")


if __name__ == "__main__":
    main()
