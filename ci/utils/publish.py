"""
Utility functions for publishing packages and documentation to a remote server.
"""
import os
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


def _hash_from_teamcity_env() -> str | None:
    """Return the SHA exposed by TeamCity as BUILD_VCS_NUMBER, if present and plausible."""
    sha = os.environ.get("BUILD_VCS_NUMBER", "").strip()
    if len(sha) >= 7 and all(c in "0123456789abcdef" for c in sha.lower()):
        return sha
    return None


def _hash_from_git_refs(repo: Path) -> str | None:
    """
    Resolve HEAD by reading ``.git/HEAD`` and the matching ref file or ``packed-refs``.

    Pure text-file walk, no access to the object store — survives a stale
    ``.git/objects/info/alternates`` (typical when a workspace is reused
    across Windows and Linux TeamCity agents).
    """
    git_dir = repo / ".git"
    if not git_dir.exists():
        return None
    head = git_dir / "HEAD"
    if not head.exists():
        return None
    try:
        content = head.read_text(encoding="utf-8").strip()
    except OSError:
        return None
    # Detached HEAD: file holds the SHA directly.
    if not content.startswith("ref:"):
        return content if len(content) >= 7 else None
    ref = content.split(":", 1)[1].strip()
    # Loose ref.
    loose = git_dir / ref
    if loose.exists():
        try:
            sha = loose.read_text(encoding="utf-8").strip()
            if len(sha) >= 7:
                return sha
        except OSError:
            pass
    # Packed ref.
    packed = git_dir / "packed-refs"
    if packed.exists():
        try:
            for line in packed.read_text(encoding="utf-8").splitlines():
                if line.startswith("#") or line.startswith("^"):
                    continue
                parts = line.strip().split(" ", 1)
                if len(parts) == 2 and parts[1] == ref and len(parts[0]) >= 7:
                    return parts[0]
        except OSError:
            pass
    return None


def get_git_hash() -> str:
    """
    Retrieve the short git hash (7 chars) for the current HEAD.

    Tries, in order:
    1. ``git log -1 --format=%h`` (the normal path);
    2. TeamCity's ``BUILD_VCS_NUMBER`` env var (set on every agent);
    3. Direct read of ``.git/HEAD`` and the matching ref file
       (resilient to a broken ``.git/objects/info/alternates``,
       e.g. a Windows path on a Linux agent — see TeamCity workspace reuse).

    :return: The short git hash, or "0000000" on failure.
    """
    from subprocess import run, PIPE

    try:
        ret = run(["git", "log", "-1", "--format=%h"], stdout=PIPE, stderr=PIPE, text=True)
        if ret.returncode == 0:
            sha = ret.stdout.strip()[:7]
            if sha:
                return sha
        log.warning(
            f"git log returned {ret.returncode}, falling back to BUILD_VCS_NUMBER / .git/HEAD"
        )
    except Exception as err:
        log.warning(f"git log raised {err}, falling back to BUILD_VCS_NUMBER / .git/HEAD")

    sha = _hash_from_teamcity_env()
    if sha:
        return sha[:7]

    sha = _hash_from_git_refs(root)
    if sha:
        return sha[:7]

    log.error("Could not retrieve git hash via git, BUILD_VCS_NUMBER, or .git/HEAD.")
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
