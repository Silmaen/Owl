"""
The scene of the isometric town demo (``scenes/iso_town.owl``), written from the map data of ``city_map``.

The scene is the map's entities (buildings, trees, chimneys, lamps), the vehicles with their engine sounds, the life of
the town (smoke, sparkles on the water, blinking lights, birds, a day and night tint) and the scripts that drive them.
"""

from __future__ import annotations

import random
from dataclasses import dataclass
from pathlib import Path

from . import city_map
from .city_map import SIZE, City, Plot, build_city, cell_corners, navigation_overrides, surface_z

CAMERA_START = (32.0, 30.0)
"""Where the camera looks at the start (the plaza of the town centre)."""
FLEET_SIZE = 12
"""Vehicles of the town."""
CAR_TINTS = [
    (1.0, 1.0, 1.0),
    (0.45, 0.9, 0.55),
    (1.0, 0.72, 0.35),
    (0.55, 0.75, 1.0),
    (1.0, 0.55, 0.6),
    (0.92, 0.9, 0.5),
    (0.75, 0.6, 0.95),
    (0.6, 0.95, 0.9),
]
"""Colour multipliers of the cars (the sheet is painted red)."""
TRUCK_TINTS = [(1.0, 1.0, 1.0), (0.78, 0.85, 1.0), (1.0, 0.85, 0.68), (0.8, 1.0, 0.8)]
"""Colour multipliers of the trucks."""
SPARKLES = 56
"""Glints on the water."""
BIRDS = 7
"""Gulls crossing the screen."""
TINTS = 5
"""Layers of the night tint, each one adding a bit of darkness."""
BEACONS = 5
"""Blinking red lights on the towers."""


class _Writer:
    """Accumulates the entities of the scene with consecutive identifiers."""

    def __init__(self) -> None:
        self.parts: list[str] = []
        self.next_id = 4300000000000000001

    def add(
        self,
        tag: str,
        position: tuple[float, float, float],
        body: str,
        scale: tuple[float, float, float] = (1, 1, 1),
        layer: str | None = "iso_world",
    ) -> None:
        """
        Append one entity.

        :param tag: Its tag.
        :param position: Translation.
        :param body: The component lines, already indented by two spaces.
        :param scale: Scale.
        :param layer: Renderer tag, ``None`` for none.
        """
        pos = ", ".join(_num(v) for v in position)
        sc = ", ".join(_num(v) for v in scale)
        text = (
            f"- Entity: {self.next_id}\n  Tag: {{tag: {tag}}}\n  Transform:\n    translation: [{pos}]\n"
            f"    rotation: [0, 0, 0]\n    scale: [{sc}]\n{body}  Visibility: {{gameVisible: true}}\n"
        )
        if layer is not None:
            text += f"  RendererTag: {{Name: {layer}}}\n"
        self.parts.append(text)
        self.next_id += 1

    def text(self) -> str:
        return "\n".join(self.parts)


def _num(value: float) -> str:
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text if text not in ("", "-0") else "0"


def _sprite(texture: str, color: tuple[float, ...] = (1, 1, 1, 1)) -> str:
    col = ", ".join(_num(v) for v in color)
    tex = f"    texture: nam:textures/iso/{texture}.png\n" if texture else ""
    return f"  SpriteRenderer:\n    color: [{col}]\n{tex}    tilingFactor: 1\n"


def _script(path: str, properties: dict[str, tuple[str, object]] | None = None) -> str:
    text = f"  LuaScript:\n    scriptPath: scripts/iso/{path}\n"
    if properties:
        text += "    properties:\n"
        for name, (kind, value) in properties.items():
            shown = f'"{value}"' if kind == "string" else _num(float(value))  # type: ignore[arg-type]
            text += f"    - name: {name}\n      type: {kind}\n      value: {shown}\n"
    return text


def _sound(
    asset: str, volume: float, *, loop: bool, spatial: bool, pitch: float = 1.0, max_distance: float = 14.0
) -> str:
    category = "Ambient" if spatial or loop else "SFX"
    return (
        f"  SoundSource:\n    soundAsset: {asset}\n    category: {category}\n    volume: {_num(volume)}\n"
        f"    pitch: {_num(pitch)}\n    loop: {str(loop).lower()}\n    spatial: {str(spatial).lower()}\n"
        f"    playOnStart: true\n    maxDistance: {_num(max_distance)}\n    rolloff: 1.5\n"
    )


