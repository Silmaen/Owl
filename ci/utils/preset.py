"""
Gathering tools for presets
"""
from typing import Any, Dict, Optional, List

from ci import root, Path, log

config_file = root / "ci" / "PresetsParameters.json"


def get_build_dir(preset: str) -> Path:
    """
    Get the build directory for a given preset.

    :param preset: The preset to get the build directory for.
    :return: The Path to the build directory.
    """
    return root / "output" / "build" / preset


class PresetConfig:
    """
    Class representing a preset configuration.
    """

    def __init__(self, preset: str):
        self.cmake_preset = preset
        # docker related fields
        self.docker_registry: Optional[str] = None
        self.docker_namespace: Optional[str] = None
        self.docker_image: Optional[str] = None
        self.docker_parameters: Optional[str] = None

        # Cmake related fields can be added here as needed
        self.cmake_generator: Optional[str] = None
        self.cmake_build_types: List[str] = []
        self.build_directory: Optional[Path] = None
        self.compiler: Optional[str] = None

        # validity flag
        self.is_valid = False

        # action related fields
        self.release_preset: Optional[str] = None
        self.run_coverage: bool = False
        self.run_tests: bool = True
        self.run_documentation: bool = True
        self.run_package: bool = False
        self.publish_doc: bool = False

        # raw config storage from CMake preset file
        self.raw_config: Optional[Dict[str, Any]] = None
        self.archive_format: Optional[str] = None
        # import the preset values
        if preset in [None, ""]:
            log.error("No preset name provided.")
            return
        if preset in ["help"]:
            # Special case for help preset, no import needed
            self.is_valid = True
            return
        self.__import()

    def __import(self):
        """Internal import method. it will reed the actual CMakePresets.json file and import the values for this preset."""
        from ci.utils.cmake import get_preset_config, list_cmake_presets
        self.raw_config = get_preset_config(self.cmake_preset)
        if not self.raw_config:
            log.error(f"Preset '{self.cmake_preset}' not found in any preset file.")
            return
        # Define substitution variables
        substitutions = {"sourceDir": str(root), "presetName": self.cmake_preset}
        # Recursively expand variables in the config
        self.raw_config = self.__expand_variables(self.raw_config, substitutions)
        if self.raw_config is None:
            log.error(
                f"Failed to load preset configuration for '{self.cmake_preset}'.")
            return

        if "vendor" in self.raw_config:
            vendor = self.raw_config["vendor"]
            if "silmaen" in vendor:
                vendor_conf = vendor["silmaen"]
                if "docker_image" in vendor_conf:
                    self.docker_image = vendor_conf["docker_image"]
                if "docker_registry" in vendor_conf:
                    self.docker_registry = vendor_conf["docker_registry"]
                if "docker_namespace" in vendor_conf:
                    self.docker_namespace = vendor_conf["docker_namespace"]
                if "docker_parameters" in vendor_conf:
                    self.docker_parameters = vendor_conf["docker_parameters"]
                if "archive_format" in vendor_conf:
                    self.archive_format = vendor_conf["archive_format"]
                else:
                    import platform

                    if platform.system().lower() == "windows":
                        self.archive_format = "zip"
                    else:
                        self.archive_format = "tar.gz"
                if "release_preset" in vendor_conf:
                    if vendor_conf["release_preset"] in list_cmake_presets():
                        self.release_preset = vendor_conf["release_preset"]
                if "publish_doc" in vendor_conf:
                    self.publish_doc = vendor_conf["publish_doc"] in [
                        "ON", "True", "true", "1"
                    ]

        if "generator" in self.raw_config:
            self.cmake_generator = self.raw_config["generator"]
        if "cacheVariables" in self.raw_config:
            cache_variables = self.raw_config["cacheVariables"]
            if "CMAKE_BUILD_TYPE" in cache_variables:
                self.cmake_build_types = [cache_variables["CMAKE_BUILD_TYPE"]]
            elif "CMAKE_CONFIGURATION_TYPES" in self.raw_config["cacheVariables"]:
                self.cmake_build_types = cache_variables[
                    "CMAKE_CONFIGURATION_TYPES"
                ].split(";")
            if "OWL_TESTING" in cache_variables:
                self.run_tests = cache_variables["OWL_TESTING"] in [
                    "ON", "True", "true", "1"
                ]
            if "OWL_ENABLE_COVERAGE" in cache_variables:
                self.run_coverage = cache_variables["OWL_ENABLE_COVERAGE"] in [
                    "ON", "True", "true", "1"
                ] and self.run_tests
            if "OWL_ENABLE_DOCUMENTATION" in cache_variables:
                self.run_documentation = cache_variables[
                                             "OWL_ENABLE_DOCUMENTATION"] in [
                                             "ON", "True", "true", "1"
                                         ] and (
                                                 "Release" in self.cmake_build_types or
                                                 self.release_preset not in [None, ""])
            if "OWL_PACKAGE_NAME" in cache_variables:
                self.run_package = cache_variables["OWL_PACKAGE_NAME"] not in [None,
                                                                               ""]
            if "CMAKE_CXX_COMPILER" in cache_variables:
                self.compiler = cache_variables["CMAKE_CXX_COMPILER"]

        if "binaryDir" in self.raw_config:
            self.build_directory = Path(self.raw_config["binaryDir"])
        else:
            self.build_directory = root / "output" / "build" / self.cmake_preset

        self.is_valid = True

    def __repr__(self):
        items = (
            "%s = %r" % (k, v)
            for k, v in self.__dict__.items()
            if v not in [None, ""] and k != "raw_config"
        )
        return f"{self.__class__.__name__}: {"\n  ".join(items)}"

    def __expand_variables(self, i_data: Any, i_substitutions: Dict[str, str]) -> Any:
        """Recursively expand ${variable} expressions in dictionaries, lists, and strings."""
        if isinstance(i_data, dict):
            result = {}
            for key, value in i_data.items():
                # Expand variables in key
                expanded_key = self.__expand_string(key, i_substitutions)
                # Expand variables in value recursively
                result[expanded_key] = self.__expand_variables(value, i_substitutions)
            return result
        elif isinstance(i_data, list):
            return [self.__expand_variables(item, i_substitutions) for item in i_data]
        elif isinstance(i_data, str):
            return self.__expand_string(i_data, i_substitutions)
        else:
            return i_data

    @staticmethod
    def __expand_string(i_string: str, i_substitutions: Dict[str, str]) -> str:
        """Expand ${variable} expressions in a string."""
        import re

        def replace_match(match):
            var_name = match.group(1)
            return i_substitutions.get(var_name, match.group(0))

        return re.sub(r"\$\{(\w+)}", replace_match, i_string)

    def get_build_dir(self) -> Path:
        """
        Get the build directory for this preset.

        :return: The Path to the build directory.
        """
        if self.build_directory is not None:
            return self.build_directory
        return root / "build" / self.cmake_preset

    def get_release_build_dir(self) -> Path:
        """
        Get the release build directory for this preset.

        :return: The Path to the release build directory.
        """
        if self.release_preset is None:
            return Path()
        cc = PresetConfig(self.release_preset)
        return cc.get_build_dir()

    def get_docker_image(self) -> str:
        """
        Get the full docker image name.

        :return: The full docker image name.
        """
        if self.docker_image in [None, ""]:
            return ""
        image = f"{self.docker_image}"
        if self.docker_namespace not in [None, ""]:
            image = f"{self.docker_namespace}/{image}"
        if self.docker_registry not in [None, ""]:
            image = f"{self.docker_registry}/{image}"
        return image
