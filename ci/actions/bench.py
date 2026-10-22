"""
Action to run the engine benchmarks (`owl_bench`) and compare them to a stored baseline.

The preset must build `owl_bench` (`OWL_BENCHMARK=ON`, e.g. `linux-bench`). The results are written to
`output/bench/<preset>.json`; the baseline lives in `test/bench/baseline/<preset>.json`. A benchmark whose median
exceeds its baseline median by more than the threshold in the full run is measured again alone, ``--confirm_rounds``
times (5 by default); it fails the action when even its fastest median stays beyond the threshold. Each failure then
leaves ``output/bench/<preset>.proposed-baseline.json``: the baseline with the failed benchmarks set to the median of
their confirmation rounds and the benchmarks it lacks taken from the full run, to commit when the change is expected.
`test/bench/thresholds.json` gives a wider threshold to the benchmarks matching a pattern, kept apart from the
baseline so a refresh does not lose it. Without a baseline the action only reports, so the first nightly run
produces the file to commit.

The calibration mode (`--calibrate=true --rounds=<n>`) runs `owl_bench` several times and never compares: it proposes
a baseline (the median of the round medians, `output/bench/<preset>.proposed-baseline.json`) and the thresholds of the
benchmarks whose spread between rounds exceeds their current one (`<preset>.proposed-thresholds.json`).
"""

from __future__ import annotations

import json
import math
import shutil
import statistics
from dataclasses import dataclass
from fnmatch import fnmatchcase

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command

DEFAULT_THRESHOLD = 0.15
DEFAULT_ROUNDS = 5
MIN_ROUNDS = 2
BASELINE_CHANGE = 0.05
THRESHOLD_STEP = 0.05


@dataclass(frozen=True)
class Regression:
    """A benchmark slower than its baseline beyond the threshold."""

    name: str
    baseline_ns: float
    current_ns: float

    @property
    def ratio(self) -> float:
        """
        :return: Current median over baseline median.
        """
        return self.current_ns / self.baseline_ns


def medians(report: dict) -> dict[str, float]:
    """
    Median of every benchmark of an `owl_bench --json` report.

    :param report: Parsed JSON report.
    :return: Benchmark name to median, in nanoseconds.
    """
    return {r["name"]: float(r["median_ns"]) for r in report.get("results", []) if r.get("median_ns", 0) > 0}


def threshold_of(name: str, threshold: float, overrides: dict[str, float] | None = None) -> float:
    """
    Allowed growth of one benchmark.

    :param name: Benchmark name.
    :param threshold: Default allowed relative growth.
    :param overrides: Name pattern (``fnmatch``) to allowed growth; the first matching pattern wins.
    :return: The allowed relative growth of the benchmark.
    """
    return next((value for pattern, value in (overrides or {}).items() if fnmatchcase(name, pattern)), threshold)


def compare(
    baseline: dict, current: dict, threshold: float, overrides: dict[str, float] | None = None
) -> list[Regression]:
    """
    Benchmarks of both reports whose median grew by more than their threshold.

    :param baseline: Parsed baseline report.
    :param current: Parsed report of this run.
    :param threshold: Allowed relative growth (0.15 = 15 %).
    :param overrides: Name pattern to allowed growth, for the benchmarks noisier than the default allows.
    :return: The regressions, slowest ratio first; benchmarks missing from either side are ignored.
    """
    base = medians(baseline)
    found = [
        Regression(name, base[name], value)
        for name, value in medians(current).items()
        if name in base and value > base[name] * (1.0 + threshold_of(name, threshold, overrides))
    ]
    return sorted(found, key=lambda r: r.ratio, reverse=True)


def best_of(first: dict, second: dict) -> dict:
    """
    Merge two reports, keeping for each benchmark the faster median (noise only ever slows a run down).

    :param first: Parsed report of the full run.
    :param second: Parsed report of the confirmation run.
    :return: A report holding the results of ``first``, each with the lower median of the two runs.
    """
    again = medians(second)
    results = [
        {**r, "median_ns": min(float(r["median_ns"]), again.get(r["name"], float(r["median_ns"])))}
        for r in first.get("results", [])
    ]
    return {**first, "results": results}


