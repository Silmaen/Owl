import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, save

required_conan_version = ">=2.0"

_CMAKELISTS = """cmake_minimum_required(VERSION 3.15)
project(imguizmo CXX)
find_package(imgui CONFIG REQUIRED)
file(GLOB SRCS src/*.cpp)
file(GLOB HDRS src/*.h)
add_library(imguizmo ${SRCS})
target_compile_features(imguizmo PUBLIC cxx_std_17)
target_link_libraries(imguizmo PUBLIC imgui::imgui)
install(TARGETS imguizmo)
install(FILES ${HDRS} DESTINATION include)
"""


class ImGuizmoConan(ConanFile):
    """ImGuizmo and its bundled widgets (GraphEditor, ImSequencer, ImCurveEdit, ImGradient, ImZoomSlider...).

    ConanCenter only has cci.20231114, which no longer compiles against imgui 1.92 (BeginChildFrame removed);
    this recipe packages the 1.10 release until ConanCenter catches up.
    """

    name = "imguizmo"
    description = "Immediate mode 3D gizmo and editor widgets for Dear ImGui"
    license = "MIT"
    homepage = "https://github.com/CedricGuillemet/ImGuizmo"
    url = "https://github.com/Silmaen/Owl"
    topics = ("imgui", "gizmo", "editor")
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
        # The consumer's own imgui requirement overrides this version.
        self.requires("imgui/1.92.9b", transitive_headers=True)

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        save(self, os.path.join(self.source_folder, "CMakeLists.txt"), _CMAKELISTS)

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.libs = ["imguizmo"]
