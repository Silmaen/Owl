"""
Tests of the optional code signing of the packaged Windows binaries: nothing is signed for real, the tools are mocked.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from ci.tests.conftest import FAKE_PASSWORD
from ci.utils import signing


@pytest.fixture
def bin_dir(tmp_path: Path) -> Path:
    """A fake Windows bin/ folder holding the Owl binaries and a third-party DLL."""
    for name in ("OwlNest.exe", "OwlRunner.exe", "OwlEngine.dll", "glfw3.dll"):
        (tmp_path / name).write_bytes(b"MZ")
    return tmp_path


@pytest.fixture
def no_signing_env(monkeypatch: pytest.MonkeyPatch) -> None:
    """Start without any signing variable of the agent."""
    for var in (signing.THUMBPRINT_ENV, signing.CERTIFICATE_ENV, signing.PASSWORD_ENV, signing.TIMESTAMP_ENV):
        monkeypatch.delenv(var, raising=False)


class _FakeRun:
    """Records the commands instead of running them; osslsigncode's output file is created."""

    def __init__(self) -> None:
        self.commands: list[list[str]] = []
        self.passwords: list[str] = []

    def __call__(self, command: list[str], detection_mode: int = 0) -> int:
        self.commands.append(command)
        if "-out" in command:
            self.passwords.append(Path(command[command.index("-readpass") + 1]).read_text())
            Path(command[command.index("-out") + 1]).write_bytes(b"MZsigned")
        return 0


def test_binaries_to_sign_lists_owl_binaries_only(bin_dir: Path) -> None:
    assert [f.name for f in signing.binaries_to_sign(bin_dir)] == list(signing.SIGNED_BINARIES)


@pytest.mark.usefixtures("no_signing_env")
def test_signing_is_off_without_certificate(bin_dir: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    fake = _FakeRun()
    monkeypatch.setattr(signing, "run_command", fake)
    assert signing.sign_binaries(bin_dir) == 0
    assert fake.commands == []


@pytest.mark.usefixtures("no_signing_env")
def test_signtool_uses_the_certificate_store(bin_dir: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    fake = _FakeRun()
    monkeypatch.setattr(signing, "run_command", fake)
    monkeypatch.setattr(signing.shutil, "which", lambda name: f"C:/tools/{name}.exe")
    monkeypatch.setenv(signing.THUMBPRINT_ENV, "0123456789abcdef")
    assert signing.sign_binaries(bin_dir) == 0
    assert len(fake.commands) == 1
    command = fake.commands[0]
    assert command[:4] == ["C:/tools/signtool.exe", "sign", "/sha1", "0123456789abcdef"]
    assert signing.DEFAULT_TIMESTAMP_URL in command
    assert command[-3:] == [str(bin_dir / name) for name in signing.SIGNED_BINARIES]


@pytest.mark.usefixtures("no_signing_env")
def test_osslsigncode_never_puts_the_password_in_argv(bin_dir: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    fake = _FakeRun()
    monkeypatch.setattr(signing, "run_command", fake)
    monkeypatch.setattr(signing.shutil, "which", lambda name: f"/usr/bin/{name}")
    certificate = bin_dir / "owl.pfx"
    certificate.write_bytes(b"pfx")
    monkeypatch.setenv(signing.CERTIFICATE_ENV, str(certificate))
    monkeypatch.setenv(signing.PASSWORD_ENV, FAKE_PASSWORD)
    monkeypatch.setenv(signing.TIMESTAMP_ENV, "http://ts.example.invalid")
    assert signing.sign_binaries(bin_dir) == 0
    assert len(fake.commands) == len(signing.SIGNED_BINARIES)
    assert all(FAKE_PASSWORD not in " ".join(command) for command in fake.commands)
    assert fake.passwords == [FAKE_PASSWORD] * len(signing.SIGNED_BINARIES)
    assert all("http://ts.example.invalid" in command for command in fake.commands)
    assert (bin_dir / "OwlNest.exe").read_bytes() == b"MZsigned"
    assert not list(bin_dir.glob("*.signed"))


@pytest.mark.usefixtures("no_signing_env")
def test_missing_tool_fails(bin_dir: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(signing.shutil, "which", lambda name: None)
    monkeypatch.setenv(signing.THUMBPRINT_ENV, "0123456789abcdef")
    assert signing.sign_binaries(bin_dir) == 1
