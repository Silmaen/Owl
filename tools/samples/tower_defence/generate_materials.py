#!/usr/bin/env python3
"""
Write the material graphs of the Tower defence terrain into ``samples/tower_defence/materials/`` and give them to the
tiles of the ten levels (deterministic).

Every tile of the kit is one model coloured by the palette texture ``models/td/Textures/colormap.png``; the three
materials keep that texture as their base colour and change it in a graph (``surface.parameters`` into the surface):

- ``td_water.owlmat`` on the river tiles: the blue of the palette ripples, a fractal noise flowing downstream over
  time lightens and darkens it and catches the light (lower roughness) where it is water only.
- ``td_ground.owlmat`` on the grass and road tiles: a broad fractal noise over the world varies the shade, so the
  ground of a level is no flat colour.
- ``td_snow.owlmat`` on the ``snow-`` tiles (snow, and the sand and ash of the dry and burnt levels, which tint them):
  a softer variation and glints of a Voronoi noise twinkling over time.

``generate_maps.py`` writes the material into each tile it places; this script also writes it into the level scenes
already saved (``scenes/level_NN.owl``), the material slot of each ``MeshRenderer`` of a tile model replaced, so the
levels edited in Owl Nest keep their edits.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.tower_defence.generate_materials``.
"""

from __future__ import annotations

import re
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3] / "samples" / "tower_defence"
"""The sample project."""
COLORMAP = "gid:f910c7bd02f03894:models/td/Textures/colormap.png"
"""The palette texture of the kit."""

SURFACE_PINS = (
    "Base Color",
    "Alpha",
    "Normal",
    "Metallic",
    "Roughness",
    "Occlusion",
    "Emissive",
    "Clearcoat",
    "Clearcoat Roughness",
    "Sheen Color",
    "Sheen Roughness",
    "Anisotropy",
    "Anisotropy Rotation",
)
"""The pins of ``surface.parameters`` and ``output.surface``."""


def material_of(mesh: str) -> str | None:
    """The terrain material of a model of the kit, None for a decor model (trees, rocks, towers)."""
    name = mesh.rsplit("/", 1)[-1].removesuffix(".glb")
    if "river" in name:
        kind = "td_water"
    elif name.startswith("snow-tile"):
        kind = "td_snow"
    elif name.startswith("tile"):
        kind = "td_ground"
    else:
        return None
    return f"gid:{guid('materials/' + kind)}:materials/{kind}.owlmat"


class Graph:
    """Nodes and links of a material graph, its surface parameters wired to the output unless replaced."""

    def __init__(self, base: int) -> None:
        self.base = base
        self.nodes: list[str] = []
        self.links: list[tuple[int, str, int, str]] = []
        self.replaced: set[str] = set()
        self.params = self.add("surface.parameters", 0, 0)
        self.out = self.add("output.surface", 1400, 0)

    def add(self, kind: str, x: int, y: int, values: dict[str, str] | None = None) -> int:
        """A node; its id."""
        node = self.base + len(self.nodes) + 1
        text = f"      - id: {node}\n        type: {kind}\n        position: [{x}, {y}]\n"
        if values:
            text += "        values:\n" + "".join(f'          {k}: "{v}"\n' for k, v in values.items())
        self.nodes.append(text)
        return node

    def link(self, source: int, pin: str, target: int, target_pin: str) -> None:
        """A link from an output pin to an input pin."""
        self.links.append((source, pin, target, target_pin))

    def output(self, source: int, pin: str, surface_pin: str) -> None:
        """A surface pin taken from a node instead of the parameters."""
        self.replaced.add(surface_pin)
        self.link(source, pin, self.out, surface_pin)

    def text(self) -> str:
        """The ``graph`` section."""
        for pin in SURFACE_PINS:
            if pin not in self.replaced:
                self.link(self.params, pin, self.out, pin)
        out = "  graph:\n    nodes:\n" + "".join(self.nodes) + "    links:\n"
        for source, pin, target, target_pin in self.links:
            out += f"      - {{from: {source}, fromPin: {pin}, to: {target}, toPin: {target_pin}}}\n"
        return out


