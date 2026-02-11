"""
Action to deploy the project using CPack for a given preset.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command, MODE_BY_COLOR


class Package(BaseAction):
    """
    Action to deploy the project using CPack for a given preset.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Deploy the project using CPack for the given preset.
        :param preset: The preset to check.
        :param extra_args: Optional extra arguments (unused).
        :return: Exit code indicating success or failure.
        """
        from os import chdir, curdir
        log.info(f"Deploying project with preset: {preset}")
        build_dir = preset.get_build_dir()

        # Generate Package
        if not build_dir.exists():
            log.error(f"Build directory does not exist: {build_dir}")
            return 1
        cwd = curdir
        chdir(build_dir)
        build_result = run_command(
            ["cpack"], detection_mode=MODE_BY_COLOR
        )
        chdir(cwd)
        if build_result != 0:
            log.error("CMake build failed.")
            return build_result
        return 0


