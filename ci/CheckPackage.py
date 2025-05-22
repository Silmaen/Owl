#!/usr/bin/env python3
"""
Script to check library dependencies in packaged binaries for different platforms.
"""

import argparse
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path
from platform import system
from sys import stderr

ignore_list_lin = [
    "libc",
    "libgcc",
    "libm",
    "libstdc++",
    "ld-linux-x86-64",
    "linux-vdso",
    "libatomic",
    "libGL",
    "libX11",
    "libGLX",
    "libxcb",
    "libXau",
    "libXdmcp",
    "libbsd",
    "libssl",
    "libcrypto",
]
ignore_list_win = [
    "kernel32.dll",
    "user32.dll",
    "gdi32.dll",
    "winspool.drv",
    "comdlg32.dll",
    "advapi32.dll",
    "shell32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "uuid.dll",
    "odbc32.dll",
    "odbccp32.dll",
    "msvcrt.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "msvcp140.dll",
    "vcruntime140_1.dll",
    "vcruntime140.dll",
    "dwmapi.dll",
    "opengl32.dll",
    "winmm.dll",
    "wldap32.dll",
    "ws2_32.dll",
    "dbghelp.dll",
    "cfgmgr32.dll",
    "avrt.dll",
    "mf.dll",
    "mfplat.dll",
    "mfreadwrite.dll"
]

tocheck = [
    "libOwlEngine",
    "OwlNest",
    "OwlDrone",
    "OwlRunner",
    "OwlSandbox"
]

platforms = [
    "Windows_x64",
    "Linux_x64",
    "Linux_arm64",
]


def is_ignored(lib_name, ignore_list):
    """
    Check if a library name is in the ignore list.

    :param lib_name: Name of the library.
    :param ignore_list: List of libraries to ignore.
    :return: True if ignored, False otherwise.
    """
    return any(ignore in lib_name for ignore in ignore_list)


def in_same_dir(dep: str, bin_dir: Path) -> bool:
    """
    Check if a dependency exists in the same directory as the binary.

    :param dep: Dependency filename.
    :param bin_dir: Directory to check.
    :return: True if exists, False otherwise.
    """
    return (bin_dir / dep).exists()


def check_shared_lib_linux(lib):
    """
    Check shared library dependencies on Linux.

    :param lib: Path to the library file.
    :return: None
    """
    try:
        result = subprocess.run(
            ["ldd", str(lib)], capture_output=True, text=True, check=True
        )
        non_system_deps = []

        for line in result.stdout.splitlines():
            if "=>" not in line:
                continue
            parts = line.split("=>", 1)

            lib_name = parts[0].strip()

            if not is_ignored(lib_name, ignore_list_lin):
                non_system_deps.append(lib_name)

        if non_system_deps:
            print(lib)
            for dep in non_system_deps:
                print(f"    {dep}")
    except subprocess.CalledProcessError:
        pass


def check_dll_dependencies_windows(dll: Path):
    """
    Check DLL dependencies on Windows.

    :param dll: Path to the DLL file.
    :return: None
    """
    try:
        result = subprocess.run(
            ["objdump", "-p", str(dll)], capture_output=True, text=True, check=True
        )
        non_system_deps = []

        bin_dir = dll.parent

        for line in result.stdout.splitlines():
            if "DLL Name" in line:
                dep = line.split()[-1].lower()
                if not is_ignored(dep, ignore_list_win) and not in_same_dir(dep, bin_dir):
                    non_system_deps.append(dep)

        if non_system_deps:
            print(dll)
            for dep in non_system_deps:
                print(f"    {dep}")
    except subprocess.CalledProcessError:
        pass


def main():
    """
    Main entry point for checking package dependencies.

    :return: None
    """
    parser = argparse.ArgumentParser(description="Check library dependencies.")
    parser.add_argument(
        "location", type=str, help="Destination to check."
    )
    args = parser.parse_args()
    location = Path(args.location).resolve()
    tt = Path(tempfile.mkdtemp())
    try:
        if not location.exists():
            print("The destination does not exist.", file=stderr)
        work_dir = location
        archive_file = None
        if location.is_file():
            archive_file = location
        else:
            for f in location.iterdir():
                if f.is_file() and f.suffix in [".zip", ".tgz"]:
                    archive_file = Path(f)
                    break
        if archive_file:
            print(f"Uncompacting: {archive_file}")
            if archive_file.suffix == ".zip":
                print(f"Uncompacting Zip file to {tt}")
                with zipfile.ZipFile(archive_file) as zf:
                    zf.extractall(tt)
            elif archive_file.suffix == ".tgz":
                print(f"Uncompacting tgz file to {tt}")
                with tarfile.open(archive_file) as tar:
                    tar.extractall(tt)
            else:
                print("The destination must be a .zip or .tgz file of an uncompressed folder.", file=stderr)
                exit(1)
            work_dir = tt / archive_file.stem
            for f in work_dir.iterdir():
                print(f"in folder: {f}")
        if (work_dir / "bin").exists():
            work_dir /= "bin"
        for plat in platforms:
            if (work_dir / plat).exists():
                work_dir /= plat
        print(f"Using work dir {work_dir}")
        for file in work_dir.iterdir():
            if not file.is_file():
                continue
            if file.stem in tocheck:
                print(f"Checking {file.name}")
                if system() == "Linux":
                    check_shared_lib_linux(file)
                elif system() == "Windows":
                    check_dll_dependencies_windows(file)
                else:
                    print("Unsupported Operating System.")
    finally:
        shutil.rmtree(tt)


if __name__ == "__main__":
    main()
