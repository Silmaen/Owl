"""Earlier successes standing for a job (`Reuse`) and the pipelines of the drafts marked ready (`ReadyPipelines`)."""

import pytest

from ci.actions.ready_pipelines import ReadyPipelines
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


def fake_project(merge_requests: list[dict], pipelines: dict[int, dict]) -> object:
    def fake_get(path: str) -> object:
        if path.startswith("merge_requests?"):
            return merge_requests
        if path.startswith("merge_requests/"):
            iid = int(path.split("/")[1])
            return [{key: value for key, value in pipelines[iid].items() if key != "name"}] if iid in pipelines else []
        if path.startswith("pipelines/"):
            pipeline_id = int(path.split("/")[1])
            return next(pipeline for pipeline in pipelines.values() if pipeline["id"] == pipeline_id)
        return None

    return fake_get


def test_only_the_drafts_marked_ready_are_run_again(monkeypatch: pytest.MonkeyPatch) -> None:
    draft = gitlab.DRAFT_PIPELINE_NAME
    merge_requests = [
        {"iid": 1, "draft": False, "sha": "a"},  # marked ready: its last pipeline is the draft one
        {"iid": 2, "draft": True, "sha": "b"},  # still a draft
        {"iid": 3, "draft": False, "sha": "c"},  # already had its full pipeline
        {"iid": 4, "draft": False, "sha": "d2"},  # pushed since: the push started a full pipeline
        {"iid": 5, "draft": False, "sha": "e"},  # the draft pipeline still runs
    ]
    pipelines = {
        1: {"id": 11, "sha": "a", "status": "success", "name": draft},
        2: {"id": 12, "sha": "b", "status": "success", "name": draft},
        3: {"id": 13, "sha": "c", "status": "success", "name": ""},
        4: {"id": 14, "sha": "d1", "status": "success", "name": draft},
        5: {"id": 15, "sha": "e", "status": "running", "name": draft},
    }
    monkeypatch.setattr(gitlab, "_api_get", fake_project(merge_requests, pipelines))
    assert gitlab.merge_requests_to_rerun() == [1]

    started: list[str] = []

    def fake_post(path: str) -> object:
        started.append(path)
        return {"id": 21}

    monkeypatch.setattr(gitlab, "_api_post", fake_post)
    assert ReadyPipelines().run(PresetConfig("linux-clang-release")) == 0
    assert started == ["merge_requests/1/pipelines"]

    monkeypatch.setattr(gitlab, "_api_post", lambda path: None)
    assert ReadyPipelines().run(PresetConfig("linux-clang-release")) == 1


def test_nothing_is_run_again_without_the_api() -> None:
    assert gitlab.merge_requests_to_rerun() == []
    assert ReadyPipelines().run(PresetConfig("linux-clang-release")) == 0