def exact_results(report: dict, name: str) -> dict:
    """
    Keep the result of one benchmark: ``--filter`` matches a substring, so ``flat1000`` also runs ``flat10000``.

    :param report: Parsed report of a filtered run.
    :param name: Benchmark name.
    :return: A report holding only the results named ``name``.
    """
    return {**report, "results": [r for r in report.get("results", []) if r["name"] == name]}


def refreshed_baseline(baseline: dict, current: dict, rounds: list[dict], names: list[str]) -> dict:
    """
    Baseline to commit when the failures are expected.

    :param baseline: Parsed baseline report.
    :param current: Parsed report of the full run.
    :param rounds: Parsed reports of the confirmation rounds of the failed benchmarks.
    :param names: Benchmarks that failed after the confirmation.
    :return: The baseline with each of ``names`` set to the median of its confirmation rounds (the closest round
        result), and every benchmark of ``current`` the baseline lacks added as measured.
    """
    proposed = {name: value for name, value in proposed_medians(rounds).items() if name in names}
    replaced = proposed_report(rounds, proposed)["results"] if proposed else []
    by_name = {r["name"]: r for r in replaced}
    known = {r["name"] for r in baseline.get("results", [])}
    results = [by_name.get(r["name"], r) for r in baseline.get("results", [])]
    results += [r for r in current.get("results", []) if r["name"] not in known and r.get("median_ns", 0) > 0]
    return {**baseline, "results": results}


def proposed_medians(rounds: list[dict]) -> dict[str, float]:
    """
    Median over the rounds of each benchmark's median.

    :param rounds: Parsed reports of the calibration rounds.
    :return: Benchmark name to proposed median, in nanoseconds; only the benchmarks measured in every round.
    """
    per_round = [medians(r) for r in rounds]
    names = set.intersection(*(set(m) for m in per_round)) if per_round else set()
    return {name: statistics.median(m[name] for m in per_round) for name in sorted(names)}


def proposed_report(rounds: list[dict], proposed: dict[str, float]) -> dict:
    """
    Report holding the proposed medians, in the format of a run report.

    :param rounds: Parsed reports of the calibration rounds, the first one gives the ``context``.
    :param proposed: Benchmark name to proposed median, see :func:`proposed_medians`.
    :return: The first round's report whose results are, for each benchmark, the round result with the median closest
        to the proposed one, its ``median_ns`` set to the proposed value.
    """
    results = []
    for name, value in proposed.items():
        candidates = [
            r for rep in rounds for r in rep.get("results", []) if r["name"] == name and r.get("median_ns", 0) > 0
        ]
        closest = min(candidates, key=lambda r: abs(float(r["median_ns"]) - value))
        results.append({**closest, "median_ns": value})
    return {**rounds[0], "results": results}


def baseline_changes(baseline: dict, proposed: dict[str, float]) -> tuple[list[tuple[str, float, float, float]], int]:
    """
    Benchmarks whose proposed median moves away from the baseline.

    :param baseline: Parsed baseline report.
    :param proposed: Benchmark name to proposed median.
    :return: The (name, old, new, relative change) of every benchmark moving by more than 5 %, and the number of
        benchmarks that stay within it or have no baseline.
    """
    base = medians(baseline)
    moved = [
        (name, base[name], value, value / base[name] - 1.0)
        for name, value in proposed.items()
        if name in base and abs(value / base[name] - 1.0) > BASELINE_CHANGE
    ]
    return moved, len(proposed) - len(moved)


def spreads(rounds: list[dict]) -> dict[str, float]:
    """
    Relative spread of each benchmark between the rounds.

    :param rounds: Parsed reports of the calibration rounds.
    :return: Benchmark name to ``max / min - 1`` of its round medians.
    """
    per_round = [medians(r) for r in rounds]
    return {
        name: max(m[name] for m in per_round) / min(m[name] for m in per_round) - 1.0
        for name in proposed_medians(rounds)
    }


