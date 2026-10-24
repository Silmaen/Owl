"""Tests of the final pass over the help bundle: no dead link, no badge, code left alone."""

from __future__ import annotations

from pathlib import Path

from tools.doc import help_bundle as h

BUNDLED = {"editor", "scene", "readme"}


def test_link_to_a_bundled_page_is_kept() -> None:
    assert h.rewrite_page("See [Scene](scene.md#components).", BUNDLED) == "See [Scene](scene.md#components)."
    assert h.rewrite_page("[Editor](doc/pages/editor.md)", BUNDLED) == "[Editor](doc/pages/editor.md)"


def test_link_outside_the_bundle_becomes_its_text() -> None:
    assert h.rewrite_page("A [plan](design/owl-rhi.md) and the [MIT License](LICENSE).", BUNDLED) == (
        "A plan and the MIT License."
    )


def test_external_links_anchors_and_images_are_kept() -> None:
    text = "[site](https://owl.argawaen.net) [top](#top) ![shot](images/nest/main_window.png) [m](mailto:a@b.c)"
    assert h.rewrite_page(text, BUNDLED) == text


def test_badges_become_inline_code_and_their_definitions_go() -> None:
    text = "- ![Done][done] Item\n- ![x](https://img.shields.io/badge/x-1-blue)\n\n[done]: https://img.shields.io/badge/-Done-2ea043\n"
    assert h.rewrite_page(text, BUNDLED) == "- `Done` Item\n- `x`\n\n"


def test_fenced_code_is_left_alone() -> None:
    text = "```markdown\n[plan](design/owl-rhi.md)\n```\n"
    assert h.rewrite_page(text, BUNDLED) == text


def test_bundle_rewrites_every_page(tmp_path: Path) -> None:
    (tmp_path / "editor.md").write_text("[Scene](scene.md) [Plan](design/x.md)\n")
    (tmp_path / "scene.md").write_text("[Editor](editor.md)\n")
    assert h.bundle(tmp_path) == 2
    assert (tmp_path / "editor.md").read_text() == "[Scene](scene.md) Plan\n"
