"""Filtering of `conan graph outdated` in the `DependencyReport` action."""

from pathlib import Path
from unittest import mock

from ci.actions import dependency_report
from ci.actions.dependency_report import DependencyReport, OutdatedDependency, is_false_positive, parse_outdated


def _entry(current: str, latest: str) -> dict:
    name = current.split("/")[0]
    return {"current_versions": [current], "version_ranges": [], "latest_remote": {"ref": f"{name}/{latest}"}}


def test_snapshots_and_dates_older_than_a_semantic_version_are_ignored() -> None:
    assert is_false_positive("1.10", "cci.20231114")
    assert is_false_positive("2.1.0", "20191104")
    assert is_false_positive("1.92.9b-docking", "cci.20230105+1.89.2.docking")
    assert is_false_positive("4.4.3", "4.4.3")


def test_real_updates_are_kept() -> None:
    assert not is_false_positive("2.13.2", "2.14.3")
    assert not is_false_positive("cci.20240531", "cci.20250101")


def test_the_conan_json_becomes_the_sorted_list_of_real_updates() -> None:
    data = {
        "imguizmo": _entry("imguizmo/1.10", "cci.20231114"),
        "freetype": _entry("freetype/2.13.2", "2.14.3"),
        "brotli": _entry("brotli/1.1.0", "1.2.0"),
        "local": {"current_versions": ["local/1.0"], "latest_remote": None},
    }

    assert parse_outdated(data) == [
        OutdatedDependency(name="brotli", current="1.1.0", latest="1.2.0"),
        OutdatedDependency(name="freetype", current="2.13.2", latest="2.14.3"),
    ]


def test_the_action_never_fails(tmp_path: Path) -> None:
    output = tmp_path / "report.json"
    with (
        mock.patch.object(dependency_report, "run_command", return_value=1),
        mock.patch.object(dependency_report, "report_statistic") as statistic,
    ):
        assert DependencyReport().run(mock.MagicMock(), [f"--output={output}"]) == 0
    statistic.assert_not_called()


def test_the_action_publishes_the_count(tmp_path: Path) -> None:
    output = tmp_path / "report.json"
    output.write_text(
        '{"freetype": {"current_versions": ["freetype/2.13.2"], "latest_remote": {"ref": "freetype/2.14.3"}}}'
    )
    with (
        mock.patch.object(dependency_report, "run_command", return_value=0) as run,
        mock.patch.object(dependency_report, "report_statistic") as statistic,
    ):
        assert DependencyReport().run(mock.MagicMock(), [f"--output={output}"]) == 0
    assert "conancenter" in run.call_args.args[0]
    statistic.assert_called_once_with("OutdatedDependencies", 1)
