"""
Buildings and props: materials and the solids of each object.

A building is described in tiles and units of height; ``solids.render`` turns it into albedo, normal and depth, and
``slice_object`` cuts it per footprint tile for the engine's D5 slices.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from PIL import Image

from .common import RGB, TILE_W, Layers, Vec3, fbm, hash01, mix, scale_color, value_noise
from .solids import Convex, Ellipsoid, Hit, Rendered, Shader, Solid, box, gable, pyramid, render

SLICE_HEIGHT = 128
"""Height in pixels, at zoom 1, of a building slice: the 32 pixels diamond plus 96 pixels of relief."""

STONE: RGB = (176.0, 170.0, 158.0)
BRICK_RED: RGB = (170.0, 84.0, 62.0)
PLASTER: RGB = (222.0, 204.0, 170.0)
FRAME: RGB = (238.0, 234.0, 222.0)


def _along(hit: Hit) -> float:
    """Coordinate along a vertical wall."""
    return hit.point[1] if abs(hit.normal[0]) > abs(hit.normal[1]) else hit.point[0]


def _glass(lz: float, lu: float) -> RGB:
    sky = mix((58.0, 92.0, 132.0), (170.0, 208.0, 232.0), 1.0 - lz)
    if abs((lu * 1.4 + lz) % 1.0 - 0.5) < 0.09:
        sky = mix(sky, (236.0, 244.0, 250.0), 0.45)
    return sky


def facade(
    base: RGB,
    bounds: tuple[float, float, float, float],
    floor_h: float,
    spacing: float,
    door: tuple[int, float, float, float] | None = None,
    window: tuple[float, float, float, float] = (0.3, 0.7, 0.3, 0.8),
    bricks: bool = True,
    dial: tuple[float, float, float] | None = None,
) -> Shader:
    """
    A wall material: bricks or plaster, a grid of windows per storey, an optional door and clock dial.

    :param base: Wall colour.
    :param bounds: Wall extent (x low, x high, y low, y high); windows keep clear of the corners.
    :param floor_h: Height of a storey.
    :param spacing: Width of a window bay.
    :param door: Optional (axis of the wall normal, centre along the wall, width, height) on the ground floor.
    :param window: Window rectangle inside a bay: (u low, u high, z low, z high) as fractions.
    :param bricks: Draw brick courses instead of plain plaster.
    :param dial: Optional clock (centre along the wall, centre height, radius) on the walls facing the viewer.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        if abs(h.normal[2]) > 0.5:
            return scale_color(base, 0.9)
        axis = 0 if abs(h.normal[0]) > abs(h.normal[1]) else 1
        u, z = _along(h), h.point[2]
        lo, hi = (bounds[2], bounds[3]) if axis == 0 else (bounds[0], bounds[1])
        color = base
        if bricks:
            row = math.floor(z / 0.125)
            col = u * 4.0 + (0.5 if row % 2 else 0.0)
            tone = 0.9 + 0.2 * hash01(h.seed, math.floor(col), row)
            color = scale_color(base, tone * (0.96 + 0.08 * hash01(h.seed + 1, h.ix, h.iy)))
            if col - math.floor(col) < 0.1 or z / 0.125 - row < 0.14:
                color = mix(color, (206.0, 200.0, 188.0), 0.5)
        else:
            color = scale_color(base, 0.94 + 0.1 * fbm(h.seed, u * 9.0, z * 9.0))
        if door is not None and door[0] == axis:
            _, centre, width, height = door
            if abs(u - centre) < width / 2.0 + 0.03 and z < height + 0.03:
                inside = abs(u - centre) < width / 2.0 and z < height
                if not inside:
                    return FRAME
                panel = 0.88 + 0.1 * math.sin((u - centre) * 40.0)
                if abs(u - (centre + width * 0.3)) < 0.012 and abs(z - height * 0.45) < 0.03:
                    return (226.0, 196.0, 90.0)
                return scale_color((96.0, 62.0, 38.0), panel)
        if dial is not None and axis in (0, 1) and abs(h.normal[2]) < 0.5:
            du, dz = u - dial[0], z - dial[1]
            radius = math.hypot(du, dz)
            if radius < dial[2]:
                angle = math.atan2(dz, du)
                if radius > dial[2] * 0.82:
                    return (50.0, 44.0, 40.0)
                if abs(math.sin(angle - 1.2)) < 0.07 and radius < dial[2] * 0.7 and math.cos(angle - 1.2) > 0:
                    return (30.0, 30.0, 34.0)
                if abs(math.sin(angle + 0.2)) < 0.05 and radius < dial[2] * 0.5 and math.cos(angle + 0.2) > 0:
                    return (30.0, 30.0, 34.0)
                return (240.0, 236.0, 220.0)
        if floor_h > 0 and spacing > 0 and lo + 0.1 < u < hi - 0.1:
            storey = math.floor(z / floor_h)
            lz = (z - storey * floor_h) / floor_h
            bay = (u - lo) / spacing
            lu = bay - math.floor(bay)
            if bay > 0.0 and math.floor(bay) < math.floor((hi - lo) / spacing):
                ulo, uhi, zlo, zhi = window
                if ulo - 0.05 < lu < uhi + 0.05 and zlo - 0.05 < lz < zhi + 0.05:
                    if ulo < lu < uhi and zlo < lz < zhi:
                        return _glass((lz - zlo) / (zhi - zlo), lu)
                    return FRAME
        return color

    return shade


