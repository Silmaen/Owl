"""
Blocks of the YAML layout of Owl scenes and HUDs, as Owl Nest saves them, shared by the generators of the Sokoban
sample.
"""

from __future__ import annotations

Colour = tuple[float, ...]


def num(value: float) -> str:
    """A number without trailing zeros."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def vec(*values: float) -> str:
    """A YAML sequence of numbers."""
    return "[" + ", ".join(num(v) for v in values) + "]"


def quote(text: str) -> str:
    """A single-quoted YAML string."""
    return "'" + text.replace("'", "''") + "'"


class Entities:
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
    ) -> int:
        """Append an entity: tag, transform, visibility, hierarchy, then the given component blocks."""
        uuid = self.new_id()
        self.lines += [
            f"  - Entity: {uuid}",
            "    Tag:",
            f"      tag: {tag}",
            "    Transform:",
            f"      translation: {vec(*position)}",
            "      rotation: [0, 0, 0]",
            f"      scale: {vec(*scale)}",
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


def scene(name: str, entities: Entities, hud: str = "") -> str:
    """A whole scene file."""
    hud_line = f"Hud: {hud}\n" if hud else ""
    return f"Scene: {name}\nFormatVersion: 5\n{hud_line}Entities:\n" + entities.text()


def hud_file(name: str, entities: Entities) -> str:
    """A whole HUD file."""
    return f"Hud: {name}\nFormatVersion: 1\nEntities:\n" + entities.text()


def camera(size: float) -> str:
    """An orthographic primary camera ``size`` units high."""
    return (
        "Camera:\n  camera:\n    projectionType: Orthographic\n    perspectiveFOV: 0.7853982\n"
        f"    perspectiveNear: 0.01\n    perspectiveFar: 1000\n    orthographicSize: {num(size)}\n"
        "    orthographicNear: -1\n    orthographicFar: 10\n  primary: true\n  fixedAspectRatio: false\n"
    )


def background(colour: Colour) -> str:
    """A flat ``BackgroundTexture``."""
    return f"BackgroundTexture:\n  mode: 0\n  type: 0\n  color: {vec(*colour)}\n  topColor: [0.8, 0.9, 1, 1]\n"


def frames(texture: str, columns: int, rows: int, frame: int) -> str:
    """An ``AnimatedSpriteRenderer`` of a sheet, stopped on one frame (``isometric.set_sprite_frame`` picks others)."""
    return (
        f"AnimatedSpriteRenderer:\n  color: [1, 1, 1, 1]\n  texture: nam:{texture}\n  columns: {columns}\n"
        f"  rows: {rows}\n  firstFrame: {frame}\n  lastFrame: {frame}\n  frameDuration: 0.1\n  loop: false\n"
    )


def script(path: str, properties: list[tuple[str, str, object]] | None = None) -> str:
    """A ``LuaScript`` block with its serialized properties."""
    out = f"LuaScript:\n  scriptPath: {path}\n"
    if properties:
        out += "  properties:\n"
        for name, kind, value in properties:
            text = ("true" if value else "false") if kind == "bool" else str(value)
            out += f"    - name: {name}\n      type: {kind}\n      value: {text}\n"
    return out


def music(path: str, volume: float) -> str:
    """A looping, non-spatial ``SoundSource`` playing at start."""
    return (
        f"SoundSource:\n  soundAsset: {path}\n  category: Music\n  volume: {num(volume)}\n  pitch: 1\n"
        "  loop: true\n  spatial: false\n  playOnStart: true\n  maxDistance: 50\n  rolloff: 1\n"
    )


def canvas() -> str:
    """A screen overlay canvas of 1280 x 720 reference pixels scaled with the height."""
    return (
        "Canvas:\n  space: ScreenOverlay\n  sortOrder: 0\n  referenceResolution: [1280, 720]\n"
        "  scaleMode: ScaleWithHeight\n"
    )


def rect(
    anchor: tuple[float, float],
    size: tuple[float, float],
    offset: tuple[float, float] = (0, 0),
    units: tuple[str, str] = ("ScreenHeight", "ScreenHeight"),
    offset_units: tuple[str, str] = ("Pixels", "Pixels"),
) -> str:
    """A ``UiRect`` anchored and pivoted on the same point (offset in reference pixels unless told otherwise)."""
    out = (
        f"UiRect:\n  anchorMin: {vec(*anchor)}\n  anchorMax: {vec(*anchor)}\n  pivot: {vec(*anchor)}\n"
        f"  size: {vec(*size)}\n  sizeUnit: [{units[0]}, {units[1]}]\n  offset: {vec(*offset)}\n"
    )
    if offset_units != ("Pixels", "Pixels"):
        out += f"  offsetUnit: [{offset_units[0]}, {offset_units[1]}]\n"
    return out


def child_rect(size: tuple[float, float]) -> str:
    """A ``UiRect`` sized on its parent, for a child of a layout."""
    return rect((0, 0), size, units=("ParentWidth", "ParentHeight"))


def text(value: str, size: int, align: str = "Center", colour: Colour = (1, 1, 1, 1), wrap: bool = False) -> str:
    """A ``UiText`` shrunk to fit its rectangle."""
    return (
        f"UiText:\n  outlineWidth: 2\n  text: {quote(value)}\n  color: {vec(*colour)}\n  fontSize: {size}\n"
        f"  horizontalAlignment: {align}\n  verticalAlignment: Middle\n  kerning: 0\n  lineSpacing: 0\n"
        f"  wrap: {'true' if wrap else 'false'}\n  fit: Shrink\n  minFontSize: 12\n"
    )


def button(callback: str, colour: tuple[float, float, float]) -> str:
    """A ``UiButton`` of a colour, lighter when hovered."""
    r, g, b = colour
    return (
        f"UiButton:\n  normalColor: {vec(r, g, b, 1)}\n  hoverColor: {vec(r + 0.1, g + 0.1, b + 0.1, 1)}\n"
        f"  pressedColor: {vec(r - 0.05, g - 0.05, b - 0.05, 1)}\n  disabledColor: [0.15, 0.15, 0.15, 0.5]\n"
        f"  onClickCallback: {callback}\n"
    )


def focus(initial: bool = False, focusable: bool = True, callback: str = "") -> str:
    """A ``UiFocus`` with the gold frame of the Engine Demo menus."""
    return (
        f"UiFocus:\n  focusable: {'true' if focusable else 'false'}\n  initial: {'true' if initial else 'false'}\n"
        f"  onFocusCallback: {quote(callback)}\n  highlightColor: [1, 0.82, 0.2, 1]\n  highlightThickness: 2\n"
    )


def layout(arrangement: str, spacing: float = 8, padding: float = 0) -> str:
    """A ``UiLayout`` row or column, centred across."""
    return (
        f"UiLayout:\n  arrangement: {arrangement}\n  crossAlign: Center\n  spacing: {num(spacing)}\n"
        f"  padding: {num(padding)}\n  columns: 1\n  cellSize: [0, 0]\n"
    )


def panel(colour: Colour = (0.08, 0.07, 0.1, 0.75)) -> str:
    """A ``UiPanel`` with a thin border."""
    return (
        f"UiPanel:\n  backgroundColor: {vec(*colour)}\n  borderColor: [0.55, 0.45, 0.3, 0.6]\n  borderWidth: 1\n"
        "  clipChildren: false\n"
    )


def window(title: str) -> str:
    """A fixed ``UiWindow`` (no drag, no close button)."""
    return (
        f"UiWindow:\n  title: {quote(title)}\n  titleBarHeight: 34\n  titleFontSize: 26\n"
        "  bodyColor: [0.1, 0.08, 0.07, 0.95]\n  titleColor: [0.42, 0.28, 0.16, 1]\n  titleTextColor: [1, 1, 1, 1]\n"
        "  draggable: false\n  closable: false\n  onCloseCallback: ''\n"
    )
