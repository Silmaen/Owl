"""Child pipeline written by `NightlyGate`."""

import pytest
import yaml

from ci.actions import nightly_gate
from ci.actions.nightly_gate import NightlyGate, nightly_pipeline
from ci.utils.preset import PresetConfig


def test_a_new_commit_gets_every_nightly_job_with_its_templates() -> None:
    pipeline = yaml.safe_load(nightly_pipeline(unchanged=False))

    assert ".build-linux" in pipeline
    assert {"linux-clang-debug", "slow-suite", "package-linux-x64", "package-windows-x64"} <= pipeline.keys()
    globals_ = ("variables", "default", "stages")
    jobs = {name: job for name, job in pipeline.items() if not name.startswith(".") and name not in globals_}
    assert all(job["stage"] in pipeline["stages"] for job in jobs.values())


def test_an_unchanged_commit_gets_a_single_job() -> None:
    pipeline = yaml.safe_load(nightly_pipeline(unchanged=True))

    assert [name for name in pipeline if not name.startswith(".")] == ["unchanged"]


@pytest.mark.parametrize(
    ("last", "expected"), [("abc123", "unchanged"), ("def456", "slow-suite"), (None, "slow-suite")]
)
def test_the_gate_compares_with_the_last_nightly(
    monkeypatch: pytest.MonkeyPatch, tmp_path, last: str | None, expected: str
) -> None:
    from ci.utils import gitlab

    monkeypatch.setenv("CI_COMMIT_SHA", "abc123")
    monkeypatch.setattr(gitlab, "last_scheduled_sha", lambda: last)
    monkeypatch.setattr(nightly_gate, "OUTPUT", tmp_path / "nightly.yml")

    assert NightlyGate().run(PresetConfig("linux-clang-release")) == 0
    assert expected in yaml.safe_load((tmp_path / "nightly.yml").read_text())
