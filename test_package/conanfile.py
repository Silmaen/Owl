"""
Consumer check of the OwlEngine Conan package (run by `conan create .`).

Builds two programs against the CMake config the engine installs, the way a downstream project such as OwlDrone
does, then runs them: one linked to `Owl::OwlEngine` alone (EnTT is its only public dependency), one to the optional
`Owl::Gui` (`find_package(OwlEngine COMPONENTS Gui)`, imgui).
"""

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout

required_conan_version = ">=2.0"


class OwlEngineTestConan(ConanFile):
    """Minimal OwlEngine consumer."""

    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        # GNU ld resolves the shared dependencies of libOwlEngine.so (RPATH $ORIGIN only) through LD_LIBRARY_PATH.
        self.run(f'cmake --build "{self.build_folder}"', env=["conanbuild", "conanrun"])

    def test(self):
        if can_run(self):
            for program in ("owl_test_package", "owl_test_package_gui"):
                self.run(os.path.join(self.cpp.build.bindir, program), env="conanrun")
