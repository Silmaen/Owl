import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get

required_conan_version = ">=2.0"


class MsdfAtlasGenConan(ConanFile):
    """msdf-atlas-gen as a library.

    ConanCenter's msdf-atlas-gen recipe only packages the command-line tool (no headers, no library), so this
    recipe builds the upstream library target against ConanCenter's msdfgen. It takes precedence over
    ConanCenter because the `owl-local` remote is listed first.
    """

    name = "msdf-atlas-gen"
    description = "Multi-channel signed distance field font atlas generator (library)"
    license = "MIT"
    homepage = "https://github.com/Chlumsky/msdf-atlas-gen"
    url = "https://github.com/Silmaen/Owl"
    topics = ("msdf", "font", "atlas")
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

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires("msdfgen/1.12", transitive_headers=True)
        self.requires("libpng/[>=1.6 <2]")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MSDF_ATLAS_BUILD_STANDALONE"] = False
        tc.cache_variables["MSDF_ATLAS_USE_VCPKG"] = False
        tc.cache_variables["MSDF_ATLAS_USE_SKIA"] = False
        tc.cache_variables["MSDF_ATLAS_NO_ARTERY_FONT"] = True
        tc.cache_variables["MSDF_ATLAS_MSDFGEN_EXTERNAL"] = True
        tc.cache_variables["MSDF_ATLAS_INSTALL"] = True
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "msdf-atlas-gen")
        self.cpp_info.set_property("cmake_target_name", "msdf-atlas-gen::msdf-atlas-gen")
        self.cpp_info.libs = ["msdf-atlas-gen"]
        self.cpp_info.defines = ["MSDF_ATLAS_NO_ARTERY_FONT", "MSDF_ATLAS_PUBLIC="]
        self.cpp_info.requires = ["msdfgen::msdfgen", "msdfgen::msdfgen-ext", "libpng::libpng"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread"]
