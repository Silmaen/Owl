#!/usr/bin/env python3
"""Capture the Owl Nest screenshots of the documentation (`doc/images/nest/`).

Runs inside the build image, headless: a private Xvfb display, Owl Nest on Vulkan (lavapipe) with the Engine Demo,
one editor start per shot. Each shot sets the editor up through its own means only: the reopened session of
`OwlNest_settings.yml` (documents, active tab, selected entity), synthetic mouse and keyboard events (XTest) and the
MCP server. The display is then grabbed, cropped and written as an optimised PNG.

Usage (from the repository root):
    ./docker_run.sh poetry run python tools/doc/capture_nest.py [--build output/build/linux-clang-release]
        [--only main_window,console] [--raw output/nest_capture/raw]
"""

from __future__ import annotations

import argparse
import ctypes
import io
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from PIL import Image, ImageGrab

ROOT = Path(__file__).resolve().parents[2]
WIDTH, HEIGHT = 1920, 1080
DISPLAY = ":87"
MCP_PORT = 47877
MAX_BYTES = 400 * 1024
# Where the pointer waits during a grab: an empty spot of the status bar, so no hover highlight or tooltip shows.
PARK = (1000, 1075)

Step = tuple[Any, ...]


@dataclass
class Shot:
    """One screenshot: the session to reopen, the gestures to replay, the part of the screen to keep."""

    name: str
    documents: list[str] = field(default_factory=list)
    selected: int = 0
    steps: list[Step] = field(default_factory=list)
    crop: tuple[int, int, int, int] | None = None
    settle: float = 12.0


# Coordinates are screen pixels of the default layout at 1920 x 1080 (ribbon tabs, Content Browser tiles, Console
# tab and line, viewport Play button): a moved widget means new coordinates here. Steps: ("wait", s), ("click", x, y),
# ("dclick", x, y), ("rclick", x, y), ("move", x, y), ("key", "ctrl+grave"), ("type", text), ("mcp", tool, args).
SHOTS: list[Shot] = [
    Shot(
        "main_window",
        documents=["scenes/iso_town.owl"],
        selected=4300000000000000010,
        steps=[("dclick", 538, 900), ("wait", 25)],
    ),
    Shot(
        "file_screen",
        documents=["scenes/iso_town.owl"],
        steps=[("click", 32, 18), ("wait", 2)],
        crop=(0, 0, 1140, 460),
    ),
    Shot("tilemap_editor", documents=["tilemaps/iso_town.owltilemap"]),
    Shot("hud_editor", documents=["huds/gameplay.owlhud"], steps=[("click", 1240, 274), ("wait", 2)]),
    Shot(
        "scene_flow",
        documents=["scenes/iso_town.owl"],
        steps=[("click", 98, 18), ("click", 192, 70), ("wait", 4)],
        crop=(0, 125, 1920, 760),
    ),
    Shot("action_map", documents=["input_actions.yml"], crop=(0, 125, 1460, 760)),
    Shot(
        "console",
        documents=["scenes/iso_town.owl"],
        steps=[
            ("mcp", "entity.create", {"name": "Lamp"}),
            ("mcp", "entity.set_transform", {"entity": "Lamp", "translation": [12, 40, 0]}),
            ("mcp", "entity.duplicate", {"entity": "Lamp"}),
            ("mcp", "entity.rename", {"entity": "Station", "name": "Central Station"}),
            ("click", 278, 773),
            ("click", 700, 1026),
            ("type", "help entity.rename"),
            ("key", "Return"),
            ("wait", 1),
        ],
        crop=(0, 760, 1460, 1050),
    ),
    Shot("play_mode", documents=["scenes/platformer_house.owl"], steps=[("click", 689, 175), ("wait", 8)]),
]


