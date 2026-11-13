#!/usr/bin/env python3
"""
Write the menus of The Moon Lantern, the point-and-click sample game, into ``samples/point_and_click/``: the title and
the settings scenes and the picture of their storybook (``textures/ui/book.png``).

Both menus are a storybook lying open over the glade at night: the title on the left page, in the ink of an old
printed tale (``IMFellEnglish-Italic`` and ``IMFellEnglish-Regular``, in ``fonts/``), the chapters to choose on the
right page; the sliders of the settings on the left page, the other settings on the right one. An entry is ink on the
paper; hovered or focused, a wash of sepia rises under it and a thin red line frames it. The book is pixel art like
the rooms, drawn near one texture pixel per reference pixel. The canvas is 1280 x 720 reference pixels, the book sized
in screen heights so that it keeps its shape. ``generate_scenes.py`` calls ``main`` with the other scenes.

Run from the repository root: ``./docker_run.sh poetry run python -m tools.samples.point_and_click.generate_menus``.
"""

from __future__ import annotations

import random
from typing import cast

from PIL import Image, ImageDraw

from tools.samples import scene_yaml as y
from tools.samples.point_and_click import generate_scenes as gs

TALE = "IMFellEnglish-Italic"
"""Font of the name of the tale and of the headings."""
PRINT = "IMFellEnglish-Regular"
"""Font of the entries and the labels."""
INK = (0.2, 0.12, 0.08, 1)
"""Ink on the pages."""
RED_INK = (0.55, 0.12, 0.08, 1)
"""Red ink: the name of the tale, the frame of the focused entry."""
WASH = (0.62, 0.38, 0.16, 0.28)
"""Sepia wash under a hovered or focused entry."""
BOOK_SIZE = (1.1, 0.715)
"""Size of the open book, in screen heights (the picture is 800 x 520)."""
SCREEN = ("ScreenHeight", "ScreenHeight")
"""Sizes and offsets in screen heights."""


def page_text(value: str, size: int, colour: y.Colour = INK, font: str = PRINT, wrap: bool = False) -> str:
    """Ink on a page; a key of the string tables is shown translated."""
    if value in gs.KEYS:
        block = y.text(gs.KEYS[value], size, "Center", colour, wrap=wrap, font=font, outline_width=0)
        return block.replace("UiText:\n", f"UiText:\n  textKey: {value}\n", 1)
    return y.text(value, size, "Center", colour, wrap=wrap, font=font, outline_width=0)


def entry(
    entities: y.Entities, parent: int, tag: str, label: str, callback: str, size: tuple[float, float], initial: bool
) -> int:
    """An entry of a page in a layout: ink on the paper, a sepia wash and a red frame when hovered or focused."""
    return entities.add(
        tag,
        [
            y.child_rect(size),
            y.button(callback, (*WASH[:3], 0), WASH, (0.62, 0.38, 0.16, 0.5), (0, 0, 0, 0), y.box(8)),
            page_text(label, 34),
            y.focus(initial=initial, colour=(*RED_INK[:3], 0.8), thickness=1.5),
        ],
        parent=parent,
    )


def book(entities: y.Entities, root: int) -> tuple[int, int]:
    """The open book on the left of the screen; returns its left and right pages (plain rectangles)."""
    art = "textures/ui/book.png"
    picture = y.panel((1, 1, 1, 1), box_style=y.box(texture=art).replace(f"nam:{art}", gs.ref(art)))
    uuid = entities.add("Storybook", [y.rect((0, 0.5), BOOK_SIZE, (0.04, 0.02), SCREEN, SCREEN), picture], parent=root)
    # A page is 40 % of the book wide, centred at 27 % and 73 % of it (a rectangle is pinned on its anchor).
    left = entities.add("LeftPage", [y.child_rect((0.4, 0.8), (0.117, 0.5))], parent=uuid)
    right = entities.add("RightPage", [y.child_rect((0.4, 0.8), (0.883, 0.5))], parent=uuid)
    return left, right


def title_scene() -> str:
    """Title: the glade at night, the hero, the storybook with the name of the tale and four entries."""
    entities = y.Entities(9400000000000000000)
    gs.menu_world(entities, "textures/rooms/title.png", (250, 168))
    entities.add("Music", [gs.sound("sounds/music.wav", "Music", 0.45)])
    root = entities.add("MenuCanvas", [y.canvas()])
    left, right = book(entities, root)
    heading = entities.add("TitleBlock", [y.child_rect((1, 1)), y.layout("Column", 6)], parent=left)
    entities.add("Ornament", [y.child_rect((0.5, 0.06)), page_text("~ * ~", 26, RED_INK)], parent=heading)
    entities.add("Tagline", [y.child_rect((1, 0.22)), page_text("title.tagline", 28, INK, TALE, True)], parent=heading)
    entities.add(
        "TitleText", [y.child_rect((1, 0.5)), page_text("title.name", 84, RED_INK, TALE, True)], parent=heading
    )
    panel = entities.add("ButtonPanel", [y.child_rect((0.9, 0.62), (0.5, 0.5)), y.layout("Column", 6)], parent=right)
    for index, (tag, label, callback) in reversed(
        list(
            enumerate(
                [
                    ("NewButton", "title.new", "on_new_clicked"),
                    ("ContinueButton", "title.continue", "on_continue_clicked"),
                    ("SettingsButton", "title.settings", "on_settings_clicked"),
                    ("QuitButton", "title.quit", "on_quit_clicked"),
                ]
            )
        )
    ):
        entry(entities, panel, tag, label, callback, (1, 0.2), index == 0)
    return gs.menu_scene("title", entities, "scripts/ui/title.owlvs")


