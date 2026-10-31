#!/usr/bin/env python3
"""Build ``tilemaps/world_map.owltilemap``: the hub of the Engine Demo, a small RPG village of 64 x 40 cells, and
``tilemaps/menu_village.owltilemap``, the strip of the village in front of the main menu.

The map is centred on the entity ``WorldTilemap`` at (32, -20), so the cell (c, r) (column, row from the top) has its
centre at the world position (c + 0.5, -r - 0.5). Layers: ``ground`` (grass, roads, plaza, fields, the river and its
bridges), ``objects`` (trees, houses, the church, stalls, fences, lamps: what the tileset marks collidable blocks the
hero) and the hidden ``walkable`` layer, the navigation graph of the villagers (one cell wherever the hero can walk).

Layout, north to south: the forest edge with the cave (north west, behind the crater of lava), the observatory and its
telescope (north east) and the shrine of the victory portal, the church at the end of
the avenue, the four level houses along the high street (platformer, raycast, iso town, voxel), the plaza with its
fountain, the market (east) and the fields (west), the river with three bridges, the south meadow with the entrance, the
information board, the lever (east) and the walled grove its gate opens (west).

The cave and the observatory are carved after the forest is planted, so the trees elsewhere keep their places.

Run with ``poetry run python`` from anywhere; ``engine_demo/textures/atlases/town_atlas.png`` must already carry the village
tiles (``generate_kenney_atlases.py``). The cell constants below are the ones the scene and the scripts use.
"""

from __future__ import annotations

import random
import struct
from pathlib import Path

import yaml

PROJECT = Path(__file__).resolve().parents[2] / "engine_demo"
WIDTH, HEIGHT = 64, 40
CHUNK = 32
EMPTY = 0xFFFF

GRASS, TUFTS, FLOWERS = 0, 1, 2
DIRT_PLAIN = (39, 40, 41, 42)
STEPS = 43
WATER = (132, 133, 137, 138)
LAVA = (134, 135)
BRIDGE = 136
COBBLE = (180, 181)
SHRINE_FLOOR = 182
CROPS = {"wheat": 183, "cabbage": 184, "carrots": 185}
BEDS = (186, 187, 191)
SOIL, HAY, LAMP = 188, 189, 190
FOUNTAIN_A = 192
TREES = (27, 28, 28, 27, 5)
SIGN, WELL, LOGS, MUSHROOMS, SPROUT = 83, 104, 106, 29, 17

# (first column, first row, last column, last row) of every piece of the river; the bridges cross it afterwards.
RIVER = ((0, 31, 25, 32), (24, 29, 25, 32), (24, 29, 44, 30), (43, 29, 44, 32), (43, 31, 63, 32))
BRIDGES = ((31, 29, 33, 30), (12, 31, 13, 32), (51, 31, 52, 32))