class XDisplay:
    """The few Xlib / XTest calls the capture needs, through ctypes (no python-xlib in the image)."""

    def __init__(self, name: str) -> None:
        self.x11 = ctypes.CDLL("libX11.so.6")
        self.xtst = ctypes.CDLL("libXtst.so.6")
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        self.x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
        self.x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        self.x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XKeycodeToKeysym.restype = ctypes.c_ulong
        self.x11.XKeycodeToKeysym.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, ctypes.c_int]
        self.x11.XFlush.argtypes = [ctypes.c_void_p]
        self.x11.XMoveResizeWindow.argtypes = [
            ctypes.c_void_p,
            ctypes.c_ulong,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_uint,
            ctypes.c_uint,
        ]
        self.xtst.XTestFakeMotionEvent.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_ulong,
        ]
        self.xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        self.xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        self.dpy = self.x11.XOpenDisplay(name.encode())
        if not self.dpy:
            raise RuntimeError(f"cannot open display {name}")

    def resize(self, window: int, width: int, height: int) -> None:
        self.x11.XMoveResizeWindow(self.dpy, window, 0, 0, width, height)
        self.x11.XFlush(self.dpy)

    def move(self, x: int, y: int) -> None:
        self.xtst.XTestFakeMotionEvent(self.dpy, -1, x, y, 0)
        self.x11.XFlush(self.dpy)

    def click(self, x: int, y: int, button: int = 1, double: bool = False) -> None:
        self.move(x, y)
        time.sleep(0.3)
        for _ in range(2 if double else 1):
            self.xtst.XTestFakeButtonEvent(self.dpy, button, 1, 0)
            self.x11.XFlush(self.dpy)
            time.sleep(0.12)
            self.xtst.XTestFakeButtonEvent(self.dpy, button, 0, 0)
            self.x11.XFlush(self.dpy)
            time.sleep(0.12)

    def _code(self, keysym: str) -> int:
        return int(self.x11.XKeysymToKeycode(self.dpy, self.x11.XStringToKeysym(keysym.encode())))

    def key(self, combo: str) -> None:
        """Press a combination such as `F5`, `ctrl+grave` or `Return` (X keysym names)."""
        names = combo.split("+")
        mods = [self._code({"ctrl": "Control_L", "shift": "Shift_L", "alt": "Alt_L"}[m]) for m in names[:-1]]
        code = self._code(names[-1])
        for mod in mods:
            self.xtst.XTestFakeKeyEvent(self.dpy, mod, 1, 0)
        self.xtst.XTestFakeKeyEvent(self.dpy, code, 1, 0)
        self.x11.XFlush(self.dpy)
        time.sleep(0.1)
        self.xtst.XTestFakeKeyEvent(self.dpy, code, 0, 0)
        for mod in reversed(mods):
            self.xtst.XTestFakeKeyEvent(self.dpy, mod, 0, 0)
        self.x11.XFlush(self.dpy)
        time.sleep(0.1)

    def type(self, text: str) -> None:
        names = {" ": "space", ".": "period", "=": "equal", "_": "underscore", "/": "slash", "-": "minus"}
        for char in text:
            keysym = names.get(char, char)
            sym = self.x11.XStringToKeysym(keysym.encode())
            code = self.x11.XKeysymToKeycode(self.dpy, sym)
            shifted = self.x11.XKeycodeToKeysym(self.dpy, code, 0) != sym
            self.key(f"shift+{keysym}" if shifted else keysym)


def find_window(display: str) -> tuple[int, int, int] | None:
    """Largest top-level window of the display: (id, width, height)."""
    out = subprocess.run(
        ["xwininfo", "-root", "-children", "-display", display], capture_output=True, text=True, check=False
    ).stdout
    best: tuple[int, int, int] | None = None
    for match in re.finditer(r"^\s+(0x[0-9a-f]+) .*?(\d+)x(\d+)\+-?\d+\+-?\d+", out, re.MULTILINE):
        wid, w, h = int(match.group(1), 16), int(match.group(2)), int(match.group(3))
        if w > 200 and h > 200 and (best is None or w * h > best[1] * best[2]):
            best = (wid, w, h)
    return best


