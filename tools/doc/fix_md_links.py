#!/usr/bin/env python3
"""Doxygen INPUT_FILTER that converts GitHub-style .md links to @ref page links.

Transforms [text](path/to/page.md) into [text](@ref page-id) for Doxygen, using the {#page-id}
heading anchor found on the first line of the linked file. The link path is resolved relative to
the filtered file, so links into sub-directories (`design/owl-rhi.md`) and back up
(`../roadmap.md`) work. Links whose target has no anchor (root `CHANGELOG.md`, `ROADMAP.md`) are
left untouched.

A line `<!-- subpage: page.md -->` becomes `\\subpage page-id` (GitHub and the help panel never show it). The page
tree comes from `doc/help.yml`, the order of the help panel: the README (main page) lists the first page of each
group, the first page of a group the other pages of its group; the markers attach the pages outside the help (the
design pages).

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
    "lua-api": "page-lua-api",
    "sound": "page-sound",
}

_ANCHOR = re.compile(r"\{#([A-Za-z0-9_-]+)\}")
_SUBPAGE = re.compile(r"<!--\s*subpage:\s*([A-Za-z0-9_./-]+\.md)\s*-->")
_LINK = re.compile(r"\[([^\]]+)\]\(([A-Za-z0-9_./-]+\.md)\)")
_TOC = Path(__file__).resolve().parents[2] / "doc" / "help.yml"
_TOC_GROUP = re.compile(r"^  - title: (.+)$")
_TOC_PAGE = re.compile(r"^      - ([A-Za-z0-9_./-]+\.md)$")


def read_toc(toc: Path) -> list[list[Path]]:
    """The groups of `doc/help.yml`, each the absolute paths of its pages in order (root = the folder above doc/)."""
    root = toc.resolve().parents[1]
    groups: list[list[Path]] = []
    try:
        lines = toc.read_text(encoding="utf-8").splitlines()
    except OSError:
        return groups
    for line in lines:
        if _TOC_GROUP.match(line):
            groups.append([])
        elif (page := _TOC_PAGE.match(line)) and groups:
            groups[-1].append((root / page.group(1)).resolve())
    return groups


def toc_subpages(source: Path, toc: Path = _TOC) -> list[Path]:
    """The pages Doxygen hangs under `source`, in the order of the help."""
    main = toc.resolve().parents[1] / "README.md"
    groups = [[page for page in group if page != main] for group in read_toc(toc)]
    groups = [group for group in groups if group]
    src = source.resolve()
    if src == main:
        return [group[0] for group in groups]
    for group in groups:
        if group[0] == src:
            return group[1:]
    return []


def _anchor_of(target: Path) -> str | None:
    """Return the `{#id}` anchor of the first heading of `target`, or None."""
    try:
        with target.open(encoding="utf-8") as f:
            first = f.readline()
    except OSError:
        return None
    m = _ANCHOR.search(first)
    return m.group(1) if m else None


def fix_links(content: str, source: Path | None = None, toc: Path = _TOC) -> str:
    """Replace [text](X.md) links with [text](@ref page-id) when the target page has an anchor, and append the
    `\\subpage` list `doc/help.yml` gives `source`."""

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

    def replace_subpage(m: re.Match) -> str:
        page_id = _anchor_of((source.parent / m.group(1)).resolve()) if source is not None else None
        return f"\\subpage {page_id}" if page_id else m.group(0)

    # Doxygen copies the project logo and the `doc/images` files flat next to the pages.
    content = content.replace('src="engine_assets/logo/', 'src="').replace('src="doc/images/', 'src="')
    content = _LINK.sub(replace_link, _SUBPAGE.sub(replace_subpage, content))
    if source is not None:
        ids = [page_id for page in toc_subpages(source, toc) if (page_id := _anchor_of(page))]
        if ids:
            content = content.rstrip("\n") + "\n\n" + "".join(f"- \\subpage {page_id}\n" for page_id in ids)
    return content


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: fix_md_links.py <input-file>")
    src = Path(sys.argv[1])
    with src.open(encoding="utf-8") as f:
        content = f.read()
    # Doxygen reads UTF-8; on Windows sys.stdout uses the locale codepage, which cannot encode ✅ / ❌.
    sys.stdout.buffer.write(fix_links(content, src).encode("utf-8"))