def corrugated(base: RGB) -> Shader:
    """
    A ribbed sheet-metal wall.

    :param base: Sheet colour.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        rib = math.sin(_along(h) * 52.0)
        return scale_color(base, 0.9 + 0.1 * rib + 0.05 * hash01(h.seed, h.ix, h.iy))

    return shade


def roof_tiles(base: RGB, wall: Shader | None = None) -> Shader:
    """
    A pitched roof of staggered tiles; vertical faces (gable ends) use the wall material.

    :param base: Tile colour.
    :param wall: Material of the vertical faces, a plain tint when None.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        if abs(h.normal[2]) < 0.3:
            return wall(h) if wall is not None else scale_color(base, 0.85)
        slope = h.point[1] if abs(h.normal[1]) > abs(h.normal[0]) else h.point[0]
        ridge = h.point[0] if abs(h.normal[1]) > abs(h.normal[0]) else h.point[1]
        row = math.floor(slope * 10.0)
        col = ridge * 8.0 + (0.5 if row % 2 else 0.0)
        tone = 0.86 + 0.28 * hash01(h.seed + 9, math.floor(col), row)
        color = scale_color(base, tone)
        if slope * 10.0 - row < 0.2:
            color = scale_color(color, 0.72)
        if col - math.floor(col) < 0.08:
            color = scale_color(color, 0.85)
        return color

    return shade


def flat_top(base: RGB, wall: Shader) -> Shader:
    """
    A flat roof of tar and gravel over a wall material.

    :param base: Gravel colour.
    :param wall: Material of the vertical faces.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        if abs(h.normal[2]) < 0.5:
            return wall(h)
        return scale_color(base, 0.82 + 0.3 * hash01(h.seed, h.ix, h.iy))

    return shade


def awning(a: RGB, b: RGB) -> Shader:
    """
    A striped canvas awning.

    :param a: First stripe colour.
    :param b: Second stripe colour.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        return a if math.floor(h.point[0] * 6.0) % 2 == 0 else b

    return shade


def plain(color: RGB, grain: float = 0.08) -> Shader:
    """
    A flat colour with a little grain.

    :param color: The colour.
    :param grain: Amplitude of the per-pixel variation.
    :return: The shader.
    """
    return lambda h: scale_color(color, 1.0 - grain / 2.0 + grain * hash01(h.seed + 3, h.ix, h.iy))


def bark(h: Hit) -> RGB:
    """
    Tree bark: vertical streaks.

    :param h: The hit.
    :return: The colour.
    """
    streak = hash01(h.seed, math.floor((h.point[0] + h.point[1]) * 60.0), 0)
    return scale_color((104.0, 70.0, 42.0), 0.75 + 0.5 * streak)


