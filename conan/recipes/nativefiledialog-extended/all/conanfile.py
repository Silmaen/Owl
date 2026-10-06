import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get
from conan.tools.gnu import PkgConfig

required_conan_version = ">=2.0"


class NativeFileDialogExtendedConan(ConanFile):
    """Native file dialogs (btzy fork). On Linux, GTK 3 (or D-Bus) and wayland-client come from the system."""

    name = "nativefiledialog-extended"
    description = "Native file open/save dialogs (GTK or xdg-desktop-portal, Win32, Cocoa)"
    license = "Zlib"
    homepage = "https://github.com/btzy/nativefiledialog-extended"
    url = "https://github.com/Silmaen/Owl"
    topics = ("file-dialog", "gui", "native")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False], "portal": [True, False]}
    default_options = {"shared": False, "fPIC": True, "portal": False}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.os != "Linux":
            del self.options.portal

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        sources = self.conan_data["sources"][self.version]
        get(self, **sources["nfd"], strip_root=True)
        get(self, **sources["wayland-protocols"], strip_root=True,
            destination=os.path.join(self.source_folder, "3ps", "wayland-protocols"))

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["NFD_BUILD_TESTS"] = False
        tc.cache_variables["NFD_INSTALL"] = True
        if self.settings.os == "Linux":
            tc.cache_variables["NFD_PORTAL"] = bool(self.options.portal)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "nfd")
        self.cpp_info.set_property("cmake_target_name", "nfd::nfd-all")
        nfd = self.cpp_info.components["nfd"]
        nfd.set_property("cmake_target_name", "nfd::nfd")
        nfd.libs = ["nfd"]
        if self.settings.os == "Linux":
            for module in ("dbus-1" if self.options.portal else "gtk+-3.0", "wayland-client"):
                component = module.split("-")[0].replace("+", "")
                PkgConfig(self, module).fill_cpp_info(self.cpp_info.components[component], is_system=True)
                nfd.requires.append(component)
        elif self.settings.os == "Windows":
            nfd.system_libs = ["ole32", "uuid", "shell32"]
        elif self.settings.os == "Macos":
            nfd.frameworks = ["AppKit", "UniformTypeIdentifiers"]
