"""
Menus shared by the generators of the v0.7.0 sample games: the title (with its How to play window), the settings
(volumes on the mixer buses, interface size, colour vision, reduced motion, subtitles and the game's own toggles),
the victory tally and the game over screens, and the visual scripts that run them (``scripts/ui/*.owlvs``).

Each game gives its look (``Look``: colours, fonts, transition, music and a live backdrop drawn behind every menu) and
its texts; the scenes and the graphs are written in the YAML layout Owl Nest saves, every size in screen heights so
that no aspect ratio nor interface size breaks the layout.

The run of a game goes through the game state: ``<prefix>_score`` is the score of the run, ``<prefix>_base`` the
score at the start of the level being played (Retry puts it back), ``<prefix>_retry`` the scene Retry starts and
``<prefix>_line_<n>`` the lines of the tally (``none`` for a line not shown); the best score is the
``<prefix>_best`` setting.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

from tools.samples import scene_yaml as y
from tools.samples.vs_graph import Graph, Node, Pin

SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""


Place = tuple[tuple[float, float], tuple[float, float], tuple[float, float]]
"""Where a block of a menu sits: its anchor on the screen, its size and its offset, in screen heights."""


@dataclass
class Layout:
    """How the menus of a game are laid out and how their buttons read (the centred column by default)."""

    title: Place = ((0.5, 0), (1.2, 0.3), (0, 0.04))
    best: Place = ((0.5, 0), (0.9, 0.06), (0, 0.35))
    best_align: str = "Center"
    buttons: Place = ((0.5, 0), (0.52, 0.46), (0, 0.45))
    button_width: float = 0.9
    button_height: float = 0.0
    """Height of a title button on the panel, 0 for the default (0.2, or 0.16 with Continue)."""
    button_spacing: float = 8
    label_size: int = 28
    label_align: str = "Center"
    label_mono: bool = False
    box: tuple[y.Colour, y.Colour] | None = None
    """Background and border of a panel behind the buttons of the title, none for the bare column."""
    box_padding: float = 0
    result_align: str = "Center"
    result: Place = ((0.5, 0), (1.1, 0.92), (0, 0.04))
    settings: Place = ((0.5, 0), (1.28, 0.94), (0, 0.03))
    focus_thickness: float = 3
    label_pad: str = ""
    """Spaces before the text of a button, for a text aligned on a side of its frame."""


@dataclass
class Look:
    """What makes the menus of a game its own."""

    prefix: str
    title_font: str
    mono_font: str
    text: y.Colour
    soft: y.Colour
    accent: y.Colour
    danger: y.Colour
    button: tuple[float, float, float]
    button_hover: y.Colour
    button_pressed: y.Colour
    window_body: y.Colour
    window_title: y.Colour
    outline: y.Colour
    transition: str
    sweep: y.Colour
    header: str
    backdrop: Callable[[y.Entities, str], None]
    title_art: str
    music: str = "sounds/music/menu.wav"
    frame: str = ""
    toggles: list[tuple[str, str]] = field(default_factory=list)
    first_scene: str = "scenes/level_1.owl"
    save_slot: int = 0
    layout: Layout = field(default_factory=Layout)


def label(
    look: Look, value: str, size: int, colour: y.Colour | None = None, align: str = "Center", mono: bool = False
) -> str:
    """A text of the game's look."""
    return y.text(
        value,
        size,
        align,
        colour or look.text,
        font=look.mono_font if mono else look.title_font,
        outline=look.outline,
        outline_width=2,
    )


def button(
    look: Look,
    entities: y.Entities,
    parent: int,
    tag: str,
    text: str,
    callback: str,
    size: tuple[float, float],
    initial: bool,
) -> int:
    """A button of the game's look, its focus frame in the accent colour."""
    style = look.layout
    components = [
        y.child_rect(size),
        y.button(callback, look.button, look.button_hover, look.button_pressed, (0.1, 0.1, 0.1, 0.5)),
        label(
            look,
            style.label_pad + text if text else "",
            style.label_size,
            align=style.label_align,
            mono=style.label_mono,
        ),
        y.focus(initial=initial, colour=look.accent, thickness=style.focus_thickness),
    ]
    uuid = entities.add(tag, components, parent=parent)
    if look.frame:
        entities.add(f"{tag}Frame", [y.child_rect((1, 1)), y.image(look.frame)], parent=uuid)
    return uuid


