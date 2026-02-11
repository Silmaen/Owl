"""
Action to define the Docker image for a given preset in a CI environment.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig


class DefineTeamCityVariables(BaseAction):
    """
    Action to define various variables for a given preset.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Define variables for the given preset and set them as TeamCity parameters.
        :param preset: The preset to check.
        :param extra_args: Optional extra arguments (unused).
        :return: Exit code indicating success or failure.
        """
        log.info(f"Defining variables for preset '{preset}'")

        from ci.utils.teamcity import set_teamcity_parameter

        # docker image and parameters
        if preset.get_docker_image() not in [None, ""]:
            set_teamcity_parameter("docker_image", preset.get_docker_image())
            if preset.docker_parameters not in [None, ""]:
                set_teamcity_parameter("docker_parameters", preset.docker_parameters)

        if preset.release_preset not in [None, ""]:
            set_teamcity_parameter("release_preset", preset.release_preset)
        if preset.cmake_preset not in [None, ""]:
            set_teamcity_parameter("cmake_preset", preset.cmake_preset)
        if preset.run_tests is not None:
            set_teamcity_parameter("run_tests", str(preset.run_tests).lower())
        if preset.run_package is not None:
            set_teamcity_parameter("run_package",
                                   str(preset.run_package).lower())
        if preset.run_coverage is not None:
            set_teamcity_parameter(
                "run_coverage", str(preset.run_coverage).lower()
            )
        if preset.run_documentation is not None:
            set_teamcity_parameter(
                "run_documentation", str(preset.run_documentation).lower()
            )
        if preset.publish_doc is not None:
            set_teamcity_parameter(
                "publish_doc", str(preset.publish_doc).lower()
            )
        artifact_path = """+:output/build/%cmake_preset%/bin => BuildArtefact.zip!bin/debug/
+:output/build/%cmake_preset%/lib => BuildArtefact.zip!lib/debug/
+:output/build/%cmake_preset%/test/*.xml => BuildArtefact.zip!test/debug/
+:output/build/%cmake_preset%/Coverage => Coverage.zip"""
        if preset.release_preset not in [None, ""]:
            artifact_path += """+:output/build/%release_preset%/bin => BuildArtefact.zip!bin/release/
+:output/build/%release_preset%/lib => BuildArtefact.zip!lib/release/
+:output/build/%release_preset%/test/*.xml => BuildArtefact.zip!test/release/
+:output/build/%release_preset%/Documentation/html => Documentation.zip
+:output/build/%release_preset%/*.zip
+:output/build/%release_preset%/*.tar.gz"""
        log.info(f"Setting artifact path:\n{artifact_path}")
        set_teamcity_parameter("artifact_path", artifact_path)

        return 0
