#!/usr/bin/env python3
"""Rasterize SVG icon sources to PNG for scene-rendered assets.

Only icons that need PNG files for Renderer2D (scene overlays) are rasterized.
All other icons are loaded as SVG at runtime by the engine via lunasvg.

SVG sources live in assets_sources/icons/<subdir>/.
PNGs are written to assets/icons/<subdir>/ (this directory).
"""

from __future__ import annotations

from pathlib import Path

import cairosvg

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent  # assets/icons/
SVG_BASE = SCRIPT_DIR.parent.parent / "assets_sources" / "icons"  # assets_sources/icons/

# ---------------------------------------------------------------------------
# Only these subdirectories need PNG rasterization (for Renderer2D scene use).
# All other icons are loaded as SVG at runtime by the engine.
# ---------------------------------------------------------------------------
RASTERIZE_DIRS: dict[str, int] = {
    "triggers": 512,  # Scene overlay icons at high resolution
}


def rasterize_svg(svg_path: Path, png_path: Path, size: int):
    """Rasterize an SVG to a PNG at the given square size."""
    png_path.parent.mkdir(parents=True, exist_ok=True)
    svg_data = svg_path.read_bytes()
    png_data = cairosvg.svg2png(
        bytestring=svg_data,
        output_width=size,
        output_height=size,
    )
    png_path.write_bytes(png_data)


def main():
    if not SVG_BASE.is_dir():
        print(f"ERROR: SVG source directory not found: {SVG_BASE}")
        return

    count = 0
    for subdir, size in RASTERIZE_DIRS.items():
        svg_dir = SVG_BASE / subdir
        if not svg_dir.is_dir():
            print(f"WARNING: SVG directory not found: {svg_dir}")
            continue
        for svg_path in sorted(svg_dir.glob("*.svg")):
            png_path = SCRIPT_DIR / subdir / svg_path.with_suffix(".png").name
            rasterize_svg(svg_path, png_path, size)
            count += 1
            print(f"  {subdir}/{svg_path.stem}.png ({size}x{size})")

    print(f"Done! {count} scene icons rasterized.")


if __name__ == "__main__":
    main()