# House: top left cell and the three rows of three tiles (roof, gable, wall; the door is the middle of the wall).
HOUSES = {
    "platformer": ((8, 13), ((52, 55, 54), (64, 67, 66), (88, 89, 77))),
    "raycast": ((19, 13), ((48, 51, 50), (60, 63, 62), (84, 85, 73))),
    "iso": ((43, 13), ((52, 55, 54), (64, 67, 66), (72, 85, 84))),
    "voxel": ((53, 13), ((48, 51, 50), (60, 63, 62), (88, 89, 77))),
}
# Houses of the villagers (nothing to enter): the same tiles, another roof or wall, a path to the high street.
HOMES = (
    ((14, 13), ((48, 49, 50), (60, 63, 62), (72, 85, 84))),
    ((24, 12), ((52, 53, 54), (64, 67, 66), (84, 85, 73))),
    ((38, 12), ((52, 55, 54), (64, 67, 66), (88, 89, 77))),
    ((48, 13), ((48, 51, 50), (60, 63, 62), (72, 85, 84))),
)
CHURCH = (29, 9)
SHRINE = (54, 3, 58, 6)  # floor of the shrine of the victory portal (first column, first row, last column, last row)
PORTAL_CELL = (56, 4)
FOUNTAIN = (31, 20)
START_CELL = (32, 36)
BOARD_CELL = (28, 36)
LEVER_CELL = (50, 36)
GATE_CELL = (8, 34)
CHEST_CELL = (8, 35)
LAVA_RECT = (5, 3, 7, 5)
FENCE = (80, 81, 82)  # left end, middle, right end of a fence (collidable)
FARM = 252  # Tiny Farm tile i is FARM + i
DUNGEON = 384  # Tiny Dungeon tile i (walls and floors) is DUNGEON + i
CAVE = (10, 1, 18, 8)  # walls of the cave (first column, first row, last column, last row), its floor inside
CAVE_DOOR = (12, 8)  # the gap in its south wall, a footpath from there down to the high street
CAVE_FLOOR = (DUNGEON + 0,) * 10 + (DUNGEON + 12, DUNGEON + 24)
CAVE_WALLS = (DUNGEON + 40, DUNGEON + 57, DUNGEON + 58, DUNGEON + 59)
CAVE_TORCH = DUNGEON + 29
ROCK, PEBBLES = FARM + 89, FARM + 77
OBSERVATORY = (46, 5)  # top left cell of the dome (3x2), its wall and door below
OBSERVATORY_PATH = 47  # column of its footpath, down to the high street
TELESCOPE_CELL = (50, 6)
DOME = 444
TELESCOPE = 450
FARM_CROPS = {"wheat": FARM + 66, "cabbage": FARM + 56, "carrots": FARM + 8, "corn": FARM + 32, "tomato": FARM + 44}
GATE_FENCE = (30, 34)  # first and last column of the fence closing the entrance, on the last row


class Grid:
    def __init__(self, fill: int) -> None:
        self.cells = [[fill] * WIDTH for _ in range(HEIGHT)]

    def put(self, c: int, r: int, tile: int) -> None:
        if 0 <= c < WIDTH and 0 <= r < HEIGHT:
            self.cells[r][c] = tile

    def get(self, c: int, r: int) -> int:
        return self.cells[r][c]

    def rect(self, c0: int, r0: int, c1: int, r1: int, tile: int | tuple[int, ...], rng: random.Random | None = None) -> None:
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                self.put(c, r, tile if isinstance(tile, int) else (rng or random).choice(tile))


def inside(rect: tuple[int, int, int, int], c: int, r: int, margin: int = 0) -> bool:
    return rect[0] - margin <= c <= rect[2] + margin and rect[1] - margin <= r <= rect[3] + margin


