#!/usr/bin/env python3
"""Doxygen INPUT_FILTER that converts GitHub-style .md links to @ref page links.

Transforms [text](path/to/page.md) into [text](@ref page-id) for Doxygen, using the {#page-id}
heading anchor found on the first line of the linked file. The link path is resolved relative to
the filtered file, so links into sub-directories (`design/owl-rhi.md`) and back up
(`../roadmap.md`) work. Links whose target has no anchor (root `CHANGELOG.md`, `ROADMAP.md`) are
left untouched.

Usage in Doxyfile:
    FILTER_PATTERNS = *.md="python3 /path/to/fix_md_links.py"

Doxygen invokes: python3 fix_md_links.py <input-file>
and reads the filtered content from stdout.
"""

import re
import sys
from pathlib import Path

# Fallback map from filename (without .md) to page ID, used when the target cannot be read.
PAGE_MAP = {
    "architecture": "page-architecture",
    "building": "page-building",
    "changelog": "page-changelog",
    "contributing": "page-contributing",
    "editor": "page-editor",
    "event_input": "page-event-input",
    "physics": "page-physics",
    "renderer": "page-renderer",
    "roadmap": "page-roadmap",
    "scene": "page-scene",
    "scripting": "page-scripting",
    "sound": "page-sound",
}

_ANCHOR = re.compile(r"\{#([A-Za-z0-9_-]+)\}")
_LINK = re.compile(r"\[([^\]]+)\]\(([A-Za-z0-9_./-]+\.md)\)")


def _anchor_of(target: Path) -> str | None:
    """Return the `{#id}` anchor of the first heading of `target`, or None."""
    try:
        with target.open(encoding="utf-8") as f:
            first = f.readline()
    except OSError:
        return None
    m = _ANCHOR.search(first)
    return m.group(1) if m else None


def fix_links(content: str, source: Path | None = None) -> str:
    """Replace [text](X.md) links with [text](@ref page-id) when the target page has an anchor."""

    def replace_link(m: re.Match) -> str:
        text = m.group(1)
        path = m.group(2)
        if path.startswith(("http:", "https:")):
            return m.group(0)
        page_id = None
        if source is not None:
            page_id = _anchor_of((source.parent / path).resolve())
        if page_id is None and "/" not in path.replace("doc/pages/", ""):
            page_id = PAGE_MAP.get(path.replace("doc/pages/", "").removesuffix(".md"))
        if page_id is None:
            return m.group(0)
        return f"[{text}](@ref {page_id})"

    return _LINK.sub(replace_link, content)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: fix_md_links.py <input-file>")
    src = Path(sys.argv[1])
    with src.open(encoding="utf-8") as f:
        content = f.read()
    # Doxygen reads UTF-8; on Windows sys.stdout uses the locale codepage, which cannot encode ✅ / ❌.
    sys.stdout.buffer.write(fix_links(content, src).encode("utf-8"))
