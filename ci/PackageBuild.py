#!/usr/bin/env python3
"""
Script to configure, build, install, and package a project using CMake presets.
"""

from sys import version_info as py_version, stderr

if py_version.major < 3:
    print(
        "You are not running a real python runner (python 3 is the only one)",
        file=stderr,
    )
    exit(-666)

from argparse import ArgumentParser
from platform import system, machine
from pathlib import Path
from shutil import rmtree, move
from subprocess import run
from os import chdir, curdir, environ

sourcePath = Path(__file__).absolute().parent.parent
buildPath = sourcePath / "output" / "build"
installPath = sourcePath / "output" / "install"
packagePath = sourcePath / "output" / "package"

system_name = ""
architecture = ""
distribution = ""
build_hash = ""
env = environ.copy()
env.pop("SDKROOT", None)
env.pop("CPATH", None)


def get_target_arch(cmake_preset: str):
    """
    Get the target architecture from the cmake preset name.

    :param cmake_preset: The cmake preset name.
    :return: Architecture string.
    """
    if "aarch64" in cmake_preset:
        return "aarch64"
    return "x64"


def host_system_introspection():
    """
    Look into the system and set global system_name, architecture, and distribution.

    :return: None
    """
    global system_name, architecture, distribution
    system_name = system().lower()
    if system_name not in ["linux", "windows", "darwin"]:
        print("Error: Packaging only runs on Linux, MacOS or Windows", file=stderr)
        exit(-666)
    if system_name == "darwin":
        system_name = "macos"
    ma_mac = machine()
    if ma_mac not in ["x86_64", "AMD64"] and system_name == "macos":
        print("Error: Packaging only runs on x86_64 machines or MacOS", file=stderr)
        exit(-666)

    architecture = "x64"
    if system_name == "windows":
        from platform import win32_ver

        distribution = win32_ver()[0]
        return
    if system_name == "macos":
        from platform import mac_ver

        distribution = mac_ver()[0]
        architecture = "aarch64"
        return
    # Import must be only done on linux platform
    os_release = Path("/etc/os-release")
    if os_release.exists():
        distribution = ""
        with open(os_release) as fp:
            lines = fp.readlines()
        for line in lines:
            try:
                key, val = line.split("=", 1)
            except ValueError:
                continue
            if key == "NAME":
                distribution += str(val).strip().replace('"', "").lower()
            if key == "VERSION_ID":
                distribution += str(val).strip().replace('"', "").lower()
    else:
        print(f"WARNING: not able get distribution info", file=stderr)
        distribution = "ubuntu20.04"


def is_mobile_compilation(preset_name: str):
    """
    Check if the preset is for mobile compilation.

    :param preset_name: The preset name.
    :return: True if mobile, False otherwise.
    """
    if "android" in preset_name:
        return True
    if "ios" in preset_name:
        return True
    return False


def configure(preset: str, clean: bool):
    """
    Cmake configure with preset.

    :param preset: Name of the preset to configure.
    :param clean: If the build dir should be cleaned before build.
    :return: True if success.
    """
    if clean:
        try:
            binary_dir = buildPath / f"{preset}"
            if binary_dir.exists():
                rmtree(binary_dir)
        except Exception as err:
            print(f"Error during configure: {str(err)}")
            return False
    command = f"cmake --preset {preset}"
    if not is_mobile_compilation(preset) and distribution not in ["", None]:
        command += f' -DOWL_DISTRIBUTION="{distribution}"'
    if build_hash not in ["", None]:
        command += f' -DOWL_GIT_HASH="{build_hash}"'
    try:
        if "ios" in preset:
            res = run(command, shell=True, env=env)
        else:
            res = run(command, shell=True)
        if res.returncode != 0:
            print(f"Error during configure: code({res.returncode})", file=stderr)
            return False
    except Exception as err:
        print(f"Error during configure: {str(err)}", file=stderr)
        return False

    return True


def build(preset: str, configs: list):
    """
    Build the preset for the given configs.

    :param preset: the preset to build
    :param configs: the list of configs
    :return: True if OK
    """
    for conf in configs:
        binary_dir = buildPath / f"{preset}"
        command = f"cmake --build {binary_dir}  --config {str(conf)}"
        try:
            if "ios" in preset:
                res = run(command, shell=True, env=env)
            else:
                res = run(command, shell=True)
            if res.returncode != 0:
                print(
                    f"Error during build configuration {conf}: code({res.returncode})",
                    file=stderr,
                )
                return False
        except Exception as err:
            print(f"Error during build configuration {conf}: {str(err)}", file=stderr)
            return False
    return True


