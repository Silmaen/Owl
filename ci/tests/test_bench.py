"""Regression detection of the `Bench` action."""

from ci.actions.bench import best_of, compare, medians


def _report(**values: float) -> dict:
    return {"results": [{"name": name.replace("__", "/"), "median_ns": value} for name, value in values.items()]}


def test_only_growth_beyond_the_threshold_is_a_regression() -> None:
    baseline = _report(scene__create=100.0, voxel__mesh=200.0, slang__cold=1000.0)
    current = _report(scene__create=114.0, voxel__mesh=260.0, slang__cold=900.0)

    regressions = compare(baseline, current, 0.15)

    assert [r.name for r in regressions] == ["voxel/mesh"]
    assert round(regressions[0].ratio, 2) == 1.3


def test_benchmarks_on_one_side_only_are_ignored() -> None:
    baseline = _report(old__case=10.0)
    current = _report(new__case=1000.0)

    assert compare(baseline, current, 0.15) == []


def test_unmeasured_benchmarks_are_skipped() -> None:
    assert medians(_report(empty=0.0, kept=5.0)) == {"kept": 5.0}


def test_a_confirmation_run_keeps_the_faster_median() -> None:
    first = _report(scene__create=130.0, voxel__mesh=260.0)
    second = _report(scene__create=101.0, voxel__mesh=290.0)

    assert medians(best_of(first, second)) == {"scene/create": 101.0, "voxel/mesh": 260.0}
