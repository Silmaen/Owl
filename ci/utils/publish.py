"""
Utility functions for publishing packages and documentation to a remote server.
"""
import platform
import re
from pathlib import Path

from ci import root, log


def download_api_script(url: str, dest: Path) -> bool:
    """
    Download the api.py script from the remote server.
    :param url: The base server URL.
    :param dest: Destination path for the downloaded script.
    :return: True on success, False on failure.
    """
    from requests import get

    script_url = f"{url}/static/scripts/api.py"
    if not script_url.startswith("http"):
        script_url = "https://" + script_url
    try:
        r = get(script_url, stream=True)
        if r.status_code != 200:
            log.error(f"HTTP error while downloading api.py: {r.status_code}")
            return False
        with open(dest, "wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
        return True
    except Exception as err:
        log.error(f"Error downloading api.py: {err}")
        return False


def get_project_version() -> str:
    """
    Read the project version from CMakeLists.txt.
    :return: The version string, or "Bad Version" if not found.
    """
    cmake_file = root / "CMakeLists.txt"
    if not cmake_file.exists():
        return "Bad Version"
    with open(cmake_file, "r") as f:
        for line in f:
            if not line.strip().startswith("project"):
                continue
            return line.split("VERSION")[-1].strip().split()[0].strip()
    return "Bad Version"


def get_git_hash() -> str:
    """
    Retrieve the short git hash (7 chars) for the current HEAD.
    :return: The short git hash, or "0000000" on failure.
    """
    from subprocess import run, PIPE

    try:
        ret = run(["git", "log", "-1", "--format=%h"], stdout=PIPE, text=True)
        if ret.returncode != 0:
            log.warning(f"Error during git hash retrieval: {ret.returncode}")
            return "0000000"
        return ret.stdout.strip()[:7]
    except Exception as err:
        log.error(f"Exception during git hash retrieval: {err}")
        return "0000000"


def get_platform_info() -> dict[str, str]:
    """
    Get platform information: OS name and architecture.
    :return: Dictionary with 'os' and 'arch' keys.
    """
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
    return {"os": os_name, "arch": arch}


def run_api_push(api_script: Path, args: dict[str, str]) -> int:
    """
    Call the api.py push command with the given arguments.
    :param api_script: Path to the downloaded api.py script.
    :param args: Dictionary of arguments to pass to the push command.
    :return: Exit code of the subprocess.
    """
    import sys
    from ci.utils.run import run_command

    cmd = [sys.executable, "-u", str(api_script), "push"]
    for key, value in args.items():
        cmd += [f"--{key}", str(value)]
    return run_command(cmd)
