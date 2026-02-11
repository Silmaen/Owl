"""
Action to run code coverage analysis and upload the results to a coverage tracking service.
"""
from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig


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
            exit_code = run_command(
                ["gcovr", "-j", "0", "-r", f"{root}", "-o",
                 f'{preset.get_build_dir() / "Coverage" / "index.html"}',
                 '--gcov-executable', f'{gcov_executable}', "."])
            if exit_code != 0:
                log.error("Coverage analysis failed.")
                return exit_code
            return 0
        except Exception as e:
            log.error(f"Coverage analysis failed: {e}")
            return 1
