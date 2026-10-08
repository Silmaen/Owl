"""Coverage figures published to TeamCity from a gcovr JSON summary."""

from ci.actions.coverage import coverage_statistics


def test_lines_and_branches_become_teamcity_statistics() -> None:
    stats = coverage_statistics({"line_covered": 750, "line_total": 1000, "branch_covered": 1, "branch_total": 4})

    assert stats["CodeCoverageL"] == 75.0
    assert stats["CodeCoverageAbsLCovered"] == 750.0
    assert stats["CodeCoverageAbsLTotal"] == 1000.0
    assert stats["CodeCoverageB"] == 25.0


def test_an_empty_report_is_zero_not_a_division_error() -> None:
    assert coverage_statistics({})["CodeCoverageL"] == 0.0
