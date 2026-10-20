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


def test_artifacts_keep_reports_and_packages_only(monkeypatch: pytest.MonkeyPatch) -> None:
    rules = exported(monkeypatch, "linux-clang-debug")["artifact_path"]
    assert "/bin" not in rules
    assert "/lib" not in rules
    assert "TestReports.zip!debug/" in rules
    assert "TestReports.zip!release/" in rules
    assert "Coverage.zip" in rules
    assert "Documentation.zip" in rules
