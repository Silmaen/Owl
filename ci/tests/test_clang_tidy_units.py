"""Translation-unit selection of the ClangTidy action."""

import json
from pathlib import Path

import pytest

from ci.actions import clang_tidy


def _touch(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("int x;\n", encoding="utf-8")
    return path


def test_only_repository_sources_are_analysed(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    repo = tmp_path / "repo"
    build = repo / "output" / "build" / "linux-clang-tidy"
    ours = _touch(repo / "source" / "owl" / "Scene.cpp")
    copied = _touch(build / "imgui_bindings" / "backends" / "imgui_impl_glfw.cpp")
    cached = _touch(tmp_path / "conan" / "p" / "imgui" / "imgui_stdlib.cpp")
    build.mkdir(parents=True, exist_ok=True)
    entries = [
        {"directory": str(build), "file": str(source), "output": f"obj/{source.stem}.o"}
        for source in (ours, copied, cached)
    ]
    (build / "compile_commands.json").write_text(json.dumps(entries), encoding="utf-8")
    monkeypatch.setattr(clang_tidy, "root", repo.resolve())

    units = clang_tidy._load_translation_units(build)

    assert units is not None
    assert list(units.values()) == [ours]


def test_the_analyzer_relaxes_only_test_files() -> None:
    from ci import root
    from ci.actions.clang_tidy import TOOL_ARGS, tool_arguments

    engine = tool_arguments("analyzer", root / "source" / "owl" / "private" / "core" / "Log.cpp")
    test = tool_arguments("analyzer", root / "test" / "core_tests" / "log_test.cpp")

    assert engine == TOOL_ARGS["analyzer"]
    assert test[0].endswith(",-clang-analyzer-optin.core.EnumCastOutOfRange,-clang-analyzer-security.FloatLoopCounter")
    assert tool_arguments("tidy", root / "test" / "core_tests" / "log_test.cpp") == []