def _pick_starts(city: City, rng: random.Random) -> list[tuple[int, int]]:
    """Cells where the vehicles start: spread over the districts, on straight roads that are not one way."""
    overrides = {cell for cell, _ in navigation_overrides()}
    straight = [
        cell
        for cell in sorted(city.roads)
        if cell not in overrides and city_map.road_mask(city.roads, *cell) in (0b1010, 0b0101)
    ]
    starts: list[tuple[int, int]] = []
    attempts = 0
    while len(starts) < FLEET_SIZE and attempts < 5000:
        attempts += 1
        cell = rng.choice(straight)
        if all(abs(cell[0] - sx) + abs(cell[1] - sy) >= 12 for sx, sy in starts):
            starts.append(cell)
    return starts


@dataclass(frozen=True)
class Vehicle:
    """
    One vehicle of the fleet.
    """

    tag: str
    """Tag of its sprite entity."""
    model: int
    """First frame of its model in the vehicle sheet: 0 for a car, 8 for a truck."""
    speed: float
    """Speed in cells per second."""
    start: tuple[int, int]
    """Cell where it starts."""
    tint: tuple[float, float, float]
    """Colour multiplier of its sprite."""
    seed: int
    """Seed of its destinations."""


def vehicles(city: City, seed: int) -> list[Vehicle]:
    """
    The fleet: which sprite model, where it starts and how fast it drives.

    :param city: The town.
    :param seed: Seed of the run.
    :return: The vehicles.
    """
    rng = random.Random(900 + seed)
    starts = _pick_starts(city, rng)
    fleet: list[Vehicle] = []
    for index, start in enumerate(starts):
        heavy = city_map.district(*start) in ("industrial", "harbour") or index % 4 == 3
        tints = TRUCK_TINTS if heavy else CAR_TINTS
        fleet.append(
            Vehicle(
                tag=f"Vehicle{index + 1:02d}",
                model=8 if heavy else 0,
                speed=round(rng.uniform(1.9, 2.4) if heavy else rng.uniform(2.6, 3.4), 1),
                start=start,
                tint=tints[(index // 2) % len(tints)],
                seed=11 + 13 * index,
            )
        )
    return fleet


def _overrides_text() -> str:
    lines = []
    for (x, y), entry in navigation_overrides():
        fields = [f"cell: [{x}, {y}]"]
        for key in ("noExit", "noEnter"):
            if key in entry:
                fields.append(f"{key}: [{', '.join(entry[key])}]")  # type: ignore[arg-type]
        if "cost" in entry:
            fields.append(f"cost: {entry['cost']}")
        lines.append("        - {" + ", ".join(fields) + "}")
    return "\n".join(lines) + "\n"


def _building(city: City, plot: Plot) -> tuple[float, float, float]:
    level = cell_corners(city.heights, *plot.cell)[0] * city_map.STEP_Z
    return (plot.cell[0], plot.cell[1], level)


def scene_text(seed: int = 1) -> str:
    """
    Build the ``iso_town`` scene.

    :param seed: Seed of the run, as for the map.
    :return: The scene file content.
    """
    city = build_city(seed)
    rng = random.Random(500 + seed)
    out = _Writer()
    heights = city.heights
    cx, cy = CAMERA_START
    out.add(
        "MainCamera",
        (cx, cy, 5),
        "  Camera:\n    camera: {projectionType: Orthographic, perspectiveFOV: 0.785398185,\n"
        "      perspectiveNear: 0.00999999978,\n"
        "      perspectiveFar: 1000, orthographicSize: 11, orthographicNear: -1, orthographicFar: 10}\n"
        "    primary: true\n    fixedAspectRatio: false\n  SoundListener:\n    primary: true\n"
        + _script("iso_camera.lua", {"speed": ("float", 14), "limit": ("float", SIZE)}),
        layer=None,
    )
    out.add(
        "Sky",
        (cx, cy, -5),
        "  BackgroundTexture:\n    mode: 0\n    type: 1\n    color: [0.16, 0.22, 0.3, 1]\n"
        "    topColor: [0.45, 0.62, 0.82, 1]\n",
        (400, 400, 1),
        "world",
    )
    clouds = [
        ("CloudNear", (6, 17), (9, 4.5), 0.8, (-6, 5, 0.5, 0.35)),
        ("CloudFar", (19, 20), (12, 6), 0.6, (7, 8, 0.3, 0.2)),
        ("CloudLow", (26, 8), (8, 4), 0.7, (14, -4, 0.4, 0.28)),
        ("CloudHigh", (-8, 4), (10, 4.5), 0.65, (-12, -7, 0.45, 0.3)),
        ("CloudWide", (30, -3), (14, 6), 0.55, (3, 3, 0.25, 0.22)),
        ("CloudSmall", (10, 30), (6, 3), 0.75, (-3, 10, 0.55, 0.4)),
    ]
    for tag, (x, y), (w, h), alpha, (ox, oy, par, speed) in clouds:
        out.add(
            tag,
            (x, y, -4),
            _sprite("smoke", (1, 1, 1, alpha))
            + _script(
                "iso_cloud.lua",
                {
                    "offset_x": ("float", ox),
                    "offset_y": ("float", oy),
                    "parallax": ("float", par),
                    "speed": ("float", speed),
                },
            ),
            (w, h, 1),
            "world",
        )
    out.add(
        "Ground",
        (0, 0, 0),
        "  Tilemap:\n    tilemapPath: tilemaps/iso_town.owltilemap\n    navigation:\n      mode: connections\n"
        "      layer: 1\n"
        "      source: overlay\n      maxRise: 1\n      climbCost: 0.5\n      allowSteep: false\n      overrides:\n"
        + _overrides_text(),
    )
    for plot in city.plots:
        out.add(
            plot.kind.capitalize(),
            _building(city, plot),
            "  IsometricObject:\n    atlasPath: textures/iso/buildings.json\n"
            f"    building: {plot.kind}\n    color: [1, 1, 1, 1]\n",
        )
    for kind, tx, ty in city.trees:
        out.add("Tree", (tx + 0.5, ty + 0.5, round(surface_z(heights, tx, ty), 3)), _sprite(kind), (1, 1.5, 1))
    for number, (hx, hy, hz) in enumerate(city.chimneys, 1):
        out.add(f"Chimney_{number}", (hx + 0.5, hy + 0.5, round(hz, 3)), _sprite("chimney"), (1, 1.5, 1))
        for puff in range(1, 5):
            out.add(
                f"Smoke_{number}_{puff}",
                (hx + 0.5, hy + 0.5, round(hz + 2.9, 3)),
                _sprite("smoke", (0.66, 0.66, 0.7, 0.85)),
                (0.5, 0.5, 1),
            )
    for number, (lx, ly) in enumerate(city.lamps, 1):
        z = round(surface_z(heights, lx, ly), 3)
        out.add(f"Lamp_{number}", (lx + 0.5, ly + 0.5, z), _sprite("lamp"), (1, 1.5, 1))
        out.add(
            f"LampGlow_{number}",
            (lx + 0.5, ly + 0.5, round(z + 1.62, 3)),
            _sprite("smoke", (1, 0.9, 0.5, 0.75)),
            (0.4, 0.4, 1),
        )
    towers = [p for p in city.plots if p.kind == "tower" and p.district == "centre"]
    rng.shuffle(towers)
    for number, plot in enumerate(sorted(towers[:BEACONS], key=lambda p: p.cell), 1):
        level = cell_corners(heights, *plot.cell)[0] * city_map.STEP_Z
        out.add(
            f"Beacon_{number}",
            (plot.cell[0] + 1.0, plot.cell[1] + 1.0, round(level + 4.9, 3)),
            _sprite("smoke", (1, 0.2, 0.15, 0.9)),
            (0.35, 0.35, 1),
        )
    # Glints on the water: the sea, the river and the pond.
    water = sorted(c for c in city.water if _open_water(city, c))
    for number in range(1, SPARKLES + 1):
        wx, wy = rng.choice(water)
        out.add(
            f"Sparkle_{number}",
            (wx + 0.5 + rng.random() * 0.8 + 0.5, wy + 0.5 + rng.random() * 0.8 + 0.5, 0.06),
            _sprite("smoke", (1, 1, 1, 0.55)),
            (0.7, 0.3, 1),
        )
    out.add("Cursor", (cx + 1, cy + 1, 0), _sprite("marker"))
    fleet = vehicles(city, seed)
    for index, vehicle in enumerate(fleet):
        sx, sy = vehicle.start
        tint = ", ".join(_num(v) for v in vehicle.tint)
        out.add(
            vehicle.tag,
            (sx + 0.5, sy + 0.5, round(surface_z(heights, sx + 0.5, sy + 0.5), 3)),
            f"  AnimatedSpriteRenderer:\n    color: [{tint}, 1]\n    texture: nam:textures/iso/vehicles.png\n"
            f"    columns: 8\n    rows: 2\n    firstFrame: {vehicle.model}\n    lastFrame: {vehicle.model}\n"
            "    frameDuration: 0.1\n    loop: true\n"
            + _sound(
                "sounds/town/engine.wav",
                0.45 if vehicle.model == 0 else 0.6,
                loop=True,
                spatial=True,
                pitch=round(0.9 + 0.04 * (index % 6) + (-0.2 if vehicle.model else 0.0), 2),
            ),
        )
    # The 2D overlay: gulls crossing the screen and the night tint, pinned to the camera by `iso_town_life.lua`.
    for number in range(1, BIRDS + 1):
        out.add(
            f"Bird_{number}",
            (0, 0, 0.5),
            "  AnimatedSpriteRenderer:\n    color: [1, 1, 1, 1]\n    texture: nam:textures/iso/birds.png\n"
            "    columns: 2\n    rows: 1\n    firstFrame: 0\n    lastFrame: 1\n"
            f"    frameDuration: {_num(0.16 + 0.02 * (number % 3))}\n    loop: true\n",
            (1.1, 0.64, 1),
            "overlay",
        )
    for number in range(1, TINTS + 1):
        out.add(
            f"Tint_{number}",
            (cx, cy, 0.9),
            "  SpriteRenderer:\n    color: [0.04, 0.07, 0.22, 0.07]\n    tilingFactor: 1\n",
            (80, 80, 1),
            "overlay",
        )
    fleet_text = ";".join(f"{v.tag},{v.model},{v.speed},{v.start[0]},{v.start[1]},{v.seed}" for v in fleet)
    out.add(
        "Manager",
        (0, 0, 0),
        _script("iso_town.lua", {"map_size": ("float", SIZE), "target_trips": ("float", TARGET_TRIPS)}),
        layer=None,
    )
    out.add(
        "Traffic",
        (0, 0, 0),
        _script(
            "iso_town_traffic.lua",
            {"map_size": ("float", SIZE), "max_trip": ("float", MAX_TRIP), "fleet": ("string", fleet_text)},
        ),
        layer=None,
    )
    out.add("Life", (0, 0, 0), _script("iso_town_life.lua"), layer=None)
    out.add(
        "Ambience",
        (cx, cy, 0),
        _sound("sounds/town/ambience.wav", 0.3, loop=True, spatial=False),
        layer=None,
    )
    out.add(
        "Waves",
        (SIZE - 4.0, 30.0, 0.3),
        _sound("sounds/town/waves.wav", 0.5, loop=True, spatial=True, max_distance=26.0),
        layer=None,
    )
    out.add(
        "Yard",
        (22.0, 50.0, 0.5),
        _sound("sounds/town/yard.wav", 0.4, loop=True, spatial=True, max_distance=20.0),
        layer=None,
    )
    header = (
        "Scene: IsoTown\nHud: huds/iso_town.owlhud\nEnabledRenderers:\n- {Name: raycast_world, Enabled: false}\n"
        "- {Name: world, Enabled: true}\n- Name: iso_world\n  Enabled: true\n  Overrides: {Zoom: 0}\n"
        "- Name: ui\n  Enabled: true\n  Overrides: {Space: Screen}\n- {Name: overlay, Enabled: true}\nEntities:\n"
    )
    return header + out.text()


TARGET_TRIPS = 100
"""Trips of the objective: reached in about two minutes of driving by the fleet, well inside a play of five."""
MAX_TRIP = 34
"""Longest trip asked of a vehicle, in cells (Manhattan)."""


def _open_water(city: City, cell: tuple[int, int]) -> bool:
    """Water cells with open water around them, so a glint never sits on the shore."""
    x, y = cell
    return all((x + dx, y + dy) in city.water for dx in (-1, 0, 1) for dy in (-1, 0, 1))


def write_city_scene(project: Path, seed: int) -> Path:
    """
    Write the town scene.

    :param project: The project folder.
    :param seed: Seed of the run.
    :return: The written file.
    """
    path = project / "scenes" / "iso_town.owl"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(scene_text(seed))
    return path
