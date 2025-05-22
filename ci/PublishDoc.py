#!/usr/bin/env python3
"""
Script to publish documentation packages to a remote server.
"""
from pathlib import Path
from sys import stderr

here = Path(__file__).resolve().parent
root = here.parent
build_folder = root / "output" / "build"
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
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="If we should skip the publication, but do all the check and print configurations.",
    )
    parser.add_argument(
        "--publish",
        type=str,
        help="empty: no effect, 'true': publish (override dry-run), else: don't publish",
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
    if global_hash in ["", None]:
        print(f"ERROR empty hash", file=stderr)
        exit(1)
    if args.publish:
        dry_run = str(args.publish).lower() == "true"
    else:
        if args.dry_run:
            dry_run = True
    return {
        "url": args.url,
        "login": args.login,
        "cred": args.cred,
        "preset": args.preset,
    }


def get_info_from_preset(preset: str):
    """
    Get documentation info from the preset name.

    :param preset: The preset name.
    :return: Dictionary with info.
    """
    if not preset.startswith("Documentation"):
        print(f"ERROR: {preset} is not a Documentation preset.")
        exit(1)
    info = {
        "branch": get_version(),
        "type": "d",
        "file": f"{preset}/Documentation/html",
    }
    return info


def publish_package(info):
    """
    Publish the documentation package.

    :param info: Dictionary with info.
    :return: None
    """
    from subprocess import run

    cmd = f"python3 -u {here}/api.py push"
    cmd += f' --type "{info["type"]}"'
    cmd += f' --branch "{info["branch"]}"'
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
    if info.get("type") in ["no_type", "", None]:
        print(f" *** BAD type {info.get('type')}", file=stderr)
        good = False
    if info.get("branch") in ["", None, "Bad Version"]:
        print(f" *** BAD branch {info.get('branch')}", file=stderr)
        good = False
    if info.get("file") in ["", None]:
        print(f" *** BAD file {info.get('file')}", file=stderr)
        good = False
    elif not (build_folder / info["file"]).exists():
        print(f" *** Doc folder not exists {build_folder / info['file']}", file=stderr)
        good = False
    else:
        info["file"] = build_folder / info["file"]
    return good


def main():
    """
    Main entry point for documentation publication.

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
    else:
        print("dry_run")


if __name__ == "__main__":
    main()
