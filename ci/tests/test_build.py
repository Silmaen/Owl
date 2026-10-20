"""`--configure_only` of the `Build` action."""

from pathlib import Path

import pytest

from ci.actions import build
from ci.actions.build import Build
from ci.utils.preset import PresetConfig


@pytest.fixture
def commands(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> list[list[str]]:
    calls: list[list[str]] = []

    def fake_run(cmd: list[str], **_: object) -> int:
        calls.append(cmd)
        return 0

    monkeypatch.setattr(build, "run_command", fake_run)
    monkeypatch.setattr(build, "get_build_dir", lambda _: tmp_path)
    return calls


def test_configure_only_stops_after_configure(commands: list[list[str]]) -> None:
    assert Build().run(PresetConfig("linux-clang-release"), ["--configure_only"]) == 0
    assert len(commands) == 1
    assert commands[0][:3] == ["cmake", "--preset", "linux-clang-release"]


def test_default_configures_then_builds(commands: list[list[str]]) -> None:
    assert Build().run(PresetConfig("linux-clang-release")) == 0
    assert [cmd[:2] for cmd in commands] == [["cmake", "--preset"], ["cmake", "--build"]]
