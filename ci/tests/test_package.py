"""`Package` restores the working directory after CPack, even when it fails."""

from os import getcwd
from pathlib import Path

import pytest

from ci.actions import package
from ci.actions.package import Package
from ci.utils.preset import PresetConfig


@pytest.fixture
def mocked(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def no_sign(_bin_dir: Path) -> int:
        return 0

    monkeypatch.setattr(package, "sign_binaries", no_sign)
    monkeypatch.setattr(PresetConfig, "get_build_dir", lambda _self: tmp_path)


def test_package_restores_the_working_directory(mocked: None, monkeypatch: pytest.MonkeyPatch) -> None:
    def fake_run(_cmd: list[str], **_: object) -> int:
        return 0

    monkeypatch.setattr(package, "run_command", fake_run)
    before = getcwd()
    assert Package().run(PresetConfig("linux-clang-release")) == 0
    assert getcwd() == before


def test_package_restores_the_working_directory_on_failure(mocked: None, monkeypatch: pytest.MonkeyPatch) -> None:
    def fake_run(_cmd: list[str], **_: object) -> int:
        return 1

    monkeypatch.setattr(package, "run_command", fake_run)
    before = getcwd()
    assert Package().run(PresetConfig("linux-clang-release")) == 1
    assert getcwd() == before