def column(
    look: Look, entities: y.Entities, parent: int, rows: list[tuple[str, str, str]], height: float, width: float
) -> None:
    """Buttons from top to bottom (a layout lays its children out from the last one added), the first focused."""
    for index, (tag, text, callback) in reversed(list(enumerate(rows))):
        button(look, entities, parent, tag, text, callback, (width, height), index == 0)


def window(
    look: Look, entities: y.Entities, parent: int, tag: str, title: str, size: tuple[float, float], modal: bool = False
) -> int:
    """A hidden window of the game's look laying its children out in a column; a modal one may lie over another."""
    frame = y.window(title, look.window_body, look.window_title, look.text, 40, 28)
    if modal:
        frame += "  modal: true\n"
    return entities.add(
        tag,
        [y.rect((0.5, 0.5), size), frame, y.layout("Column", 8, 12)],
        parent=parent,
        visible=False,
    )


def _music(look: Look, entities: y.Entities) -> None:
    entities.add("MenuMusic", [y.music(look.music, 0.55)])


# --------------------------------------------------------------------------------------------------------- scenes


def title_scene(look: Look, rules: str, controls: str) -> str:
    """Title: the title art, the best score, Play, How to play, Settings, Quit and the How to play window."""
    entities = y.Entities(7300000000000000000 + _salt(look.prefix))
    look.backdrop(entities, "title")
    _music(look, entities)
    script = y.script(
        "scripts/ui/title.owlvs",
        [("first_scene", "string", look.first_scene), ("best_key", "string", f"{look.prefix}_best")],
    )
    root = entities.add("MenuCanvas", [y.canvas(), script])
    style = look.layout
    entities.add("TitleArt", [y.rect(*style.title, SCREEN, SCREEN), y.image(look.title_art)], parent=root)
    entities.add(
        "BestText",
        [y.rect(*style.best, SCREEN, SCREEN), label(look, "", 26, look.accent, style.best_align, mono=True)],
        parent=root,
    )
    box = [y.panel(style.box[0], style.box[1], 2)] if style.box else []
    panel = entities.add(
        "ButtonPanel",
        [y.rect(*style.buttons, SCREEN, SCREEN), *box, y.layout("Column", style.button_spacing, style.box_padding)],
        parent=root,
    )
    column(
        look,
        entities,
        panel,
        [
            ("PlayButton", "PLAY", "on_play_clicked"),
            ("HowToButton", "HOW TO PLAY", "on_how_to_clicked"),
            *([("ContinueButton", "CONTINUE", "on_continue_clicked")] if look.save_slot else []),
            ("SettingsButton", "SETTINGS", "on_settings_clicked"),
            ("QuitButton", "QUIT", "on_quit_clicked"),
        ],
        style.button_height or (0.16 if look.save_slot else 0.2),
        style.button_width,
    )
    how_to = window(look, entities, root, "HowToWindow", "HOW TO PLAY", (1.3, 0.88))
    button(look, entities, how_to, "HowToBackButton", "BACK", "on_how_to_closed", (0.4, 0.11), False)
    entities.add(
        "HowToControls",
        [
            y.child_rect((0.94, 0.4)),
            y.text(controls, 21, "Left", look.accent, wrap=True, font=look.mono_font, line_spacing=0.25),
        ],
        parent=how_to,
    )
    entities.add(
        "HowToRules",
        [y.child_rect((0.94, 0.36)), y.text(rules, 22, "Left", look.soft, wrap=True, line_spacing=0.2)],
        parent=how_to,
    )
    return y.scene("title", entities, header=look.header)


ACCESSIBILITY = [
    ("ColourButton", "on_colour_clicked"),
    ("MotionButton", "on_motion_clicked"),
    ("SubtitlesButton", "on_subtitles_clicked"),
]
"""The accessibility rows of every settings screen."""


