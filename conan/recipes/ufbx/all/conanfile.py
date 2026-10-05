import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, save

required_conan_version = ">=2.0"

_CMAKELISTS = """cmake_minimum_required(VERSION 3.15)
project(ufbx C)
add_library(ufbx ufbx.c)
install(TARGETS ufbx)
install(FILES ufbx.h DESTINATION include)
"""


class UfbxConan(ConanFile):
    """Single-file FBX loader; upstream ships no build system, so the recipe adds a two-line one."""

    name = "ufbx"
    description = "Single source file FBX loader"
    license = "MIT"
    homepage = "https://github.com/ufbx/ufbx"
    url = "https://github.com/Silmaen/Owl"
    topics = ("fbx", "3d", "mesh", "loader")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        save(self, os.path.join(self.source_folder, "CMakeLists.txt"), _CMAKELISTS)

    def generate(self):
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ufbx")
        self.cpp_info.set_property("cmake_target_name", "ufbx::ufbx")
        self.cpp_info.libs = ["ufbx"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["m"]
