"""
Tests of the mermaid to SVG renderer used by the in-editor help: every diagram of the documentation renders to
well-formed plain SVG (no `foreignObject`), and unsupported syntax is reported instead of dropped.
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from tools.doc import mermaid_svg as m

ROOT = Path(__file__).resolve().parents[2]
FENCE = re.compile(r"^```mermaid\n(.*?)^```", re.DOTALL | re.MULTILINE)


def _doc_diagrams() -> list[tuple[str, str]]:
    found: list[tuple[str, str]] = []
    for page in [*sorted((ROOT / "doc" / "pages").rglob("*.md")), ROOT / "README.md", ROOT / "CONTRIBUTING.md"]:
        for index, match in enumerate(FENCE.finditer(page.read_text(encoding="utf-8"))):
            found.append((f"{page.name}#{index}", match.group(1)))
    return found


def _texts(svg: str) -> list[str]:
    root = ET.fromstring(svg)
    return ["".join(t.itertext()) for t in root.iter("{http://www.w3.org/2000/svg}text")]


@pytest.mark.parametrize(
    ("name", "source"), _doc_diagrams(), ids=lambda v: v if isinstance(v, str) and "#" in v else ""
)
def test_every_documented_diagram_renders(name: str, source: str) -> None:
    svg = m.render(source)
    root = ET.fromstring(svg)
    assert root.tag.endswith("svg")
    assert "foreignObject" not in svg
    assert "<script" not in svg
    assert float(root.attrib["width"]) > 0
    assert float(root.attrib["height"]) > 0


def test_there_are_documented_diagrams() -> None:
    assert len(_doc_diagrams()) >= 50


def test_flowchart_labels_and_edges() -> None:
    svg = m.render('flowchart LR\n  A[First] -->|go| B{Choice?}\n  B -->|yes| C["Done<br/>now"]\n  B -.-> A\n')
    texts = _texts(svg)
    for expected in ("First", "Choice?", "Done", "now", "go", "yes"):
        assert expected in texts
    assert svg.count("<path") >= 3 * 2  # three edges, each with a head


def test_flowchart_subgraph_and_class_def() -> None:
    svg = m.render(
        "flowchart TD\n subgraph G[Group]\n  A & B\n end\n classDef hot fill:#ff0000\n class A hot\n A --> B\n"
    )
    assert "Group" in _texts(svg)
    assert "#ff0000" in svg


def test_cycle_and_self_loop_do_not_hang() -> None:
    assert m.render("flowchart LR\n A --> B\n B --> A\n A --> A\n")


def test_edges_both_ways_between_two_nodes_do_not_overlap() -> None:
    svg = m.render("flowchart LR\n  A[Edit] -->|go| B[Play]\n  B -->|back| A\n")
    paths = re.findall(r'<path d="([^"]+)" fill="none"', svg)
    assert len(paths) == 2 and paths[0] != paths[1]
    boxes = re.findall(r'<rect x="([-\d.]+)" y="([-\d.]+)" width="[\d.]+" height="([\d.]+)" rx="4"', svg)
    assert len(boxes) == 2
    (_, y0, h0), (_, y1, _) = boxes
    assert abs(float(y0) - float(y1)) >= float(h0)


def test_state_diagram_pseudo_states() -> None:
    svg = m.render("stateDiagram-v2\n [*] --> Idle\n Idle --> Busy : start\n Busy --> [*]\n")
    assert "Idle" in _texts(svg)
    assert "start" in _texts(svg)
    assert svg.count("<circle") >= 3


def test_sequence_diagram_blocks_and_notes() -> None:
    svg = m.render(
        "sequenceDiagram\n participant A as Alpha\n A->>B: hi\n loop twice\n B-->>A: back\n end\n"
        " Note over A,B: shared\n"
    )
    texts = _texts(svg)
    assert "Alpha" in texts
    assert "hi" in texts
    assert "shared" in texts
    assert "loop" in texts


def test_class_diagram_members() -> None:
    svg = m.render("classDiagram\n class A {\n  +run()\n }\n class B\n B --|> A : extends\n")
    assert "+run()" in _texts(svg)


@pytest.mark.parametrize(
    "source",
    [
        "pie\n a: 1",
        "flowchart LR\n A --> \n",
        "sequenceDiagram\n loop x\n A->>B: m\n",
        "stateDiagram-v2\n note right of A: x\n",
        "",
    ],
)
def test_unsupported_or_broken_input_is_reported(source: str) -> None:
    with pytest.raises(m.MermaidError):
        m.render(source)


def test_markdown_rewrite_replaces_fences(tmp_path: Path) -> None:
    text = (
        "Intro\n\n```mermaid\nflowchart LR\n A --> B\n```\n\n"
        "- item\n\n  ```mermaid\n  flowchart LR\n   C --> D\n  ```\n"
    )
    out = m.rewrite_markdown(text, tmp_path)
    assert "```mermaid" not in out
    refs = re.findall(r"!\[[^\]]*\]\((images/mermaid/[0-9a-f]+\.svg)\)", out)
    assert len(refs) == 2
    for ref in refs:
        assert (tmp_path / Path(ref).name).is_file()
    assert "  ![Flowchart](" in out


def test_bundle_rewrites_pages_in_place(tmp_path: Path) -> None:
    (tmp_path / "page.md").write_text("# T\n\n```mermaid\nflowchart LR\n A --> B\n```\n", encoding="utf-8")
    assert m.bundle(tmp_path) == 1
    assert "```mermaid" not in (tmp_path / "page.md").read_text(encoding="utf-8")
    assert len(list((tmp_path / "images" / "mermaid").glob("*.svg"))) == 1
