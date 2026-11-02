"""
Layout of the rooms of The Moon Lantern, in pixels of their 320 x 180 backdrops (y down), shared by the art
(``generate_assets``) and the scenes (``generate_scenes``).

A room shows 16 x 9 world units: one unit is 20 pixels, the world origin is the centre of the backdrop. The floor of a
room is its walk area (an outline and holes), scaled by depth between the back of the floor (``far``) and its front
(``near``); the walk-behind masks are parts of the backdrop drawn over the actors whose feet are above their baseline.
"""

from __future__ import annotations

from dataclasses import dataclass, field

WIDTH, HEIGHT = 320, 180
"""Size of a backdrop, in pixels."""
PIXELS_PER_UNIT = 20
"""Pixels of a backdrop in one world unit."""

Point = tuple[float, float]
Rect = tuple[int, int, int, int]


def world(point: Point) -> tuple[float, float]:
    """The world point of a pixel of a backdrop."""
    half_width, half_height = WIDTH / PIXELS_PER_UNIT / 2, HEIGHT / PIXELS_PER_UNIT / 2
    return point[0] / PIXELS_PER_UNIT - half_width, half_height - point[1] / PIXELS_PER_UNIT


def world_y(pixel_y: float) -> float:
    """The world height of a row of a backdrop."""
    return HEIGHT / PIXELS_PER_UNIT / 2 - pixel_y / PIXELS_PER_UNIT


def rect_ring(rect: Rect) -> list[Point]:
    """The four corners of a rectangle."""
    x0, y0, x1, y1 = rect
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


@dataclass
class Hotspot:
    """Something the cursor names: a rectangle, what a click does, where the hero stands to do it (the scene an exit
    leads to is a property of the room script)."""

    id: str
    rect: Rect
    verb: str
    stand: Point


@dataclass
class Prop:
    """A sprite cut from the art of a room (shown, hidden or animated by its script)."""

    name: str
    rect: Rect
    frames: int = 1


@dataclass
class Mask:
    """A part of the backdrop drawn over the actors whose feet are above its baseline (a pixel row)."""

    name: str
    rect: Rect
    baseline: int


@dataclass
class Room:
    """A room: its floor, its depth, its hotspots, props, masks, entry points and characters."""

    name: str
    outline: list[Point]
    holes: list[list[Point]]
    near: int
    far: int
    far_scale: float
    hotspots: list[Hotspot]
    props: list[Prop] = field(default_factory=list)
    masks: list[Mask] = field(default_factory=list)
    entries: dict[str, Point] = field(default_factory=dict)
    npcs: dict[str, Point] = field(default_factory=dict)


COTTAGE = Room(
    name="cottage",
    outline=[(20, 118), (280, 118), (284, 128), (318, 134), (318, 178), (2, 178), (2, 130)],
    holes=[rect_ring((74, 140, 146, 160)), rect_ring((190, 122, 210, 131))],
    near=178,
    far=118,
    far_scale=0.74,
    hotspots=[
        Hotspot("jar", (57, 37, 71, 51), "pick", (64, 122)),
        Hotspot("crust", (93, 121, 117, 133), "pick", (106, 171)),
        Hotspot("barnaby", (186, 70, 214, 128), "talk", (172, 132)),
        Hotspot("lantern", (285, 50, 317, 110), "use", (272, 134)),
        Hotspot("door", (6, 34, 38, 114), "exit", (26, 124)),
        Hotspot("window", (118, 28, 172, 82), "look", (145, 122)),
        Hotspot("fireplace", (218, 38, 292, 116), "look", (252, 124)),
        Hotspot("shelf", (28, 38, 92, 58), "look", (60, 122)),
        Hotspot("table", (68, 120, 152, 166), "look", (110, 172)),
    ],
    props=[
        Prop("jar", (57, 37, 71, 51)),
        Prop("crust", (93, 121, 117, 133)),
        Prop("lantern", (285, 50, 317, 110), 2),
        Prop("fire", (236, 84, 274, 114), 3),
    ],
    masks=[Mask("table", (66, 118, 154, 168), 140)],
    entries={"start": (150, 150), "glade": (28, 126)},
    npcs={"barnaby": (200, 128)},
)
"""Barnaby's cottage: the Moon Lantern, the shelf with the jar, the pie crust on the table, the door to the glade."""

GLADE = Room(
    name="glade",
    outline=[(2, 126), (200, 122), (304, 128), (318, 128), (318, 178), (2, 178)],
    holes=[rect_ring((98, 164, 126, 175))],
    near=178,
    far=122,
    far_scale=0.72,
    hotspots=[
        Hotspot("fireflies", (212, 68, 270, 104), "use", (236, 132)),
        Hotspot("bush", (26, 84, 86, 122), "pick", (58, 132)),
        Hotspot("mushrooms", (144, 112, 186, 128), "look", (164, 138)),
        Hotspot("pond", (204, 104, 302, 124), "look", (252, 134)),
        Hotspot("to_cottage", (0, 108, 26, 178), "exit", (10, 150)),
        Hotspot("to_bridge", (294, 108, 319, 178), "exit", (312, 150)),
    ],
    props=[Prop("berries", (34, 92, 78, 116)), Prop("fireflies", (212, 68, 270, 104), 4)],
    masks=[Mask("trunk", (88, 0, 136, 178), 168)],
    entries={"cottage": (20, 150), "bridge": (300, 150)},
)
"""The glade: the berry bush, the fireflies over the pond, the mushroom ring, the old oak in front."""

BRIDGE = Room(
    name="bridge",
    outline=[
        (2, 148),
        (100, 148),
        (104, 124),
        (236, 124),
        (236, 137),
        (128, 137),
        (132, 148),
        (165, 148),
        (165, 178),
        (2, 178),
    ],
    holes=[],
    near=178,
    far=124,
    far_scale=0.76,
    hotspots=[
        Hotspot("moonstone", (168, 124, 184, 136), "pick", (158, 132)),
        Hotspot("troll", (164, 82, 192, 134), "talk", (150, 132)),
        Hotspot("sign", (38, 104, 66, 146), "look", (58, 156)),
        Hotspot("garden", (240, 86, 320, 122), "look", (226, 130)),
        Hotspot("stream", (166, 146, 320, 180), "look", (150, 166)),
        Hotspot("to_glade", (0, 138, 26, 178), "exit", (10, 162)),
    ],
    props=[Prop("moonstone", (168, 124, 184, 136))],
    masks=[Mask("parapet", (98, 132, 242, 152), 138)],
    entries={"glade": (20, 162)},
    npcs={"troll": (176, 131), "troll_aside": (226, 130)},
)
"""The troll bridge: the troll sitting on the moonstone, the toll sign, the stream, his very private garden."""

ROOMS = (COTTAGE, GLADE, BRIDGE)
"""Every room."""

ITEMS = ("jar", "firefly_jar", "berries", "crust", "pie", "moonstone")
"""The objects of the inventory, in the order of their icons."""
