#!/usr/bin/env python3
"""Rasterize SVG icon sources to PNG at the correct size tier.

SVG sources live in assets_sources/icons/<subdir>/.
PNGs are written to assets/icons/<subdir>/ (this directory).

Directory structure (identical for SVG and PNG):
  - toolbar/      : 64x64   (play, pause, stop, step, ctrl_*)
  - browser/      : 512x512 (file type icons for content browser)
  - visibility/   : 32x32   (eye_open, eye_closed, camera_on, camera_off)
  - triggers/     : 32x32   (victory, death, target, teleport)
  - components/   : 32x32   (transform, camera, sprite, etc.)
  - panels/       : 32x32   (scene_hierarchy, content_browser, etc.)
  - actions/      : 32x32   (save, open, delete, etc.)
  - templates/    : SVG-only (base templates, not rasterized)
"""

from __future__ import annotations

from pathlib import Path

import cairosvg

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent  # assets/icons/
SVG_BASE = SCRIPT_DIR.parent.parent / "assets_sources" / "icons"  # assets_sources/icons/
PNG_BASE = SCRIPT_DIR  # assets/icons/

# ---------------------------------------------------------------------------
# Size tier configuration: subdir -> png size
# ---------------------------------------------------------------------------
TIER_BY_DIR: dict[str, int] = {
    "toolbar": 64,
    "browser": 512,
    "visibility": 32,
    "triggers": 32,
    "components": 32,
    "panels": 32,
    "actions": 32,
}

# Subdirectories to skip (SVG-only, not rasterized)
SKIP_DIRS: set[str] = {"templates"}

DEFAULT_SIZE = 32


def get_tier_size(svg_path: Path) -> int:
    """Determine the PNG output size for a given SVG source."""
    rel_dir = svg_path.parent.name
    if rel_dir in TIER_BY_DIR:
        return TIER_BY_DIR[rel_dir]
    return DEFAULT_SIZE


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
    for svg_path in sorted(SVG_BASE.rglob("*.svg")):
        rel = svg_path.relative_to(SVG_BASE)
        # Skip SVGs at the root level (should not exist after reorganization).
        if rel.parent == Path("."):
            continue
        # Skip template-only directories.
        if rel.parts[0] in SKIP_DIRS:
            continue
        png_path = PNG_BASE / rel.with_suffix(".png")
        size = get_tier_size(svg_path)

        rasterize_svg(svg_path, png_path, size)
        count += 1
        print(f"  {rel.with_suffix('.png')} ({size}x{size})")

    print(f"Done! {count} icons rasterized.")


if __name__ == "__main__":
    main()