class Mcp:
    """Minimal JSON-RPC client of the editor's MCP server."""

    def __init__(self, home: Path) -> None:
        self.token = (home / ".config" / "owlnest" / "mcp_token").read_text().strip()
        self.next_id = 1

    def call(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        body = {"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params or {}}
        self.next_id += 1
        request = urllib.request.Request(
            f"http://127.0.0.1:{MCP_PORT}/mcp",
            data=json.dumps(body).encode(),
            headers={"Authorization": f"Bearer {self.token}", "Content-Type": "application/json"},
        )
        with urllib.request.urlopen(request, timeout=600) as response:
            result: dict[str, Any] = json.loads(response.read())
        return result

    def tool(self, name: str, arguments: dict[str, Any]) -> dict[str, Any]:
        return self.call("tools/call", {"name": name, "arguments": arguments})


def write_settings(run_dir: Path, shot: Shot, demo: Path) -> None:
    lines = [
        "EditorSettings:",
        "  showStats: true",
        "  themePreset: Owl Dark",
        "  autosaveIntervalSeconds: 0",
        "  mcpEnabled: true",
        f"  mcpPort: {MCP_PORT}",
        "  layoutPreset: Default",
        "  recentProjects:",
        f"    - {demo.as_posix()}",
    ]
    if shot.documents:
        lines += ["  projectSessions:", f"    - project: {demo.as_posix()}", "      documents:"]
        lines += [f"        - {doc}" for doc in shot.documents]
        lines += [f"      active: {shot.documents[-1]}", f"      selectedEntity: {shot.selected}"]
    (run_dir / "OwlNest_settings.yml").write_text("\n".join(lines) + "\n")
    config = ["AppConfig:", f"  width: {WIDTH}", f"  height: {HEIGHT}", "  renderer: Vulkan", "  sound: Null"]
    config += ["  hasGui: true", "  installDesktopEntry: false", "  hotReload: false"]
    (run_dir / "config.yml").write_text("\n".join(config) + "\n")
    # A fresh default layout, and no crash-recovery prompt from the documents a previous shot left modified.
    (run_dir / "OwlNest_imgui.ini").unlink(missing_ok=True)
    shutil.rmtree(run_dir / "OwlNest_recovery", ignore_errors=True)


def save_png(image: Image.Image, path: Path) -> int:
    """Write the smallest acceptable PNG: full colour, else 256 colours, else downscaled until it fits."""
    candidates = [image]
    while True:
        for candidate in candidates:
            buffer = io.BytesIO()
            candidate.save(buffer, "PNG", optimize=True)
            if buffer.tell() <= MAX_BYTES:
                path.write_bytes(buffer.getvalue())
                return buffer.tell()
            buffer = io.BytesIO()
            candidate.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(
                buffer, "PNG", optimize=True
            )
            if buffer.tell() <= MAX_BYTES:
                path.write_bytes(buffer.getvalue())
                return buffer.tell()
        image = image.resize((image.width * 7 // 8, image.height * 7 // 8), Image.Resampling.LANCZOS)
        candidates = [image]


def run_steps(xd: XDisplay, mcp: Mcp | None, steps: list[Step]) -> None:
    for step in steps:
        kind = step[0]
        if kind == "wait":
            time.sleep(step[1])
        elif kind == "click":
            xd.click(step[1], step[2])
        elif kind == "dclick":
            xd.click(step[1], step[2], double=True)
        elif kind == "rclick":
            xd.click(step[1], step[2], button=3)
        elif kind == "move":
            xd.move(step[1], step[2])
        elif kind == "key":
            xd.key(step[1])
        elif kind == "type":
            xd.type(step[1])
        elif kind == "mcp" and mcp is not None:
            result = mcp.tool(step[1], step[2])
            if result.get("result", {}).get("isError"):
                print(f"  mcp {step[1]}: {json.dumps(result)[:300]}", file=sys.stderr)


# The File screen prints the project paths: open a copy of the projects at a short path (inside the container) so no
# local path shows; a link would not do, the editor resolves it.
NEUTRAL_ROOT = Path("/tmp/OwlEngine")


def link_neutral_root() -> None:
    shutil.rmtree(NEUTRAL_ROOT, ignore_errors=True)
    for folder in ("engine_demo", "samples"):
        shutil.copytree(ROOT / folder, NEUTRAL_ROOT / folder)


def capture(shot: Shot, args: argparse.Namespace) -> Image.Image | None:
    run_dir = ROOT / "output" / "nest_capture" / "run"
    run_dir.mkdir(parents=True, exist_ok=True)
    demo = NEUTRAL_ROOT / "engine_demo"
    write_settings(run_dir, shot, demo)
    binary = (ROOT / args.build / "bin" / "OwlNest").resolve()
    env = dict(os.environ, DISPLAY=DISPLAY, VK_ICD_FILENAMES="/usr/share/vulkan/icd.d/lvp_icd.json")
    env.pop("WAYLAND_DISPLAY", None)
    env.pop("XDG_RUNTIME_DIR", None)
    command = [str(binary), "--project", str(demo)]
    log = (run_dir / f"{shot.name}.log").open("w")
    editor = subprocess.Popen(command, cwd=run_dir, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        window = None
        for _ in range(240):
            window = find_window(DISPLAY)
            if window is not None or editor.poll() is not None:
                break
            time.sleep(0.5)
        if window is None:
            print(f"{shot.name}: no editor window (see {log.name})", file=sys.stderr)
            return None
        xd = XDisplay(DISPLAY)
        xd.resize(window[0], WIDTH, HEIGHT)
        xd.move(WIDTH // 2, HEIGHT // 2)
        time.sleep(shot.settle)
        mcp = None
        if any(step[0] == "mcp" for step in shot.steps):
            mcp = Mcp(Path(os.environ.get("HOME", "/fhome")))
            mcp.call("initialize", {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "doc"}})
        run_steps(xd, mcp, shot.steps)
        xd.move(*PARK)
        time.sleep(1.5)
        image = ImageGrab.grab(xdisplay=DISPLAY).convert("RGB")
        if args.raw:
            raw = ROOT / args.raw
            raw.mkdir(parents=True, exist_ok=True)
            image.save(raw / f"{shot.name}.png")
        return image.crop(shot.crop) if shot.crop else image
    finally:
        editor.terminate()
        try:
            editor.wait(timeout=20)
        except subprocess.TimeoutExpired:
            editor.kill()
            editor.wait()
        log.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default="output/build/linux-clang-release", help="Build tree holding bin/OwlNest.")
    parser.add_argument("--out", default="doc/images/nest", help="Folder of the screenshots.")
    parser.add_argument("--only", default="", help="Comma-separated shot names (default: all).")
    parser.add_argument("--raw", default="", help="Also keep the uncropped frames in this folder.")
    args = parser.parse_args()
    wanted = {name for name in args.only.split(",") if name}
    if shutil.which("Xvfb") is None:
        print("Xvfb is missing from the image.", file=sys.stderr)
        return 1
    link_neutral_root()
    xvfb = subprocess.Popen(
        ["Xvfb", DISPLAY, "-screen", "0", f"{WIDTH}x{HEIGHT}x24", "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    failures = 0
    try:
        time.sleep(2)
        out = ROOT / args.out
        out.mkdir(parents=True, exist_ok=True)
        for shot in SHOTS:
            if wanted and shot.name not in wanted:
                continue
            image = capture(shot, args)
            if image is None:
                failures += 1
                continue
            size = save_png(image, out / f"{shot.name}.png")
            print(f"{shot.name}.png: {image.width}x{image.height}, {size // 1024} KB")
    finally:
        xvfb.terminate()
        xvfb.wait()
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
