from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, replace_in_file, rm, rmdir
import os

required_conan_version = ">=2.0.9"


class JoltPhysicsConan(ConanFile):
    name = "joltphysics"
    description = (
        "A multi core friendly rigid body physics and collision detection "
        "library, written in C++, suitable for games and VR applications."
    )
    license = "MIT"
    topics = ("physics", "simulation", "physics-engine", "rigid-body", "game", "collision")
    homepage = "https://github.com/jrouwe/JoltPhysics"
    url = "https://github.com/conan-io/conan-center-index"

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    implements = ["auto_shared_fpic"]

    # Owl: one fixed instruction set per architecture, so that no CPU changes a simulation result.
    # x86_64 is SSE4.2 (x86-64-v2); arm64 always has NEON; no FMA anywhere.
    @property
    def _is_x86_64(self):
        return str(self.settings.arch) == "x86_64"

    def layout(self):
        cmake_layout(self, src_folder="src")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.20 <5]")

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        replace_in_file(self, os.path.join(self.source_folder, "Jolt", "Jolt.cmake"), "set(CMAKE_CXX_STANDARD 17)", "")

    def generate(self):
        tc = CMakeToolchain(self)
        for target in ("UNIT_TESTS", "HELLO_WORLD", "PERFORMANCE_TEST", "SAMPLES", "VIEWER"):
            tc.cache_variables[f"TARGET_{target}"] = False
        tc.cache_variables["JPH_BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.cache_variables["CROSS_PLATFORM_DETERMINISTIC"] = True
        tc.cache_variables["USE_SSE4_1"] = self._is_x86_64
        tc.cache_variables["USE_SSE4_2"] = self._is_x86_64
        for feature in ("AVX", "AVX2", "AVX512", "LZCNT", "TZCNT", "F16C", "FMADD"):
            tc.cache_variables[f"USE_{feature}"] = False
        tc.cache_variables["DOUBLE_PRECISION"] = False
        # Owl derives from Jolt's listener interfaces: their typeinfo must exist in the library.
        tc.cache_variables["CPP_RTTI_ENABLED"] = True
        tc.cache_variables["USE_ASSERTS"] = False
        tc.cache_variables["ENABLE_OBJECT_STREAM"] = False
        tc.cache_variables["DEBUG_RENDERER_IN_DEBUG_AND_RELEASE"] = False
        tc.cache_variables["PROFILER_IN_DEBUG_AND_RELEASE"] = False
        tc.cache_variables["INTERPROCEDURAL_OPTIMIZATION"] = False
        tc.cache_variables["GENERATE_DEBUG_SYMBOLS"] = False
        tc.cache_variables["ENABLE_ALL_WARNINGS"] = False
        tc.cache_variables["OVERRIDE_CXX_FLAGS"] = False
        tc.cache_variables["FLOATING_POINT_EXCEPTIONS_ENABLED"] = False
        for backend in ("DX12", "VK", "MTL", "CPU_COMPUTE"):
            tc.cache_variables[f"JPH_USE_{backend}"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "Build"))
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rm(self, "*.cmake", os.path.join(self.package_folder, "include", "Jolt"))

    def package_info(self):
        self.cpp_info.libs = ["Jolt"]
        self.cpp_info.set_property("cmake_file_name", "Jolt")
        self.cpp_info.set_property("cmake_target_name", "Jolt::Jolt")
        # The public defines of Jolt.cmake; a consumer built with other ones fails JPH::VerifyJoltVersionID().
        self.cpp_info.defines = ["JPH_CROSS_PLATFORM_DETERMINISTIC"]
        if self._is_x86_64:
            self.cpp_info.defines.extend(["JPH_USE_SSE4_1", "JPH_USE_SSE4_2"])
        if self.options.shared:
            self.cpp_info.defines.append("JPH_SHARED_LIBRARY")
        # No cxxflags: the consumer compiles the files that include Jolt with -msse4.2 -mpopcnt on x86_64, and only
        # those, so the rest of the engine keeps the x86-64 baseline.
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "pthread"])
