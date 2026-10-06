"""
Tests of the in-repository publication client and of the publish / remote actions' secret handling.

Every HTTP call and DepManager call is mocked: nothing leaves the machine.
"""

from __future__ import annotations

import tarfile
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest
import requests

from ci.actions.publish_doc import PublishDoc
from ci.actions.publish_package import PublishPackage
from ci.tests.conftest import FAKE_PASSWORD
from ci.utils import publish, remote, secrets
from ci.utils.publish import Revision, normalize_server_url, push_revision


class _FakePost:
    """Records the calls made to `requests.post` and answers with a canned response."""

    def __init__(self, status: int = 200, body: bytes = b"ok") -> None:
        self.status = status
        self.body = body
        self.calls: list[dict[str, Any]] = []

    def __call__(self, url: str, **kwargs: Any) -> SimpleNamespace:
        encoder = kwargs["data"]
        self.calls.append({"url": url, "auth": kwargs["auth"], "fields": dict(encoder.fields)})
        encoder.to_string()  # drain the multipart stream while the file is still open
        return SimpleNamespace(status_code=self.status, reason="R", content=self.body)


@pytest.fixture
def fake_post(monkeypatch: pytest.MonkeyPatch) -> _FakePost:
    """Replace `requests.post` by a recorder."""
    fake = _FakePost()
    monkeypatch.setattr(requests, "post", fake)
    return fake


@pytest.mark.parametrize(
    "url, expected",
    [
        ("delivery.example.org", "https://delivery.example.org"),
        ("https://delivery.example.org/", "https://delivery.example.org"),
        ("http://delivery.example.org", None),
        ("ftp://delivery.example.org", None),
    ],
)
def test_normalize_server_url_enforces_https(url: str, expected: str | None) -> None:
    assert normalize_server_url(url) == expected


def test_push_package(tmp_path: Path, fake_post: _FakePost, masked_caplog: pytest.LogCaptureFixture) -> None:
    secrets.register_secret(FAKE_PASSWORD)
    package = tmp_path / "OwlEngine-0.3.0-abcdef0-linux-x64.tar.gz"
    package.write_bytes(b"payload")
    rev = Revision(rev_type="e", branch="0.3.0", file=package, hash="abcdef0", name="Owl Engine",
                   flavor_name="linux x64", date="2026-10-05")
    assert push_revision("delivery.example.org", "bob", FAKE_PASSWORD, rev) == 0
    (call,) = fake_post.calls
    assert call["url"] == "https://delivery.example.org/api"
    assert (call["auth"].username, call["auth"].password) == ("bob", FAKE_PASSWORD)
    assert call["fields"]["action"] == "push"
    assert call["fields"]["hash"] == "abcdef0"
    assert FAKE_PASSWORD not in masked_caplog.text


