#!/usr/bin/env python3
"""
Script to build a project inside a Docker container using CMake presets.
"""
import os
from argparse import ArgumentParser
from json import load
from pathlib import Path
from sys import stderr

here = Path(__file__).resolve().parent
root = here.parent

ccache = root / "output" / "cache" / ".ccache"
edm = root / "output" / "cache" / ".edm"
script = root / "output" / "cache" / "script"
script_file = script / "exec.sh"

default_registry = "registry.argawaen.net/builder"
supported_platform = ["linux/arm64", "linux/amd64"]


def run_cmd(cmd: str, print_out: bool = False, output: bool = False):
    """
    Run a shell command.

    :param cmd: The command to run.
    :param print_out: If True, print output to stdout.
    :param output: If True, return output.
    :return: Tuple of (return code, output string).
    """
    from subprocess import run, STDOUT, PIPE
    try:
        if print_out:
            out = run(cmd, bufsize=0, shell=True)
        else:
            out = run(cmd, bufsize=0, shell=True, stdout=PIPE, stderr=STDOUT)
        if out.returncode != 0:
            print(f"ERROR: during run '{cmd}' :\n {out.stdout}", file=stderr)
        if output:
            return out.returncode, out.stderr.decode()
        return out.returncode, ""
    except Exception as err:
        print(f"ERROR: Exception during run '{cmd}' : {err}", file=stderr)
        exit(1)


def execute(image: str, platform: str):
    """
    Execute the docker run command with the given image and platform.

    :param image: Docker image to use.
    :param platform: Platform to use.
    :return: None
    """
    cmd = f'docker run --rm -u $(id -u):$(id -g)'
    cmd += f' -v {ccache}:/.ccache'
    cmd += f' -v {edm}:/.edm'
    cmd += f' -v {script}:/script'
    cmd += f' -v {root}:/sources'
    if platform not in [None, ""]:
        cmd += f' --platform={platform}'
    cmd += f" {image}"
    cmd += f" /script/{script_file.name}"
    run_cmd(cmd, print_out=True)


def create_dir():
    """
    Create required directories for cache and scripts.

    :return: None
    """
    try:
        ccache.mkdir(parents=True, exist_ok=True)
        edm.mkdir(parents=True, exist_ok=True)
        script.mkdir(parents=True, exist_ok=True)
    except Exception as err:
        print(f"ERROR: Exception while creating dirs : {err}", file=stderr)
        exit(1)


def create_script(preset: str):
    """
    Create the shell script to be executed inside the docker container.

    :param preset: The CMake preset to use.
    :return: None
    """
    try:
        with open(script_file, "w") as sc:
            sc.write("#!/bin/bash\n\n")
            sc.write(f"cd /sources\n")
            sc.write(f"python3 -u ci/DependencyCheck.py\n")
            sc.write(f"cmake --preset {preset}\n")
            sc.write(f"cmake --build output/build/{preset}\n")
        mode = os.stat(script_file).st_mode
        mode |= (mode & 0o444) >> 2
        os.chmod(script_file, mode)

    except Exception as err:
        print(f"ERROR: Exception while creating script : {err}", file=stderr)
        exit(1)


def main():
    """
    Main entry point for building in Docker.

    :return: None
    """
    parser = ArgumentParser()
    parser.add_argument("--preset", "-p", required=True, type=str, help="The cmake preset to use")
    parser.add_argument("--image", "-i", type=str, help="The docker image to run")
    parser.add_argument("--platform", "-a", type=str, help="The docker platform")
    args = parser.parse_args()

    create_dir()
    preset = args.preset
    image = args.image
    if image in [None, ""]:
        with open(here / "dockerImages.json") as js:
            data = load(js)
            if preset in data:
                image = data[preset]["image"]
    if "/" not in image:
        image = f"{default_registry}/{image}"
    platform = args.platform
    if platform not in [None, ""] and "/" not in platform:
        platform = f"linux/{platform}"
    if platform not in [None, ""] + supported_platform:
        print(f"ERROR: unsupported platform: {platform}", file=stderr)
        exit(1)

    create_script(preset)

    execute(image, platform)


if __name__ == "__main__":
    main()
