"""
Depmanager recipe for OwlEngine
"""

import re
from pathlib import Path

from depmanager.api.recipe import Recipe

# Single source of the version: project(Owl VERSION ...) in CMakeLists.txt.
_VERSION = re.search(
    r"project\(\s*Owl\s+VERSION\s+([0-9.]+)", (Path(__file__).parent / "CMakeLists.txt").read_text()
).group(1)


class OwlEngineShared(Recipe):
    """
    Shared version
    """

    name = "owl_engine"
    version = _VERSION
    source_dir = "."
    kind = "shared"
    dependencies = [
        {"name": "EnTT"},
        {"name": "imgui", "kind": "shared"},
        {"name": "yaml-cpp"},
    ]
    description = "Owl game engine - C++23 multi-backend game engine with ECS architecture"

    def configure(self):
        self.cache_variables["OWL_BUILD_SHARED"] = "ON"
        self.cache_variables["OWL_BUILD_NEST"] = "OFF"
        self.cache_variables["OWL_TESTING"] = "OFF"
        self.cache_variables["OWL_PACKAGING"] = "ON"
        self.cache_variables["OWL_PACKAGE_ENGINE"] = "ON"


class OwlEngineStatic(OwlEngineShared):
    """
    Static version
    """

    kind = "static"

    def configure(self):
        super().configure()
        self.cache_variables["OWL_BUILD_SHARED"] = "OFF"
