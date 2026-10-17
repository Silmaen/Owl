"""
Command line of the isometric atlas generator: ``poetry run python -m tools.iso_atlas``.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from .atlas import LEVELS, generate


def main() -> int:
    """
    Generate the atlases into a project folder.

    :return: The process exit code.
    """
    parser = argparse.ArgumentParser(prog="tools.iso_atlas", description=__doc__)
    parser.add_argument("--project", type=Path, default=Path("engine_demo"), help="project folder to write into")
    parser.add_argument("--seed", type=int, default=1, help="seed; the same seed always gives the same pixels")
    parser.add_argument("--levels", default=",".join(LEVELS), help="comma separated zoom levels: 1x, 2x, half")
    args = parser.parse_args()
    levels = tuple(level for level in args.levels.split(",") if level)
    unknown = [level for level in levels if level not in LEVELS]
    if unknown:
        parser.error(f"unknown zoom level(s): {', '.join(unknown)}")
    for path in generate(args.project, args.seed, levels):
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
