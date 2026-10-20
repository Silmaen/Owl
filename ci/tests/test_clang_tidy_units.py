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
    test = tool_arguments("analyzer", root / "test" / "unit" / "core_tests" / "log_test.cpp")

    assert engine == TOOL_ARGS["analyzer"]
    assert test[0].endswith(",-clang-analyzer-optin.core.EnumCastOutOfRange,-clang-analyzer-security.FloatLoopCounter")
    assert tool_arguments("tidy", root / "test" / "unit" / "core_tests" / "log_test.cpp") == []


def test_ninja_and_make_closures_agree(tmp_path: Path) -> None:
    ninja = (
        "obj/a.o: #deps 2, deps mtime 1 (VALID)\n    ../src/a.cpp\n    ../inc/x.h\n\n"
        "obj/b.o: #deps 1\n    ../src/b.cpp\n"
    )
    make = "obj/a.o: ../src/a.cpp \\\n  ../inc/x.h\nobj/b.o: ../src/b.cpp\n"

    from_ninja = dict(clang_tidy._ninja_closures(ninja, tmp_path))
    from_make = dict(clang_tidy._make_closures(make, tmp_path))

    expected = {
        str(tmp_path / "obj" / "a.o"): ["../src/a.cpp", "../inc/x.h"],
        str(tmp_path / "obj" / "b.o"): ["../src/b.cpp"],
    }
    assert from_ninja == expected
    assert from_make == expected


def test_a_configured_tree_is_scoped_by_the_scanner(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    header = clang_tidy._norm("../inc/x.h", tmp_path)
    scan = "obj/a.o: ../src/a.cpp ../inc/x.h\nobj/b.o: b.cpp\n"
    monkeypatch.setattr(clang_tidy, "_scanned_dependencies", lambda _: scan)

    assert clang_tidy._dependent_objects(tmp_path, {header}) == {str(tmp_path / "obj" / "a.o")}


def test_a_failed_scan_widens_to_the_full_scope(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(clang_tidy, "_scanned_dependencies", lambda _: None)

    assert clang_tidy._dependent_objects(tmp_path, {"x.h"}) is None
