#!/usr/bin/env python3
"""Generate UI icons for OwlNest editor.

Style: white monochrome outline on transparent background, anti-aliased.
Matches the existing file/folder icons theme.
Draws at 4x resolution then downscales for smooth anti-aliasing.
"""

from PIL import Image, ImageDraw
import math
import os

SIZE = 512
SCALE = 4  # Draw at 4x for anti-aliasing
S = SIZE * SCALE
WHITE = (255, 255, 255, 255)
LINE = S // 16  # Line width scaled (thick enough for 16x16 display)


def new_img():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def finish(img, name):
    """Downscale from 4x to 512x512 and save."""
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    path = os.path.join(os.path.dirname(__file__), "actions", name + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"  {name}.png")


def P(x, y):
    """Scale a coordinate from 512-space to drawing space."""
    return (int(x * SCALE), int(y * SCALE))


def icon_delete():
    """Trash can - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Lid line
    d.rounded_rectangle([P(130, 110), P(382, 140)], radius=w * 2, outline=WHITE, width=w)
    # Handle on lid
    d.rounded_rectangle([P(200, 80), P(312, 115)], radius=w * 2, outline=WHITE, width=w)
    # Body
    d.rounded_rectangle([P(150, 145), P(362, 420)], radius=w * 3, outline=WHITE, width=w)
    # Vertical lines inside
    for x in [220, 256, 292]:
        d.line([P(x, 195), P(x, 370)], fill=WHITE, width=w)
    finish(img, "delete")


def icon_rename():
    """Pencil/edit - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Pencil body (diagonal rectangle)
    cx, cy = S // 2, S // 2
    pts = [P(370, 80), P(415, 125), P(185, 395), P(140, 350)]
    d.polygon(pts, outline=WHITE, width=w)
    # Pencil tip line
    d.line([P(185, 395), P(140, 350)], fill=WHITE, width=w)
    # Tip
    d.line([P(140, 350), P(105, 430)], fill=WHITE, width=w)
    d.line([P(185, 395), P(105, 430)], fill=WHITE, width=w)
    # Eraser separator line
    d.line([P(340, 115), P(385, 160)], fill=WHITE, width=w)
    finish(img, "rename")