def settings_scene(look: Look) -> str:
    """Settings: the three volumes, the interface size, the accessibility rows, the game's toggles, Reset and Back."""
    entities = y.Entities(7310000000000000000 + _salt(look.prefix))
    look.backdrop(entities, "settings")
    _music(look, entities)
    script = y.script("scripts/ui/settings.owlvs")
    root = entities.add("SettingsCanvas", [y.canvas(), script])
    group = entities.add(
        "SettingsList", [y.rect(*look.layout.settings, SCREEN, SCREEN), y.layout("Row", 16)], parent=root
    )
    right = entities.add("RightColumn", [y.child_rect((0.48, 1)), y.layout("Column", 6)], parent=group)
    left = entities.add("LeftColumn", [y.child_rect((0.48, 1)), y.layout("Column", 5)], parent=group)
    for index in reversed(range(len(look.toggles))):
        button(
            look, entities, right, f"ToggleButton{index + 1}", "", f"on_toggle_{index + 1}_clicked", (1, 0.085), False
        )
    if look.toggles:
        entities.add("GameTitle", [y.child_rect((1, 0.08)), label(look, "GAME", 30, look.accent)], parent=right)
    for tag, callback in reversed(ACCESSIBILITY):
        button(look, entities, right, tag, "", callback, (1, 0.085), False)
    entities.add("AccessTitle", [y.child_rect((1, 0.08)), label(look, "ACCESSIBILITY", 30, look.accent)], parent=right)
    button(look, entities, left, "BackButton", "BACK", "go_back", (1, 0.085), False)
    button(look, entities, left, "ResetButton", "RESET DEFAULTS", "on_reset_clicked", (1, 0.085), False)
    rows = [
        ("MasterVolLabel", "MasterVolSlider", "on_master_vol_changed", 1.0),
        ("MusicVolLabel", "MusicVolSlider", "on_music_vol_changed", 0.8),
        ("SfxVolLabel", "SfxVolSlider", "on_sfx_vol_changed", 1.0),
        ("UiScaleLabel", "UiScaleSlider", "on_ui_scale_changed", 0.2),
    ]
    for index, (text_tag, slider, callback, value) in reversed(list(enumerate(rows))):
        focus = y.focus(initial=index == 0, colour=look.accent, thickness=3)
        bar = y.slider(callback, value, y.lighter(look.window_body, 0.08), look.text, look.accent)
        entities.add(slider, [y.child_rect((1, 0.032)), bar, focus], parent=left)
        entities.add(text_tag, [y.child_rect((1, 0.062)), label(look, "", 24, look.soft, mono=True)], parent=left)
    entities.add("TitleText", [y.child_rect((1, 0.12)), label(look, "SETTINGS", 56)], parent=left)
    return y.scene("settings", entities, header=look.header)


STAT_LINES = 6
"""Lines of the tally."""


def result_scene(look: Look, name: str, title: str, victory: bool, retry: str) -> str:
    """Victory tally or game over: the title, the lines of the tally one after the other, the score, the best score,
    Retry and Title."""
    entities = y.Entities((7320000000000000000 if victory else 7330000000000000000) + _salt(look.prefix))
    look.backdrop(entities, name)
    if not victory:
        _music(look, entities)
    script = y.script(
        "scripts/ui/result.owlvs",
        [
            ("victory", "bool", victory),
            ("retry_scene", "string", retry),
            ("best_key", "string", f"{look.prefix}_best"),
            ("prefix", "string", look.prefix),
        ],
    )
    root = entities.add("ResultCanvas", [y.canvas(), script])
    group = entities.add(
        "ResultGroup", [y.rect(*look.layout.result, SCREEN, SCREEN), y.layout("Column", 4)], parent=root
    )
    row = entities.add("ButtonRow", [y.child_rect((1, 0.11)), y.layout("Row", 16)], parent=group)
    rows = [
        ("RetryButton", "PLAY AGAIN" if victory else "RETRY", "on_retry_clicked"),
        ("MenuButton", "TITLE", "on_menu_clicked"),
    ]
    for index, (tag, text, callback) in reversed(list(enumerate(rows))):
        button(look, entities, row, tag, text, callback, (0.42, 1), index == 0)
    entities.add("BestLabel", [y.child_rect((1, 0.065)), label(look, "", 26, look.accent, mono=True)], parent=group)
    entities.add("ScoreLabel", [y.child_rect((1, 0.1)), label(look, "", 36, look.text, mono=True)], parent=group)
    for index in reversed(range(1, STAT_LINES + 1)):
        entities.add(
            f"StatLine{index}",
            [y.child_rect((0.8, 0.06)), label(look, "", 24, look.soft, "Left", True)],
            parent=group,
            visible=False,
        )
    colour = look.text if victory else look.danger
    entities.add(
        "ResultTitle", [y.child_rect((1, 0.15)), label(look, title, 64, colour, look.layout.result_align)], parent=group
    )
    return y.scene(name, entities, header=look.header)


