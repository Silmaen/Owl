#!/usr/bin/env python3
"""Final pass over the help bundle of Owl Nest (`<build>/help_bundle/`, written by `cmake/HelpAssets.cmake`).

The help panel reads plain Markdown without the web around it, so every page is rewritten:

* a link to a page that is not in the bundle (a design page, a source file, `LICENSE`) becomes its text: the help
  never shows a link that leads nowhere;
* a status badge (a shields.io image, inline or by reference) becomes its label as inline code, and the reference
  definitions of the badges go: the panel never fetches an image from the network.

Links to bundled pages, external links (`http(s)://`, `mailto:`) and anchors (`#section`) are kept. Fenced code
blocks are left untouched.

Usage: python tools/doc/help_bundle.py <help_dir>
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_FENCE = re.compile(r"^\s*(```|~~~)")
_BADGE_HOSTS = ("img.shields.io",)
_REF_DEF = re.compile(r"^\[([^\]]+)\]:\s*(\S+)\s*$")
_INLINE_IMAGE = re.compile(r"!\[([^\]\n]*)\]\((https?://[^)\s]+)\)")
_REF_IMAGE = re.compile(r"!\[([^\]\n]*)\]\[([^\]\n]+)\]")
_LINK = re.compile(r"(?<![!\]])\[([^\]\n]+)\]\(([^)\s]+)\)")
_KEPT_SCHEMES = ("http://", "https://", "mailto:", "#")


def _is_badge(url: str) -> bool:
    return any(f"//{host}/" in url for host in _BADGE_HOSTS)


def _page_id(target: str) -> str | None:
    """The page id of a relative `.md` link (file name up to its first dot, lower case), else None."""
    path = target.split("#", 1)[0]
    if not path.endswith(".md"):
        return None
    return path.rsplit("/", 1)[-1].split(".", 1)[0].lower()


def rewrite_page(text: str, bundled: set[str]) -> str:
    """Rewrite one page: links outside `bundled` (lower-case page ids) to text, badges to inline code."""
    lines = text.split("\n")
    badge_refs = set()
    for line in lines:
        match = _REF_DEF.match(line)
        if match and _is_badge(match.group(2)):
            badge_refs.add(match.group(1).lower())

    def image(match: re.Match[str]) -> str:
        return f"`{match.group(1)}`" if _is_badge(match.group(2)) else match.group(0)

    def ref_image(match: re.Match[str]) -> str:
        return f"`{match.group(1)}`" if match.group(2).lower() in badge_refs else match.group(0)

    def link(match: re.Match[str]) -> str:
        target = match.group(2)
        if target.startswith(_KEPT_SCHEMES):
            return match.group(0)
        page = _page_id(target)
        if page is not None and page in bundled:
            return match.group(0)
        return match.group(1)

    out: list[str] = []
    in_fence = False
    for line in lines:
        if _FENCE.match(line):
            in_fence = not in_fence
            out.append(line)
            continue
        if in_fence:
            out.append(line)
            continue
        definition = _REF_DEF.match(line)
        if definition and definition.group(1).lower() in badge_refs:
            continue
        line = _INLINE_IMAGE.sub(image, line)
        line = _REF_IMAGE.sub(ref_image, line)
        out.append(_LINK.sub(link, line))
    return "\n".join(out)


def bundle(help_dir: Path) -> int:
    """Rewrite every page of a help bundle; returns the number of pages."""
    pages = sorted(help_dir.glob("*.md"))
    bundled = {page.name.split(".", 1)[0].lower() for page in pages}
    for page in pages:
        original = page.read_text(encoding="utf-8")
        rewritten = rewrite_page(original, bundled)
        if rewritten != original:
            page.write_text(rewritten, encoding="utf-8")
    return len(pages)


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 1:
        print("Usage: help_bundle.py <help_dir>", file=sys.stderr)
        return 2
    help_dir = Path(args[0])
    if not help_dir.is_dir():
        print(f"help_bundle.py: no folder {help_dir}", file=sys.stderr)
        return 1
    bundle(help_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