def material(fields: str, graph: Graph) -> str:
    """A root material: its fields, then its graph."""
    return f"Material:\n  FormatVersion: 1\n{fields}  baseColorMap: {COLORMAP}\n" + graph.text()


def water() -> str:
    """Ripples flowing over the blue of the palette."""
    g = Graph(7650000000000000000)
    split = g.add("vector.split", 220, -200)
    g.link(g.params, "Base Color", split, "Vector")
    blue = g.add("math.subtract", 440, -200)
    g.link(split, "Z", blue, "A")
    g.link(split, "X", blue, "B")
    mask = g.add("math.smoothstep", 660, -200, {"Min": "0.05", "Max": "0.25"})
    g.link(blue, "Result", mask, "Value")
    time = g.add("input.time", 0, 520)
    drift_x = g.add("math.multiply", 220, 480, {"B": "0.45"})
    drift_z = g.add("math.multiply", 220, 600, {"B": "0.3"})
    g.link(time, "Time", drift_x, "A")
    g.link(time, "Time", drift_z, "A")
    flow = g.add("vector.combine", 440, 520)
    g.link(drift_x, "Result", flow, "X")
    g.link(drift_z, "Result", flow, "Z")
    position = g.add("input.position", 440, 400)
    moved = g.add("math.add", 660, 440)
    g.link(position, "Position", moved, "A")
    g.link(flow, "XYZ", moved, "B")
    ripple = g.add("noise.fbm", 880, 440, {"Scale": "1.8", "octaves": "3"})
    g.link(moved, "Result", ripple, "Coordinates")
    # base × (1 + mask × (ripple - 0.5) × 0.5) + mask × crest × 0.25
    centred = g.add("math.subtract", 1000, 300, {"B": "0.5"})  # the noise swings by about 0.15 round 0.5
    g.link(ripple, "Value", centred, "A")
    swing = g.add("math.multiply", 1100, 300, {"B": "2.2"})
    g.link(centred, "Result", swing, "A")
    masked = g.add("math.multiply", 1100, 200)
    g.link(swing, "Result", masked, "A")
    g.link(mask, "Result", masked, "B")
    shade = g.add("math.add", 1200, 200, {"A": "1"})
    g.link(masked, "Result", shade, "B")
    tinted = g.add("math.multiply", 1250, 100)
    g.link(g.params, "Base Color", tinted, "A")
    g.link(shade, "Result", tinted, "B")
    crest = g.add("math.smoothstep", 1000, 560, {"Min": "0.58", "Max": "0.72"})
    g.link(ripple, "Value", crest, "Value")
    foam = g.add("math.multiply", 1100, 560, {"B": "0.45"})
    g.link(crest, "Result", foam, "A")
    foam_masked = g.add("math.multiply", 1200, 560)
    g.link(foam, "Result", foam_masked, "A")
    g.link(mask, "Result", foam_masked, "B")
    colour = g.add("math.add", 1300, 120)
    g.link(tinted, "Result", colour, "A")
    g.link(foam_masked, "Result", colour, "B")
    g.output(colour, "Result", "Base Color")
    shiny = g.add("math.lerp", 1250, 700, {"B": "0.12"})
    g.link(g.params, "Roughness", shiny, "A")
    g.link(mask, "Result", shiny, "T")
    g.output(shiny, "Result", "Roughness")
    return material("  metallic: 0\n  roughness: 0.85\n", g)


def ground() -> str:
    """A broad change of shade over the world."""
    g = Graph(7651000000000000000)
    noise = g.add("noise.fbm", 300, -200, {"Scale": "0.45", "octaves": "4"})
    # the noise stays around 0.5: its middle range stretched to the whole change of shade
    spread = g.add("math.smoothstep", 450, -200, {"Min": "0.35", "Max": "0.65"})
    g.link(noise, "Value", spread, "Value")
    shade = g.add("math.lerp", 600, -200, {"A": "0.8", "B": "1.15"})
    g.link(spread, "Result", shade, "T")
    colour = g.add("math.multiply", 900, -100)
    g.link(g.params, "Base Color", colour, "A")
    g.link(shade, "Result", colour, "B")
    g.output(colour, "Result", "Base Color")
    return material("  metallic: 0\n  roughness: 0.92\n", g)


