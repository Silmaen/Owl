"""
The moon of the Lunar lander sample game: its size, its gravity, its five landing pads and the five landings, shared
by the model, level and menu generators of ``tools/samples/lunar_lander/``.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

MOON_RADIUS = 160.0
"""Radius of the moon at its mean ground, in world units (metres)."""
GRAVITY = 1.62
"""Pull of the moon at its surface, in units per second squared."""

Vec3 = tuple[float, float, float]


def direction(latitude: float, longitude: float) -> Vec3:
    """Unit vector from the centre of the moon at a latitude and a longitude in degrees (Y is the north pole)."""
    lat, lon = math.radians(latitude), math.radians(longitude)
    return math.cos(lat) * math.cos(lon), math.sin(lat), -math.cos(lat) * math.sin(lon)


@dataclass(frozen=True)
class Pad:
    """A landing pad: its name on the HUD, where it stands and the ground flattened around it."""

    tag: str
    name: str
    latitude: float
    longitude: float
    flat: float = 14.0

    @property
    def up(self) -> Vec3:
        """Local vertical of the pad."""
        return direction(self.latitude, self.longitude)


PADS = [
    Pad("Pad1", "TRANQUILITY BASE", 10.0, 0.0),
    Pad("Pad2", "HADLEY RILLE", 24.0, 26.0),
    Pad("Pad3", "COPERNICUS RIM", -14.0, 58.0),
    Pad("Pad4", "SHACKLETON RIDGE", -38.0, 128.0),
    Pad("Pad5", "DAEDALUS CRATER", 6.0, 196.0, 12.0),
]
"""The five pads, one per landing."""


@dataclass(frozen=True)
class Landing:
    """A landing: its pad, where the lander starts (height above the pad, arc of ground before it along the approach
    heading, sideways offset) and how fast it moves, its fuel and how hard its legs are."""

    pad: int
    height: float
    downrange: float
    heading: float
    speed: float
    fuel: float
    crossrange: float = 0.0
    orbit: bool = False


LANDINGS = [
    Landing(0, 28.0, 0.0, 0.0, 0.0, 45.0),
    Landing(1, 55.0, 60.0, 30.0, 4.5, 55.0),
    Landing(2, 95.0, 230.0, -50.0, 9.0, 75.0, 6.0),
    Landing(3, 45.0, 260.0, 80.0, 0.0, 95.0, 0.0, True),
    Landing(4, 70.0, 420.0, -20.0, 0.0, 110.0, 14.0, True),
]
"""The five landings, of rising difficulty: a hover descent, a short glide, a long approach, two from orbit."""
