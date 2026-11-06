"""
Action to run the code coverage analysis (gcovr, configured by `gcovr.cfg`) and publish its line and branch figures.

The HTML report covers the engine without its platform backends (Vulkan, OpenGL, OpenAL, GLFW, devices), which the
headless tests cannot reach: the published percentage is that scope. GitLab reads it from the job log (the
`coverage:` pattern of `.gitlab-ci.yml`) and marks the covered lines of the merge request diff from the Cobertura
report. Inside GitLab CI the action fails when the line coverage drops more than `MAX_DROP` points below the last
successful job of the same name on the default branch.
"""

from __future__ import annotations

import json
from pathlib import Path

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig

MAX_DROP = 1.0
"""Line coverage points a job may lose against its reference before it fails."""


def coverage_statistics(summary: dict) -> dict[str, float]:
    """
    Line and branch percentages of a gcovr JSON summary.

    :param summary: The parsed `--json-summary` document.
    :return: `lines` and `branches`, in percent.
    """
    stats: dict[str, float] = {}
    for kind, key in (("line", "lines"), ("branch", "branches")):
        covered = float(summary.get(f"{kind}_covered", 0))
        total = float(summary.get(f"{kind}_total", 0))
        stats[key] = 100.0 * covered / total if total > 0 else 0.0
    return stats


def coverage_dropped(current: float, reference: float | None) -> bool:
    """
    Tell whether a line coverage fell too far below its reference.

    :param current: The line coverage of this job, in percent.
    :param reference: The one of the last successful job on the default branch, None when there is none.
    :return: True when the drop exceeds `MAX_DROP` points.
    """
    return reference is not None and current < reference - MAX_DROP


def publish_summary(summary_file: Path) -> float:
    """
    Log the coverage figures of a gcovr JSON summary.

    The `lines: <n>%` line is the one the `coverage:` pattern of `.gitlab-ci.yml` reads.

    :param summary_file: The `--json-summary` file.
    :return: The line coverage, in percent.
    """
    stats = coverage_statistics(json.loads(summary_file.read_text(encoding="utf-8")))
    log.info(
        f"Coverage (engine without backends, gcovr.cfg): lines: {stats['lines']:.2f}%, "
        f"branches: {stats['branches']:.2f}%."
    )
    return stats["lines"]


def check_drop(lines: float) -> int:
    """
    Compare the line coverage with the reference GitLab holds for this job.

    :param lines: The line coverage of this job, in percent.
    :return: 1 when it dropped too far, else 0 (no reference, no token or outside GitLab included).
    """
    from ci.utils.gitlab import TOKEN_VARIABLE, in_gitlab_ci, reference_coverage

    if not in_gitlab_ci():
        return 0
    reference = reference_coverage()
    if reference is None:
        log.warning(f"Coverage: no reference on the default branch (or no {TOKEN_VARIABLE}), drop not checked.")
        return 0
    log.info(f"Coverage: reference {reference:.2f}% (last successful job of the default branch).")
    if coverage_dropped(lines, reference):
        log.error(f"Coverage: lines dropped from {reference:.2f}% to {lines:.2f}%, more than {MAX_DROP:g} point.")
        return 1
    return 0


class Coverage(BaseAction):
    """
    Action to run code coverage analysis and check it against the reference of the default branch.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Executes the coverage analysis and checks the drop.
        :param preset: The preset to use for the action.
        :param extra_args: Optional extra arguments (unused).
        :return:  Exit code indicating success or failure.
        """
        log.info("Starting code coverage analysis...")
        try:
            # we only need to run gcovr, assuming tests have already been run with coverage flags
            from ci.utils.run import run_command

            gcov_executable = "gcov"
            if preset.compiler and "clang" in preset.compiler:
                gcov_executable = "llvm-cov gcov"
            coverage_dir = preset.get_build_dir() / "Coverage"
            summary_file = coverage_dir / "summary.json"
            exit_code = run_command(
                [
                    "gcovr",
                    "-j",
                    "0",
                    "-r",
                    f"{root}",
                    "-o",
                    f"{coverage_dir / 'index.html'}",
                    # Read by GitLab to mark the covered lines on the merge request diff.
                    "--cobertura",
                    f"{coverage_dir / 'cobertura.xml'}",
                    "--gcov-executable",
                    f"{gcov_executable}",
                    "--gcov-ignore-parse-errors",
                    "suspicious_hits.warn_once_per_file",
                    "--json-summary",
                    f"{summary_file}",
                    # Only this tree's notes: another compiler's tree in the same checkout breaks gcov.
                    f"{preset.get_build_dir()}",
                ]
            )
            if exit_code != 0:
                log.error("Coverage analysis failed.")
                return exit_code
            return check_drop(publish_summary(summary_file))
        except Exception as e:
            log.error(f"Coverage analysis failed: {e}")
            return 1
