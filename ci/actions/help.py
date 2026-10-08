"""
Action for helping lost developers.
"""

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig


class Help(BaseAction):
    """
    Action for helping lost developers.
    """

    @staticmethod
    def print_preset_info(preset: PresetConfig) -> int:
        log.info(preset)
        if preset.get_docker_image() not in [None, ""]:
            log.info(f"Will be run in docker image: `{preset.get_docker_image()}`")
        else:
            log.info("Will be run in native environment.")
        log.info(f"Build dir: {preset.get_build_dir()}")
        log.debug(f"raw config: {preset.raw_config}")
        return 0

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Help lost developers find their way.
        :param preset: not used
        :param extra_args: Optional extra arguments (unused).
        :return: 0
        """
        if preset.cmake_preset != "help":
            return self.print_preset_info(preset)
        log.info("You're a smart developer, but you are lost..")
        log.info("I will help you find the way back to the path of the light!")
        log.info("We both know that this is the only path. All else is darkness.")
        log.info("Follow me, and I will guide you!")
        log.info("Just kidding, read the documentation instead :)")
        log.info(
            f"""
        RTFM: {root}/ci/readme.md
"""
        )
        log.info("`log.info('Good luck!')` example of info message.")
        log.debug("`log.debug('Debugging info!')` example of debug message.")
        log.warning("`log.warning('Warning!')` example of warning message.")
        log.error("`log.error('Bad luck!')` example of error message.")
        log.critical("`log.critical('Critical issue!')` example of critical message.")
        log.info("Good luck!")
        log.info("May the source be with you!")
        log.info("\n\n- The ReshaperTools Team\n\n")
        log.info("Here is a clean list of available actions:")
        from ci.actions import get_actions

        actions = get_actions()
        for action in actions:
            log.info(f"- {action}")
        log.info("Here is a clean list of available presets:")
        from ci.utils.cmake import list_cmake_presets

        for preset_name in list_cmake_presets():
            log.info(f"- {preset_name}")

        log.info("You can also run `./ci_action.py Help <preset>` to get info about a specific preset.")
        return 0