def pack(preset: str, configs: list):
    """
    Create the package for the preset and given configs.

    :param preset: the preset to build
    :param configs: the list of configs
    :return: True if OK
    """
    binary_dir = buildPath / f"{preset}"
    try:
        # remove old package
        pack_names = (
            list(binary_dir.glob("*.zip"))
            + list(binary_dir.glob("*.tgz"))
            + list(binary_dir.glob("*.tar.gz"))
        )
        for pack_name in pack_names:
            pack_name.unlink()
        chdir(binary_dir)
        command = f'cpack -C "{";".join(configs)}"'
        if "ios" in preset:
            res = run(command, shell=True, env=env)
        else:
            res = run(command, shell=True)
        if res.returncode != 0:
            chdir(sourcePath)
            return False
        # move newly created package to package dir
        pack_names = (
            list(binary_dir.glob("*.zip"))
            + list(binary_dir.glob("*.tgz"))
            + list(binary_dir.glob("*.tar.gz"))
        )
        for pack_name in pack_names:
            print(f"Move {pack_name.name} from {binary_dir} to {packagePath}")
            packagePath.mkdir(parents=True, exist_ok=True)
            (packagePath / pack_name.name).unlink(missing_ok=True)
            move(pack_name, packagePath / pack_name.name)
    except Exception as err:
        print(f"Error during packaging {preset}: {str(err)}", file=stderr)
        chdir(sourcePath)
        return False
    chdir(sourcePath)
    return True


def install(preset: str, configs: list, clean: bool):
    """
    Do an installation for the preset and given configs.

    :param preset: the preset to build
    :param configs: the list of configs
    :param clean: If the build dir should be cleaned before build.
    :return: True if OK
    """
    binary_dir = buildPath / f"{preset}"
    install_dir = installPath / f"{preset}"
    if clean:
        try:
            if install_dir.exists():
                rmtree(install_dir)
        except Exception as err:
            print(f"Error during install: {str(err)}")
            return False
    for conf in configs:
        command = (
            f"cmake --install {binary_dir}  --config {str(conf)} --prefix={install_dir}"
        )
        try:
            if "ios" in preset:
                res = run(command, shell=True, env=env)
            else:
                res = run(command, shell=True)
            if res.returncode != 0:
                print(
                    f"Error during install configuration {conf}: code({res.returncode})",
                    file=stderr,
                )
                return False
        except Exception as err:
            print(f"Error during install configuration {conf}: {str(err)}", file=stderr)
            return False
    return True


def main():
    """
    Main entry point for package build.

    :return: None
    """
    global build_hash
    host_system_introspection()
    parser = ArgumentParser()
    parser.add_argument("--preset", type=str, help="The preset name")
    parser.add_argument(
        "--no-clean", "-n", action="store_true", help="Do not delete the build folder"
    )
    parser.add_argument(
        "--install-only", "-i", action="store_true", help="Only do a install no package"
    )
    parser.add_argument("--hash", type=str, help="The current git hash.")
    args = parser.parse_args()

    if args.preset in [None, ""]:
        preset_name = "package-engine-static"
    else:
        preset_name = args.preset

    if not preset_name.startswith("package"):
        print(
            "ERROR: Preset name should start by 'package', check your configuration",
            file=stderr,
        )
        exit(2)

    clean = True
    if args.no_clean:
        clean = False
    packing = True
    if args.install_only:
        packing = False
    if args.hash not in [None, ""]:
        build_hash = args.hash

    if packing:
        print(f"Running packaging on {system_name} {distribution} {architecture}")
    else:
        print(f"Running installation on {system_name} {distribution} {architecture}")

    save = curdir
    chdir(sourcePath)
    configs = ["Debug", "Release"]
    if "app" in preset_name:
        configs = ["Release"]

    if not configure(preset_name, clean):
        chdir(save)
        exit(-666)

    if not build(preset_name, configs):
        chdir(save)
        exit(-666)

    if packing:
        if not pack(preset_name, configs):
            chdir(save)
            exit(-666)
    else:
        if not install(preset_name, configs, True):
            chdir(save)
            exit(-666)

    chdir(save)


if __name__ == "__main__":
    main()
