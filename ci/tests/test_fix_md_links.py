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


def _toc(tmp_path: Path) -> Path:
    (tmp_path / "doc" / "pages").mkdir(parents=True)
    for name, anchor in (("home", "page-home"), ("tour", "page-tour"), ("guide", "page-guide"), ("api", "page-api")):
        (tmp_path / "doc" / "pages" / f"{name}.md").write_text(f"# {name} {{#{anchor}}}\n")
    (tmp_path / "README.md").write_text("# Readme\n")
    toc = tmp_path / "doc" / "help.yml"
    toc.write_text(
        "Groups:\n"
        "  - title: Start\n    pages:\n      - doc/pages/home.md\n      - README.md\n      - doc/pages/tour.md\n"
        "  - title: Guides\n    pages:\n      - doc/pages/guide.md\n      - doc/pages/api.md\n"
    )
    return toc


def test_readme_lists_the_first_page_of_each_group(tmp_path: Path) -> None:
    toc = _toc(tmp_path)
    text = fix_md_links.fix_links("# Readme\n", tmp_path / "README.md", toc)
    assert text == "# Readme\n\n- \\subpage page-home\n- \\subpage page-guide\n"


def test_group_head_lists_the_rest_of_its_group_without_the_readme(tmp_path: Path) -> None:
    toc = _toc(tmp_path)
    home = tmp_path / "doc" / "pages" / "home.md"
    assert fix_md_links.fix_links("# home\n", home, toc).endswith("- \\subpage page-tour\n")
    assert fix_md_links.fix_links("x\n", tmp_path / "doc" / "pages" / "api.md", toc) == "x\n"


def test_repository_toc_lists_every_doc_page_once() -> None:
    groups = fix_md_links.read_toc(fix_md_links._TOC)
    pages = [page for group in groups for page in group]
    assert len(pages) == len(set(pages))
    doc_pages = {page.resolve() for page in (fix_md_links._TOC.parent / "pages").glob("*.md")}
    assert doc_pages <= set(pages)
