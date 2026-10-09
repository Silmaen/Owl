import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file, rmdir

required_conan_version = ">=2.0"


class GlfwConan(ConanFile):
    """GLFW 3.5 with both Linux backends, built against the system X11 and Wayland development files.

    ConanCenter's recipe builds X11 only by default; with Wayland on it pulls its own libwayland and libxkbcommon,
    which then shadow the system ones next to the binaries and break the GPU drivers (Mesa's Vulkan ICDs need the
    system libwayland). GLFW dlopens every platform library, so nothing is linked nor shipped here.
    """

    name = "glfw"
    description = "Multi-platform library for OpenGL, OpenGL ES and Vulkan windows, contexts, surfaces and input"
    license = "Zlib"
    homepage = "https://github.com/glfw/glfw"
    url = "https://github.com/Silmaen/Owl"
    topics = ("graphics", "opengl", "vulkan", "window")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False], "with_x11": [True, False],
               "with_wayland": [True, False]}
    default_options = {"shared": False, "fPIC": True, "with_x11": True, "with_wayland": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if self.settings.os != "Linux":
            del self.options.with_x11
            del self.options.with_wayland

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        # Headers only: GLFW loads the GL library itself.
        self.requires("opengl/system", libs=False, transitive_headers=True)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["GLFW_BUILD_DOCS"] = False
        tc.cache_variables["GLFW_BUILD_EXAMPLES"] = False
        tc.cache_variables["GLFW_BUILD_TESTS"] = False
        tc.cache_variables["GLFW_INSTALL"] = True
        tc.cache_variables["GLFW_BUILD_X11"] = bool(self.options.get_safe("with_x11", False))
        tc.cache_variables["GLFW_BUILD_WAYLAND"] = bool(self.options.get_safe("with_wayland", False))
        tc.generate()

    def build(self):
        src_cmake = os.path.join(self.source_folder, "src", "CMakeLists.txt")
        replace_in_file(self, src_cmake, "POSITION_INDEPENDENT_CODE ON", "")
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", self.source_folder, os.path.join(self.package_folder, "licenses"))
        CMake(self).install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "glfw3")
        self.cpp_info.set_property("cmake_target_name", "glfw")
        self.cpp_info.set_property("pkg_config_name", "glfw3")
        libname = "glfw"
        if self.settings.os == "Windows" or not self.options.shared:
            libname += "3"
        if self.settings.os == "Windows" and self.options.shared:
            libname += "dll"
            self.cpp_info.defines.append("GLFW_DLL")
        self.cpp_info.libs = [libname]
        if self.settings.os == "Linux":
            self.cpp_info.system_libs.extend(["m", "pthread", "dl", "rt"])
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs.append("gdi32")
        # GLFW loads its platform and GL libraries at run time: nothing else to link.
        self.cpp_info.requires = []