def icon_new_folder():
    """Folder with + sign - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Folder tab
    d.rounded_rectangle([P(80, 110), P(230, 175)], radius=w * 3, outline=WHITE, width=w)
    # Folder body
    d.rounded_rectangle([P(80, 160), P(432, 410)], radius=w * 3, outline=WHITE, width=w)
    # Plus sign
    cx, cy = 256, 300
    d.line([P(cx - 55, cy), P(cx + 55, cy)], fill=WHITE, width=w + w // 2)
    d.line([P(cx, cy - 55), P(cx, cy + 55)], fill=WHITE, width=w + w // 2)
    finish(img, "new_folder")


def icon_import_file():
    """Document with down arrow - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Document outline with folded corner
    pts = [P(140, 80), P(330, 80), P(390, 140), P(390, 430), P(140, 430)]
    d.polygon(pts, outline=WHITE, width=w)
    # Fold line
    d.line([P(330, 80), P(330, 140), P(390, 140)], fill=WHITE, width=w)
    # Down arrow
    cx = 265
    d.line([P(cx, 200), P(cx, 340)], fill=WHITE, width=w + w // 2)
    d.line([P(cx - 50, 295), P(cx, 345), P(cx + 50, 295)], fill=WHITE, width=w + w // 2)
    finish(img, "import_file")


def icon_import_folder():
    """Folder with down arrow - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Folder tab
    d.rounded_rectangle([P(80, 110), P(230, 175)], radius=w * 3, outline=WHITE, width=w)
    # Folder body
    d.rounded_rectangle([P(80, 160), P(432, 410)], radius=w * 3, outline=WHITE, width=w)
    # Down arrow
    cx, cy = 256, 295
    d.line([P(cx, cy - 55), P(cx, cy + 35)], fill=WHITE, width=w + w // 2)
    d.line([P(cx - 45, cy), P(cx, cy + 45), P(cx + 45, cy)], fill=WHITE, width=w + w // 2)
    finish(img, "import_folder")


def icon_add_entity():
    """Plus in circle - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    cx, cy = S // 2, S // 2
    r = int(180 * SCALE)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=w)
    arm = int(100 * SCALE)
    d.line([(cx - arm, cy), (cx + arm, cy)], fill=WHITE, width=w + w // 2)
    d.line([(cx, cy - arm), (cx, cy + arm)], fill=WHITE, width=w + w // 2)
    finish(img, "add_entity")


def icon_delete_entity():
    """X in circle - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    cx, cy = S // 2, S // 2
    r = int(180 * SCALE)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=w)
    arm = int(75 * SCALE)
    d.line([(cx - arm, cy - arm), (cx + arm, cy + arm)], fill=WHITE, width=w + w // 2)
    d.line([(cx - arm, cy + arm), (cx + arm, cy - arm)], fill=WHITE, width=w + w // 2)
    finish(img, "delete_entity")


def icon_save():
    """Floppy disk - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Outer frame
    d.rounded_rectangle([P(95, 80), P(417, 430)], radius=w * 3, outline=WHITE, width=w)
    # Top slot
    d.rectangle([P(175, 80), P(350, 195)], outline=WHITE, width=w)
    # Metal slider in slot
    d.rectangle([P(275, 100), P(325, 185)], outline=WHITE, width=w)
    # Label at bottom
    d.rounded_rectangle([P(155, 265), P(357, 400)], radius=w * 2, outline=WHITE, width=w)
    # Lines on label
    d.line([P(185, 310), P(327, 310)], fill=WHITE, width=w // 2 + 1)
    d.line([P(185, 345), P(327, 345)], fill=WHITE, width=w // 2 + 1)
    finish(img, "save")


def icon_open():
    """Open folder - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Back folder
    d.rounded_rectangle([P(80, 120), P(210, 185)], radius=w * 3, outline=WHITE, width=w)
    d.rounded_rectangle([P(80, 170), P(400, 400)], radius=w * 3, outline=WHITE, width=w)
    # Front flap (opened)
    pts = [P(100, 235), P(435, 235), P(400, 400), P(80, 400)]
    d.polygon(pts, outline=WHITE, width=w)
    finish(img, "open")


def icon_new_scene():
    """Document with star - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Document
    pts = [P(140, 80), P(330, 80), P(390, 140), P(390, 430), P(140, 430)]
    d.polygon(pts, outline=WHITE, width=w)
    d.line([P(330, 80), P(330, 140), P(390, 140)], fill=WHITE, width=w)
    # 4-pointed star
    cx, cy = 265, 275
    for angle in range(0, 360, 90):
        rad = math.radians(angle)
        x1 = cx + int(65 * math.cos(rad))
        y1 = cy + int(65 * math.sin(rad))
        d.line([P(cx, cy), P(x1, y1)], fill=WHITE, width=w)
    for angle in range(45, 360, 90):
        rad = math.radians(angle)
        x1 = cx + int(35 * math.cos(rad))
        y1 = cy + int(35 * math.sin(rad))
        d.line([P(cx, cy), P(x1, y1)], fill=WHITE, width=w)
    finish(img, "new_scene")


def icon_duplicate():
    """Two overlapping documents - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Back document
    d.rounded_rectangle([P(170, 80), P(410, 370)], radius=w * 2, outline=WHITE, width=w)
    # Front document
    d.rounded_rectangle([P(100, 150), P(340, 430)], radius=w * 2, fill=(0, 0, 0, 0), outline=WHITE, width=w)
    # Fill front doc background to hide back doc lines
    d.rounded_rectangle([P(100 + 3, 150 + 3), P(340 - 3, 430 - 3)], radius=w * 2, fill=(0, 0, 0, 0))
    finish(img, "duplicate")


def icon_undo():
    """Curved arrow left - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE + LINE // 2
    # Arc
    d.arc([P(130, 130), P(420, 380)], start=200, end=360, fill=WHITE, width=w)
    # Arrow head
    d.line([P(145, 200), P(130, 270), P(210, 265)], fill=WHITE, width=w)
    finish(img, "undo")


def icon_redo():
    """Curved arrow right - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE + LINE // 2
    d.arc([P(92, 130), P(382, 380)], start=180, end=340, fill=WHITE, width=w)
    d.line([P(365, 200), P(382, 270), P(300, 265)], fill=WHITE, width=w)
    finish(img, "redo")


def icon_settings():
    """Gear - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    cx, cy = S // 2, S // 2
    outer_r = int(200 * SCALE)
    inner_r = int(160 * SCALE)
    hole_r = int(70 * SCALE)
    # Draw gear teeth
    teeth = 8
    for i in range(teeth):
        a1 = math.radians(i * 360 / teeth - 14)
        a2 = math.radians(i * 360 / teeth + 14)
        a_mid = math.radians(i * 360 / teeth)
        pts = [
            (cx + int(inner_r * math.cos(a1)), cy + int(inner_r * math.sin(a1))),
            (cx + int(outer_r * math.cos(a1)), cy + int(outer_r * math.sin(a1))),
            (cx + int(outer_r * math.cos(a2)), cy + int(outer_r * math.sin(a2))),
            (cx + int(inner_r * math.cos(a2)), cy + int(inner_r * math.sin(a2))),
        ]
        d.polygon(pts, outline=WHITE, width=w)
        d.line([pts[0], pts[1]], fill=WHITE, width=w)
        d.line([pts[2], pts[3]], fill=WHITE, width=w)
    # Outer circle connecting teeth
    d.ellipse([cx - inner_r, cy - inner_r, cx + inner_r, cy + inner_r], outline=WHITE, width=w)
    # Center hole
    d.ellipse([cx - hole_r, cy - hole_r, cx + hole_r, cy + hole_r], outline=WHITE, width=w)
    finish(img, "settings")


def icon_search():
    """Magnifying glass - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE + LINE // 2
    cx, cy = int(220 * SCALE), int(220 * SCALE)
    r = int(120 * SCALE)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=w)
    # Handle
    hx, hy = int(340 * SCALE), int(340 * SCALE)
    ex, ey = int(420 * SCALE), int(420 * SCALE)
    d.line([(hx, hy), (ex, ey)], fill=WHITE, width=w + w // 2)
    finish(img, "search")


def icon_pack():
    """Package/box - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Box body
    d.rounded_rectangle([P(95, 180), P(417, 420)], radius=w * 3, outline=WHITE, width=w)
    # Lid (trapezoid)
    d.polygon([P(95, 180), P(170, 100), P(345, 100), P(417, 180)], outline=WHITE, width=w)
    # Center divider on lid
    d.line([P(256, 100), P(256, 180)], fill=WHITE, width=w)
    # Center tape on body
    d.line([P(256, 180), P(256, 420)], fill=WHITE, width=w)
    # Cross on top for packing
    d.line([P(200, 140), P(312, 140)], fill=WHITE, width=w)
    finish(img, "pack")


def icon_back():
    """Left chevron/arrow - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE + LINE // 2
    # Chevron <
    d.line([P(310, 110), P(180, 256), P(310, 402)], fill=WHITE, width=w, joint="curve")
    finish(img, "back")


def icon_scene_hierarchy():
    """Tree/hierarchy - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Root node
    d.ellipse([P(100, 80), P(180, 160)], outline=WHITE, width=w)
    # Vertical trunk from root
    d.line([P(140, 160), P(140, 370)], fill=WHITE, width=w)
    # Branch 1
    d.line([P(140, 220), P(260, 220)], fill=WHITE, width=w)
    d.ellipse([P(260, 190), P(320, 250)], outline=WHITE, width=w)
    # Branch 2
    d.line([P(140, 310), P(260, 310)], fill=WHITE, width=w)
    d.ellipse([P(260, 280), P(320, 340)], outline=WHITE, width=w)
    # Sub-branch from branch 2
    d.line([P(290, 340), P(290, 400)], fill=WHITE, width=w)
    d.line([P(290, 400), P(370, 400)], fill=WHITE, width=w)
    d.ellipse([P(370, 370), P(430, 430)], outline=WHITE, width=w)
    finish(img, "scene_hierarchy")


def icon_content_browser():
    """Grid of files - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # 2x2 grid of small documents
    for col in range(2):
        for row in range(2):
            x0 = 95 + col * 175
            y0 = 85 + row * 180
            x1 = x0 + 145
            y1 = y0 + 150
            d.rounded_rectangle([P(x0, y0), P(x1, y1)], radius=w * 2, outline=WHITE, width=w)
            # Small lines inside each
            d.line([P(x0 + 25, y0 + 55), P(x1 - 25, y0 + 55)], fill=WHITE, width=w // 2 + 1)
            d.line([P(x0 + 25, y0 + 85), P(x1 - 25, y0 + 85)], fill=WHITE, width=w // 2 + 1)
    finish(img, "content_browser")


def icon_stats():
    """Bar chart - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Axes
    d.line([P(100, 100), P(100, 420), P(420, 420)], fill=WHITE, width=w)
    # Bars
    bar_w = 55
    bars = [(160, 280), (235, 180), (310, 320), (385, 140)]
    for x, top in bars:
        d.rectangle([P(x, top), P(x + bar_w, 415)], outline=WHITE, width=w)
    finish(img, "stats")


def icon_properties():
    """Sliders/parameters - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Three horizontal slider lines with knobs
    for i, (knob_x) in enumerate([200, 310, 170]):
        y = 150 + i * 100
        d.line([P(100, y), P(412, y)], fill=WHITE, width=w)
        r = 22
        d.ellipse([P(knob_x - r, y - r), P(knob_x + r, y + r)], fill=WHITE)
    finish(img, "properties")


def icon_log():
    """Terminal/console - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Monitor frame
    d.rounded_rectangle([P(85, 95), P(427, 375)], radius=w * 4, outline=WHITE, width=w)
    # Top bar
    d.line([P(85, 145), P(427, 145)], fill=WHITE, width=w)
    # Dots in top bar
    for x in [115, 150, 185]:
        d.ellipse([P(x - 8, 112), P(x + 8, 128)], fill=WHITE)
    # Prompt lines
    d.line([P(115, 195), P(165, 195)], fill=WHITE, width=w)
    d.line([P(175, 195), P(340, 195)], fill=WHITE, width=w)
    d.line([P(115, 245), P(290, 245)], fill=WHITE, width=w)
    d.line([P(115, 295), P(155, 295)], fill=WHITE, width=w)
    # Stand
    d.line([P(210, 375), P(210, 415)], fill=WHITE, width=w)
    d.line([P(302, 375), P(302, 415)], fill=WHITE, width=w)
    d.line([P(170, 415), P(342, 415)], fill=WHITE, width=w)
    finish(img, "log")


def icon_viewport():
    """Camera/viewport frame - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Outer frame
    d.rounded_rectangle([P(85, 120), P(427, 400)], radius=w * 3, outline=WHITE, width=w)
    # Crosshair
    cx, cy = 256, 260
    arm = 45
    d.line([P(cx - arm, cy), P(cx + arm, cy)], fill=WHITE, width=w)
    d.line([P(cx, cy - arm), P(cx, cy + arm)], fill=WHITE, width=w)
    # Corner brackets (viewfinder)
    blen = 35
    # Top-left
    d.line([P(130, 160), P(130 + blen, 160)], fill=WHITE, width=w)
    d.line([P(130, 160), P(130, 160 + blen)], fill=WHITE, width=w)
    # Top-right
    d.line([P(382, 160), P(382 - blen, 160)], fill=WHITE, width=w)
    d.line([P(382, 160), P(382, 160 + blen)], fill=WHITE, width=w)
    # Bottom-left
    d.line([P(130, 360), P(130 + blen, 360)], fill=WHITE, width=w)
    d.line([P(130, 360), P(130, 360 - blen)], fill=WHITE, width=w)
    # Bottom-right
    d.line([P(382, 360), P(382 - blen, 360)], fill=WHITE, width=w)
    d.line([P(382, 360), P(382, 360 - blen)], fill=WHITE, width=w)
    finish(img, "viewport")


def icon_close():
    """X mark - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE + LINE // 2
    d.line([P(150, 150), P(362, 362)], fill=WHITE, width=w)
    d.line([P(362, 150), P(150, 362)], fill=WHITE, width=w)
    finish(img, "close")


def icon_project():
    """Briefcase - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Handle
    d.rounded_rectangle([P(185, 90), P(327, 175)], radius=w * 3, outline=WHITE, width=w)
    # Body
    d.rounded_rectangle([P(80, 170), P(432, 400)], radius=w * 4, outline=WHITE, width=w)
    # Center clasp
    d.line([P(80, 270), P(210, 270)], fill=WHITE, width=w)
    d.line([P(302, 270), P(432, 270)], fill=WHITE, width=w)
    d.rounded_rectangle([P(210, 250), P(302, 290)], radius=w * 2, outline=WHITE, width=w)
    finish(img, "project")


def icon_exit():
    """Door with arrow - outline style."""
    img = new_img()
    d = ImageDraw.Draw(img)
    w = LINE
    # Door frame
    d.rounded_rectangle([P(160, 80), P(380, 430)], radius=w * 3, outline=WHITE, width=w)
    # Door handle
    d.ellipse([P(330, 245), P(355, 270)], outline=WHITE, width=w)
    # Arrow pointing out (left)
    d.line([P(80, 256), P(200, 256)], fill=WHITE, width=w + w // 2)
    d.line([P(80, 256), P(130, 210)], fill=WHITE, width=w + w // 2)
    d.line([P(80, 256), P(130, 302)], fill=WHITE, width=w + w // 2)
    finish(img, "exit")


if __name__ == "__main__":
    print("Generating OwlNest icons (512x512 white outline)...")
    icon_delete()
    icon_rename()
    icon_new_folder()
    icon_import_file()
    icon_import_folder()
    icon_add_entity()
    icon_delete_entity()
    icon_save()
    icon_open()
    icon_new_scene()
    icon_duplicate()
    icon_undo()
    icon_redo()
    icon_settings()
    icon_search()
    icon_pack()
    icon_back()
    icon_scene_hierarchy()
    icon_content_browser()
    icon_stats()
    icon_properties()
    icon_log()
    icon_viewport()
    icon_close()
    icon_project()
    icon_exit()
    print(f"Done! 26 icons generated.")
