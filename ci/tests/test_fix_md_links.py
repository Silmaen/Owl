"""Doxygen markdown filter: page links and the subpage markers."""

import importlib.util
from pathlib import Path

_PATH = Path(__file__).parents[2] / "tools" / "doc" / "fix_md_links.py"
_SPEC = importlib.util.spec_from_file_location("fix_md_links", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
fix_md_links = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(fix_md_links)


def _pages(tmp_path: Path) -> Path:
    (tmp_path / "child.md").write_text("# Child {#page-child}\n")
    page = tmp_path / "parent.md"
    page.write_text("# Parent {#page-parent}\n")
    return page


def test_subpage_marker_becomes_subpage_command(tmp_path: Path) -> None:
    page = _pages(tmp_path)
    assert fix_md_links.fix_links("<!-- subpage: child.md -->\n", page) == "\\subpage page-child\n"


def test_subpage_marker_of_unknown_page_is_left_alone(tmp_path: Path) -> None:
    page = _pages(tmp_path)
    marker = "<!-- subpage: missing.md -->"
    assert fix_md_links.fix_links(marker, page) == marker


def test_page_link_becomes_ref(tmp_path: Path) -> None:
    page = _pages(tmp_path)
    assert fix_md_links.fix_links("[Child](child.md)", page) == "[Child](@ref page-child)"


def test_img_source_points_at_the_flat_doxygen_images(tmp_path: Path) -> None:
    page = _pages(tmp_path)
    assert (
        fix_md_links.fix_links('<img src="engine_assets/logo/a.png" width="9">', page) == '<img src="a.png" width="9">'
    )
