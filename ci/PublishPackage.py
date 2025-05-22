#!/usr/bin/env python3
"""
Script to publish built packages to a remote server.
"""
import platform
import re
from datetime import datetime
from json import load
from pathlib import Path
from sys import stderr

here = Path(__file__).resolve().parent
root = here.parent
packages_folder = root / "output" / "package"
global_hash = "0000000"
dry_run = False


def get_git_hash():
    """
    Get the git hash without using the git command.

    :return: The git hash.
    """
    from subprocess import run, PIPE

    global global_hash
    if global_hash not in ["0000000", "", None]:
        return global_hash[:7]
    try:
        ret = run("git log -1 --format=%h", stdout=PIPE, shell=True)
        if ret.returncode != 0:
            print(
                f"WARNING: Error during git hash search: {ret.returncode}", file=stderr
            )
            return global_hash
        global_hash = ret.stdout.decode().strip()
        return global_hash[:7]
    except Exception as err:
        print(f"ERROR: Exception during git hash search: {err}", file=stderr)
        exit(1)


def get_version():
    """
    Get the project version from CMakeLists.txt.

    :return: The version string.
    """
    with open(root / "CMakeLists.txt", "r") as fb:
        lines = fb.readlines()
        for line in lines:
            if not line.strip().startswith("project"):
                continue
            return line.split("VERSION")[-1].strip().split()[0].strip()
    return "Bad Version"


def get_api_script(url: str):
    """
    Download the api.py script from the given URL.

    :param url: The URL to download from.
    :return: None
    """
    from requests import get

    try:
        if not url.startswith("http"):
            url = "https://" + url
        r = get(url, stream=True)
        if r.status_code != 200:
            print(
                f"ERROR http error while getting the script: {r.status_code}",
                file=stderr,
            )
            exit(1)
        with open(here / "api.py", "wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
    except Exception as err:
        print(f"ERROR while getting the script: {err}", file=stderr)
        exit(1)


def parse_args():
    """
    Parse command line arguments.

    :return: Dictionary of parsed arguments.
    """
    from argparse import ArgumentParser
    from subprocess import run

    global global_hash, dry_run
    parser = ArgumentParser()
    parser.add_argument("--url", "-u", type=str, help="The packaging server url.")
    parser.add_argument(
        "--login", "-l", type=str, help="The user for the server connexion."
    )
    parser.add_argument(
        "--cred", "-c", type=str, help="The password for server connexion."
    )
    parser.add_argument("--preset", "-p", type=str, help="The package preset.")
    parser.add_argument("--hash", type=str, help="The current git hash.")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="If we should skip the publication, but do all the check and print configurations.",
    )
    args = parser.parse_args()

    if args.url in ["", None]:
        print(f"ERROR empty server url", file=stderr)
        exit(1)
    if args.login in ["", None]:
        print(f"ERROR empty server login", file=stderr)
        exit(1)
    if args.cred in ["", None]:
        print(f"ERROR empty server password", file=stderr)
        exit(1)
    if args.preset in ["", None]:
        print(f"ERROR empty package preset", file=stderr)
        exit(1)
    if args.hash in ["", None]:
        try:
            ret = run(
                "git log -1 --format=%h",
                shell=True,
                capture_output=True,
                text=True,
                check=True,
            )
            global_hash = ret.stdout.splitlines(keepends=False)[0].strip()
        except Exception as err:
            print(f"ERROR cannot retrieve hash: {err}", file=stderr)
            exit(1)
    else:
        global_hash = args.hash[:7]
    if global_hash in ["", None]:
        print(f"ERROR empty hash", file=stderr)
        exit(1)
    if args.dry_run:
        dry_run = True
    return {
        "url": args.url,
        "login": args.login,
        "cred": args.cred,
        "preset": args.preset,
        "hash": global_hash,
    }