def _salt(prefix: str) -> int:
    """A stable offset of the entity identifiers of a game."""
    return sum(ord(c) * 7919**i for i, c in enumerate(prefix)) % 900000 * 1000000


# --------------------------------------------------------------------------------------------------------- graphs


def _leave(g: Graph, look: Look, start: Node | Pin, scene_path: object, jingle: bool) -> None:
    """Leave for a scene once: the transition of the game, the start jingle when told."""
    branch = g.branch(g.get("leaving"))
    g.chain(start, branch)
    nodes = [g.set("leaving", True)]
    if jingle:
        nodes.append(g.call("sound.play", asset_path="sounds/ui/start.wav"))
    nodes.append(
        g.call("scene.transition_to", scene_path=scene_path, kind=look.transition, duration=0.45, rgba=g.get("sweep"))
    )
    g.chain(branch["False"], *nodes)


def _transition_in(g: Graph, look: Look, start: Node | Pin) -> Pin:
    """Play the transition of the game unless one is already running; the next node of the chain follows."""
    branch = g.branch(g.op("logic.not", g.call("ui.is_transition_active")["active"]))
    g.chain(start, branch)
    play = g.call("ui.transition_play", kind=look.transition, duration=0.5, rgba=g.get("sweep"))
    g.chain(branch["True"], play)
    return g.join([branch["False"], play], g.sequence())["Then 1"]


def title_graph(look: Look) -> str:
    """The title: transition in, the best score, Play (clears the run), How to play, Settings, Quit."""
    g = Graph(6100000000000000000 + _salt(look.prefix))
    g.var("first_scene", "string", look.first_scene)
    g.var("best_key", "string", f"{look.prefix}_best")
    g.var("sweep", "color", look.sweep)
    g.var("leaving", "bool", False, exposed=False)
    g.var("how_to", "bool", False, exposed=False)
    flag = f"{look.prefix}_how_to"

    create = g.event("on_create")
    after = _transition_in(g, look, create)
    best = g.call("settings.get", key=g.get("best_key"), default=0)["value"]
    text = g.node(
        "logic.select",
        Condition=g.op("compare.greater", best, 0),
        **{"True": g.fmt("BEST SCORE %d", best), "False": "NO SCORE YET"},
    )
    shown = g.chain(
        after,
        g.call("ui.set_text", entity_id=g.call("scene.find_entity", name="BestText")["entity_id"], text=text["Result"]),
        g.call("gamestate.set", key=flag, value=False),
        g.call("gamestate.set", key=f"{look.prefix}_title_best", value=best),
    )
    if look.save_slot:
        # Continue: the room of the last save, with what was found; disabled without a save.
        saved = g.call("save.has_save", slot=look.save_slot)["exists"]
        g.chain(
            shown,
            g.call(
                "ui.set_button_enabled",
                entity_id=g.call("scene.find_entity", name="ContinueButton")["entity_id"],
                enabled=saved,
            ),
            g.call(
                "gamestate.set",
                key=f"{look.prefix}_has_save",
                value=g.call("save.has_save", slot=look.save_slot)["exists"],
            ),
        )
        resume = g.event("on_continue_clicked")
        can = g.branch(
            g.op(
                "logic.and", g.op("logic.not", g.get("leaving")), g.call("save.has_save", slot=look.save_slot)["exists"]
            )
        )
        g.chain(
            resume,
            can,
            g.set("leaving", True),
            g.call("sound.play", asset_path="sounds/ui/start.wav"),
            g.call("save.load_game", slot=look.save_slot),
        )

    update = g.event("on_update")
    cancel = g.branch(
        g.op("logic.and", g.get("how_to"), g.call("input.is_action_just_pressed", action="ui_cancel")["pressed"])
    )
    g.chain(update, cancel, g.node("func.call", function="close_how_to"))

    play = g.event("on_play_clicked")
    clear = g.call("gamestate.clear")
    g.chain(play, clear)
    _leave(g, look, clear, g.get("first_scene"), True)

    how_to = g.event("on_how_to_clicked")
    window_id = g.call("scene.find_entity", name="HowToWindow")["entity_id"]
    branch = g.branch(g.get("leaving"))
    g.chain(how_to, branch)
    g.chain(
        branch["False"],
        g.call("ui.set_visible", entity_id=window_id, visible=True),
        g.call("ui.raise_window", entity_id=g.call("scene.find_entity", name="HowToWindow")["entity_id"]),
        g.call("ui.set_focus", entity_id=g.call("scene.find_entity", name="HowToBackButton")["entity_id"]),
        g.set("how_to", True),
        g.call("gamestate.set", key=flag, value=True),
    )

    closed = g.event("on_how_to_closed")
    g.chain(closed, g.node("func.call", function="close_how_to"))

    g.function("close_how_to")
    entry = g.entry("close_how_to")
    g.chain(
        entry,
        g.call("ui.set_visible", entity_id=g.call("scene.find_entity", name="HowToWindow")["entity_id"], visible=False),
        g.call("ui.set_focus", entity_id=g.call("scene.find_entity", name="HowToButton")["entity_id"]),
        g.set("how_to", False),
        g.call("gamestate.set", key=flag, value=False),
        g.node("func.return", function="close_how_to"),
    )

    settings = g.event("on_settings_clicked")
    _leave(g, look, settings, "scenes/settings.owl", False)

    quit_ = g.event("on_quit_clicked")
    g.chain(quit_, g.call("scene.quit"))
    return g.text()


