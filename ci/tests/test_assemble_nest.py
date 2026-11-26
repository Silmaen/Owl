"""AssembleNest downloading the runner archives from the delivery server, and the download helper."""

import json
import zipfile
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

from ci.actions.assemble_nest import AssembleNest
from ci.utils import runners
from ci.utils.publish import get_project_version
from ci.utils.runners import TARGETS, archive_name, download_runner, write_archive

HASH = "abcdef1"
SERVER = "https://delivery.example.org"


class FakeResponse:
    """The part of a `requests` response the download reads."""

    def __init__(self, content: bytes, status_code: int = 200) -> None:
        self.content = content
        self.status_code = status_code
        self.reason = "OK" if status_code == 200 else "Error"


def web_archive(folder: Path) -> bytes:
    bin_dir = folder / "web"
    bin_dir.mkdir(parents=True, exist_ok=True)
    for name in runners.WEB_RUNNER_FILES:
        (bin_dir / name).write_bytes(name.encode())
    archive = folder / "web.tar.gz"
    write_archive([(bin_dir / name, name) for name in runners.WEB_RUNNER_FILES], archive)
    return archive.read_bytes()


def fake_get(tmp_path: Path, calls: list[tuple[str, dict[str, str]]], fail: str | None = None) -> Any:
    web = web_archive(tmp_path / "fake")

    def get(url: str, headers: dict[str, str] | None = None, timeout: float | None = None) -> FakeResponse:
        calls.append((url, headers or {}))
        if fail is not None and fail in url:
            return FakeResponse(b"", 403)
        return FakeResponse(web if url.endswith("-web.tar.gz") else url.encode())

    return get


def make_nest(folder: Path, version: str) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(folder / f"OwlNest-{version}-{HASH}-windows-x64.zip", "w") as archive:
        archive.writestr(f"OwlNest-{version}-{HASH}-windows-x64/bin/OwlNest.exe", b"exe")


def test_download_runner_sends_the_nest_user_agent_to_the_runner_url(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    calls: list[tuple[str, dict[str, str]]] = []
    monkeypatch.setattr("requests.get", fake_get(tmp_path, calls))
    found = download_runner(SERVER + "/", "0.8.0", HASH, "linux-x64", tmp_path / "dl")
    name = archive_name("0.8.0", HASH, "linux-x64")
    assert found == tmp_path / "dl" / name
    assert found.read_bytes() == f"{SERVER}/runners/0.8.0/{HASH}/{name}".encode()
    assert calls == [(f"{SERVER}/runners/0.8.0/{HASH}/{name}", {"User-Agent": "OwlNest/0.8.0"})]


def test_download_runner_returns_none_on_failure(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr("requests.get", fake_get(tmp_path, [], fail="linux-x64"))
    assert download_runner(SERVER, "0.8.0", HASH, "linux-x64", tmp_path / "dl") is None

    def broken(*_args: Any, **_kwargs: Any) -> FakeResponse:
        raise ConnectionError("down")

    monkeypatch.setattr("requests.get", broken)
    assert download_runner(SERVER, "0.8.0", HASH, "web", tmp_path / "dl") is None


def test_assemble_nest_downloads_every_runner_when_given_a_url(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    version = get_project_version()
    calls: list[tuple[str, dict[str, str]]] = []
    monkeypatch.setattr("requests.get", fake_get(tmp_path, calls))
    make_nest(tmp_path / "in", version)
    preset: Any = SimpleNamespace(cmake_preset="linux-clang-release")
    args = [f"--input={tmp_path / 'in'}", f"--output={tmp_path / 'out'}", f"--hash={HASH}", f"--url={SERVER}"]
    assert AssembleNest().run(preset, args) == 0

    assert [url for url, _ in calls] == [
        f"{SERVER}/runners/{version}/{HASH}/{archive_name(version, HASH, target)}" for target in TARGETS
    ]
    assert all(headers == {"User-Agent": f"OwlNest/{version}"} for _, headers in calls)
    manifest = json.loads((tmp_path / "out" / "runners.json").read_text())
    assert sorted(manifest["runners"]) == sorted(TARGETS)
    root = f"OwlNest-{version}-{HASH}-windows-x64"
    with zipfile.ZipFile(tmp_path / "out" / f"{root}.zip") as archive:
        assert archive.read(f"{root}/bin/web/OwlGameRunner.js") == b"OwlGameRunner.js"
        assert f"{root}/bin/runners.json" in archive.namelist()


def test_assemble_nest_fails_when_a_download_fails(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, masked_caplog: pytest.LogCaptureFixture
) -> None:
    version = get_project_version()
    monkeypatch.setattr("requests.get", fake_get(tmp_path, [], fail="linux-arm64"))
    make_nest(tmp_path / "in", version)
    preset: Any = SimpleNamespace(cmake_preset="linux-clang-release")
    args = [f"--input={tmp_path / 'in'}", f"--output={tmp_path / 'out'}", f"--hash={HASH}", f"--url={SERVER}"]
    assert AssembleNest().run(preset, args) == 1
    assert "cannot download the linux-arm64 runner" in masked_caplog.text
    assert not (tmp_path / "out" / "runners.json").exists()