def settings_scene() -> str:
    """Settings: the volumes on the left page, the language, the subtitles, reset and Back on the right; Back saves."""
    entities = y.Entities(9410000000000000000)
    gs.menu_world(entities, "textures/rooms/title.png", None, 0.85)
    root = entities.add("SettingsCanvas", [y.canvas()])
    left, right = book(entities, root)
    sliders = entities.add("SettingsList", [y.child_rect((1, 1)), y.layout("Column", 6)], parent=left)

    def slider(callback: str, initial: bool) -> list[str]:
        frame = y.box(6, INK, 1)
        bar = y.slider(callback, 1.0, (0.86, 0.78, 0.62, 1), RED_INK, INK, frame)
        return [y.child_rect((0.9, 0.06)), bar, y.focus(initial, True, "", (*RED_INK[:3], 0.8), 1.5)]

    rows: list[tuple[str, list[str]]] = [
        ("SettingsTitle", [y.child_rect((1, 0.2)), page_text("settings.title", 60, RED_INK, TALE)]),
        ("MasterLabel", [y.child_rect((1, 0.1)), page_text("", 30)]),
        ("MasterSlider", slider("on_master_changed", True)),
        ("MusicLabel", [y.child_rect((1, 0.1)), page_text("", 30)]),
        ("MusicSlider", slider("on_music_changed", False)),
        ("EffectsLabel", [y.child_rect((1, 0.1)), page_text("", 30)]),
        ("EffectsSlider", slider("on_effects_changed", False)),
    ]
    ids = {tag: entities.add(tag, components, parent=sliders) for tag, components in reversed(rows)}
    choices = entities.add(
        "SettingsChoices", [y.child_rect((0.95, 0.7), (0.5, 0.5)), y.layout("Column", 6)], parent=right
    )
    entry(entities, choices, "BackButton", "settings.back", "go_back", (1, 0.2), False)
    entry(entities, choices, "ResetButton", "settings.reset", "on_reset_clicked", (1, 0.2), False)
    entry(entities, choices, "SubtitlesButton", "", "on_subtitles_clicked", (1, 0.2), False)
    language = entry(entities, choices, "LanguageButton", "", "on_language_clicked", (1, 0.2), False)
    # The pages sit side by side: down from the last slider turns the page, up from the language turns it back.
    y.neighbours(entities, ids["EffectsSlider"], down=language)
    y.neighbours(entities, language, up=ids["EffectsSlider"])
    return gs.menu_scene("settings", entities, "scripts/ui/settings.owlvs")


def book_art() -> Image.Image:
    """The open storybook (800 x 520), drawn in 2 x 2 pixels like the rooms: a leather cover, two pages of old paper
    darker toward the spine, the edges of the leaves under them, a red ribbon and corner flourishes."""
    width, height = 400, 260
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([0, 6, width - 1, height - 1], 10, fill=(74, 32, 22, 255), outline=(38, 16, 10, 255))
    draw.rounded_rectangle([3, 9, width - 4, height - 4], 8, outline=(150, 102, 48, 255))
    for leaf in range(4):
        inset = 10 + leaf
        draw.rectangle([inset, height - 18 + leaf, width - 1 - inset, height - 17 + leaf], fill=(214, 196, 158, 255))
    spine = width // 2
    rng = random.Random(11)
    for x in range(12, width - 12):
        distance = abs(x - spine) / (spine - 12)
        shade = 0.72 + 0.28 * min(1.0, distance * 3.0)
        base = (240, 226, 192)
        colour = tuple(int(c * shade) for c in base)
        top = 12 + int(6 * (1 - distance) ** 2)
        draw.line([(x, top), (x, height - 22)], fill=(*colour, 255))
    for _ in range(700):
        x, y0 = rng.randrange(14, width - 14), rng.randrange(16, height - 24)
        r, g, b, a = cast(tuple[int, int, int, int], img.getpixel((x, y0)))
        tone = rng.choice((-10, -6, 6))
        img.putpixel((x, y0), (max(0, r + tone), max(0, g + tone), max(0, b + tone), a))
    draw.line([(spine, 16), (spine, height - 20)], fill=(120, 90, 60, 255))
    ribbon = spine + 172
    draw.polygon(
        [(ribbon, 10), (ribbon + 8, 10), (ribbon + 8, height + 0), (ribbon + 4, height - 6), (ribbon, height)],
        fill=(150, 30, 26, 255),
    )
    draw.line([(ribbon + 1, 10), (ribbon + 1, height - 4)], fill=(196, 64, 52, 255))
    for cx, cy, dx, dy in (
        (22, 22, 1, 1),
        (width - 23, 22, -1, 1),
        (22, height - 32, 1, -1),
        (width - 23, height - 32, -1, -1),
    ):
        draw.line([(cx, cy), (cx + dx * 16, cy)], fill=(150, 40, 30, 255))
        draw.line([(cx, cy), (cx, cy + dy * 16)], fill=(150, 40, 30, 255))
        draw.point((cx + dx * 4, cy + dy * 4), fill=(150, 40, 30, 255))
    return img.resize((width * 2, height * 2), Image.Resampling.NEAREST)


def main() -> None:
    """Write the book and the two menu scenes."""
    gs.ROOT.joinpath("textures", "ui").mkdir(parents=True, exist_ok=True)
    book_art().save(gs.ROOT / "textures" / "ui" / "book.png", optimize=True)
    gs.write("scenes/title.owl", title_scene())
    gs.write("scenes/settings.owl", settings_scene())


if __name__ == "__main__":
    main()
