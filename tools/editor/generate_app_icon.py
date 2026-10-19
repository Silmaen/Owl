#!/usr/bin/env python3
"""Build the Windows icon of the Owl executables from the engine logo.

Reads engine_assets/logo/logo_owl.png and writes source/owlnest/resources/owl.ico (16 to 256 px), embedded in
OwlNest.exe and OwlRunner.exe by their Windows resource script. Run it again when the logo changes:

    ./docker_run.sh poetry run python tools/editor/generate_app_icon.py
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
LOGO = ROOT / "engine_assets" / "logo" / "logo_owl.png"
ICON = ROOT / "source" / "owlnest" / "resources" / "owl.ico"
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def build_icon(logo: Path, icon: Path) -> list[int]:
    """Write a multi-size icon of a square logo.

    :param logo: The source image (any size, square).
    :param icon: The .ico file to write.
    :return: The sizes written.
    """
    image = Image.open(logo).convert("RGBA")
    side = max(image.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    largest = square.resize((SIZES[-1], SIZES[-1]), Image.Resampling.LANCZOS)
    icon.parent.mkdir(parents=True, exist_ok=True)
    largest.save(icon, format="ICO", sizes=[(size, size) for size in SIZES])
    return SIZES


def main() -> None:
    """Generate the icon and print what was written."""
    sizes = build_icon(LOGO, ICON)
    print(f"{ICON.relative_to(ROOT)}: {', '.join(str(size) for size in sizes)} px")


if __name__ == "__main__":
    main()
