#
# Linux arm64 cross compilation on x86_64: Clang `--target=aarch64-linux-gnu`, lld and an arm64 sysroot
# (`linux-cross-arm64` preset, see doc/pages/building.md#arm64-cross-compilation).
#
# The sysroot is an arm64 root filesystem holding the -dev packages of the native arm64 build (libstdc++, Vulkan,
# GL, X11, Wayland, GTK 3, ALSA / PulseAudio), its absolute symlinks made relative. The binaries built here
# (shader bake, tests) run on the build machine through qemu-user: the kernel must have a binfmt_misc entry for
# aarch64 with the F (fix binary) flag, which also makes it work inside a container without qemu in it.
#
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(OWL_SYSROOT "$ENV{OWL_SYSROOT}" CACHE PATH "arm64 sysroot (default: /opt/sysroot/aarch64-linux-gnu)")
if (NOT OWL_SYSROOT)
    set(OWL_SYSROOT "/opt/sysroot/aarch64-linux-gnu" CACHE PATH "arm64 sysroot" FORCE)
endif ()
if (NOT EXISTS "${OWL_SYSROOT}/usr/include/aarch64-linux-gnu")
    message(FATAL_ERROR "No arm64 sysroot at '${OWL_SYSROOT}' (set OWL_SYSROOT, see doc/pages/building.md).")
endif ()
# try_compile projects get the sysroot too.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES OWL_SYSROOT)

set(CMAKE_SYSROOT "${OWL_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_ASM_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# Libraries, headers and packages from the find roots (Conan's folders, cmake/Conan.cmake, then the sysroot) and outside
# them; programs from the build machine.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# binfmt_misc starts qemu-aarch64 for any arm64 binary; qemu finds the arm64 loader and libraries in the sysroot.
set(CMAKE_CROSSCOMPILING_EMULATOR env "QEMU_LD_PREFIX=${OWL_SYSROOT}")
