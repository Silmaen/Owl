"""
Run a Web build of Owl in a headless browser and return its exit code.

The folder holding the page (``OwlGameRunner.html``, its ``.js`` and ``.wasm``, the game files and their
``files.json``) is served on 127.0.0.1 with the cross-origin isolation headers the threads need. The page, opened with
``?report``, sends its console output to ``/owl-log`` and its exit code to ``/owl-exit``: the output is printed here,
the exit code is this program's.

Usage::

    python tools/web/browser_run.py <folder> [--page OwlGameRunner.html] [--timeout 120] [--browser chrome]
        [--browser-flag <flag>]... [--env NAME=value]... [--browser-log] [--shot frame.png] [-- <runner arguments>]

The browser is ``--browser``, else ``$OWL_CHROME`` (set in the Web build image), else ``chrome-headless-shell`` on the
``PATH``. It renders WebGPU with SwiftShader, without a GPU.
"""

from __future__ import annotations

import argparse
import functools
import os
import shutil
import subprocess
import sys
import tempfile
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlencode


class _Outcome:
    """What the page reported: its exit code once it ended, and where its canvas capture goes."""

    def __init__(self, shot: Path | None = None) -> None:
        self.code: str | None = None
        self.shot = shot
        self.done = threading.Event()


class _Handler(SimpleHTTPRequestHandler):
    """Serves the folder with the isolation headers and receives the reports of the page."""

    outcome: _Outcome

    def end_headers(self) -> None:
        # SharedArrayBuffer, hence the threads, exists only in a cross-origin isolated page.
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_POST(self) -> None:  # noqa: N802 (name set by BaseHTTPRequestHandler)
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        text = body.decode("utf-8", errors="replace")
        if self.path == "/owl-log":
            sys.stdout.write(text)
            sys.stdout.flush()
        elif self.path == "/owl-shot" and self.outcome.shot is not None:
            self.outcome.shot.write_bytes(body)
        elif self.path == "/owl-exit":
            self.outcome.code = text.strip()
            self.outcome.done.set()
        self.send_response(204)
        self.end_headers()

    def log_message(self, format: str, *args: object) -> None:  # noqa: A002 (signature of the base class)
        """Silent: the page's own output is what matters."""


def find_browser(explicit: str | None) -> str | None:
    """The headless browser to run: the explicit one, $OWL_CHROME, else chrome-headless-shell on the PATH."""
    for candidate in (explicit, os.environ.get("OWL_CHROME"), shutil.which("chrome-headless-shell")):
        if candidate:
            return candidate
    return None


def run_page(
    folder: Path,
    page: str,
    runner_args: list[str],
    browser: str,
    timeout: float,
    flags: list[str] | None = None,
    env: list[str] | None = None,
    browser_log: bool = False,
    shot: Path | None = None,
) -> int:
    """Serve the folder, open the page in the browser and wait for its exit code (124 on timeout)."""
    outcome = _Outcome(shot)
    handler = type("Handler", (_Handler,), {"outcome": outcome})
    server = ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(handler, directory=str(folder)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    pairs = [("report", "1")] + [("arg", arg) for arg in runner_args] + [("env", item) for item in env or []]
    query = urlencode(pairs)
    url = f"http://127.0.0.1:{server.server_address[1]}/{page}?{query}"
    with tempfile.TemporaryDirectory(prefix="owl-browser-") as profile:
        command = [
            browser,
            "--headless",
            "--no-sandbox",
            "--no-first-run",
            f"--user-data-dir={profile}",
            # WebGPU on SwiftShader's Vulkan (without the Vulkan feature the adapter gives no device), the page
            # composited by ANGLE on SwiftShader (else the canvas cannot get its shared image: the device is lost).
            "--enable-unsafe-swiftshader",
            "--enable-unsafe-webgpu",
            "--enable-features=Vulkan",
            "--use-vulkan=swiftshader",
            "--use-webgpu-adapter=swiftshader",
            "--use-gl=angle",
            "--use-angle=swiftshader",
            *(flags or []),
            *(["--enable-logging=stderr", "--v=0"] if browser_log else []),
            url,
        ]
        output = None if browser_log else subprocess.DEVNULL
        with subprocess.Popen(command, stdout=output, stderr=output) as process:
            ended = outcome.done.wait(timeout)
            process.terminate()
            try:
                process.wait(10)
            except subprocess.TimeoutExpired:
                process.kill()
    server.shutdown()
    if not ended:
        print(f"browser_run: no exit code from the page after {timeout:.0f} s.", file=sys.stderr)
        return 124
    try:
        return int(outcome.code or "")
    except ValueError:
        print(f"browser_run: the page stopped: {outcome.code}.", file=sys.stderr)
        return 125


def main(argv: list[str] | None = None) -> int:
    """Command line entry point."""
    arguments = list(sys.argv[1:] if argv is None else argv)
    runner_args: list[str] = []
    if "--" in arguments:
        split = arguments.index("--")
        arguments, runner_args = arguments[:split], arguments[split + 1 :]
    parser = argparse.ArgumentParser(description="Run a Web build of Owl in a headless browser.")
    parser.add_argument("folder", type=Path, help="Folder holding the page and the game files")
    parser.add_argument("--page", default="OwlGameRunner.html", help="Page to open in the folder")
    parser.add_argument("--timeout", type=float, default=120.0, help="Seconds to wait for the exit code")
    parser.add_argument("--browser", default=None, help="Headless browser (default: $OWL_CHROME)")
    parser.add_argument("--browser-flag", action="append", default=[], help="Extra flag of the browser, repeatable")
    parser.add_argument("--env", action="append", default=[], help="NAME=value of the page's environment, repeatable")
    parser.add_argument("--browser-log", action="store_true", help="Print the log of the browser (its console too)")
    parser.add_argument(
        "--shot", type=Path, default=None, help="PNG file for the frame the page captures (OWL_WEBGPU_CAPTURE=<frame>)"
    )
    options = parser.parse_args(arguments)
    browser = find_browser(options.browser)
    if browser is None:
        print("browser_run: no headless browser (set OWL_CHROME or pass --browser).", file=sys.stderr)
        return 2
    if not (options.folder / options.page).is_file():
        print(f"browser_run: no page '{options.page}' in '{options.folder}'.", file=sys.stderr)
        return 2
    return run_page(
        options.folder,
        options.page,
        runner_args,
        browser,
        options.timeout,
        options.browser_flag,
        options.env,
        options.browser_log,
        options.shot,
    )


if __name__ == "__main__":
    sys.exit(main())
