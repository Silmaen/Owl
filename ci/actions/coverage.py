"""
Action to run the code coverage analysis (gcovr, configured by `gcovr.cfg`) and publish its line and branch figures.

The HTML report covers the engine without its platform backends (Vulkan, OpenGL, OpenAL, GLFW, devices), which the
headless tests cannot reach: the published percentage is that scope, and a TeamCity failure condition fails a build
whose line coverage drops against the last successful one.
"""

from __future__ import annotations

import json
from pathlib import Path

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.teamcity import report_statistic


def coverage_statistics(summary: dict) -> dict[str, float]:
    """
    TeamCity coverage statistics of a gcovr JSON summary.

    :param summary: The parsed `--json-summary` document.
    :return: The `CodeCoverage*` statistics (absolute counts and percentages) for lines and branches.
    """
    stats: dict[str, float] = {}
    for kind, key in (("line", "L"), ("branch", "B")):
        covered = float(summary.get(f"{kind}_covered", 0))
        total = float(summary.get(f"{kind}_total", 0))
        stats[f"CodeCoverageAbs{key}Covered"] = covered
        stats[f"CodeCoverageAbs{key}Total"] = total
        stats[f"CodeCoverage{key}"] = 100.0 * covered / total if total > 0 else 0.0
    return stats


def publish_summary(summary_file: Path) -> None:
    """
    Log the coverage figures of a gcovr JSON summary and publish them to TeamCity.

    :param summary_file: The `--json-summary` file.
    """
    stats = coverage_statistics(json.loads(summary_file.read_text(encoding="utf-8")))
    log.info(
        f"Coverage (engine without backends, gcovr.cfg): lines {stats['CodeCoverageL']:.1f} %, "
        f"branches {stats['CodeCoverageB']:.1f} %."
    )
    for key, value in stats.items():
        report_statistic(key, value)


class Coverage(BaseAction):
    """
    Action to run code coverage analysis and upload the results to a coverage tracking service.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Executes the coverage analysis and uploads the results.
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
            summary_file = preset.get_build_dir() / "Coverage" / "summary.json"
            exit_code = run_command(
                [
                    "gcovr",
                    "-j",
                    "0",
                    "-r",
                    f"{root}",
                    "-o",
                    f"{preset.get_build_dir() / 'Coverage' / 'index.html'}",
                    "--gcov-executable",
                    f"{gcov_executable}",
                    "--gcov-ignore-parse-errors",
                    "suspicious_hits.warn_once_per_file",
                    "--json-summary",
                    f"{summary_file}",
                    ".",
                ]
            )
            if exit_code != 0:
                log.error("Coverage analysis failed.")
                return exit_code
            publish_summary(summary_file)
            return 0
        except Exception as e:
            log.error(f"Coverage analysis failed: {e}")
            return 1
