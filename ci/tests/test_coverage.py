"""Coverage figures of a gcovr JSON summary and the drop gate."""

from ci.actions.coverage import MAX_DROP, coverage_dropped, coverage_statistics


def test_lines_and_branches_become_percentages() -> None:
    stats = coverage_statistics({"line_covered": 750, "line_total": 1000, "branch_covered": 1, "branch_total": 4})

    assert stats["lines"] == 75.0
    assert stats["branches"] == 25.0


def test_an_empty_report_is_zero_not_a_division_error() -> None:
    assert coverage_statistics({})["lines"] == 0.0


def test_a_drop_beyond_the_margin_fails() -> None:
    assert coverage_dropped(80.0 - MAX_DROP - 0.01, 80.0)


def test_a_drop_within_the_margin_passes() -> None:
    assert not coverage_dropped(80.0 - MAX_DROP, 80.0)
    assert not coverage_dropped(85.0, 80.0)


def test_no_reference_never_fails() -> None:
    assert not coverage_dropped(0.0, None)
