"""Artifact rules exported by `DefineTeamCityVariables`."""

import pytest

from ci.actions.define_variables import DefineTeamCityVariables
from ci.utils import teamcity
from ci.utils.preset import PresetConfig


def exported(monkeypatch: pytest.MonkeyPatch, preset: str) -> dict[str, str]:
    params: dict[str, str] = {}
    monkeypatch.setattr(teamcity, "set_teamcity_parameter", lambda name, value: params.__setitem__(name, value))
    assert DefineTeamCityVariables().run(PresetConfig(preset)) == 0
    return params


def test_debug_tree_keeps_reports_and_coverage(monkeypatch: pytest.MonkeyPatch) -> None:
    params = exported(monkeypatch, "linux-gcc-debug")
    rules = params["artifact_path"]
    assert "/bin" not in rules
    assert "/lib" not in rules
    assert "TestReports.zip" in rules
    assert "Coverage.zip" in rules
    assert "Documentation.zip" not in rules
    assert "tar.gz" not in rules
    assert "release_preset" not in params


def test_release_tree_keeps_documentation_and_archives(monkeypatch: pytest.MonkeyPatch) -> None:
    params = exported(monkeypatch, "linux-clang-release")
    rules = params["artifact_path"]
    assert "Coverage.zip" not in rules
    assert "Documentation.zip" in rules
    assert "/*.tar.gz" in rules
    assert params["run_documentation"] == "true"
    assert params["run_package"] == "true"


def test_arm64_release_tree_is_packaged_without_documentation(monkeypatch: pytest.MonkeyPatch) -> None:
    params = exported(monkeypatch, "linux-cross-arm64")
    assert params["run_package"] == "true"
    assert params["run_tests"] == "true"
    assert params["run_documentation"] == "false"
    assert params["publish_doc"] == "false"
