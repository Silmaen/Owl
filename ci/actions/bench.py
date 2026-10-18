"""
Action to run the engine benchmarks (`owl_bench`) and compare them to a stored baseline.

The preset must build `owl_bench` (`OWL_BENCHMARK=ON`, e.g. `linux-bench`). The results are written to
`output/bench/<preset>.json`; the baseline lives in `test/bench/baseline/<preset>.json`. A benchmark whose median
exceeds its baseline median by more than the threshold, in the full run and again when measured alone, fails the
action. `test/bench/thresholds.json` gives a wider threshold to the benchmarks matching a pattern, kept apart from the
baseline so a refresh does not lose it. Without a baseline the action only reports, so the first nightly run
produces the file to commit.
"""

from __future__ import annotations

import json
import shutil
from dataclasses import dataclass
from fnmatch import fnmatchcase

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command

DEFAULT_THRESHOLD = 0.15


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


class Bench(BaseAction):
    """Run `owl_bench` for a preset and compare it to the baseline.

    Extra arguments (after ``--``):
      * ``--threshold=<ratio>`` — allowed median growth, default 0.15;
      * ``--update-baseline`` — write this run as the new baseline instead of comparing.
    """

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
        if regressions:
            # A shared agent is noisy: a suspect is measured once more alone, a real regression stays slow.
            log.info(f"Bench: {len(regressions)} suspect(s), measuring them again.")
            confirm_file = out_dir / f"{preset.cmake_preset}.confirm.json"
            again: dict = {"results": []}
            for reg in regressions:
                if run_command([str(binary), f"--filter={reg.name}", f"--json={confirm_file}"], cwd=binary.parent) == 0:
                    again["results"] += json.loads(confirm_file.read_text(encoding="utf-8")).get("results", [])
            current = best_of(current, again)
            regressions = compare(baseline, current, threshold, overrides)
        log.info(f"Bench: {len(medians(current))} benchmarks against the baseline, threshold +{threshold:.0%}.")
        for reg in regressions:
            log.error(
                f"Bench: {reg.name} regressed: {reg.current_ns:.0f} ns vs {reg.baseline_ns:.0f} ns (x{reg.ratio:.2f})."
            )
        if regressions:
            log.error(f"Bench: {len(regressions)} benchmark(s) slower than the baseline beyond the threshold.")
            return 1
        log.info("Bench: no regression.")
        return 0
