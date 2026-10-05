"""
Consumer check of the OwlEngine Conan package (run by `conan create .`).

Builds a program against the CMake config the engine installs (`find_package(OwlEngine)`, `Owl::OwlEngine`),
the way a downstream project such as OwlDrone does, then runs it.
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
            self.run(os.path.join(self.cpp.build.bindir, "owl_test_package"), env="conanrun")