def get_info_from_preset(preset: str):
    """
    Get package info from the preset name.

    :param preset: The preset name.
    :return: Dictionary with info.
    """
    if not preset.startswith("package"):
        print(f"ERROR: {preset} is not a package preset.")
        exit(1)
    os_name = platform.system().replace("Darwin", "MacOS").lower()
    if os_name == "linux":
        os_name += f" glibc_{platform.libc_ver()[1]}"
    arch = (
        platform.machine()
        .lower()
        .replace("amd", "x")
        .replace("86_", "")
        .replace("arch", "rm")
        .replace("v8", "64")
    )
    info = {
        "branch": get_version(),
        "hash": get_git_hash(),
        "name": "noname",
        "type": "no_type",
        "os": os_name,
        "arch": arch,
    }
    # load the json file with preset information:
    with open(root / "cmake" / "CMakePresetsPackage.json") as js:
        data = load(js)
    presets = data["configurePresets"]
    preset_data = {}
    for p in presets:
        if p["name"] != preset:
            continue
        preset_data = p
    if preset_data == {}:
        print(f"ERROR: {preset} is not in packages list.", file=stderr)
        exit(1)
    base_name = preset_data["cacheVariables"]["OWL_PACKAGE_NAME"]
    info["name"] = " ".join(re.findall("[A-Z][^A-Z]*", base_name))
    if "app" in preset:
        info["type"] = "a"
    elif "engine" in preset:
        info["type"] = "e"

    ext = "tar.gz"
    if os_name == "windows":
        ext = "zip"
    filename = f"{base_name}-{get_version()}-{get_git_hash()}-{os_name.replace(' ', '-')}-{arch}.{ext}"
    info["file"] = filename
    info["flavor_name"] = f"{os_name} {arch}"
    info["date"] = datetime.now().isoformat()
    return info


def publish_package(info):
    """
    Publish the package.

    :param info: Dictionary with info.
    :return: None
    """
    from subprocess import run

    cmd = f"python3 -u {here}/api.py push"
    cmd += f' --type "{info["type"]}"'
    cmd += f' --hash "{info["hash"]}"'
    cmd += f' --branch "{info["branch"]}"'
    cmd += f' --name "{info["name"]}"'
    cmd += f' --flavor_name "{info["flavor_name"]}"'
    cmd += f' --date "{info["date"]}"'
    cmd += f' --file "{info["file"]}"'
    cmd += f' --user "{info["login"]}"'
    cmd += f' --passwd "{info["cred"]}"'
    cmd += f' --url "{info["url"]}"'
    try:
        ret = run(cmd, shell=True)
        if ret.returncode != 0:
            print(f"ERROR: Error during publish: {ret.returncode}")
            exit(1)
    except Exception as err:
        print(f"ERROR: Exception during publish: {err}")
        exit(1)


def check_info(info):
    """
    Check the validity of the info dictionary.

    :param info: Dictionary with info.
    :return: True if valid, False otherwise.
    """
    good = True
    if info.get("hash") in ["0000000", "", None]:
        print(f" *** BAD hash {info.get('hash')}", file=stderr)
        good = False
    if info.get("name") in ["noname", "", None]:
        print(f" *** BAD name {info.get('name')}", file=stderr)
        good = False
    if info.get("type") in ["no_type", "", None]:
        print(f" *** BAD type {info.get('type')}", file=stderr)
        good = False
    if info.get("os") in ["", None]:
        print(f" *** BAD os {info.get('os')}", file=stderr)
        good = False
    if info.get("arch") in ["", None]:
        print(f" *** BAD arch {info.get('arch')}", file=stderr)
        good = False
    if info.get("flavor_name") in ["", None]:
        print(f" *** BAD flavor_name {info.get('flavor_name')}", file=stderr)
        good = False
    if info.get("branch") in ["", None, "Bad Version"]:
        print(f" *** BAD branch {info.get('branch')}", file=stderr)
        good = False
    if info.get("file") in ["", None]:
        print(f" *** BAD file {info.get('file')}", file=stderr)
        good = False
    elif not (packages_folder / info["file"]).exists():
        print(f" *** File not exists {packages_folder / info['file']}", file=stderr)
        good = False
    else:
        info["file"] = packages_folder / info["file"]
    if info.get("date") in ["", None]:
        print(f" *** BAD date {info.get('date')}", file=stderr)
        good = False
    return good


def main():
    """
    Main entry point for package publication.

    :return: None
    """
    data = parse_args()
    get_api_script(data["url"] + "/static/scripts/api.py")
    info = get_info_from_preset(data["preset"])
    # overload infos:
    info["url"] = data["url"]
    info["login"] = data["login"]
    info["cred"] = data["cred"]
    if not check_info(info):
        print(f"ERROR: bad infos: {info}", file=stderr)
        exit(1)
    print(f"INFOS: {info}")
    if not dry_run:
        publish_package(info)


if __name__ == "__main__":
    main()
