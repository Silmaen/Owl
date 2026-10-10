import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file, save

required_conan_version = ">=2.0"

_CMAKELISTS = """cmake_minimum_required(VERSION 3.15)
project(imgui_color_text_edit CXX)
find_package(imgui CONFIG REQUIRED)
add_library(imgui_color_text_edit TextEditor.cpp TextDiff.cpp)
target_compile_features(imgui_color_text_edit PUBLIC cxx_std_17)
target_link_libraries(imgui_color_text_edit PUBLIC imgui::imgui)
install(TARGETS imgui_color_text_edit)
install(FILES TextEditor.h TextDiff.h dtl.h DESTINATION include)
"""


class ImGuiColorTextEditConan(ConanFile):
    """Syntax-highlighting text editor widget for Dear ImGui (goossens fork, tracks imgui releases)."""

    name = "imgui-color-text-edit"
    description = "Colorizing text editor and text diff for Dear ImGui"
    license = "MIT"
    homepage = "https://github.com/goossens/ImGuiColorTextEdit"
    url = "https://github.com/Silmaen/Owl"
    topics = ("imgui", "text-editor", "syntax-highlighting")
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
        # Upstream exports the classes with IMGUI_API: with a shared imgui that is dllimport inside this static library.
        for header, cls in (("TextEditor.h", "TextEditor"), ("TextDiff.h", "TextDiff")):
            replace_in_file(self, os.path.join(self.source_folder, header), f"class IMGUI_API {cls} {{", f"class {cls} {{")

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
        self.cpp_info.set_property("cmake_file_name", "imgui_color_text_edit")
        self.cpp_info.set_property("cmake_target_name", "imgui_color_text_edit::imgui_color_text_edit")
        self.cpp_info.libs = ["imgui_color_text_edit"]
