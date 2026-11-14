"""
Shared fixtures of the CI tooling tests.
"""

from __future__ import annotations

import logging
from collections.abc import Iterator

import pytest

import ci  # noqa: F401 — configures sys.path and logging like ci_action.py does
from ci.utils import secrets

FAKE_PASSWORD = "fake-Pa55word-for-tests"
"""A dummy credential; no real secret is ever used by the tests."""


CI_VARIABLE_PREFIXES = ("CI_", "GITLAB_")
"""Variables a GitLab job sets: a test run inside a job would read the real pipeline context."""

CI_VARIABLES = ("OWL_CI_REUSE", "OWL_GITLAB_API_TOKEN", "OWL_GITLAB_PIPELINE_TOKEN", "NIGHTLY", "READY_WATCH")
"""The project's own CI/CD variables the actions read."""


@pytest.fixture(autouse=True)
def _isolate_ci_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    """Start every test outside any pipeline: a test that needs a CI variable sets it."""
    import os

    for name in list(os.environ):
        if name.startswith(CI_VARIABLE_PREFIXES) or name in CI_VARIABLES:
            monkeypatch.delenv(name)


@pytest.fixture(autouse=True)
def _reset_secrets() -> Iterator[None]:
    """Start and end every test with an empty secret registry."""
    secrets.clear_secrets()
    yield
    secrets.clear_secrets()


@pytest.fixture
def masked_caplog(caplog: pytest.LogCaptureFixture) -> Iterator[pytest.LogCaptureFixture]:
    """`caplog` whose handler masks secrets like the CI handlers do."""
    flt = secrets.SecretFilter()
    caplog.handler.addFilter(flt)
    caplog.set_level(logging.DEBUG)
    yield caplog
    caplog.handler.removeFilter(flt)
