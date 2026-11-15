#!/usr/bin/env python3
"""Write the texture of the demo mod of the Engine Demo: the first-aid kit of the raycast level, recoloured.

The mod ``demo.recolour`` (``engine_demo/extensions/demo.recolour``) overrides ``textures/items/health.png`` by
holding a file at the same path: its red and blue channels are swapped, so the red of the kit reads blue in the HUD when the
mod is loaded. Run with ``poetry run python``.
"""

from pathlib import Path

from PIL import Image

DEMO = Path(__file__).resolve().parents[2] / "engine_demo"
ITEM = Path("textures") / "items" / "health.png"
MOD = DEMO / "extensions" / "demo.recolour"


def recolour() -> None:
    """Swap the red and blue channels of the game texture into the mod."""
    red, green, blue, alpha = Image.open(DEMO / ITEM).convert("RGBA").split()
    out = MOD / ITEM
    out.parent.mkdir(parents=True, exist_ok=True)
    Image.merge("RGBA", (blue, green, red, alpha)).save(out)


if __name__ == "__main__":
    recolour()
