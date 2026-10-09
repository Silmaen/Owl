"""
Conan 2 recipe of the Owl engine.

Two uses:

- `cmake/Conan.cmake` runs `conan install` on it at configure time and finds the packages through the generated
  `CMakeDeps` files;
- `conan create .` builds and packages OwlEngine (`owlengine/<version>`), then builds `test_package/`
  against it: `find_package(OwlEngine)` on the installed CMake config and a program linked to `Owl::OwlEngine`.

Recipes come from ConanCenter; the few that are missing there live in `conan/recipes/` (a local-recipes-index
remote registered by `cmake/Conan.cmake`). See `doc/pages/design/conan-migration.md` for the inventory, the
version choices and the commands.
"""

import os
import re

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from conan.tools.files import load

required_conan_version = ">=2.0"


class OwlEngineConan(ConanFile):
    """The Owl engine library, and the dependencies of the Owl Nest editor and the tests."""

    name = "owlengine"
    description = "Owl game engine - C++23 multi-backend game engine with ECS architecture"
    license = "MIT"
    homepage = "https://github.com/Silmaen/Owl"
    url = "https://github.com/Silmaen/Owl"
    topics = ("game-engine", "ecs", "vulkan", "opengl")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "testing": [True, False],
        "nest": [True, False],
        "tracy": [True, False],
        # Engine modules (OWL_MODULE_*): one turned off drops its packages.
        "render": [True, False],
        "physics": [True, False],
        "audio": [True, False],
        "script": [True, False],
        "gui": [True, False],
    }
    default_options = {
        "shared": True,
        "testing": False,
        "nest": False,
        "tracy": False,
        "render": True,
        "physics": True,
        "audio": True,
        "script": True,
        "gui": True,
        # Tracy client: zones cost a flag test until a profiler connects, nothing is buffered before.
        "tracy/*:on_demand": True,
        # LGPL libraries stay shared; imgui is shared so the engine and the editor see one context.
        "imgui/*:shared": True,
        # ImGuizmo declares its functions IMGUI_API (dllimport on Windows): a static ImGuizmo cannot satisfy them.
        "imguizmo/*:shared": True,
        # plutovg (lunasvg's) drops dllimport only with PLUTOVG_BUILD_STATIC, which its recipe does not export.
        "plutovg/*:shared": True,
        "glfw/*:shared": True,
        "openal-soft/*:shared": True,
        "libsndfile/*:shared": True,
        "libsndfile/*:programs": False,
        "glad/*:gl_profile": "compatibility",
        "glad/*:gl_version": "4.6",
        "spdlog/*:use_std_fmt": True,
        "spirv-cross/*:build_executable": False,
        "spirv-cross/*:c_api": False,
        "spirv-cross/*:hlsl": False,
        "spirv-cross/*:msl": False,
        "spirv-cross/*:util": False,
    }
    # What a `conan create` needs to configure and install the engine alone (no editor, no tests).
    exports_sources = (
        "CMakeLists.txt",
        "CPackConfig.cmake",
        "DoxyfileTemplate",
        "README.md",
        "CONTRIBUTING.md",
        "CHANGELOG.md",
        "LICENSE",
        "cmake/*",
        "doc/pages/*",
        "doc/images/*",
        "engine_assets/*",
        "!engine_assets/help/*",
        "source/CMakeLists.txt",
        "source/owl/*",
        "source/tools/*",
    )

    def set_version(self):
        cmake_lists = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        self.version = re.search(r"project\(\s*Owl\s+VERSION\s+([0-9.]+)", cmake_lists).group(1)

    def validate(self):
        if not self.options.shared:
            # A static OwlEngine exports its private dependencies (OwlEnginePrivate): not packaged yet.
            raise ConanInvalidConfiguration(f"{self.ref}: only the shared library is packaged for now")
        if self.options.gui and not self.options.render:
            raise ConanInvalidConfiguration(f"{self.ref}: the gui module needs the render module")

    def requirements(self):
        # The only public dependency of Owl::OwlEngine (scene/Scene.h, scene/Entity.h include it).
        self.requires("entt/4.0.0", transitive_headers=True)
        self.requires("cpptrace/1.0.4")
        self.requires("magic_enum/0.9.8")
        self.requires("msdf-atlas-gen/1.4")
        self.requires("nativefiledialog-extended/1.4.1")
        self.requires("spdlog/1.17.0")
        self.requires("stb/cci.20240531")
        self.requires("taskflow/4.1.0")
        self.requires("tinygltf/2.9.7")
        self.requires("tinyobjloader/2.0.0-rc13")
        self.requires("ufbx/0.23.1")
        self.requires("rapidyaml/0.15.2")
        self.requires("yaml-cpp/0.9.0")
        self.requires("zstd/1.5.7")
        if self.options.render:
            self.requires("glad/2.0.8")
            self.requires("glfw/3.5.1")
            self.requires("lunasvg/3.5.0")
            self.requires("slang/2026.19")
            self.requires("spirv-cross/1.4.357.0")
            self.requires("vulkan-headers/1.4.357.0")
            self.requires("vulkan-loader/1.4.357.0")
            self.requires("vulkan-memory-allocator/3.3.0")
            self.requires("vulkan-utility-libraries/1.4.357.0")
        if self.options.physics:
            self.requires("box2d/3.1.1")
        if self.options.audio:
            self.requires("openal-soft/1.25.2")
            self.requires("libsndfile/1.2.2")
        if self.options.script:
            self.requires("lua/5.5.0")
        if self.options.gui:
            self.requires("imgui/1.92.9b-docking", transitive_headers=True, force=True)
            self.requires("imguizmo/1.10")
        if self.options.tracy:
            self.requires("tracy/0.13.1")
        # Owl Nest only.
        if self.options.nest:
            self.requires("imgui-color-text-edit/cci.20260417")
            self.requires("md4c/0.5.2")
        # Transitive versions newer than the ones their ConanCenter recipes pin.
        self.requires("brotli/1.2.0", override=True)
        self.requires("flac/1.5.0", override=True)
        self.requires("plutovg/1.3.3", override=True)

    def build_requirements(self):
        if self.options.testing:
            self.test_requires("gtest/1.18.0")

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        # Never write a CMakeUserPresets.json in the source tree.
        tc.user_presets_path = False
        # Only read by `conan create` (CMake driven by Conan); cmake/Conan.cmake ignores this toolchain.
        tc.cache_variables["OWL_CONAN_INSTALL"] = False
        tc.cache_variables["OWL_BUILD_SHARED"] = bool(self.options.shared)
        tc.cache_variables["OWL_BUILD_NEST"] = False
        tc.cache_variables["OWL_TESTING"] = False
        tc.cache_variables["OWL_PROFILER"] = "tracy" if self.options.tracy else "none"
        tc.cache_variables["OWL_USE_CCACHE"] = False
        for module in ("render", "physics", "audio", "script", "gui"):
            tc.cache_variables[f"OWL_MODULE_{module.upper()}"] = bool(self.options.get_safe(module))
        if self.options.physics:
            tc.cache_variables["OWL_PHYSICS_AVX2"] = bool(self.dependencies["box2d"].options.get_safe("avx2"))
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        # Consumers use the CMake config installed by the engine (lib/cmake/OwlEngine, target Owl::OwlEngine):
        # the one OwlDrone and the CPack archives use, so `test_package` checks it.
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["."]
        self.cpp_info.libs = ["OwlEngine"]
        self.cpp_info.resdirs = ["assets"]
        self.cpp_info.defines = ["OWL_BUILD_SHARED", f"OWL_PLATFORM_{str(self.settings.os).upper()}"] + [
            f"OWL_WITH_{module.upper()}={int(bool(self.options.get_safe(module)))}"
            for module in ("render", "physics", "audio", "script", "gui")
        ]
