"""
Action to build the project using CMake and Ninja for a given preset.
"""

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.preset import get_build_dir
from ci.utils.run import run_command, MODE_BY_COLOR, MODE_FOR_NINJA


class Build(BaseAction):
    """
    Action to build the project using CMake and Ninja for a given preset.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Compile the project for the given preset.
        :param preset: The preset to check.
        :param extra_args: Optional extra arguments (unused).
        :return: Exit code indicating success or failure.
        """
        log.info(f"Building project with preset: {preset}")
        # run cmake configure, capture output in real-time, and print it using logging
        cmd = ["cmake", "--preset", preset.cmake_preset, "-S", str(root)]
        if preset.cmake_generator not in [None, ""]:
            cmd += ["-G", preset.cmake_generator]
        configure = run_command(cmd, detection_mode=MODE_BY_COLOR)
        if configure != 0:
            log.error("CMake configuration failed.")
            return configure
        build_dir = get_build_dir(preset.cmake_preset)
        if not build_dir.exists():
            log.error(f"Build directory does not exist: {build_dir}")
            return 1
        build_detection_mode = MODE_BY_COLOR
        if preset.cmake_generator and "Ninja" in preset.cmake_generator:
            build_detection_mode = MODE_FOR_NINJA
        cmd = ["cmake", "--build", str(build_dir)]
        build_result = 0
        if len(preset.cmake_build_types) > 0:
            for config in preset.cmake_build_types:
                cmd_d = cmd + ["--config", config]
                build_result = run_command(cmd_d, detection_mode=build_detection_mode)
        else:
            build_result = run_command(cmd, detection_mode=build_detection_mode)
        if build_result != 0:
            log.error("CMake build failed.")
            return build_result
        return 0
