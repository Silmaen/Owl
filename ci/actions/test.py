"""
Action to test the project using CTest for a given preset.
"""

import os
from pathlib import Path

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command


def default_jobs() -> int:
    """
    Number of tests run at once by default: the processors this process may use.

    :return: The processor count of the CPU affinity mask where it exists, else of the machine, at least 1.
    """
    if hasattr(os, "sched_getaffinity"):
        return max(1, len(os.sched_getaffinity(0)))
    return max(1, os.cpu_count() or 1)


def ctest_command(build_dir: Path, jobs: int) -> list[str]:
    """
    CTest command line of a build tree.

    :param build_dir: The build tree to test.
    :param jobs: Number of tests run at once.
    :return: The command; the `RESOURCE_LOCK` of a test keeps it apart from the ones sharing its resource.
    """
    return ["ctest", "--test-dir", str(build_dir), "--output-on-failure", "--parallel", str(jobs)]


class Test(BaseAction):
    """
    Action to test the project using CTest for a given preset.

    Extra arguments (after ``--``):
      * ``--jobs=<n>`` — number of tests run at once, default the processors available.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Test the project using CTest for the given preset.
        :param preset: The preset to check.
        :param extra_args: Optional extra arguments, see the class docstring.
        :return: Exit code indicating success or failure.
        """
        args = self.parse_extra_args(extra_args)
        jobs = max(1, int(args.get("jobs", default_jobs())))
        log.info(f"Running tests with preset: {preset}, {jobs} at once")
        build_dir = preset.get_build_dir()
        if not build_dir.exists():
            log.error(f"Build directory does not exist: {build_dir}")
            return 1
        test = run_command(ctest_command(build_dir, jobs))
        if test != 0:
            log.error("Tests failed.")
            return test
        return 0