def snow() -> str:
    """A soft change of shade and glints twinkling over time."""
    g = Graph(7652000000000000000)
    noise = g.add("noise.fbm", 300, -200, {"Scale": "0.4", "octaves": "3"})
    spread = g.add("math.smoothstep", 450, -200, {"Min": "0.35", "Max": "0.65"})
    g.link(noise, "Value", spread, "Value")
    shade = g.add("math.lerp", 600, -200, {"A": "0.9", "B": "1.05"})
    g.link(spread, "Result", shade, "T")
    cells = g.add("noise.voronoi", 300, 300, {"Scale": "9"})
    near = g.add("math.smoothstep", 520, 260, {"Min": "0.12", "Max": "0.02"})
    g.link(cells, "Distance", near, "Value")
    time = g.add("input.time", 300, 480)
    phase = g.add("math.multiply", 520, 420, {"B": "40"})
    g.link(cells, "Cell", phase, "A")
    speed = g.add("math.multiply", 520, 520, {"B": "2.5"})
    g.link(time, "Time", speed, "A")
    angle = g.add("math.add", 700, 460)
    g.link(phase, "Result", angle, "A")
    g.link(speed, "Result", angle, "B")
    wave = g.add("math.sin", 860, 460)
    g.link(angle, "Result", wave, "Value")
    twinkle = g.add("math.saturate", 1000, 460)
    g.link(wave, "Result", twinkle, "Value")
    glint = g.add("math.multiply", 1000, 300, {"B": "0.35"})
    g.link(near, "Result", glint, "A")
    sparkle = g.add("math.multiply", 1150, 360)
    g.link(glint, "Result", sparkle, "A")
    g.link(twinkle, "Result", sparkle, "B")
    shaded = g.add("math.multiply", 900, -100)
    g.link(g.params, "Base Color", shaded, "A")
    g.link(shade, "Result", shaded, "B")
    colour = g.add("math.add", 1250, -40)
    g.link(shaded, "Result", colour, "A")
    g.link(sparkle, "Result", colour, "B")
    g.output(colour, "Result", "Base Color")
    return material("  metallic: 0\n  roughness: 0.8\n", g)


def guid(path: str) -> str:
    """A GUID of an asset path, the same at each run."""
    return f"{zlib.crc32(path.encode()):08x}{zlib.crc32(path[::-1].encode()):08x}"


def write_materials() -> None:
    """The three materials and their sidecars."""
    folder = ROOT / "materials"
    folder.mkdir(exist_ok=True)
    for name, text in (("td_water", water()), ("td_ground", ground()), ("td_snow", snow())):
        (folder / f"{name}.owlmat").write_text(text)
        meta = f"FormatVersion: 1\nGuid: {guid('materials/' + name)}\nImporter: Document\n"
        (folder / f"{name}.owlmat.owlmeta").write_text(meta)


def assign(text: str) -> str:
    """The scene with the material of each tile model in the slot of its ``MeshRenderer``."""
    pattern = re.compile(
        r"(?m)^(?P<indent> +)MeshRenderer:\n(?P=indent)  mesh: (?P<mesh>\S+)\n"
        r"(?P<rest>(?:(?P=indent)  (?!materials:).*\n)*)"
        r"(?P<slots>(?P=indent)  materials:\n(?:(?P=indent)    .*\n)*)?"
    )

    def replace(match: re.Match[str]) -> str:
        found = material_of(match["mesh"])
        if found is None:
            return match[0]
        indent = match["indent"]
        head = f"{indent}MeshRenderer:\n{indent}  mesh: {match['mesh']}\n{match['rest']}"
        return head + f"{indent}  materials:\n{indent}    - material: {found}\n"

    return pattern.sub(replace, text)


def main() -> None:
    """Write the materials, give them to the tiles of the saved levels."""
    write_materials()
    for path in sorted((ROOT / "scenes").glob("level_*.owl")):
        path.write_text(assign(path.read_text()))


if __name__ == "__main__":
    main()