def proposed_thresholds(
    spread: dict[str, float], threshold: float, overrides: dict[str, float] | None = None
) -> dict[str, float]:
    """
    Thresholds for the benchmarks noisier than the one they have.

    :param spread: Benchmark name to spread between rounds, see :func:`spreads`.
    :param threshold: Default allowed relative growth.
    :param overrides: Name pattern to allowed growth, as in ``thresholds.json``.
    :return: For each benchmark whose spread exceeds its threshold, the spread rounded up to the next 0.05, plus 0.05.
    """
    return {
        name: round(math.ceil(round(value / THRESHOLD_STEP, 6)) * THRESHOLD_STEP + THRESHOLD_STEP, 2)
        for name, value in spread.items()
        if value > threshold_of(name, threshold, overrides)
    }


class Bench(BaseAction):
    """Run `owl_bench` for a preset and compare it to the baseline, or calibrate the baseline and thresholds.

    Extra arguments (after ``--``):
      * ``--threshold=<ratio>`` — allowed median growth, default 0.15;
      * ``--confirm_rounds=<n>`` — runs of each failed benchmark measured again alone, default 5;
      * ``--update-baseline`` — write this run as the new baseline instead of comparing;
      * ``--calibrate=true`` — run the benchmarks ``--rounds`` times (default 5, at least 2) and write the proposed
        baseline and thresholds to ``output/bench/`` instead of comparing.
    """

    def _calibrate(self, preset: PresetConfig, binary, out_dir, threshold: float, rounds: int) -> int:
        """Run the benchmarks several times and write the proposed baseline and thresholds.

        :param preset: A preset building `owl_bench`.
        :param binary: Path of `owl_bench`.
        :param out_dir: Directory of the reports.
        :param threshold: Default allowed relative growth.
        :param rounds: Number of runs.
        :return: 0, or the status of the first failed `owl_bench` run.
        """
        reports: list[dict] = []
        for i in range(1, rounds + 1):
            round_file = out_dir / f"{preset.cmake_preset}.round{i}.json"
            log.info(f"Bench: calibration round {i}/{rounds}.")
            status = run_command([str(binary), f"--json={round_file}"], cwd=binary.parent)
            if status != 0 or not round_file.is_file():
                log.error(f"Bench: owl_bench failed in round {i}.")
                return status or 1
            reports.append(json.loads(round_file.read_text(encoding="utf-8")))
        proposed = proposed_medians(reports)
        proposed_file = out_dir / f"{preset.cmake_preset}.proposed-baseline.json"
        proposed_file.write_text(json.dumps(proposed_report(reports, proposed), indent=2) + "\n", encoding="utf-8")
        log.info(
            f"Bench: proposed baseline of {len(proposed)} benchmarks written to {proposed_file.relative_to(root)}."
        )
        baseline_file = root / "test" / "bench" / "baseline" / f"{preset.cmake_preset}.json"
        if baseline_file.is_file():
            moved, unchanged = baseline_changes(json.loads(baseline_file.read_text(encoding="utf-8")), proposed)
            for name, old, new, change in moved:
                log.info(f"Bench: {name}: {old:.0f} ns -> {new:.0f} ns ({change:+.1%}).")
            log.info(f"Bench: {unchanged} benchmark(s) within {BASELINE_CHANGE:.0%} of the baseline.")
        overrides_file = root / "test" / "bench" / "thresholds.json"
        overrides: dict[str, float] = {}
        if overrides_file.is_file():
            overrides = json.loads(overrides_file.read_text(encoding="utf-8")).get("thresholds", {})
        wider = proposed_thresholds(spreads(reports), threshold, overrides)
        thresholds_file = out_dir / f"{preset.cmake_preset}.proposed-thresholds.json"
        thresholds_file.write_text(json.dumps({"thresholds": wider}, indent=2) + "\n", encoding="utf-8")
        for name, value in wider.items():
            log.info(f"Bench: threshold {name}: {value:.2f} (was {threshold_of(name, threshold, overrides):.2f}).")
        log.info(f"Bench: {len(wider)} proposed threshold(s) written to {thresholds_file.relative_to(root)}.")
        return 0

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """Run the benchmarks and compare them.

        :param preset: A preset building `owl_bench`.
        :param extra_args: Optional extra arguments, see the class docstring.
        :return: 0 when no benchmark regressed, 1 otherwise or on failure.
        """
        args = self.parse_extra_args(extra_args)
        threshold = float(args.get("threshold", DEFAULT_THRESHOLD))
        binary = (preset.get_build_dir() / "bin" / "owl_bench").resolve()
        if not binary.is_file():
            log.error(f"Bench: {binary} not found; build the preset with OWL_BENCHMARK=ON first.")
            return 1
        out_dir = root / "output" / "bench"
        out_dir.mkdir(parents=True, exist_ok=True)
        if args.get("calibrate") == "true":
            return self._calibrate(
                preset, binary, out_dir, threshold, max(MIN_ROUNDS, int(args.get("rounds", DEFAULT_ROUNDS)))
            )
        result_file = out_dir / f"{preset.cmake_preset}.json"
        # The dummy application looks up engine_assets/ and engine_demo/ from its working directory.
        status = run_command([str(binary), f"--json={result_file}"], cwd=binary.parent)
        if status != 0 or not result_file.is_file():
            log.error("Bench: owl_bench failed.")
            return status or 1
        current = json.loads(result_file.read_text(encoding="utf-8"))
        baseline_file = root / "test" / "bench" / "baseline" / f"{preset.cmake_preset}.json"
        if "update-baseline" in args:
            baseline_file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(result_file, baseline_file)
            log.info(f"Bench: baseline written to {baseline_file.relative_to(root)}.")
            return 0
        if not baseline_file.is_file():
            log.warning(f"Bench: no baseline {baseline_file.relative_to(root)}; commit {result_file.name} as it.")
            return 0
        baseline = json.loads(baseline_file.read_text(encoding="utf-8"))
        overrides_file = root / "test" / "bench" / "thresholds.json"
        overrides: dict[str, float] = {}
        if overrides_file.is_file():
            overrides = json.loads(overrides_file.read_text(encoding="utf-8")).get("thresholds", {})
        regressions = compare(baseline, current, threshold, overrides)
        rounds: list[dict] = []
        if regressions:
            # A shared agent is noisy: each suspect alone, several times; a real regression stays slow in every round.
            confirm_rounds = max(1, int(args.get("confirm_rounds", DEFAULT_ROUNDS)))
            log.info(f"Bench: {len(regressions)} suspect(s), measuring them again alone {confirm_rounds} time(s).")
            confirm_file = out_dir / f"{preset.cmake_preset}.confirm.json"
            for index in range(confirm_rounds):
                again: dict = {"results": []}
                for reg in regressions:
                    command = [str(binary), f"--filter={reg.name}", f"--json={confirm_file}"]
                    if run_command(command, cwd=binary.parent) == 0:
                        report = json.loads(confirm_file.read_text(encoding="utf-8"))
                        again["results"] += exact_results(report, reg.name)["results"]
                log.info(f"Bench: confirmation round {index + 1}/{confirm_rounds} done.")
                rounds.append(again)
                current = best_of(current, again)
            regressions = compare(baseline, current, threshold, overrides)
        log.info(f"Bench: {len(medians(current))} benchmarks against the baseline, threshold +{threshold:.0%}.")
        for reg in regressions:
            log.error(
                f"Bench: {reg.name} regressed: {reg.current_ns:.0f} ns vs {reg.baseline_ns:.0f} ns (x{reg.ratio:.2f})."
            )
        if regressions:
            log.error(f"Bench: {len(regressions)} benchmark(s) slower than the baseline beyond the threshold.")
            refreshed = refreshed_baseline(baseline, current, rounds, [reg.name for reg in regressions])
            proposed_file = out_dir / f"{preset.cmake_preset}.proposed-baseline.json"
            proposed_file.write_text(json.dumps(refreshed, indent=2) + "\n", encoding="utf-8")
            for name, value in proposed_medians(rounds).items():
                if any(reg.name == name for reg in regressions):
                    log.info(f"Bench: proposed baseline for {name}: {value:.0f} ns.")
            log.info(
                f"Bench: if the change is expected, commit {proposed_file.relative_to(root)} "
                f"as {baseline_file.relative_to(root)}."
            )
            return 1
        log.info("Bench: no regression.")
        return 0
