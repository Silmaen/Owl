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


def test_package_logs_each_package_size(
    mocked: None, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    (tmp_path / "OwlNest-0.8.0-abc1234-linux-x64.tar.gz").write_bytes(b"x" * 2_500_000)
    (tmp_path / "OwlEngine-0.8.0-abc1234-linux-x64.tar.gz").write_bytes(b"x" * 500_000)

    def fake_run(_cmd: list[str], **_: object) -> int:
        return 0

    monkeypatch.setattr(package, "run_command", fake_run)
    with caplog.at_level("INFO"):
        assert Package().run(PresetConfig("linux-clang-release")) == 0
    messages = "\n".join(caplog.messages)
    assert "Package: OwlNest-0.8.0-abc1234-linux-x64.tar.gz, 2.5 MB." in messages
    assert "Package: OwlEngine-0.8.0-abc1234-linux-x64.tar.gz, 0.5 MB." in messages
