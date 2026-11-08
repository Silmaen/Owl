"""Steps `BuildAndTest` chains for each kind of preset."""

import pytest

from ci.actions import build, clean, coverage, documentation, package, test
from ci.actions.build_and_test import BuildAndTest
from ci.utils.preset import PresetConfig


@pytest.fixture
def steps(monkeypatch: pytest.MonkeyPatch) -> list[tuple[str, list[str] | None]]:
    calls: list[tuple[str, list[str] | None]] = []
    for module, name in (
        (clean, "Clean"),
        (build, "Build"),
        (test, "Test"),
        (coverage, "Coverage"),
        (documentation, "Documentation"),
        (package, "Package"),
    ):
        def fake_run(self, preset, args=None, n=name) -> int:
            calls.append((n, args))
            return 0

        monkeypatch.setattr(getattr(module, name), "run", fake_run)
    monkeypatch.delenv("CI_COMMIT_BRANCH", raising=False)
    return calls


def names(steps: list[tuple[str, list[str] | None]]) -> list[str]:
    return [name for name, _ in steps]


def test_debug_tree_measures_its_coverage(steps: list[tuple[str, list[str] | None]]) -> None:
    assert BuildAndTest().run(PresetConfig("linux-gcc-debug")) == 0
    assert names(steps) == ["Clean", "Build", "Test", "Coverage"]
    assert ("Test", ["--label_exclude=slow"]) in steps


def test_release_tree_is_documented_and_packaged_on_main_only(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch
) -> None:
    assert BuildAndTest().run(PresetConfig("linux-clang-release")) == 0
    assert names(steps) == ["Clean", "Build", "Test", "Documentation"]

    steps.clear()
    monkeypatch.setenv("CI_COMMIT_BRANCH", "main")
    monkeypatch.setenv("CI_DEFAULT_BRANCH", "main")
    assert BuildAndTest().run(PresetConfig("linux-clang-release")) == 0
    assert names(steps) == ["Clean", "Build", "Test", "Documentation", "Package"]


def test_arm64_tree_is_packaged_without_documentation(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("CI_COMMIT_BRANCH", "main")
    monkeypatch.setenv("CI_DEFAULT_BRANCH", "main")
    assert BuildAndTest().run(PresetConfig("linux-cross-arm64")) == 0
    assert names(steps) == ["Clean", "Build", "Test", "Package"]


def test_the_first_failure_stops_the_chain(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(build.Build, "run", lambda self, preset, args=None: 3)
    assert BuildAndTest().run(PresetConfig("linux-gcc-debug")) == 3
    assert names(steps) == ["Clean"]


def test_each_step_is_a_log_section_in_gitlab(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    monkeypatch.setenv("GITLAB_CI", "true")
    assert BuildAndTest().run(PresetConfig("linux-gcc-debug")) == 0
    err = capsys.readouterr().err
    assert "section_start:" in err and ":build[collapsed=true]\r" in err
    assert ":test\r" in err and ":coverage\r" in err
    assert err.count("section_start:") == err.count("section_end:") == 4


def test_no_section_outside_gitlab(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    monkeypatch.delenv("GITLAB_CI", raising=False)
    assert BuildAndTest().run(PresetConfig("linux-gcc-debug")) == 0
    assert "section_start" not in capsys.readouterr().err


def test_an_earlier_success_on_the_same_tree_stands(
    steps: list[tuple[str, list[str] | None]], monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.utils import gitlab

    monkeypatch.setattr(gitlab, "reuse_allowed", lambda: True)
    monkeypatch.setattr(gitlab, "same_tree_success", lambda: {"id": 7, "coverage": 82.14, "commit": {"id": "a" * 40}})
    assert BuildAndTest().run(PresetConfig("linux-gcc-debug")) == 0
    assert steps == []

    # A job that packages builds its own archives.
    monkeypatch.setenv("CI_COMMIT_BRANCH", "main")
    monkeypatch.setenv("CI_DEFAULT_BRANCH", "main")
    assert BuildAndTest().run(PresetConfig("linux-clang-release")) == 0
    assert names(steps)[-1] == "Package"


@pytest.mark.parametrize(
    ("source", "manual", "switch", "expected"),
    [
        ("merge_request_event", "", "", True),
        ("push", "", "", True),
        ("push", "true", "", False),
        ("push", "", "false", False),
        ("web", "", "", False),
        ("parent_pipeline", "", "", False),
    ],
)
def test_reuse_is_for_the_pipelines_a_push_starts(
    monkeypatch: pytest.MonkeyPatch, source: str, manual: str, switch: str, expected: bool
) -> None:
    from ci.utils import gitlab

    monkeypatch.setenv("GITLAB_CI", "true")
    monkeypatch.setenv("CI_PIPELINE_SOURCE", source)
    monkeypatch.setenv("CI_JOB_MANUAL", manual)
    monkeypatch.setenv("OWL_CI_REUSE", switch or "true")
    assert gitlab.reuse_allowed() is expected


def test_only_a_commit_with_the_same_tree_is_reused(monkeypatch: pytest.MonkeyPatch) -> None:
    from ci.utils import gitlab

    jobs = [
        {"id": 3, "name": "linux-gcc-debug", "commit": {"id": "changed"}},
        {"id": 2, "name": "windows-clang-release", "commit": {"id": "same"}},
        {"id": 1, "name": "linux-gcc-debug", "commit": {"id": "same"}},
    ]

    def fake_get(path: str) -> object:
        if path.startswith("jobs"):
            return jobs
        return {"diffs": [] if "from=same" in path else [{"new_path": "source/x.cpp"}]}

    monkeypatch.setenv("CI_JOB_NAME", "linux-gcc-debug")
    monkeypatch.setenv("CI_COMMIT_SHA", "head")
    monkeypatch.setattr(gitlab, "_api_get", fake_get)
    assert (gitlab.same_tree_success() or {}).get("id") == 1
    jobs.pop()
    assert gitlab.same_tree_success() is None
