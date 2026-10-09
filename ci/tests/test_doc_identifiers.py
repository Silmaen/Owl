"""Stale identifier detection of the CodeStyle `doc-identifiers` sub-check."""

from pathlib import Path

from ci.utils.doc_identifiers import build_index, classify, code_spans, doc_pages, stale_identifiers


def _repo(tmp_path: Path) -> Path:
    """A tiny repository: one public header, one CMake module and three pages."""
    header = tmp_path / "source" / "owl" / "public" / "renderer" / "Camera.h"
    header.parent.mkdir(parents=True)
    header.write_text(
        'namespace owl::renderer {\nclass CameraOrtho { void setPosition(); };\nconstexpr auto k = "runner.yml";\n}\n'
    )
    cmake = tmp_path / "cmake" / "Options.cmake"
    cmake.parent.mkdir()
    cmake.write_text('option(OWL_BUILD_NEST "Nest" ON)\nset(${PROJECT_PREFIX}_CONAN_HOME "" CACHE PATH "")\n')
    pages = tmp_path / "doc" / "pages"
    (pages / "design").mkdir(parents=True)
    (pages / "changelog.md").write_text("`RemovedLongAgo`\n")
    (pages / "design" / "plan.md").write_text("`FutureRenderer`\n")
    return tmp_path


def _files(repo: Path) -> list[str]:
    return [p.relative_to(repo).as_posix() for p in repo.rglob("*") if p.is_file()]


def test_spans_are_classified_by_shape() -> None:
    assert classify("source/owl/public/scene/Scene.h") == "path"
    assert classify("Renderer3D.cpp") == "path"
    assert classify("OWL_BUILD_NEST") == "option"
    assert classify("renderer::CameraOrtho") == "symbol"
    assert classify("CameraOrtho::setPosition()") == "symbol"
    assert classify("scene.find_entity") == "call"
    assert classify(".owl") is None
    assert classify("docker/run.sh cmake --build") is None
    assert classify("true") is None
    assert classify("output/build/<preset>") is None
    assert classify("tracy/0.13.1") is None


def test_fenced_blocks_are_ignored() -> None:
    spans = [span for _, _, span in code_spans("`A`\n```c++\n`B`\n```\ntext `C` and `D`\n")]
    assert spans == ["A", "C", "D"]


def test_only_missing_identifiers_are_reported(tmp_path: Path) -> None:
    repo = _repo(tmp_path)
    page = repo / "doc" / "pages" / "renderer.md"
    page.write_text(
        "`CameraOrtho`, `renderer::CameraOrtho`, `CameraOrtho::setPosition()`, `Camera.h`,\n"
        "`source/owl/public/renderer/`, `OWL_BUILD_NEST`, `OWL_CONAN_HOME`, `runner.yml`, `std::vector`.\n"
        "`renderer::stack::CameraOrtho`, `CameraPerspective`, `Camera.cpp`, `OWL_ENABLE_GONE`, `ImLightRig`.\n"
    )
    index = build_index(repo, _files(repo))

    found = stale_identifiers(doc_pages(repo), index)

    assert [(f.line, f.span) for f in found] == [
        (3, "renderer::stack::CameraOrtho"),
        (3, "CameraPerspective"),
        (3, "Camera.cpp"),
        (3, "OWL_ENABLE_GONE"),
    ]


def test_changelog_and_design_pages_are_skipped(tmp_path: Path) -> None:
    repo = _repo(tmp_path)

    assert doc_pages(repo) == []
