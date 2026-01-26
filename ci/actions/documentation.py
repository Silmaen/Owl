"""
Action to run Documentation generation using doxygen.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command


class Documentation(BaseAction):
    """
    Action to run Documentation generation using doxygen.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Executes the documentation generation process.
        :param preset: The preset to use for the action.
        :param extra_args: Optional extra arguments (unused).
        :return: Exit code indicating success or failure.
        """
        log.info("Starting documentation generation using Doxygen...")
        try:
            build_dir = preset.get_build_dir()
            if preset.release_preset not in [None, ""]:
                build_dir = preset.get_release_build_dir()
            exit_code = run_command(
                ["cmake", "--build", f"{build_dir}", "--target",
                 "documentation"])
            if exit_code != 0:
                log.error("Documentation generation failed.")
                return exit_code
            log.info("Documentation generated successfully.")
            return 0
        except Exception as e:
            log.error(f"Documentation generation failed: {e}")
            return 1
