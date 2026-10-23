"""
Blocks of the YAML layout of Owl scenes and HUDs, as Owl Nest saves them, shared by the generators of the sample games
(``tools/samples/<game>/``).
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
    """A single-quoted YAML string, double-quoted with escaped line breaks when it has several lines."""
    if "\n" in text:
        return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
    return "'" + text.replace("'", "''") + "'"


def lighter(colour: Colour, amount: float) -> tuple[float, float, float, float]:
    """A colour moved towards white (or black for a negative amount), alpha kept or opaque."""
    r, g, b = (min(1.0, max(0.0, c + amount)) for c in colour[:3])
    return r, g, b, colour[3] if len(colour) > 3 else 1.0


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
        rotation: tuple[float, float, float] = (0, 0, 0),
    ) -> int:
        """Append an entity: tag, transform, visibility, hierarchy, then the given component blocks."""
        uuid = self.new_id()
        self.lines += [
            f"  - Entity: {uuid}",
            "    Tag:",
            f"      tag: {tag}",
            "    Transform:",
            f"      translation: {vec(*position)}",
            f"      rotation: {vec(*rotation)}",
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


def scene(name: str, entities: Entities, hud: str = "", header: str = "") -> str:
    """A whole scene file; ``header`` holds extra root keys (``EnabledRenderers``)."""
    hud_line = f"Hud: {hud}\n" if hud else ""
    return f"Scene: {name}\nFormatVersion: 7\n{hud_line}{header}Entities:\n" + entities.text()


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


def sprite(texture: str, colour: Colour = (1, 1, 1, 1), tiling: tuple[float, float] = (1, 1)) -> str:
    """A ``SpriteRenderer`` of a texture of the project, tinted, repeated ``tiling`` times."""
    tiling_text = num(tiling[0]) if tiling[0] == tiling[1] else vec(*tiling)
    return f"SpriteRenderer:\n  color: {vec(*colour)}\n  tilingFactor: {tiling_text}\n  texture: nam:{texture}\n"


def frames(texture: str, columns: int, rows: int, frame: int, colour: Colour = (1, 1, 1, 1)) -> str:
    """An ``AnimatedSpriteRenderer`` of a sheet, stopped on one frame (``isometric.set_sprite_frame`` picks others)."""
    return (
        f"AnimatedSpriteRenderer:\n  color: {vec(*colour)}\n  texture: nam:{texture}\n  columns: {columns}\n"
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


def child_rect(size: tuple[float, float], anchor: tuple[float, float] = (0, 0)) -> str:
    """A ``UiRect`` sized on its parent, for a child of a layout (or placed on an anchor of a plain parent)."""
    return rect(anchor, size, units=("ParentWidth", "ParentHeight"))


def text(
    value: str,
    size: int,
    align: str = "Center",
    colour: Colour = (1, 1, 1, 1),
    wrap: bool = False,
    font: str = "",
    outline: Colour | None = None,
    outline_width: float = 2,
    kerning: float = 0,
    line_spacing: float = 0,
    vertical: str = "Middle",
) -> str:
    """A ``UiText`` shrunk to fit its rectangle, in an engine font (``OpenSans-ExtraBold``) or the default one."""
    out = f"UiText:\n  outlineWidth: {num(outline_width)}\n"
    if outline is not None:
        out += f"  outlineColor: {vec(*outline)}\n"
    out += (
        f"  text: {quote(value)}\n  color: {vec(*colour)}\n  fontSize: {size}\n"
        f"  horizontalAlignment: {align}\n  verticalAlignment: {vertical}\n  kerning: {num(kerning)}\n"
        f"  lineSpacing: {num(line_spacing)}\n  wrap: {'true' if wrap else 'false'}\n  fit: Shrink\n  minFontSize: 12\n"
    )
    if font:
        out += f"  font: {font}\n"
    return out


def image(texture: str, tint: Colour = (1, 1, 1, 1)) -> str:
    """A ``UiImage`` of a texture of the project."""
    return f"UiImage:\n  tint: {vec(*tint)}\n  texture: nam:{texture}\n"


def button(
    callback: str,
    colour: tuple[float, float, float],
    hover: Colour | None = None,
    pressed: Colour | None = None,
    disabled: Colour = (0.15, 0.15, 0.15, 0.5),
) -> str:
    """A ``UiButton`` of a colour, lighter when hovered unless told otherwise."""
    r, g, b = colour
    hover_colour = hover if hover is not None else (r + 0.1, g + 0.1, b + 0.1, 1)
    pressed_colour = pressed if pressed is not None else (r - 0.05, g - 0.05, b - 0.05, 1)
    return (
        f"UiButton:\n  normalColor: {vec(r, g, b, 1)}\n  hoverColor: {vec(*hover_colour)}\n"
        f"  pressedColor: {vec(*pressed_colour)}\n  disabledColor: {vec(*disabled)}\n"
        f"  onClickCallback: {callback}\n"
    )


def focus(
    initial: bool = False,
    focusable: bool = True,
    callback: str = "",
    colour: Colour = (1, 0.82, 0.2, 1),
    thickness: float = 2,
) -> str:
    """A ``UiFocus`` with a frame of a colour (the gold of the Engine Demo menus by default)."""
    return (
        f"UiFocus:\n  focusable: {'true' if focusable else 'false'}\n  initial: {'true' if initial else 'false'}\n"
        f"  onFocusCallback: {quote(callback)}\n  highlightColor: {vec(*colour)}\n"
        f"  highlightThickness: {num(thickness)}\n"
    )


def layout(arrangement: str, spacing: float = 8, padding: float = 0, cross: str = "Center") -> str:
    """A ``UiLayout`` row or column, centred across unless told otherwise."""
    return (
        f"UiLayout:\n  arrangement: {arrangement}\n  crossAlign: {cross}\n  spacing: {num(spacing)}\n"
        f"  padding: {num(padding)}\n  columns: 1\n  cellSize: [0, 0]\n"
    )


def panel(
    colour: Colour = (0.08, 0.07, 0.1, 0.75), border: Colour = (0.55, 0.45, 0.3, 0.6), border_width: float = 1
) -> str:
    """A ``UiPanel`` with a thin border."""
    return (
        f"UiPanel:\n  backgroundColor: {vec(*colour)}\n  borderColor: {vec(*border)}\n"
        f"  borderWidth: {num(border_width)}\n  clipChildren: false\n"
    )


def window(
    title: str,
    body: Colour = (0.1, 0.08, 0.07, 0.95),
    title_colour: Colour = (0.42, 0.28, 0.16, 1),
    title_text: Colour = (1, 1, 1, 1),
    title_height: float = 34,
    title_size: float = 26,
) -> str:
    """A fixed ``UiWindow`` (no drag, no close button)."""
    return (
        f"UiWindow:\n  title: {quote(title)}\n  titleBarHeight: {num(title_height)}\n"
        f"  titleFontSize: {num(title_size)}\n"
        f"  bodyColor: {vec(*body)}\n  titleColor: {vec(*title_colour)}\n  titleTextColor: {vec(*title_text)}\n"
        "  draggable: false\n  closable: false\n  onCloseCallback: ''\n"
    )


def slider(
    callback: str,
    value: float,
    track: Colour = (0.2, 0.2, 0.2, 1),
    fill: Colour = (0.95, 0.7, 0.3, 1),
    handle: Colour = (1, 1, 1, 1),
) -> str:
    """A ``UiSlider`` from 0 to 1."""
    return (
        f"UiSlider:\n  value: {num(value)}\n  minValue: 0\n  maxValue: 1\n  trackColor: {vec(*track)}\n"
        f"  fillColor: {vec(*fill)}\n  handleColor: {vec(*handle)}\n  onValueChangedCallback: {callback}\n"
    )


def ui_list(
    count: int,
    row_height: float,
    font_size: float,
    colours: tuple[Colour, Colour, Colour, Colour, Colour],
    select: str,
    bind: str,
) -> str:
    """A ``UiList``: ``colours`` are the row, alternate row, hover, selected and text colours."""
    row, alt, hover, selected, text_colour = colours
    return (
        f"UiList:\n  itemCount: {count}\n  rowHeight: {num(row_height)}\n  rowSpacing: 2\n  selectedIndex: 0\n"
        f"  scrollOffset: 0\n  wheelStep: {num(row_height * 2 + 4)}\n  fontSize: {num(font_size)}\n"
        f"  rowColor: {vec(*row)}\n  rowAltColor: {vec(*alt)}\n  hoverColor: {vec(*hover)}\n"
        f"  selectedColor: {vec(*selected)}\n  textColor: {vec(*text_colour)}\n"
        f"  onSelectCallback: {select}\n  onBindCallback: {bind}\n"
    )
