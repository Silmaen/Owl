"""Earlier successes standing for a job (`Reuse`)."""

import pytest

from ci.actions.reuse import Reuse
from ci.utils import gitlab
from ci.utils.preset import PresetConfig

JOBS = [
    {"id": 3, "name": "code-style", "commit": {"id": "other"}},
    {"id": 2, "name": "code-style", "commit": {"id": "head"}},
]


@pytest.fixture
def in_merge_request(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("GITLAB_CI", "true")
    monkeypatch.setenv("CI_PIPELINE_SOURCE", "merge_request_event")
    monkeypatch.setenv("CI_JOB_NAME", "code-style")
    monkeypatch.setenv("CI_COMMIT_SHA", "head")


def test_reuse_runs_the_job_outside_gitlab() -> None:
    assert Reuse().run(PresetConfig("linux-clang-debug")) == 1


def test_reuse_stands_on_the_same_tree(monkeypatch: pytest.MonkeyPatch, in_merge_request: None) -> None:
    def fake_get(path: str) -> object:
        if path.startswith("jobs"):
            return JOBS
        return {"diffs": [] if "from=other" in path else [{"new_path": "source/x.cpp"}]}

    monkeypatch.setattr(gitlab, "_api_get", fake_get)
    # Another commit with the same tree is enough, unless the very commit is asked for.
    assert (gitlab.same_tree_success() or {}).get("id") == 3
    assert (gitlab.same_tree_success(same_commit=True) or {}).get("id") == 2
    assert Reuse().run(PresetConfig("linux-clang-debug")) == 0
    assert Reuse().run(PresetConfig("linux-clang-debug"), ["--same_commit"]) == 0


def test_reuse_runs_the_job_without_an_earlier_success(
    monkeypatch: pytest.MonkeyPatch, in_merge_request: None
) -> None:
    monkeypatch.setattr(gitlab, "_api_get", lambda path: [] if path.startswith("jobs") else None)
    assert Reuse().run(PresetConfig("linux-clang-debug")) == 1
    monkeypatch.setenv("CI_JOB_MANUAL", "true")
    monkeypatch.setattr(gitlab, "_api_get", lambda path: JOBS if path.startswith("jobs") else {"diffs": []})
    assert Reuse().run(PresetConfig("linux-clang-debug")) == 1