def foliage(base: RGB) -> Shader:
    """
    Leaves: clumps of noise and bright specks.

    :param base: Leaf colour.
    :return: The shader.
    """

    def shade(h: Hit) -> RGB:
        p = h.point
        n = value_noise(h.seed, p[0] * 13.0 + p[2] * 7.0, p[1] * 13.0 + p[2] * 11.0)
        color = scale_color(base, 0.72 + 0.5 * n)
        r = hash01(h.seed + 4, h.ix, h.iy)
        if r > 0.93:
            color = scale_color(color, 1.22)
        elif r < 0.07:
            color = scale_color(color, 0.8)
        return color

    return shade


@dataclass(frozen=True)
class ObjectSpec:
    """
    One building or prop.
    """

    name: str
    """File name stem."""
    footprint: tuple[int, int]
    """Width and depth in tiles."""
    height_px: int
    """Image height at zoom 1."""
    solids: Callable[[], list[Solid]]
    """The shapes."""
    seed: int
    """Seed of the object's textures."""


def _cottage() -> list[Solid]:
    wall = facade(PLASTER, (0.14, 0.86, 0.14, 0.86), 0.85, 0.36, door=(1, 0.5, 0.2, 0.62), bricks=False)
    return [
        box((0.14, 0.14, 0.0), (0.86, 0.86, 1.7), wall),
        gable((0.07, 0.07, 1.7), (0.93, 0.93, 0.0), 2.7, True, roof_tiles((176.0, 66.0, 52.0), wall)),
        box((0.64, 0.28, 1.9), (0.76, 0.4, 2.85), plain(BRICK_RED)),
    ]


def _townhouse() -> list[Solid]:
    wall = facade(BRICK_RED, (0.08, 1.92, 0.1, 0.9), 1.0, 0.46, door=(1, 1.65, 0.24, 0.7))
    return [
        box((0.08, 0.1, 0.0), (1.92, 0.9, 3.0), wall),
        box((0.04, 0.06, 3.0), (1.96, 0.94, 3.16), flat_top((96.0, 94.0, 100.0), plain(STONE))),
        box((0.14, 0.9, 0.72), (1.34, 1.0, 0.8), awning((196.0, 52.0, 48.0), (240.0, 236.0, 226.0))),
    ]


def _warehouse() -> list[Solid]:
    wall = corrugated((142.0, 150.0, 158.0))
    brick = facade((156.0, 150.0, 140.0), (0.1, 1.9, 0.1, 1.9), 1.5, 0.6, door=(1, 1.0, 0.7, 1.05), bricks=False)
    return [
        box((0.1, 0.1, 0.0), (1.9, 1.9, 0.5), plain((120.0, 114.0, 106.0))),
        box((0.1, 0.1, 0.5), (1.9, 1.9, 1.5), _mix_shader(wall, brick)),
        gable((0.04, 0.04, 1.5), (1.96, 1.96, 0.0), 2.4, True, roof_tiles((96.0, 112.0, 130.0), wall)),
    ]


def _mix_shader(corrugated_wall: Shader, plain_wall: Shader) -> Shader:
    """Corrugated sheet, with the loading door (a plain facade) on the front."""

    def shade(h: Hit) -> RGB:
        if h.normal[1] > 0.5 and abs(h.point[0] - 1.0) < 0.4 and h.point[2] < 1.05:
            return plain_wall(h)
        return corrugated_wall(h)

    return shade


