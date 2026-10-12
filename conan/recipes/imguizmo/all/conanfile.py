import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file, save

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

_DRAW_ARGUMENT_FIXES = {
    "ImCurveEdit.cpp": [
        ("0xFFFFFFFF, true, 3.0f)", "0xFFFFFFFF, 3.0f, ImDrawFlags_Closed)"),
        ("0xFF80B0FF, true, 2.0f)", "0xFF80B0FF, 2.0f, ImDrawFlags_Closed)"),
        ("0xFF0080FF, true, 2.0f)", "0xFF0080FF, 2.0f, ImDrawFlags_Closed)"),
    ],
    "GraphEditor.cpp": [
        ("pass ? col : 0xFF000000, false, ", "pass ? col : 0xFF000000, "),
        (
            """options.mRounding,
                      ImDrawFlags_RoundCornersAll,
                      currentSelectedNode ? options.mBorderSelectionThickness : options.mBorderThickness);""",
            """options.mRounding,
                      currentSelectedNode ? options.mBorderSelectionThickness : options.mBorderThickness,
                      ImDrawFlags_RoundCornersAll);""",
        ),
        ("options.mFrameFocus, 1.f, 0, 2.f)", "options.mFrameFocus, 1.f, 2.f)"),
    ],
    "ImGradient.cpp": [
        ("0xFFFFFFFF, 2.f, 15, 2.5f)", "0xFFFFFFFF, 2.f, 2.5f)"),
        ("0x80FFFFFF, 2.f, 15, 1.25f)", "0x80FFFFFF, 2.f, 1.25f)"),
    ],
}


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
        # imgui 1.92.8 swapped the last two parameters of AddRect / AddPolyline (thickness before flags) and moved
        # ImDrawFlags_Closed: the old calls compile, then imgui refuses them and draws nothing.
        for name, fixes in _DRAW_ARGUMENT_FIXES.items():
            for old, new in fixes:
                replace_in_file(self, os.path.join(self.source_folder, "src", name), old, new)

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