def test_push_large_file_goes_to_upload(tmp_path: Path, fake_post: _FakePost, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(publish, "LARGE_FILE_THRESHOLD", 4)
    package = tmp_path / "big.zip"
    package.write_bytes(b"0123456789")
    assert push_revision("https://h", "bob", FAKE_PASSWORD, Revision("a", "0.3.0", package)) == 0
    assert fake_post.calls[0]["url"] == "https://h/upload"


def test_push_documentation_is_compacted(tmp_path: Path, fake_post: _FakePost) -> None:
    html = tmp_path / "html"
    html.mkdir()
    (html / "index.html").write_text("<html/>")
    assert push_revision("https://h", "bob", FAKE_PASSWORD, Revision("d", "0.3.0", html)) == 0
    assert fake_post.calls[0]["fields"]["action"] == "push_doc"
    with tarfile.open(tmp_path / "Archive_0.3.0.tgz") as tar:
        assert tar.getnames() == ["index.html"]


def test_push_refuses_plain_http(tmp_path: Path, fake_post: _FakePost) -> None:
    package = tmp_path / "p.zip"
    package.write_bytes(b"x")
    assert push_revision("http://h", "bob", FAKE_PASSWORD, Revision("a", "0.3.0", package)) == 1
    assert fake_post.calls == []


def test_push_error_body_is_masked(tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                                   masked_caplog: pytest.LogCaptureFixture) -> None:
    secrets.register_secret(FAKE_PASSWORD)
    monkeypatch.setattr(requests, "post", _FakePost(500, f"bad credentials {FAKE_PASSWORD}".encode()))
    package = tmp_path / "p.zip"
    package.write_bytes(b"x")
    assert push_revision("https://h", "bob", FAKE_PASSWORD, Revision("a", "0.3.0", package)) == 1
    assert "500" in masked_caplog.text
    assert FAKE_PASSWORD not in masked_caplog.text


def test_publication_never_downloads_code() -> None:
    source = Path(publish.__file__).read_text(encoding="utf-8")
    assert "static/scripts" not in source
    assert "requests.get" not in source and "from requests import get" not in source


@pytest.mark.parametrize("action", [PublishPackage, PublishDoc])
def test_publish_actions_refuse_password_argument(action: type, masked_caplog: pytest.LogCaptureFixture) -> None:
    preset = SimpleNamespace(cmake_preset="package-engine-linux")
    code = action().run(preset, ["--url=https://h", "--login=bob", f"--password={FAKE_PASSWORD}"])
    assert code == 1
    assert "OWL_DEPLOY_PASSWORD" in masked_caplog.text
    assert FAKE_PASSWORD not in masked_caplog.text


@pytest.mark.parametrize("action", [PublishPackage, PublishDoc])
def test_publish_actions_require_password_env(action: type, monkeypatch: pytest.MonkeyPatch,
                                              masked_caplog: pytest.LogCaptureFixture) -> None:
    monkeypatch.delenv(publish.DEPLOY_PASSWORD_ENV, raising=False)
    preset = SimpleNamespace(cmake_preset="package-engine-linux")
    assert action().run(preset, ["--url=https://h", "--login=bob"]) == 1
    assert publish.DEPLOY_PASSWORD_ENV in masked_caplog.text


def test_remote_refuses_password_argument(masked_caplog: pytest.LogCaptureFixture) -> None:
    config = remote.parse_remote_args({"remote_url": "srvs://h", "remote_passwd": FAKE_PASSWORD})
    assert remote.configure_remote(config) == 1
    assert remote.REMOTE_PASSWORD_ENV in masked_caplog.text
    assert FAKE_PASSWORD not in masked_caplog.text


def test_remote_reads_password_from_environment(monkeypatch: pytest.MonkeyPatch,
                                                masked_caplog: pytest.LogCaptureFixture) -> None:
    calls: list[tuple[Any, ...]] = []

    class _FakeRemoteCommand:
        def add(self, *args: Any) -> None:
            calls.append(args)

    import depmanager.command.remote as dm_remote

    monkeypatch.setattr(dm_remote, "RemoteCommand", _FakeRemoteCommand)
    monkeypatch.setenv(remote.REMOTE_PASSWORD_ENV, FAKE_PASSWORD)
    config = remote.parse_remote_args({"remote_url": "srvs://h", "remote_login": "bob"})
    assert remote.configure_remote(config) == 0
    assert calls == [("default", "srvs://h", True, "bob", FAKE_PASSWORD)]
    assert FAKE_PASSWORD not in masked_caplog.text


def test_remote_invalid_url_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    class _ExitingRemoteCommand:
        def add(self, *args: Any) -> None:
            raise SystemExit(-666)

    import depmanager.command.remote as dm_remote

    monkeypatch.setattr(dm_remote, "RemoteCommand", _ExitingRemoteCommand)
    monkeypatch.delenv(remote.REMOTE_PASSWORD_ENV, raising=False)
    assert remote.configure_remote(remote.parse_remote_args({"remote_url": "nope"})) != 0