def _station() -> list[Solid]:
    wall = facade(
        PLASTER, (0.1, 2.9, 0.05, 0.5), 1.4, 0.7, door=(1, 1.5, 0.3, 0.7), bricks=False, window=(0.25, 0.75, 0.25, 0.6)
    )
    solids: list[Solid] = [
        box((0.0, 0.0, 0.0), (3.0, 1.0, 0.12), plain((150.0, 148.0, 144.0))),
        box((0.1, 0.05, 0.12), (2.9, 0.5, 1.4), wall),
        gable((0.04, 0.0, 1.4), (2.96, 0.56, 0.0), 1.95, True, roof_tiles((120.0, 70.0, 56.0), wall)),
        box((0.0, 0.5, 1.5), (3.0, 1.0, 1.62), plain((78.0, 90.0, 104.0))),
    ]
    solids += [box((x - 0.03, 0.92, 0.12), (x + 0.03, 0.98, 1.5), plain((70.0, 74.0, 80.0))) for x in (0.2, 1.5, 2.8)]
    return solids


def _tower() -> list[Solid]:
    stone = facade(
        BRICK_RED, (0.18, 0.82, 0.18, 0.82), 1.0, 0.32, door=(1, 0.5, 0.2, 0.6), window=(0.35, 0.65, 0.3, 0.8)
    )
    belfry = facade(STONE, (0.26, 0.74, 0.26, 0.74), 0.0, 0.0, bricks=False, dial=(0.5, 3.45, 0.2))
    return [
        box((0.18, 0.18, 0.0), (0.82, 0.82, 3.0), stone),
        box((0.26, 0.26, 3.0), (0.74, 0.74, 3.9), belfry),
        pyramid((0.2, 0.2, 3.9), (0.8, 0.8, 0.0), 4.85, roof_tiles((70.0, 110.0, 96.0))),
    ]


def _oak() -> list[Solid]:
    leaves = foliage((62.0, 132.0, 52.0))
    light_leaves = foliage((82.0, 156.0, 62.0))
    return [
        box((0.44, 0.44, 0.0), (0.56, 0.56, 1.4), bark),
        Ellipsoid((0.5, 0.5, 2.1), (0.44, 0.44, 0.62), leaves),
        Ellipsoid((0.36, 0.6, 1.75), (0.3, 0.3, 0.4), leaves),
        Ellipsoid((0.64, 0.38, 2.6), (0.3, 0.3, 0.42), light_leaves),
        Ellipsoid((0.6, 0.62, 2.35), (0.28, 0.28, 0.36), light_leaves),
    ]


def _pine() -> list[Solid]:
    leaves = foliage((38.0, 96.0, 62.0))
    return [
        box((0.45, 0.45, 0.0), (0.55, 0.55, 0.9), bark),
        Ellipsoid(
            (
                0.5,
                0.5,
                1.0,
            ),
            (0.36, 0.36, 0.5),
            leaves,
        ),
        Ellipsoid((0.5, 0.5, 1.7), (0.3, 0.3, 0.45), leaves),
        Ellipsoid((0.5, 0.5, 2.35), (0.23, 0.23, 0.4), leaves),
        Ellipsoid((0.5, 0.5, 2.9), (0.15, 0.15, 0.35), leaves),
    ]


