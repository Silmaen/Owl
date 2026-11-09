from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get
import os

required_conan_version = ">=2"


class LuauConan(ConanFile):
    name = "luau"
    description = "A fast, small, safe, gradually typed embeddable scripting language derived from Lua"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://luau.org/"
    topics = ("lua", "scripting", "typed", "embed")
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False], "native_code_gen": [True, False]}
    default_options = {"fPIC": True, "native_code_gen": True}

    # Libraries in link order: each one only needs those after it.
    _libraries = ("Analysis", "Config", "Require", "CodeGen", "Compiler", "Bytecode", "Ast", "VM", "Common")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["LUAU_BUILD_CLI"] = False
        tc.cache_variables["LUAU_BUILD_TESTS"] = False
        tc.cache_variables["LUAU_BUILD_WEB"] = False
        tc.cache_variables["LUAU_WERROR"] = False
        tc.cache_variables["LUAU_STATIC_CRT"] = False
        tc.cache_variables["LUAU_NATIVE"] = bool(self.options.native_code_gen)
        # Errors unwind with longjmp, like the reference Lua: a host catching std::exception never swallows them.
        tc.preprocessor_definitions["LUA_USE_LONGJMP"] = 1
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        targets = [f"Luau.{name}" for name in self._libraries]
        cmake.build(target=targets)

    def package(self):
        copy(self, "LICENSE.txt", self.source_folder, os.path.join(self.package_folder, "licenses"))
        copy(self, "lua_LICENSE.txt", self.source_folder, os.path.join(self.package_folder, "licenses"))
        for name in self._libraries:
            copy(self, "*.h", os.path.join(self.source_folder, name, "include"),
                 os.path.join(self.package_folder, "include"))
        for pattern in ("*.a", "*.lib"):
            copy(self, pattern, self.build_folder, os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Luau")
        self.cpp_info.set_property("cmake_target_name", "Luau::Luau")
        requires = {
            "Common": [],
            "VM": ["Common"],
            "Ast": ["Common"],
            "Bytecode": ["Common"],
            "Compiler": ["Ast", "Bytecode"],
            "CodeGen": ["VM", "Common"],
            "Config": ["Ast", "Compiler", "VM"],
            "Require": ["Config", "VM"],
            "Analysis": ["Ast", "Config", "Compiler", "VM"],
        }
        for name in self._libraries:
            component = self.cpp_info.components[name]
            component.libs = [f"Luau.{name}"]
            component.set_property("cmake_target_name", f"Luau::{name}")
            component.requires = requires[name]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.components["VM"].system_libs = ["m"]