_VOLUMES = [
    ("volume_master", "MasterVolSlider", "MasterVolLabel", "MASTER VOLUME", "on_master_vol_changed"),
    ("volume_music", "MusicVolSlider", "MusicVolLabel", "MUSIC", "on_music_vol_changed"),
    ("volume_sfx", "SfxVolSlider", "SfxVolLabel", "EFFECTS", "on_sfx_vol_changed"),
]
"""The volume rows: setting, slider, label, text, callback."""

_NEXT_COLOUR = """
local modes = { "none", "protanopia", "deuteranopia", "tritanopia" }
for index, name in ipairs(modes) do
	if name == mode then
		return modes[index % #modes + 1]
	end
end
return "none"
"""
"""Body of the custom node cycling the colour vision correction."""


def settings_graph(look: Look) -> str:
    """The settings: sliders and buttons written from the settings, every change applied at once, saved by Back."""
    pad = look.layout.label_pad
    g = Graph(6200000000000000000 + _salt(look.prefix))
    g.var("ui_scale_min", "float", 0.75, low=0.1)
    g.var("ui_scale_max", "float", 2.0, low=0.1)
    g.var("toggle_keys", "strings", ", ".join(key for key, _ in look.toggles))
    g.var("toggle_names", "strings", ", ".join(text for _, text in look.toggles))
    g.var("sweep", "color", look.sweep)
    g.var("leaving", "bool", False, exposed=False)
    g.custom_node(
        "next_colour_mode", "Next colour mode", [("mode", "string")], [("next", "string")], _NEXT_COLOUR, pure=True
    )

    def find(tag: str) -> object:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    def on_off(key: str, default: bool) -> object:
        return g.node(
            "logic.select",
            Condition=g.call("settings.get", key=key, default=default)["value"],
            **{"True": "ON", "False": "OFF"},
        )["Result"]

    # refresh: every row from the settings.
    g.function("refresh")
    entry = g.entry("refresh")
    nodes = []
    for setting, slider, text_tag, text, _ in _VOLUMES:
        value = g.call("settings.get", key=setting, default=1.0)["value"]
        nodes.append(g.call("ui.set_slider_value", entity_id=find(slider), value=value))
        percent = g.node(
            "math.round", A=g.op("math.multiply", g.call("settings.get", key=setting, default=1.0)["value"], 100)
        )["Result"]
        nodes.append(g.call("ui.set_text", entity_id=find(text_tag), text=g.fmt(f"{text} %d%%", percent)))
    scale = g.call("settings.get", key="ui_scale", default=1.0)["value"]
    fraction = g.op(
        "math.divide",
        g.op("math.subtract", scale, g.get("ui_scale_min")),
        g.op("math.subtract", g.get("ui_scale_max"), g.get("ui_scale_min")),
    )
    nodes.append(g.call("ui.set_slider_value", entity_id=find("UiScaleSlider"), value=fraction))
    scale_percent = g.node(
        "math.round", A=g.op("math.multiply", g.call("settings.get", key="ui_scale", default=1.0)["value"], 100)
    )["Result"]
    nodes.append(
        g.call("ui.set_text", entity_id=find("UiScaleLabel"), text=g.fmt("INTERFACE SIZE %d%%", scale_percent))
    )
    mode = g.call("settings.get", key="color_blind_mode", default="none")["value"]
    nodes.append(
        g.call(
            "ui.set_text",
            entity_id=find("ColourButton"),
            text=g.fmt(f"{pad}COLOUR VISION: %s", g.node("text.upper", Text=mode)["Result"]),
        )
    )
    nodes.append(
        g.call(
            "ui.set_text",
            entity_id=find("MotionButton"),
            text=g.fmt(f"{pad}REDUCED MOTION: %s", on_off("reduced_motion", False)),
        )
    )
    nodes.append(
        g.call(
            "ui.set_text",
            entity_id=find("SubtitlesButton"),
            text=g.fmt(f"{pad}SUBTITLES: %s", on_off("subtitles", True)),
        )
    )
    loop = g.node("flow.for_each", List=g.get("toggle_keys"))
    g.chain(entry, *nodes, loop)
    toggle_text = g.fmt(
        f"{pad}%s: %s",
        g.node("table.get_item", List=g.get("toggle_names"), Index=loop["Index"])["Result"],
        g.node(
            "logic.select",
            Condition=g.call("settings.get", key=loop["Item"], default=False)["value"],
            **{"True": "ON", "False": "OFF"},
        )["Result"],
    )
    g.chain(
        loop["Body"],
        g.call(
            "ui.set_text",
            entity_id=g.call("scene.find_entity", name=g.op("text.concat", "ToggleButton", loop["Index"]))["entity_id"],
            text=toggle_text,
        ),
    )
    g.chain(loop["Completed"], g.node("func.return", function="refresh"))

    create = g.event("on_create")
    after = _transition_in(g, look, create)
    g.chain(after, g.node("func.call", function="refresh"))

    update = g.event("on_update")
    cancel = g.branch(
        g.op(
            "logic.and",
            g.op("logic.not", g.get("leaving")),
            g.call("input.is_action_just_pressed", action="ui_cancel")["pressed"],
        )
    )
    mirror = g.call(
        "gamestate.set",
        key=f"{look.prefix}_focus",
        value=g.call("entity.get_name", entity_id=g.call("ui.get_focus")["entity_id"])["name"],
    )
    g.chain(update, mirror, cancel, g.node("func.call", function="go_back_now"))

    for setting, slider, _, _, callback in _VOLUMES:
        event = g.event(callback)
        nodes = [
            g.call("settings.set", key=setting, value=g.call("ui.get_slider_value", entity_id=find(slider))["value"]),
            g.call("settings.apply"),
        ]
        if setting == "volume_sfx":
            nodes.append(g.call("sound.play", asset_path="sounds/ui/select.wav"))
        nodes.append(g.node("func.call", function="refresh"))
        g.chain(event, *nodes)

    scale_event = g.event("on_ui_scale_changed")
    span = g.op("math.subtract", g.get("ui_scale_max"), g.get("ui_scale_min"))
    value = g.op(
        "math.add",
        g.get("ui_scale_min"),
        g.op("math.multiply", g.call("ui.get_slider_value", entity_id=find("UiScaleSlider"))["value"], span),
    )
    g.chain(scale_event, g.call("settings.set", key="ui_scale", value=value), g.node("func.call", function="refresh"))

    def toggle(event_name: str, key: object, default: bool) -> None:
        event = g.event(event_name)
        flipped = g.op("logic.not", g.call("settings.get", key=key, default=default)["value"])
        g.chain(
            event,
            g.call("settings.set", key=key, value=flipped),
            g.call("settings.apply"),
            g.call("sound.play", asset_path="sounds/ui/select.wav"),
            g.node("func.call", function="refresh"),
        )

    colour = g.event("on_colour_clicked")
    next_mode = g.node(
        "custom.pure",
        custom="next_colour_mode",
        mode=g.call("settings.get", key="color_blind_mode", default="none")["value"],
    )["next"]
    g.chain(
        colour,
        g.call("settings.set", key="color_blind_mode", value=next_mode),
        g.call("settings.apply"),
        g.call("sound.play", asset_path="sounds/ui/select.wav"),
        g.node("func.call", function="refresh"),
    )
    toggle("on_motion_clicked", "reduced_motion", False)
    toggle("on_subtitles_clicked", "subtitles", True)
    for index, (key, _) in enumerate(look.toggles):
        toggle(f"on_toggle_{index + 1}_clicked", key, False)

    reset = g.event("on_reset_clicked")
    keys = [
        "volume_master",
        "volume_music",
        "volume_sfx",
        "ui_scale",
        "color_blind_mode",
        "reduced_motion",
        "subtitles",
    ]
    keys += [key for key, _ in look.toggles]
    g.chain(
        reset,
        *[g.call("settings.reset", key=key) for key in keys],
        g.call("settings.apply"),
        g.node("func.call", function="refresh"),
    )

    back = g.event("go_back")
    g.chain(back, g.node("func.call", function="go_back_now"))
    g.function("go_back_now")
    entry = g.entry("go_back_now")
    branch = g.branch(g.get("leaving"))
    g.chain(entry, branch)
    g.chain(
        branch["False"],
        g.set("leaving", True),
        g.call("settings.save"),
        g.call(
            "scene.transition_to",
            scene_path="scenes/title.owl",
            kind=look.transition,
            duration=0.45,
            rgba=g.get("sweep"),
        ),
        g.node("func.return", function="go_back_now"),
    )
    return g.text()


