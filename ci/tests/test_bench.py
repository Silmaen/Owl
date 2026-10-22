"""Regression detection of the `Bench` action."""

from ci.actions.bench import (
    baseline_changes,
    best_of,
    compare,
    exact_results,
    medians,
    proposed_medians,
    proposed_report,
    proposed_thresholds,
    refreshed_baseline,
    spreads,
)


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


def test_a_pattern_widens_the_threshold_of_its_benchmarks() -> None:
    baseline = _report(physics__mt2__small=100.0, physics__mt2__large=100.0)
    current = _report(physics__mt2__small=130.0, physics__mt2__large=130.0)

    regressions = compare(baseline, current, 0.15, {"physics/mt*/small": 0.4})

    assert [r.name for r in regressions] == ["physics/mt2/large"]


def _rounds() -> list[dict]:
    return [
        {"context": {"agent": "a"}, **_report(scene__create=100.0, voxel__mesh=200.0)},
        _report(scene__create=130.0, voxel__mesh=201.0),
        _report(scene__create=110.0, voxel__mesh=199.0),
    ]


def test_the_proposed_median_is_the_median_of_the_round_medians() -> None:
    assert proposed_medians(_rounds()) == {"scene/create": 110.0, "voxel/mesh": 200.0}


def test_the_proposed_report_keeps_the_context_and_the_closest_result() -> None:
    rounds = _rounds()
    rounds[1]["results"][0]["samples"] = 7

    report = proposed_report(rounds, {"scene/create": 125.0, "voxel/mesh": 200.0})

    assert report["context"] == {"agent": "a"}
    assert medians(report) == {"scene/create": 125.0, "voxel/mesh": 200.0}
    assert report["results"][0]["samples"] == 7


def test_only_baseline_moves_beyond_five_percent_are_listed() -> None:
    moved, unchanged = baseline_changes(_report(a=100.0, b=100.0, c=100.0), {"a": 104.0, "b": 90.0, "d": 5.0})

    assert [(m[0], round(m[3], 2)) for m in moved] == [("b", -0.1)]
    assert unchanged == 2


def test_the_spread_is_max_over_min_of_the_round_medians() -> None:
    assert {k: round(v, 2) for k, v in spreads(_rounds()).items()} == {"scene/create": 0.3, "voxel/mesh": 0.01}


def test_a_threshold_is_proposed_only_above_the_current_one() -> None:
    spread = {"quiet": 0.1, "noisy": 0.32, "covered": 0.25, "exact": 0.2}

    proposed = proposed_thresholds(spread, 0.15, {"covered": 0.3})

    assert proposed == {"noisy": 0.4, "exact": 0.25}


def test_a_filtered_run_keeps_only_the_exact_name() -> None:
    report = _report(frame__flat1000=10.0, frame__flat10000=100.0)
    assert medians(exact_results(report, "frame/flat1000")) == {"frame/flat1000": 10.0}


def test_the_refreshed_baseline_takes_the_round_median_of_the_failures_and_adds_new_benchmarks() -> None:
    baseline = _report(slang__all=700.0, scene__create=100.0)
    current = _report(slang__all=900.0, scene__create=101.0, culling__query=50.0)
    rounds = [_report(slang__all=880.0), _report(slang__all=860.0), _report(slang__all=870.0)]
    refreshed = refreshed_baseline(baseline, current, rounds, ["slang/all"])
    assert medians(refreshed) == {"slang/all": 870.0, "scene/create": 100.0, "culling/query": 50.0}
