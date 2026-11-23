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
    assert all(pipeline[job["extends"]]["stage"] in pipeline["stages"] for job in jobs.values())


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
    monkeypatch.setenv("CI_PIPELINE_SOURCE", "schedule")
    monkeypatch.setattr(gitlab, "last_scheduled_sha", lambda: last)
    monkeypatch.setattr(nightly_gate, "OUTPUT", tmp_path / "nightly.yml")

    assert NightlyGate().run(PresetConfig("linux-clang-release")) == 0
    assert expected in yaml.safe_load((tmp_path / "nightly.yml").read_text())


def test_a_nightly_run_by_hand_always_builds(monkeypatch: pytest.MonkeyPatch, tmp_path) -> None:
    from ci.utils import gitlab

    monkeypatch.setenv("CI_COMMIT_SHA", "abc123")
    monkeypatch.setenv("CI_PIPELINE_SOURCE", "web")
    monkeypatch.setattr(gitlab, "last_scheduled_sha", lambda: "abc123")
    monkeypatch.setattr(nightly_gate, "OUTPUT", tmp_path / "nightly.yml")

    assert NightlyGate().run(PresetConfig("linux-clang-release")) == 0
    assert "slow-suite" in yaml.safe_load((tmp_path / "nightly.yml").read_text())


def _merge_request_pipeline() -> dict:
    """The jobs of `.gitlab-ci.yml` with its includes, `!reference` read as a plain list."""
    from ci import root

    class Loader(yaml.SafeLoader):
        pass

    Loader.add_constructor("!reference", lambda loader, node: loader.construct_sequence(node))
    pipeline: dict = {}
    for name in ("ci/gitlab/templates.yml", "ci/gitlab/nightly-jobs.yml", ".gitlab-ci.yml"):
        pipeline.update(yaml.load((root / name).read_text(encoding="utf-8"), Loader=Loader))
    return pipeline


def test_a_merge_request_offers_every_nightly_job_on_its_play_button() -> None:
    pipeline = _merge_request_pipeline()
    nightly = yaml.safe_load(nightly_pipeline(unchanged=False))
    offered = {job for job in nightly if not job.startswith(".") and job not in ("variables", "default", "stages")}
    # The merge requests run these two already, scoped to their diff; only `main` writes the archives assembled.
    offered -= {"clang-tidy", "bench", "assemble-nest"}

    for job in offered:
        assert pipeline[job]["extends"] == [f".night-{job}", ".nightly-on-demand"]
    assert pipeline[".nightly-on-demand"]["stage"] in pipeline["stages"]
