#!/usr/bin/env python3
"""Doxygen INPUT_FILTER that converts GitHub-style .md links to @ref page links.

Transforms [text](filename.md) into [text](@ref page-id) for Doxygen,
using the {#page-id} heading anchors present in each .md file.

Usage in Doxyfile:
    FILTER_PATTERNS = *.md="python3 /path/to/fix_md_links.py"

Doxygen invokes: python3 fix_md_links.py <input-file>
and reads the filtered content from stdout.
"""

import re
import sys

# Map from filename (without .md) to page ID.
PAGE_MAP = {
    "architecture": "page-architecture",
    "building": "page-building",
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


def fix_links(content: str) -> str:
    """Replace [text](X.md) and [text](doc/pages/X.md) with [text](@ref page-X)."""

    def replace_link(m: re.Match) -> str:
        text = m.group(1)
        path = m.group(2)
        # Strip optional doc/pages/ prefix.
        basename = path.replace("doc/pages/", "").removesuffix(".md")
        if basename in PAGE_MAP:
            return f"[{text}](@ref {PAGE_MAP[basename]})"
        return m.group(0)

    return re.sub(r"\[([^\]]+)\]\(((?:doc/pages/)?[a-z_]+\.md)\)", replace_link, content)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: fix_md_links.py <input-file>")
    with open(sys.argv[1]) as f:
        content = f.read()
    sys.stdout.write(fix_links(content))