def build() -> tuple[Grid, Grid, Grid]:
    rng = random.Random(2026)
    ground, objects = Grid(GRASS), Grid(EMPTY)
    for r in range(HEIGHT):
        for c in range(WIDTH):
            ground.put(c, r, rng.choices((GRASS, TUFTS, FLOWERS), (74, 22, 4))[0])
    reserved: set[tuple[int, int]] = set()

    def keep(c0: int, r0: int, c1: int, r1: int, margin: int = 0) -> None:
        reserved.update((c, r) for r in range(r0 - margin, r1 + margin + 1) for c in range(c0 - margin, c1 + margin + 1))

    # Roads: the avenue from the entrance to the church, the high street, the paths to the doors.
    ground.rect(31, 33, 33, 39, DIRT_PLAIN, rng)
    ground.rect(31, 10, 33, 28, DIRT_PLAIN, rng)
    ground.rect(4, 21, 58, 22, DIRT_PLAIN, rng)
    keep(31, 33, 33, 39, 1)
    keep(31, 10, 33, 28, 1)
    keep(4, 21, 58, 22, 1)
    ground.rect(28, 17, 36, 25, COBBLE, rng)  # the plaza
    keep(27, 16, 37, 26)
    for c, r in ((29, 18), (35, 18), (29, 24), (35, 24)):  # a flower bed in each corner
        ground.put(c, r, BEDS[(c + r) % 3])
    ground.rect(30, 13, 34, 16, COBBLE, rng)  # the church apron
    keep(28, 13, 36, 16)
    for _, ((c0, r0), _) in HOUSES.items():
        ground.rect(c0 + 1, r0 + 3, c0 + 1, 20, DIRT_PLAIN, rng)
        keep(c0 - 2, r0 - 2, c0 + 4, 20)
    for c in range(56, 57):  # the footpath of the shrine, from the high street to its floor
        for r in range(7, 21):
            ground.put(c, r, STEPS)
    keep(55, 7, 57, 20)
    ground.rect(*SHRINE, SHRINE_FLOOR)
    keep(SHRINE[0] - 1, SHRINE[1] - 1, SHRINE[2] + 1, SHRINE[3] + 2)
    # The forest footpath of the woodcutter, from the west end of the high street.
    for r in range(9, 21):
        ground.put(5, r, STEPS)
    ground.rect(3, 8, 8, 12, GRASS)
    keep(3, 8, 8, 20)
    # Fountain and lamps of the plaza.
    for k, (dc, dr) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
        objects.put(FOUNTAIN[0] + dc, FOUNTAIN[1] + dr, FOUNTAIN_A + k)
    for c, r in ((28, 17), (36, 17), (28, 19), (36, 19), (28, 24), (36, 24), (28, 25), (36, 25)):  # none on the high street
        objects.put(c, r, LAMP)

    # The river and its bridges.
    for c0, r0, c1, r1 in RIVER:
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                ground.put(c, r, WATER[(c + r) % 2])
    for c0, r0, c1, r1 in BRIDGES:
        ground.rect(c0, r0, c1, r1, BRIDGE)
        for r in range(r0, r1 + 1):  # rails on both sides
            objects.put(c0 - 1, r, 59)
            objects.put(c1 + 1, r, 59)
        keep(c0 - 1, r0 - 2, c1 + 1, r1 + 2)

    # Houses, each with its path, lamp, flower beds and signpost.
    for index, (_, ((c0, r0), rows)) in enumerate(HOUSES.items()):
        for dr, row in enumerate(rows):
            for dc, tile in enumerate(row):
                objects.put(c0 + dc, r0 + dr, tile)
        objects.put(c0 + 3, r0 + 3, SIGN)
        objects.put(c0 - 1, r0 + 3, LAMP)
        for dr in (4, 5):
            ground.put(c0 - 1, r0 + dr, BEDS[(index + dr) % 3])
            ground.put(c0 + 3, r0 + dr, BEDS[(index + dr + 1) % 3])
    for (c0, r0), rows in HOMES:
        for dr, row in enumerate(rows):
            for dc, tile in enumerate(row):
                objects.put(c0 + dc, r0 + dr, tile)
        ground.rect(c0 + 1, r0 + 3, c0 + 1, 20, DIRT_PLAIN, rng)
        objects.put(c0 + 3, r0 + 3, LAMP if c0 % 2 else HAY)
        keep(c0 - 1, r0 - 1, c0 + 3, 20)
    # The church and its tower.
    cc, cr = CHURCH
    church = ((48, 49, 49, 49, 49, 49, 50), (60, 61, 61, 63, 61, 61, 62), (76, 88, 77, 77, 77, 88, 78), (77, 76, 77, 89, 77, 76, 78))
    for dr, row in enumerate(church):
        for dc, tile in enumerate(row):
            objects.put(cc + dc, cr + dr, tile)
    for dr, row in enumerate(((None, 63, None), (60, 61, 62), (76, 88, 78), (76, 77, 78))):
        for dc, tile in enumerate(row):
            if tile is not None:
                objects.put(cc + 2 + dc, cr - 4 + dr, tile)
    keep(28, 4, 36, 16)
    for c in (28, 36):
        objects.put(c, 13, LAMP)

    # The market: three stalls east of the plaza.
    ground.rect(40, 24, 51, 28, COBBLE, rng)
    keep(39, 23, 52, 29)
    for stall, goods in zip((41, 45, 49), ((130, 107, 94), (131, 106, 93), (107, 130, 131))):
        for dc, tile in enumerate((64, 65, 66)):
            objects.put(stall + dc, 24, tile)
        for dc, tile in enumerate((80, 81, 82)):
            objects.put(stall + dc, 25, tile)
        for dc, tile in enumerate(goods):
            if dc != 1:
                objects.put(stall + dc, 26, tile)
    objects.put(46, 28, HAY)

    # The fields and the garden west of the plaza (fenced, a gap in the north fence).
    ground.rect(6, 24, 21, 28, SOIL)
    for (c0, c1), kind in (((7, 9), "wheat"), ((10, 11), "corn"), ((12, 14), "cabbage"), ((15, 16), "tomato"),
                           ((17, 20), "carrots")):
        objects.rect(c0, 25, c1, 26, FARM_CROPS[kind])
    for c in range(7, 21):
        ground.put(c, 27, BEDS[(c // 2) % 3])
    ground.rect(7, 28, 20, 28, GRASS)
    for c in range(5, 23):
        if c not in (12, 13):
            objects.put(c, 23, 81)
        objects.put(c, 29, 81)
    for r in range(24, 29):
        objects.put(5, r, 59)
        objects.put(22, r, 59)
    objects.put(6, 24, HAY)
    objects.put(21, 24, HAY)
    keep(5, 23, 22, 29, 1)
    objects.put(24, 24, WELL)
    keep(23, 23, 25, 25)
    # A bench of logs and mushrooms in the clearing of the woodcutter, the crater of lava behind a warning sign.
    for c, r in ((4, 10), (6, 11)):
        objects.put(c, r, LOGS)
    objects.put(7, 9, MUSHROOMS)
    ground.rect(*LAVA_RECT, LAVA[0])
    for c, r in ((6, 4), (5, 4)):
        ground.put(c, r, LAVA[(c + r) % 2])
    objects.put(8, 6, SIGN)
    keep(LAVA_RECT[0] - 2, LAVA_RECT[1] - 2, LAVA_RECT[2] + 2, LAVA_RECT[3] + 3)
    # The shrine: four pillars round the floor, the portal in the middle.
    for c, r in ((SHRINE[0] - 1, SHRINE[1] - 1), (SHRINE[2] + 1, SHRINE[1] - 1), (SHRINE[0] - 1, SHRINE[3] + 1), (SHRINE[2] + 1, SHRINE[3] + 1)):
        objects.put(c, r, 125)
    for c in range(SHRINE[0], SHRINE[2] + 1):
        objects.put(c, SHRINE[1] - 1, 126)
    for r in range(SHRINE[1], SHRINE[3] + 1):
        objects.put(SHRINE[0] - 1, r, 126)
        objects.put(SHRINE[2] + 1, r, 126)
    # The south meadow: entrance, board, lever, and the walled grove of the gate.
    ground.rect(31, 34, 33, 39, DIRT_PLAIN, rng)
    for c in range(6, 11):
        objects.put(c, 37, TREES[c % 5])
        objects.put(c, 34, TREES[(c + 2) % 5])
    for r in range(35, 37):
        objects.put(6, r, TREES[r % 5])
        objects.put(10, r, TREES[(r + 1) % 5])
    objects.put(*GATE_CELL, EMPTY)  # the gate is an entity
    keep(5, 33, 11, 38)
    keep(BOARD_CELL[0] - 1, BOARD_CELL[1] - 1, BOARD_CELL[0] + 1, BOARD_CELL[1] + 1)
    keep(LEVER_CELL[0] - 1, LEVER_CELL[1] - 1, LEVER_CELL[0] + 1, LEVER_CELL[1] + 1)
    # Footpaths of stepping stones along the south meadow: east to the lever, west to the front of the gate.
    for c in range(34, LEVER_CELL[0]):
        ground.put(c, LEVER_CELL[1], STEPS)
    keep(30, LEVER_CELL[1] - 1, LEVER_CELL[0] + 1, LEVER_CELL[1] + 1)
    for c in range(GATE_CELL[0], 31):
        ground.put(c, GATE_CELL[1] - 1, STEPS)
    keep(GATE_CELL[0] - 1, GATE_CELL[1] - 2, 31, GATE_CELL[1] - 1)

    # Forest: dense at the edges, thinning inwards; clearings and roads stay free.
    for r in range(HEIGHT):
        for c in range(WIDTH):
            if (c, r) in reserved or objects.get(c, r) != EMPTY or ground.get(c, r) in WATER + LAVA + (BRIDGE,):
                continue
            edge = min(c, r, WIDTH - 1 - c, HEIGHT - 1 - r)
            north = r
            chance = (1.0, 0.97, 0.66, 0.38, 0.2, 0.1, 0.05)[edge] if edge < 7 else 0.0
            if north < 8:
                chance = max(chance, (1.0, 1.0, 0.95, 0.8, 0.6, 0.4, 0.25, 0.12)[north])
            if rng.random() < chance:
                objects.put(c, r, rng.choice(TREES))
            elif 8 < r < 36 and rng.random() < 0.035 and ground.get(c, r) in (GRASS, TUFTS, FLOWERS):
                objects.put(c, r, rng.choice((5, 5, SPROUT, MUSHROOMS)))
    # The entrance is a gap in the south edge.
    for r in range(36, 40):
        for c in range(30, 35):
            if (c, r) not in ((28, 36),) and objects.get(c, r) in TREES:
                objects.put(c, r, EMPTY)
    # ... closed by the village gate: a fence across the end of the avenue, a lamp on each side.
    for c in range(GATE_FENCE[0], GATE_FENCE[1] + 1):
        objects.put(c, HEIGHT - 1, FENCE[0] if c == GATE_FENCE[0] else FENCE[2] if c == GATE_FENCE[1] else FENCE[1])
    for c in GATE_FENCE:
        objects.put(c, HEIGHT - 2, LAMP)

    carve_places(ground, objects, rng)
    walkable = Grid(EMPTY)
    return ground, objects, walkable


def carve_places(ground: Grid, objects: Grid, rng: random.Random) -> None:
    """The cave (north west) and the observatory with its telescope (north east), carved out of the forest."""
    c0, r0, c1, r1 = CAVE
    for r in range(max(r0 - 1, 1), r1 + 2):  # a clearing round the rock, the edge of the map kept
        for c in range(c0 - 1, c1 + 2):
            if objects.get(c, r) in TREES + (SPROUT, MUSHROOMS):
                objects.put(c, r, EMPTY)
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1):
            if c in (c0, c1) or r in (r0, r1):
                objects.put(c, r, rng.choice(CAVE_WALLS))
            else:
                ground.put(c, r, rng.choice(CAVE_FLOOR))
    objects.put(*CAVE_DOOR, EMPTY)
    ground.put(*CAVE_DOOR, CAVE_FLOOR[0])
    objects.put(c0 + 4, r0, CAVE_TORCH)
    for c, r in ((c0 + 2, r0 + 2), (c1 - 1, r1 - 1), (c1 - 2, r0 + 1)):
        objects.put(c, r, PEBBLES)
    objects.put(c0 + 5, r0 + 3, MUSHROOMS)
    for c, r in ((c0 - 1, r1), (c0 + 4, r1 + 1), (c1 + 1, r0 + 2), (c1 + 1, r1 - 1), (c1, r1 + 1)):  # boulders outside
        objects.put(c, r, ROCK)
    for r in range(CAVE_DOOR[1] + 1, 21):
        ground.put(CAVE_DOOR[0], r, STEPS)
        if objects.get(CAVE_DOOR[0], r) != EMPTY:
            objects.put(CAVE_DOOR[0], r, EMPTY)
    # The observatory: a copper dome over a stone wall with its door, a paved terrace and the telescope.
    oc, orow = OBSERVATORY
    for r in range(orow - 1, orow + 5):
        for c in range(oc - 1, TELESCOPE_CELL[0] + 2):
            if objects.get(c, r) in TREES + (SPROUT, MUSHROOMS):
                objects.put(c, r, EMPTY)
    for k in range(6):
        objects.put(oc + k % 3, orow + k // 3, DOME + k)
    for dc, tile in enumerate((76, 89, 78)):
        objects.put(oc + dc, orow + 2, tile)
    ground.rect(oc + 3, orow, TELESCOPE_CELL[0] + 1, orow + 3, COBBLE, rng)
    ground.rect(oc, orow + 3, oc + 2, orow + 3, COBBLE, rng)
    objects.put(*TELESCOPE_CELL, TELESCOPE)
    objects.put(TELESCOPE_CELL[0] + 1, orow + 3, LAMP)
    for r in range(orow + 4, 21):
        ground.put(OBSERVATORY_PATH, r, STEPS)
        if objects.get(OBSERVATORY_PATH, r) != EMPTY:
            objects.put(OBSERVATORY_PATH, r, EMPTY)


def tile_flags() -> set[int]:
    """Collidable tile ids of the town atlas tileset."""
    document = yaml.safe_load((PROJECT / "tilesets" / "town_atlas.owltileset").read_text())
    return {tile["index"] for tile in document["tiles"] if tile.get("collidable")}


def fnv1a(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value = ((value ^ byte) * 0x01000193) & 0xFFFFFFFF
    return value


def write(name: str, layers: tuple[tuple[str, Grid, bool], ...], width: int = WIDTH, height: int = HEIGHT) -> None:
    """Write ``tilemaps/<name>.owltilemap``: each layer is (name, grid, visible)."""
    body = b"OWLTCHNK" + struct.pack("<II", CHUNK, len(layers))
    for _, layer, _ in layers:
        chunks = []
        for cy in range((height + CHUNK - 1) // CHUNK):
            for cx in range((width + CHUNK - 1) // CHUNK):
                terrain = []
                for k in range(CHUNK * CHUNK):
                    c, r = cx * CHUNK + k % CHUNK, cy * CHUNK + k // CHUNK
                    terrain.append(layer.get(c, r) if c < width and r < height else EMPTY)
                if all(tile == EMPTY for tile in terrain):
                    continue
                chunks.append(struct.pack("<II", cx, cy) + struct.pack(f"<{CHUNK * CHUNK}H", *terrain)
                              + struct.pack(f"<{CHUNK * CHUNK}H", *([EMPTY] * CHUNK * CHUNK)) + bytes(CHUNK * CHUNK))
        body += struct.pack("<I", len(chunks)) + b"".join(chunks)
    body += b"\x00"
    payload = body + struct.pack("<I", fnv1a(body))
    names = "".join(f"  - name: {layer}\n" + ("" if visible else "    visible: false\n") for layer, _, visible in layers)
    header = (f"Tilemap: {name}\nFormatVersion: 3\ntilesetPath: tilesets/town_atlas.owltileset\nwidth: {width}\n"
              f"height: {height}\ncellSize: 1\nlayers:\n{names}payloadBytes: {len(payload)}\n")
    (PROJECT / "tilemaps" / f"{name}.owltilemap").write_bytes(header.encode() + b"...\n" + payload)


MENU_WIDTH, MENU_HEIGHT = 40, 4


class Strip(Grid):
    """A grid of the size of the village strip of the main menu."""

    def __init__(self, fill: int) -> None:
        self.cells = [[fill] * MENU_WIDTH for _ in range(MENU_HEIGHT)]

    def put(self, c: int, r: int, tile: int) -> None:
        if 0 <= c < MENU_WIDTH and 0 <= r < MENU_HEIGHT:
            self.cells[r][c] = tile


def menu_strip() -> tuple[Grid, Grid]:
    """The village of the main menu, 40 x 4 cells: a row of houses, trees, lamps and the farm on a grassy street,
    open on the sky above the roofs (the 3D tavern of the menu shows through)."""
    rng = random.Random(7)
    ground, objects = Strip(EMPTY), Strip(EMPTY)
    for c in range(MENU_WIDTH):
        ground.put(c, 3, rng.choices((GRASS, TUFTS, FLOWERS), (70, 24, 6))[0])
        ground.put(c, 2, rng.choices((GRASS, TUFTS), (80, 20))[0])
    houses = [rows for _, rows in HOUSES.values()] + [rows for _, rows in HOMES]
    for index, c0 in enumerate((2, 8, 16, 22, 30, 35)):
        rows = houses[index % len(houses)]
        for dr, row in enumerate(rows):
            for dc, tile in enumerate(row):
                objects.put(c0 + dc, dr, tile)
    for c in (1, 7, 15, 21, 29):
        objects.put(c, 2, LAMP)
    for c in (5, 6, 12, 26, 27, 39):
        objects.put(c, 2, rng.choice(TREES))
    for c, tile in ((11, FARM + 83), (13, FARM + 96), (14, FARM + 85), (19, FARM + 121), (20, FARM + 120),
                    (25, FARM + 83), (33, SIGN), (34, FARM + 97), (0, FARM + 39)):
        objects.put(c, 2, tile)
    for c in range(11, 15):
        objects.put(c, 3, FARM_CROPS["wheat" if c % 2 else "cabbage"])
    return ground, objects


def reachable(walkable: Grid, start: tuple[int, int]) -> set[tuple[int, int]]:
    seen, queue = {start}, [start]
    while queue:
        c, r = queue.pop()
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (c + dc, r + dr)
            if 0 <= n[0] < WIDTH and 0 <= n[1] < HEIGHT and n not in seen and walkable.get(*n) != EMPTY:
                seen.add(n)
                queue.append(n)
    return seen


def main() -> None:
    ground, objects, walkable = build()
    solid = tile_flags()
    for r in range(HEIGHT):
        for c in range(WIDTH):
            tile = objects.get(c, r)
            blocked = (tile != EMPTY and tile in solid) or ground.get(c, r) in WATER + LAVA or (c, r) == GATE_CELL
            if not blocked:
                walkable.put(c, r, 0)
    flood = reachable(walkable, START_CELL)
    goals = {f"{name} door": (c0 + 1, r0 + 3) for name, ((c0, r0), _) in HOUSES.items()}
    goals |= {"shrine": (PORTAL_CELL[0], PORTAL_CELL[1] + 1), "board": (BOARD_CELL[0], BOARD_CELL[1] + 1),
              "lever": (LEVER_CELL[0], LEVER_CELL[1] + 1), "gate": (GATE_CELL[0], GATE_CELL[1] - 1),
              "church": (32, 14), "market": (47, 27), "well": (24, 25), "field": (14, 27), "woodcutter": (5, 10),
              "cave": (CAVE[0] + 4, CAVE[1] + 3), "telescope": (TELESCOPE_CELL[0], TELESCOPE_CELL[1] + 1)}
    for name, cell in goals.items():
        print(f"{name:16} {cell}: {'ok' if cell in flood else 'UNREACHABLE'}")
    # The hero must never walk off the map: every edge cell is blocked, or water (the river runs off both sides, the
    # scene closes them with the colliders of the `MapEdge_*` entities, invisible walls round the map).
    passable = Grid(EMPTY)
    for r in range(HEIGHT):
        for c in range(WIDTH):
            if objects.get(c, r) == EMPTY or objects.get(c, r) not in solid:
                passable.put(c, r, 0)
    edges = {(c, r) for c in range(WIDTH) for r in (0, HEIGHT - 1)} | {(c, r) for r in range(HEIGHT) for c in (0, WIDTH - 1)}
    open_edges = sorted(cell for cell in edges & reachable(passable, START_CELL) if ground.get(*cell) not in WATER)
    print(f"open map edges   {open_edges if open_edges else 'none'}")
    write("world_map", (("ground", ground, True), ("objects", objects, True), ("walkable", walkable, False)))
    strip_ground, strip_objects = menu_strip()
    write("menu_village", (("ground", strip_ground, True), ("objects", strip_objects, True)), MENU_WIDTH, MENU_HEIGHT)


if __name__ == "__main__":
    main()
