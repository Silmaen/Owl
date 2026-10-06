"""
Tests of the CodeStyle committed-secret scan (H-12).

Fake secrets are assembled at run time so this file never matches the scan itself.
"""

from __future__ import annotations

import pytest

from ci import root
from ci.actions.code_style import _check_secrets, _tracked_files, scan_text_for_secrets

_DASHES = "-" * 5


@pytest.mark.parametrize(
    "text, kind",
    [
        (f"{_DASHES}BEGIN OPENSSH PRIVATE KEY{_DASHES}", "private key"),
        (f"{_DASHES}BEGIN PRIVATE KEY{_DASHES}", "private key"),
        ("token = '" + "ghp" + "_" + "a1B2" * 9 + "'", "GitHub token"),
        ("key: " + "AKIA" + "ABCDEFGHIJKLMNOP", "AWS access key"),
        ("hook " + "xoxb" + "-1234567890-abcdef", "Slack token"),
        ("url: " + "https://bob" + ":hunter22@example.org/x", "credentials in URL"),
    ],
)
def test_secret_shapes_are_detected(text: str, kind: str) -> None:
    findings = scan_text_for_secrets(f"first line\n{text}\n")
    assert [(line, k) for line, _, k in findings] == [(2, kind)]


@pytest.mark.parametrize(
    "text",
    [
        "extraArgs = \"--url=%deploy_url% --login=%deploy_login%\"",
        "export OWL_DEPLOY_PASSWORD='%deploy_passwd%'",
        "remote add -n <name> -u <protocol>://<url[:port]>",
        "https://example.org:8080/path",
        "srvs://user:${PASSWORD}@host",
        "password = get_secret(DEPLOY_PASSWORD_ENV)",
    ],
)
def test_placeholders_are_not_flagged(text: str) -> None:
    assert scan_text_for_secrets(text) == []


def test_repository_has_no_committed_secret() -> None:
    tracked = _tracked_files()
    assert any(p.name == "pyproject.toml" for p in tracked)
    assert not any(".git" in p.relative_to(root).parts or "output" == p.relative_to(root).parts[0] for p in tracked)
    assert _check_secrets() == 0
