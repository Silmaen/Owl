"""Command line of the `Test` action."""

from pathlib import Path

from ci.actions.test import ctest_command, default_jobs


def test_ctest_runs_in_parallel(tmp_path: Path) -> None:
    assert ctest_command(tmp_path, 12) == [
        "ctest",
        "--test-dir",
        str(tmp_path),
        "--output-on-failure",
        "--parallel",
        "12",
    ]


def test_default_jobs_is_at_least_one() -> None:
    assert default_jobs() >= 1
