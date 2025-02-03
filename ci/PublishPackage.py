#!/usr/bin/env python3
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


def get_git_hash():
    """
    Get the git hash without using the git command
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
    with open(root / "CMakeLists.txt", "r") as fb:
        lines = fb.readlines()
        for line in lines:
            if not line.strip().startswith("project"):
                continue
            return line.split("VERSION")[-1].strip().split()[0].strip()


def get_api_script(url: str):
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
    from argparse import ArgumentParser

    global global_hash
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
    global_hash = args.hash
    return {
        "url": args.url,
        "login": args.login,
        "cred": args.cred,
        "preset": args.preset,
        "hash": args.hash[:7],
    }


def get_info_from_preset(preset: str):
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
        "kind": "null",
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
    if "static" in preset:
        info["kind"] = f"static"
    elif "shared" in preset:
        info["kind"] = f"shared"
    ext = "tar.gz"
    if os_name == "windows":
        ext = "zip"
    if info["kind"] == "null":
        filename = f"{base_name}-{get_version()}-{get_git_hash()}-{os_name.replace(' ', '-')}-{arch}-{info['kind']}.{ext}"
        info["file"] = filename
        info["flavor_name"] = f"{os_name} {arch} {info['kind']}"
    else:
        filename = f"{base_name}-{get_version()}-{get_git_hash()}-{os_name.replace(' ', '-')}-{arch}.{ext}"
        info["file"] = filename
        info["flavor_name"] = f"{os_name} {arch}"
        info["kind"] = f"shared"
    info["date"] = datetime.now().isoformat()
    return info


def publish_package(info):
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
    if info.get("kind") in ["", None]:
        print(f" *** BAD kind {info.get('kind')}", file=stderr)
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
    data = parse_args()
    get_api_script(data["url"] + "/static/scripts/api.py")
    info = get_info_from_preset(data["preset"])
    # overload infos:
    info["url"] = data["url"]
    info["login"] = data["login"]
    info["cred"] = data["cred"]
    if data["branch"] not in ["", None]:
        info["branch"] = data["branch"]
    if not check_info(info):
        print(f"ERROR: bad infos: {info}", file=stderr)
        exit(1)
    print(f"INFOS: {info}")
    publish_package(info)


if __name__ == "__main__":
    main()