def _vehicle(length: float, width: float, body: RGB, cargo: RGB | None, heading: int) -> list[Solid]:
    """
    A car or a truck standing at the centre of its tile, heading along one of the eight ground directions.

    The engine puts the front corner of the tile on the entity, so a vehicle at world position ``p`` is drawn centred on
    that tile whatever its heading: the entity sits at ``p + (0.5, 0.5)``.

    :param length: Length along its heading, in tiles.
    :param width: Width across its heading, in tiles.
    :param body: Paint of the body.
    :param cargo: Colour of a cargo box behind the cab, None for a car.
    :param heading: Heading in steps of 45 degrees counted from +x towards +y (0: +x, lower right; 2: +y, lower left).
    :return: The solids.
    """
    glass = plain((58.0, 78.0, 104.0), 0.04)
    tyre = plain((34.0, 34.0, 38.0), 0.04)
    headlight = plain((252.0, 238.0, 170.0), 0.0)
    taillight = plain((198.0, 30.0, 28.0), 0.0)
    angle = math.radians(45.0 * heading)
    forward = (math.cos(angle), math.sin(angle))
    side = (-forward[1], forward[0])

    def oriented(
        low: float, high: float, side_low: float, side_high: float, bottom: float, top: float, shader: Shader
    ) -> Solid:
        """A box from ``low`` to ``high`` along the heading (rear to front) and across it, around the tile centre."""
        a_low, a_high = low - length / 2.0, high - length / 2.0
        c_low, c_high = side_low - width / 2.0, side_high - width / 2.0
        centre = (0.5, 0.5)
        along = forward[0] * centre[0] + forward[1] * centre[1]
        across = side[0] * centre[0] + side[1] * centre[1]
        planes: list[tuple[Vec3, float]] = [
            ((forward[0], forward[1], 0.0), a_high + along),
            ((-forward[0], -forward[1], 0.0), -a_low - along),
            ((side[0], side[1], 0.0), c_high + across),
            ((-side[0], -side[1], 0.0), -c_low - across),
            ((0.0, 0.0, 1.0), top),
            ((0.0, 0.0, -1.0), -bottom),
        ]
        return Convex(planes, top, shader)

    solids: list[Solid] = [
        oriented(0.0, length, 0.0, width, 0.07, 0.3, plain(body)),
        oriented(length * 0.08, length * 0.2, -0.02, width + 0.02, 0.0, 0.14, tyre),
        oriented(length * 0.72, length * 0.86, -0.02, width + 0.02, 0.0, 0.14, tyre),
    ]
    if cargo is None:
        solids.append(oriented(length * 0.22, length * 0.62, 0.03, width - 0.03, 0.3, 0.5, glass))
    else:
        solids.append(oriented(length * 0.62, length * 0.94, 0.03, width - 0.03, 0.3, 0.5, glass))
        solids.append(oriented(0.0, length * 0.58, 0.0, width, 0.3, 0.58, plain(cargo)))
    for low, high in ((0.02, 0.08), (width - 0.08, width - 0.02)):
        solids.append(oriented(length - 0.02, length + 0.012, low, high, 0.12, 0.22, headlight))
        solids.append(oriented(-0.012, 0.02, low, high, 0.12, 0.22, taillight))
    return solids


VEHICLE_HEADINGS = 8
"""Headings drawn per vehicle, every 45 degrees from +x towards +y (frame ``i`` heads along ``45 * i`` degrees)."""

VEHICLE_MODELS: list[tuple[str, float, float, RGB, RGB | None]] = [
    ("car", 0.55, 0.26, (196.0, 52.0, 48.0), None),
    ("truck", 0.85, 0.32, (62.0, 108.0, 168.0), (226.0, 222.0, 210.0)),
]
"""The vehicle models: name, length, width, body paint and cargo colour."""


def _vehicle_spec(model: int, heading: int) -> ObjectSpec:
    name, length, width, body, cargo = VEHICLE_MODELS[model]
    return ObjectSpec(
        f"{name}_{heading}",
        (1, 1),
        64,
        lambda: _vehicle(length, width, body, cargo, heading),
        301 + model * VEHICLE_HEADINGS + heading,
    )


VEHICLES: list[ObjectSpec] = [
    _vehicle_spec(model, heading) for model in range(len(VEHICLE_MODELS)) for heading in range(VEHICLE_HEADINGS)
]
"""Vehicles, one cell each: one row of ``VEHICLE_HEADINGS`` headings per model in the vehicle sheet."""

BUILDINGS: list[ObjectSpec] = [
    ObjectSpec("cottage", (1, 1), 80, _cottage, 101),
    ObjectSpec("townhouse", (2, 1), 104, _townhouse, 102),
    ObjectSpec("warehouse", (2, 2), 104, _warehouse, 103),
    ObjectSpec("station", (3, 1), 96, _station, 104),
    ObjectSpec("tower", (1, 1), 112, _tower, 105),
]
"""The buildings, sliced per footprint tile into the building sheet."""


def _marker() -> list[Solid]:
    return [
        box((0.47, 0.47, 0.0), (0.53, 0.53, 1.0), plain((226.0, 226.0, 230.0))),
        box((0.53, 0.47, 0.6), (0.92, 0.53, 1.0), plain((214.0, 58.0, 48.0))),
    ]


