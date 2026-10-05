import os
import shutil

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get

required_conan_version = ">=2.0"


class SlangConan(ConanFile):
    """Slang shader compiler, repackaged from the upstream release binaries.

    Slang is not on ConanCenter and building it from source pulls LLVM-sized
    dependencies, so this recipe only redistributes the official release archives.
    """

    name = "slang"
    description = "Slang shading language compiler (upstream release binaries)"
    license = "Apache-2.0 WITH LLVM-exception"
    homepage = "https://github.com/shader-slang/slang"
    url = "https://github.com/Silmaen/Owl"
    topics = ("shader", "spirv", "vulkan", "pre-built")
    package_type = "shared-library"
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        self.folders.source = "src"

    def package_id(self):
        del self.info.settings.compiler
        del self.info.settings.build_type

    def validate(self):
        sources = self.conan_data["sources"][self.version]
        if str(self.settings.os) not in sources or str(self.settings.arch) not in sources[str(self.settings.os)]:
            raise ConanInvalidConfiguration(f"{self.ref}: no upstream binary for {self.settings.os}/{self.settings.arch}")

    def build(self):
        get(self, **self.conan_data["sources"][self.version][str(self.settings.os)][str(self.settings.arch)],
            destination=self.build_folder)

    def package(self):
        copy(self, "LICENSE", self.build_folder, os.path.join(self.package_folder, "licenses"))
        copy(self, "*", os.path.join(self.build_folder, "LICENSES"), os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", os.path.join(self.build_folder, "include"), os.path.join(self.package_folder, "include"))
        if self.settings.os == "Windows":
            copy(self, "*.lib", os.path.join(self.build_folder, "lib"), os.path.join(self.package_folder, "lib"))
            # MinGW linkers search lib<name>.dll.a: the MSVC import library is the same archive format.
            lib_dir = os.path.join(self.package_folder, "lib")
            for name in os.listdir(lib_dir):
                if name.endswith(".lib"):
                    shutil.copy2(os.path.join(lib_dir, name), os.path.join(lib_dir, f"lib{name[:-4]}.dll.a"))
            copy(self, "*.dll", os.path.join(self.build_folder, "bin"), os.path.join(self.package_folder, "bin"))
        else:
            copy(self, "*.so*", os.path.join(self.build_folder, "lib"), os.path.join(self.package_folder, "lib"))
        copy(self, "*", os.path.join(self.build_folder, "lib", f"slang-standard-module-{self.version}"),
             os.path.join(self.package_folder, "lib", f"slang-standard-module-{self.version}"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "slang")
        self.cpp_info.set_property("cmake_target_name", "slang::slang")
        self.cpp_info.libs = ["slang"]
