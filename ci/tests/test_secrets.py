"""
Tests of the central secret masking (`ci.utils.secrets`) and its use by `run_command`.
"""

from __future__ import annotations

import logging
import sys

import pytest

from ci.tests.conftest import FAKE_PASSWORD
from ci.utils import secrets
from ci.utils.run import run_command


@pytest.mark.parametrize(
    "command, expected",
    [
        (["tool", "--passwd", "abc123"], "tool --passwd ******"),
        (["tool", "--password=abc123"], "tool --password=******"),
        (["tool", "--remote_passwd=abc123", "--login=bob"], "tool --remote_passwd=****** --login=bob"),
        (["tool", "--api-key", "k", "--token=t"], "tool --api-key ****** --token=******"),
        (["tool", "--url=https://host", "-v"], "tool --url=https://host -v"),
        ("tool --passwd abc123", "tool --passwd ******"),
    ],
)
def test_redact_command_masks_sensitive_flags(command: list[str] | str, expected: str) -> None:
    assert secrets.redact_command(command) == expected


def test_registered_secret_is_masked_anywhere() -> None:
    secrets.register_secret(FAKE_PASSWORD)
    assert FAKE_PASSWORD not in secrets.redact_command(["curl", f"https://bob:{FAKE_PASSWORD}@host"])
    assert secrets.redact(f"prefix {FAKE_PASSWORD} suffix") == f"prefix {secrets.MASK} suffix"


def test_short_values_are_not_registered() -> None:
    secrets.register_secret("ab")
    secrets.register_secret("")
    secrets.register_secret(None)
    assert secrets.redact("ab cd") == "ab cd"


def test_get_secret_reads_environment_and_registers(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("OWL_TEST_SECRET", FAKE_PASSWORD)
    assert secrets.get_secret("OWL_TEST_SECRET") == FAKE_PASSWORD
    assert FAKE_PASSWORD not in secrets.redact(f"value={FAKE_PASSWORD}")
    monkeypatch.delenv("OWL_TEST_SECRET")
    assert secrets.get_secret("OWL_TEST_SECRET") == ""


def test_reject_secret_args() -> None:
    assert secrets.reject_secret_args({"url": "x"}, {"password": "OWL_DEPLOY_PASSWORD"}) is None
    message = secrets.reject_secret_args({"password": FAKE_PASSWORD}, {"password": "OWL_DEPLOY_PASSWORD"})
    assert message is not None and "OWL_DEPLOY_PASSWORD" in message
    assert FAKE_PASSWORD not in message
    # The refused value is registered, so it cannot leak through a later log line.
    assert FAKE_PASSWORD not in secrets.redact(FAKE_PASSWORD)


def test_secret_filter_masks_formatted_record() -> None:
    secrets.register_secret(FAKE_PASSWORD)
    record = logging.LogRecord("ci", logging.INFO, __file__, 1, "pass=%s", (FAKE_PASSWORD,), None)
    assert secrets.SecretFilter().filter(record)
    assert record.getMessage() == f"pass={secrets.MASK}"


def test_run_command_never_logs_the_secret(
    monkeypatch: pytest.MonkeyPatch, masked_caplog: pytest.LogCaptureFixture
) -> None:
    monkeypatch.setenv("OWL_TEST_SECRET", FAKE_PASSWORD)
    secrets.get_secret("OWL_TEST_SECRET")
    code = run_command(
        [sys.executable, "-c", "import os; print('echo', os.environ['OWL_TEST_SECRET'])", "--passwd", FAKE_PASSWORD]
    )
    assert code == 0
    assert FAKE_PASSWORD not in masked_caplog.text
    assert "Running command:" in masked_caplog.text
    # The child's output is masked too (unbuffered prints may arrive as several records).
    output = [r.getMessage() for r in masked_caplog.records if "Running command" not in r.getMessage()]
    assert any(secrets.MASK in line for line in output)
