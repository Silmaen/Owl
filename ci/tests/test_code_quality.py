"""Diagnostic lines turned into a GitLab Code Quality report."""

import json
from pathlib import Path

import pytest

from ci import root
from ci.utils import code_quality
from ci.utils.code_quality import to_issues, write_report


def test_code_style_finding_keeps_its_sub_check() -> None:
    [issue] = to_issues("code-style", ["source/owl/public/core/Core.h:12:1: error: cpp-style: banned std::unique_ptr"])

    assert issue["check_name"] == "cpp-style"
    assert issue["severity"] == "major"
    assert issue["location"] == {"path": "source/owl/public/core/Core.h", "lines": {"begin": 12}}


def test_clang_tidy_finding_takes_its_trailing_check_and_a_relative_path() -> None:
    line = f"{root}/source/owl/private/core/Log.cpp:3:7: warning: use auto [modernize-use-auto]"
    [issue] = to_issues("clang-tidy", [line])

    assert issue["check_name"] == "modernize-use-auto"
    assert issue["severity"] == "minor"
    assert issue["location"]["path"] == "source/owl/private/core/Log.cpp"


def test_notes_and_progress_lines_are_not_findings() -> None:
    assert to_issues("clang-tidy", ["[1/3] source/a.cpp", "source/a.cpp:1:1: note: expanded from macro"]) == []


def test_the_fingerprint_is_stable_and_tells_findings_apart() -> None:
    first = to_issues("t", ["a.cpp:1:1: error: x: one"])[0]["fingerprint"]
    assert first == to_issues("t", ["a.cpp:1:1: error: x: one"])[0]["fingerprint"]
    assert first != to_issues("t", ["a.cpp:2:1: error: x: one"])[0]["fingerprint"]


def test_an_empty_report_is_still_written(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(code_quality, "report_dir", lambda: tmp_path)
    write_report("code-style", "code-style", [])

    assert json.loads((tmp_path / "code-style.json").read_text()) == []
