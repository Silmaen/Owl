"""
Depmanager recipe for OwlEngine
"""

from depmanager.api.recipe import Recipe


class OwlEngineShared(Recipe):
    """
    Shared version
    """

    name = "owl_engine"
    version = "0.2.2"
    source_dir = "."
    kind = "shared"
    dependencies = [
        {"name": "EnTT"},
        {"name": "imgui", "kind": "shared"},
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
