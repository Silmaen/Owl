#!/usr/bin/env python3
"""
Write the five level scenes of the Breakout sample game, its brick prefab and its play HUD into ``samples/breakout/``.

The levels are drawn as text grids (``LEVELS``): one character per brick of a 10 x 8 grid. Each brick of a scene is an
instance of ``prefabs/brick.owlprefab`` overriding its colour and its script properties, the way Owl Nest saves a
placed prefab; the scenes stay editable in Owl Nest afterwards (move, add or delete bricks, change their properties),
and running this script again overwrites those edits.

Run from the repository root: ``./docker_run.sh poetry run python tools/samples/breakout/generate_levels.py``.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3] / "samples" / "breakout"
"""The sample project."""

COLUMNS = 10
"""Bricks per row."""
ROWS = 8
"""Rows of a level grid."""
BRICK_SIZE = (1.5, 0.5)
"""World size of a brick."""
PITCH = (1.6, 0.6)
"""Distance between two brick centres."""
TOP_ROW_Y = 5.2
"""Height of the centre of the first row."""
FIELD_HALF = 8.0
"""Inner half width of the field."""
PADDLE_Y = -6.8
"""Height of the paddle."""
PARK_Y = -30.0
"""Where the unused entities wait, below the field."""

ROW_COLOURS = [
    (0.92, 0.30, 0.30),
    (0.95, 0.58, 0.22),
    (0.95, 0.85, 0.25),
    (0.40, 0.85, 0.35),
    (0.30, 0.75, 0.90),
    (0.40, 0.48, 0.95),
    (0.72, 0.42, 0.92),
    (0.92, 0.45, 0.75),
]
"""Colour of the one-hit bricks of each row."""

POINTS = {1: 10, 2: 25, 3: 50}
"""Points of a brick by its hits."""

LEVELS = [
    [
        "..........",
        "1111111111",
        "1111w11111",
        "1111111111",
        "11111s1111",
        "..........",
        "..........",
        "..........",
    ],
    [
        "2222222222",
        ".11111111.",
        "..1w111s..",
        "...1111...",
        "....11....",
        "..........",
        "..........",
        "..........",
    ],
    [
        "2.2.2.2.2.",
        ".1.1.1.1.1",
        "1.w.1.1.s.",
        ".2.2.2.2.2",
        "1.1.1.1.1.",
        ".1.1.w.1.1",
        "..........",
        "..........",
    ],
    [
        "3333333333",
        "3........3",
        "3.11ww11.3",
        "3.1s22s1.3",
        "3.111111.3",
        "3........3",
        "33.3333.33",
        "..........",
    ],
    [
        "3.3.3.3.3.",
        "2222222222",
        "1w111111s1",
        "2222222222",
        "1111111111",
        ".3.3.3.3.3",
        "11s1111w11",
        "2222222222",
    ],
]
"""The five levels: ``.`` nothing, ``1`` to ``3`` hits, ``w`` / ``s`` a one-hit brick dropping a power-up."""

BALL_SPEED = [7.0, 7.5, 8.0, 8.5, 9.0]
"""Serve speed of the ball in each level."""


@dataclass(frozen=True)
class Brick:
    """One brick of a level."""

    column: int
    row: int
    hits: int
    drop: str

    @property
    def position(self) -> tuple[float, float]:
        """World centre of the brick."""
        x = (self.column - (COLUMNS - 1) / 2.0) * PITCH[0]
        return round(x, 3), round(TOP_ROW_Y - self.row * PITCH[1], 3)

    @property
    def colour(self) -> tuple[float, float, float]:
        """Colour of the brick at its last hit."""
        return ROW_COLOURS[self.row % len(ROW_COLOURS)]

    @property
    def start_colour(self) -> tuple[float, float, float]:
        """Colour of the brick as placed (steel and silver for the strong ones, as ``brick.lua`` tints them)."""
        if self.hits >= 3:
            return 0.55, 0.6, 0.72
        if self.hits == 2:
            return 0.82, 0.85, 0.92
        return self.colour


def layout(level: int) -> list[Brick]:
    """The bricks of a level, from 0."""
    bricks = []
    for row, line in enumerate(LEVELS[level]):
        assert len(line) == COLUMNS, f"level {level + 1}, row {row}: {len(line)} columns"
        for column, char in enumerate(line):
            if char == ".":
                continue
            drop = {"w": "wide", "s": "slow"}.get(char, "")
            bricks.append(Brick(column, row, 1 if drop else int(char), drop))
    return bricks


def _num(value: float) -> str:
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def _vec(*values: float) -> str:
    return "[" + ", ".join(_num(v) for v in values) + "]"


class _Entities:
    """Accumulates entities in the YAML layout of a scene, with consecutive identifiers from a base."""

    def __init__(self, base: int) -> None:
        self.next_id = base
        self.lines: list[str] = []

    def new_id(self) -> int:
        """Reserve the next identifier."""
        self.next_id += 1
        return self.next_id

    def add(
        self,
        tag: str,
        components: list[str],
        position: tuple[float, float, float] = (0, 0, 0),
        scale: tuple[float, float, float] = (1, 1, 1),
        parent: int = 0,
        visible: bool = True,
        uuid: int = 0,
    ) -> int:
        """Append an entity: tag, transform, visibility, hierarchy, then the given component blocks."""
        uuid = uuid or self.new_id()
        self.lines += [
            f"  - Entity: {uuid}",
            "    Tag:",
            f"      tag: {tag}",
            "    Transform:",
            f"      translation: {_vec(*position)}",
            "      rotation: [0, 0, 0]",
            f"      scale: {_vec(*scale)}",
            "    Visibility:",
            f"      gameVisible: {'true' if visible else 'false'}",
            "    Hierarchy:" + (f"\n      parentId: {parent}" if parent else " {}"),
        ]
        for component in components:
            self.lines += ["    " + line if line else "" for line in component.splitlines()]
        return uuid

    def text(self) -> str:
        """The entities block."""
        return "\n".join(self.lines) + "\n"


def sprite(texture: str, colour: tuple[float, ...] = (1, 1, 1, 1), tiling: tuple[float, float] = (1, 1)) -> str:
    """A ``SpriteRenderer`` block."""
    rgba = (*colour, 1.0) if len(colour) == 3 else colour
    tiling_text = _num(tiling[0]) if tiling[0] == tiling[1] else _vec(*tiling)
    return (
        f"SpriteRenderer:\n  color: {_vec(*rgba)}\n  tilingFactor: {tiling_text}\n  texture: nam:textures/{texture}\n"
    )


def body(kind: str, restitution: float = 0.0, friction: float = 0.0) -> str:
    """A ``PhysicBody`` block of a box collider filling the entity."""
    density = 1 if kind == "Dynamic" else 0
    return (
        f"PhysicBody:\n  type: {kind}\n  fixedRotation: true\n  colliderSize: [1, 1, 1]\n  density: {density}\n"
        f"  restitution: {_num(restitution)}\n  friction: {_num(friction)}\n"
    )


def script(path: str, properties: list[tuple[str, str, object]]) -> str:
    """A ``LuaScript`` block with its serialized properties."""
    out = f"LuaScript:\n  scriptPath: {path}\n"
    if properties:
        out += "  properties:\n"
        for name, kind, value in properties:
            if kind == "bool":
                text = "true" if value else "false"
            elif kind == "string":
                text = str(value) if "/" in str(value) else f"'{value}'"  # a path as Owl Nest writes it
            elif kind == "float":
                text = _num(float(value))  # type: ignore[arg-type]
            else:
                text = str(value)
            out += f"    - name: {name}\n      type: {kind}\n      value: {text}\n"
    return out


def brick_script(brick: Brick) -> str:
    """The script block of a brick."""
    r, g, b = brick.colour
    return script(
        "scripts/game/brick.lua",
        [
            ("hits", "int", brick.hits),
            ("points", "int", POINTS[brick.hits]),
            ("drop", "string", brick.drop),
            ("r", "float", r),
            ("g", "float", g),
            ("b", "float", b),
        ],
    )


PREFAB_ROOT = 7000000000000000001
"""Identifier of the brick inside its prefab."""


def prefab() -> str:
    """The brick prefab: a static box, its sprite and its script."""
    entities = _Entities(0)
    sample = Brick(0, 0, 1, "")
    entities.add(
        "Brick",
        [sprite("brick.png", sample.start_colour), body("Static", restitution=1.0), brick_script(sample)],
        scale=(*BRICK_SIZE, 1),
        uuid=PREFAB_ROOT,
    )
    return "Prefab: Brick\nVersion: 1\nEntities:\n" + entities.text()


def camera() -> str:
    """The orthographic camera block, 16 units high."""
    return (
        "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.7853982\n"
        "    perspectiveNear: 0.01\n    perspectiveFar: 1000\n    orthographicSize: 16\n    orthographicNear: -1\n"
        "    orthographicFar: 10\n  primary: true\n  fixedAspectRatio: false\n"
    )


def level_scene(level: int) -> str:
    """The scene of a level, from 0."""
    entities = _Entities(7100000000000000000 + (level + 1) * 1000000)
    entities.add("MainCamera", [camera()], position=(0, 0, 5))
    entities.add(
        "Background",
        ["BackgroundTexture:\n  mode: 0\n  type: 0\n  color: [0.04, 0.04, 0.09, 1]\n  topColor: [0.8, 0.9, 1, 1]\n"],
        position=(0, 0, -1),
        scale=(20, 15, 1),
    )
    entities.add("Backdrop", [sprite("backdrop.png", tiling=(4, 3.5))], position=(0, -1, -0.5), scale=(16, 14, 1))
    wall_body = body("Static", restitution=1.0)
    entities.add(
        "Wall", [sprite("wall.png", tiling=(1, 29)), wall_body], position=(-8.25, -0.75, 0), scale=(0.5, 14.5, 1)
    )
    entities.add(
        "Wall", [sprite("wall.png", tiling=(1, 29)), wall_body], position=(8.25, -0.75, 0), scale=(0.5, 14.5, 1)
    )
    entities.add("Wall", [sprite("wall.png", tiling=(34, 1)), wall_body], position=(0, 6.25, 0), scale=(17, 0.5, 1))
    paddle_props: list[tuple[str, str, object]] = [
        ("speed", "float", 13.0),
        ("half_width", "float", 1.2),
        ("wide_half_width", "float", 1.8),
    ]
    entities.add(
        "Paddle",
        [sprite("paddle.png"), body("Kinematic"), script("scripts/game/paddle.lua", paddle_props)],
        position=(0, PADDLE_Y, 0),
        scale=(2.4, 0.36, 1),
    )
    entities.add(
        "PaddleWide", [sprite("paddle_wide.png"), body("Kinematic")], position=(0, PARK_Y, 0), scale=(3.6, 0.36, 1)
    )
    ball_props: list[tuple[str, str, object]] = [
        ("speed", "float", BALL_SPEED[level]),
        ("max_speed", "float", BALL_SPEED[level] + 3.5),
        ("auto_serve", "float", 5.0),
    ]
    entities.add(
        "Ball",
        [sprite("ball.png"), body("Dynamic", restitution=1.0), script("scripts/game/ball.lua", ball_props)],
        position=(0, PADDLE_Y + 0.36, 0.1),
        scale=(0.3, 0.3, 1),
    )
    next_scene = f"scenes/level_{level + 2}.owl" if level + 1 < len(LEVELS) else "scenes/victory.owl"
    entities.add(
        "Game", [script("scripts/game/game.lua", [("level", "int", level + 1), ("next_scene", "string", next_scene)])]
    )
    entities.add(
        "Music",
        [
            "SoundSource:\n  soundAsset: sounds/music/loop.wav\n  category: Music\n  volume: 0.35\n  pitch: 1\n"
            "  loop: true\n  spatial: false\n  playOnStart: true\n  maxDistance: 50\n  rolloff: 1\n"
        ],
    )
    for kind in ("Wide", "Slow"):
        for index in (1, 2):
            entities.add(
                f"Power{kind}{index}",
                [sprite(f"power_{kind.lower()}.png")],
                position=(0, PARK_Y, 0.2),
                scale=(1.2, 0.525, 1),
                visible=False,
            )
    for index in range(1, 7):
        entities.add(
            f"BreakFx{index}", [sprite("brick.png")], position=(0, PARK_Y, 0.2), scale=(*BRICK_SIZE, 1), visible=False
        )
    for brick in layout(level):
        uuid = entities.new_id()
        link = (
            f"PrefabLink:\n  prefabAssetPath: prefabs/brick.owlprefab\n  syncedVersion: 1\n  uuidMapping:\n"
            f"    - inst: {uuid}\n      canon: {PREFAB_ROOT}\n  overrides:\n"
            f"    - {PREFAB_ROOT}:SpriteRenderer\n    - {PREFAB_ROOT}:LuaScript\n"
        )
        x, y = brick.position
        entities.add(
            "Brick",
            [sprite("brick.png", brick.start_colour), body("Static", restitution=1.0), brick_script(brick), link],
            position=(x, y, 0),
            scale=(*BRICK_SIZE, 1),
            uuid=uuid,
        )
    return "Scene: untitled\nFormatVersion: 5\nHud: huds/play.owlhud\nEntities:\n" + entities.text()


def _rect(
    anchor: tuple[float, float],
    size: tuple[float, float],
    offset: tuple[float, float] = (0, 0),
    units: tuple[str, str] = ("ScreenHeight", "ScreenHeight"),
) -> str:
    return (
        f"UiRect:\n  anchorMin: {_vec(*anchor)}\n  anchorMax: {_vec(*anchor)}\n  pivot: {_vec(*anchor)}\n"
        f"  size: {_vec(*size)}\n  sizeUnit: [{units[0]}, {units[1]}]\n  offset: {_vec(*offset)}\n"
    )


def _text(text: str, size: int, align: str = "Center", colour: tuple[float, ...] = (1, 1, 1, 1)) -> str:
    return (
        f"UiText:\n  outlineWidth: 2\n  text: '{text}'\n  color: {_vec(*colour)}\n  fontSize: {size}\n"
        f"  horizontalAlignment: {align}\n  verticalAlignment: Middle\n  kerning: 0\n  lineSpacing: 0\n"
        "  wrap: false\n  fit: Shrink\n  minFontSize: 12\n"
    )


def _button(callback: str, colour: tuple[float, float, float]) -> str:
    r, g, b = colour
    return (
        f"UiButton:\n  normalColor: {_vec(r, g, b, 1)}\n  hoverColor: {_vec(r + 0.1, g + 0.1, b + 0.1, 1)}\n"
        f"  pressedColor: {_vec(r - 0.05, g - 0.05, b - 0.05, 1)}\n  disabledColor: [0.15, 0.15, 0.15, 0.5]\n"
        f"  onClickCallback: {callback}\n"
    )


def hud() -> str:
    """The play HUD: score, level and lives across the top, a message in the middle, the pause window."""
    entities = _Entities(7090000000000000000)
    canvas = entities.add(
        "HUDCanvas",
        [
            "Canvas:\n  space: ScreenOverlay\n  sortOrder: 0\n  referenceResolution: [1280, 720]\n"
            "  scaleMode: ScaleWithHeight\n"
        ],
    )
    gold = (1, 0.92, 0.5, 1)
    cyan = (0.6, 0.97, 1, 1)
    mono = "  font: JetBrainsMono-Regular\n"
    score = _text("SCORE 000000", 34, "Left", gold) + mono
    entities.add("ScoreText", [_rect((0, 0), (0.42, 0.07), (16, 6)), score], parent=canvas)
    level_text = _text("LEVEL 1", 34, colour=cyan) + mono
    entities.add("LevelText", [_rect((0.5, 0), (0.36, 0.07), (0, 6)), level_text], parent=canvas)
    lives = _text("LIVES 3", 34, "Right", gold) + mono
    entities.add("LivesText", [_rect((1, 0), (0.36, 0.07), (-16, 6)), lives], parent=canvas)
    message = _text("Level 1", 44, colour=cyan).replace("wrap: false", "wrap: true")
    message += "  font: OpenSans-ExtraBoldItalic\n"
    entities.add("MessageText", [_rect((0.5, 0.6), (0.9, 0.16)), message], parent=canvas)
    window = entities.add(
        "PauseWindow",
        [
            _rect((0.5, 0.5), (0.9, 0.66)),
            "UiWindow:\n  title: PAUSED\n  titleBarHeight: 38\n  titleFontSize: 28\n"
            "  bodyColor: [0.04, 0.02, 0.1, 0.96]\n  titleColor: [0.35, 0.05, 0.45, 1]\n"
            "  titleTextColor: [0.6, 0.97, 1, 1]\n"
            "  draggable: false\n  closable: false\n  onCloseCallback: ''\n",
            "UiLayout:\n  arrangement: Column\n  crossAlign: Center\n  spacing: 8\n  padding: 10\n  columns: 1\n"
            "  cellSize: [0, 0]\n",
        ],
        parent=canvas,
        visible=False,
    )
    # The controls are shown here and in the How to play window of the title only, never during play.
    controls = (
        f"UiText:\n  outlineWidth: 2\n  text: {_quote_block(CONTROLS)}\n  color: [1, 0.88, 0.3, 1]\n  fontSize: 22\n"
        "  horizontalAlignment: Left\n  verticalAlignment: Middle\n  kerning: 0\n  lineSpacing: 0.25\n"
        "  wrap: true\n  fit: Shrink\n  minFontSize: 12\n  font: JetBrainsMono-Regular\n"
    )
    controls_rect = _rect((0, 0), (0.92, 0.3), units=("ParentWidth", "ParentHeight"))
    entities.add("ControlsText", [controls_rect, controls], parent=window)
    rows = [
        ("QuitButton", "QUIT TO TITLE", "on_pause_quit", False),
        ("RestartButton", "RESTART LEVEL", "on_pause_restart", False),
        ("ResumeButton", "RESUME", "on_pause_resume", True),
    ]
    for tag, label, callback, initial in rows:
        focus = "UiFocus:\n  focusable: true\n  initial: " + ("true" if initial else "false")
        focus += "\n  highlightColor: [1, 0.3, 0.8, 1]\n  highlightThickness: 3\n"
        parent_units = ("ParentWidth", "ParentHeight")
        label_text = _text(label, 30, colour=(0.6, 0.97, 1, 1)) + "  font: OpenSans-ExtraBoldItalic\n"
        uuid = entities.add(
            tag,
            [
                _rect((0, 0), (0.55, 0.16), units=parent_units),
                NEON_BUTTON.replace("CALLBACK", callback),
                label_text,
                focus,
            ],
            parent=window,
        )
        frame = "UiImage:\n  tint: [1, 1, 1, 1]\n  texture: nam:textures/ui/button_frame.png\n"
        entities.add(f"{tag}Frame", [_rect((0, 0), (1, 1), units=parent_units), frame], parent=uuid)
    return "Hud: untitled\nFormatVersion: 1\nEntities:\n" + entities.text()


CONTROLS = (
    "MOVE    A / D, arrows, mouse, left stick, D-pad\n"
    "SERVE   Space, Up, left click, pad A\n"
    "PAUSE   Escape, P, pad Start"
)
"""The controls, shown in the pause window."""
NEON_BUTTON = (
    "UiButton:\n  normalColor: [0.07, 0.02, 0.14, 1]\n  hoverColor: [0.32, 0.05, 0.42, 1]\n"
    "  pressedColor: [0.55, 0.1, 0.6, 1]\n  disabledColor: [0.06, 0.04, 0.08, 0.6]\n  onClickCallback: CALLBACK\n"
)
"""A button of the neon menus (``generate_menus.py``)."""


def _quote_block(text: str) -> str:
    """A double-quoted YAML string, line breaks escaped."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def main() -> None:
    """Write every generated file of the sample."""
    (ROOT / "prefabs").mkdir(parents=True, exist_ok=True)
    (ROOT / "prefabs" / "brick.owlprefab").write_text(prefab())
    (ROOT / "huds").mkdir(parents=True, exist_ok=True)
    (ROOT / "huds" / "play.owlhud").write_text(hud())
    for level in range(len(LEVELS)):
        (ROOT / "scenes" / f"level_{level + 1}.owl").write_text(level_scene(level))


if __name__ == "__main__":
    main()