PROPS: list[ObjectSpec] = [
    ObjectSpec("oak", (1, 1), 96, _oak, 201),
    ObjectSpec("pine", (1, 1), 96, _pine, 202),
    ObjectSpec("marker", (1, 1), 64, _marker, 203),
]
"""Single-sprite props."""


def render_object(spec: ObjectSpec, seed: int, scale: int) -> Rendered:
    """
    Render an object.

    :param spec: The object.
    :param seed: Global seed of the run.
    :param scale: Zoom factor.
    :return: The rendered layers and footprint cells.
    """
    solids = spec.solids()
    top = max(s.top() for s in solids)
    assert top * 16.0 + 16.0 * sum(spec.footprint) <= spec.height_px, f"{spec.name}: taller than its canvas"
    return render(solids, spec.footprint, spec.height_px, seed * 1000 + spec.seed, scale)


def slice_object(spec: ObjectSpec, rendered: Rendered) -> dict[tuple[int, int], tuple[Layers, float]]:
    """
    Cut a rendered object into one sprite per footprint tile.

    Each slice is a tile cell (64 px wide, ``SLICE_HEIGHT`` high at zoom 1) whose bottom centre is the front corner
    of its tile; only the pixels whose hit point stands over that tile are kept.

    :param spec: The object.
    :param rendered: Its whole rendering.
    :return: For each footprint tile (i, j), the slice and the height of its highest point.
    """
    w, d = spec.footprint
    scale = rendered.scale
    ox, oy = rendered.origin
    source = rendered.layers
    slices: dict[tuple[int, int], tuple[Layers, float]] = {}
    images = (source.albedo, source.normal, source.depth)
    for j in range(d):
        for i in range(w):
            cell = Layers(TILE_W * scale, SLICE_HEIGHT * scale)
            left = round((ox + 32.0 * (i - j - 1)) * scale)
            top = round((oy + 16.0 * (i + j + 2) - SLICE_HEIGHT) * scale)
            mask = Image.new("L", (source.width, source.height), 0)
            mask_px = mask.load()
            assert mask_px is not None
            for (x, y), owner in rendered.cells.items():
                if owner == (i, j):
                    mask_px[x, y] = 255
            box_ = (left, top, left + TILE_W * scale, top + SLICE_HEIGHT * scale)
            for target, image in zip((cell.albedo, cell.normal, cell.depth), images, strict=True):
                target.paste(image.crop(box_), (0, 0), mask.crop(box_))
            slices[(i, j)] = (cell, rendered.zmax.get((i, j), 0.0))
    return slices


def smoke(scale: int, seed: int) -> Layers:
    """
    A soft, translucent smoke puff of 48x48 pixels at zoom 1 (albedo only is meaningful; the normal faces the viewer).

    :param scale: Zoom factor.
    :param seed: Global seed of the run.
    :return: The layers.
    """
    size = 48 * scale
    layers = Layers(size, size)
    albedo = layers.albedo.load()
    assert albedo is not None
    for y in range(size):
        for x in range(size):
            fx, fy = (x + 0.5) / scale, (y + 0.5) / scale
            wobble = 1.0 + 0.28 * (value_noise(seed + 301, fx * 0.16, fy * 0.16) - 0.5)
            d = math.hypot(fx - 24.0, fy - 24.0) / 24.0 / wobble
            alpha = max(0.0, 1.0 - d) ** 1.4
            tone = (
                224.0 + 20.0 * value_noise(seed + 302, fx * 0.2, fy * 0.2) - 28.0 * max(0.0, (fy - 24.0) / 24.0) * 0.5
            )
            albedo[x, y] = (int(tone), int(tone), int(tone + 5), int(alpha * 255))
    alpha_channel = layers.albedo.getchannel("A")
    layers.normal.paste((128, 128, 255, 255), (0, 0, size, size))
    layers.depth.paste((0, 0, 0, 255), (0, 0, size, size))
    layers.normal.putalpha(alpha_channel)
    layers.depth.putalpha(alpha_channel)
    return layers