def result_graph(look: Look) -> str:
    """Victory tally or game over: the lines of the tally one after the other, the best score kept, Retry, Title."""
    g = Graph(6300000000000000000 + _salt(look.prefix))
    g.var("victory", "bool", True)
    g.var("retry_scene", "string", look.first_scene)
    g.var("best_key", "string", f"{look.prefix}_best")
    g.var("prefix", "string", look.prefix)
    g.var("line_time", "float", 0.45, low=0.05)
    g.var("sweep", "color", look.sweep)
    g.var("clock", "float", 0.0, exposed=False)
    g.var("leaving", "bool", False, exposed=False)

    def key(suffix: str) -> object:
        return g.op("text.concat", g.get("prefix"), suffix)

    def find(tag: str) -> object:
        return g.call("scene.find_entity", name=tag)["entity_id"]

    create = g.event("on_create")
    after = _transition_in(g, look, create)
    jingle = g.call(
        "sound.play",
        asset_path=g.node(
            "logic.select",
            Condition=g.get("victory"),
            **{"True": "sounds/ui/victory.wav", "False": "sounds/ui/game_over.wav"},
        )["Result"],
    )
    score = g.call("gamestate.get", key=key("_score"), default=0)["value"]
    best = g.call("settings.get", key=g.get("best_key"), default=0)["value"]
    record = g.branch(g.op("compare.greater", score, best))
    g.chain(after, jingle, g.call("ui.set_text", entity_id=find("ScoreLabel"), text=g.fmt("SCORE %d", score)), record)
    new_best = g.call(
        "settings.set", key=g.get("best_key"), value=g.call("gamestate.get", key=key("_score"), default=0)["value"]
    )
    g.chain(
        record["True"],
        new_best,
        g.call("settings.save"),
        g.call("ui.set_text", entity_id=find("BestLabel"), text="NEW BEST SCORE!"),
        g.node("func.call", function="write_tally"),
    )
    old_best = g.fmt("BEST SCORE %d", g.call("settings.get", key=g.get("best_key"), default=0)["value"])
    g.chain(
        record["False"],
        g.call("ui.set_text", entity_id=find("BestLabel"), text=old_best),
        g.node("func.call", function="write_tally"),
    )

    # write_tally: the lines of the tally from the game state, shown one after the other by on_update.
    g.function("write_tally")
    entry = g.entry("write_tally")
    loop = g.node("flow.for", First=1, Last=STAT_LINES)
    g.chain(entry, loop)
    line_key = g.op("text.concat", key("_line_"), loop["Index"])
    line_tag = g.op("text.concat", "StatLine", loop["Index"])
    g.chain(
        loop["Body"],
        g.call(
            "ui.set_text",
            entity_id=g.call("scene.find_entity", name=line_tag)["entity_id"],
            text=g.call("gamestate.get", key=line_key, default="none")["value"],
        ),
    )
    g.chain(
        loop["Completed"],
        g.call("gamestate.set", key=key("_tally"), value=True),
        g.node("func.return", function="write_tally"),
    )

    update = g.event("on_update")
    clock = g.set("clock", g.op("math.add", g.get("clock"), update["dt"]))
    shown = g.node("flow.for", First=1, Last=STAT_LINES)
    g.chain(update, clock, shown)
    due = g.op("compare.greater", g.get("clock"), g.op("math.multiply", shown["Index"], g.get("line_time")))
    line = g.call("gamestate.get", key=g.op("text.concat", key("_line_"), shown["Index"]), default="none")["value"]
    has_text = g.op("compare.not_equal", line, "none")
    g.chain(
        shown["Body"],
        g.call(
            "ui.set_visible",
            entity_id=g.call("scene.find_entity", name=g.op("text.concat", "StatLine", shown["Index"]))["entity_id"],
            visible=g.op("logic.and", due, has_text),
        ),
    )

    retry = g.event("on_retry_clicked")
    restore = g.call(
        "gamestate.set", key=key("_score"), value=g.call("gamestate.get", key=key("_base"), default=0)["value"]
    )
    scene_path = g.node(
        "logic.default", Value=g.call("gamestate.get", key=key("_retry"))["value"], Default=g.get("retry_scene")
    )["Result"]
    branch = g.branch(g.get("leaving"))
    g.chain(retry, branch)
    g.chain(
        branch["False"],
        g.set("leaving", True),
        restore,
        g.call("scene.transition_to", scene_path=scene_path, kind=look.transition, duration=0.45, rgba=g.get("sweep")),
    )
    menu = g.event("on_menu_clicked")
    branch = g.branch(g.get("leaving"))
    g.chain(menu, branch)
    g.chain(
        branch["False"],
        g.set("leaving", True),
        g.call(
            "scene.transition_to",
            scene_path="scenes/title.owl",
            kind=look.transition,
            duration=0.45,
            rgba=g.get("sweep"),
        ),
    )
    return g.text()


def write_menus(root: Path, look: Look, rules: str, controls: str, victory_title: str, defeat_title: str = "") -> None:
    """Write the menu scenes and their graphs into the project: the game over screen only with a ``defeat_title``."""
    scenes, ui = root / "scenes", root / "scripts" / "ui"
    scenes.mkdir(parents=True, exist_ok=True)
    ui.mkdir(parents=True, exist_ok=True)
    (scenes / "title.owl").write_text(title_scene(look, rules, controls))
    (scenes / "settings.owl").write_text(settings_scene(look))
    (scenes / "victory.owl").write_text(result_scene(look, "victory", victory_title, True, look.first_scene))
    if defeat_title:
        (scenes / "game_over.owl").write_text(result_scene(look, "game_over", defeat_title, False, look.first_scene))
    (ui / "title.owlvs").write_text(title_graph(look))
    (ui / "settings.owlvs").write_text(settings_graph(look))
    (ui / "result.owlvs").write_text(result_graph(look))
