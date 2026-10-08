"""
Utility functions for publishing packages and documentation to a remote server.

The upload client lives here, in the repository. It used to be an ``api.py``
downloaded from the target server at each publication and executed with the
credentials on its command line; it is now reviewed code, run in process, and
the password comes from the environment (``OWL_DEPLOY_PASSWORD``).
"""

import os
import platform
import tarfile
from dataclasses import dataclass
from pathlib import Path

from ci import log, root
from ci.utils.secrets import redact

DEPLOY_PASSWORD_ENV = "OWL_DEPLOY_PASSWORD"
"""Environment variable holding the publication server password."""

LARGE_FILE_THRESHOLD = 100 * 1024 * 1024
"""Size above which a file goes to ``/upload`` (nginx upload module) instead of ``/api``."""

UPLOAD_TIMEOUT = (30, 3600)
"""Connect and read timeouts (seconds) of an upload request."""


@dataclass
class Revision:
    """One package or documentation revision to push to the delivery server."""

    rev_type: str
    """``e`` engine package, ``a`` application package, ``d`` documentation."""
    branch: str
    """Version the revision belongs to."""
    file: Path
    """Archive to upload (a documentation directory is compacted first)."""
    hash: str = ""
    """Short git hash (packages only)."""
    name: str = ""
    """Human-readable package name (packages only)."""
    flavor_name: str = ""
    """Platform flavour, e.g. ``linux glibc_2.39 x64`` (packages only)."""
    date: str = ""
    """ISO build date (packages only)."""


def normalize_server_url(url: str) -> str | None:
    """
    Normalise the delivery server URL and enforce HTTPS.

    :param url: The URL as given (``host``, ``host/path`` or ``https://host``).
    :return: The ``https://`` URL without trailing slash, or ``None`` for any other scheme.
    """
    url = url.strip().rstrip("/")
    if "://" not in url:
        return f"https://{url}"
    if url.startswith("https://"):
        return url
    return None


def compact_directory(directory: Path, branch: str) -> Path:
    """
    Pack a documentation directory into ``Archive_<branch>.tgz`` next to it.

    :param directory: The directory to pack (its content goes at the archive root).
    :param branch: The version, used in the archive name.
    :return: Path of the archive.
    """
    output = directory.parent / f"Archive_{branch}.tgz"
    with tarfile.open(output, "w:gz") as tar:
        for item in sorted(directory.iterdir()):
            tar.add(item, arcname=item.relative_to(directory))
    return output


def push_revision(server_url: str, login: str, password: str, revision: Revision) -> int:
    """
    Upload a revision to the delivery server (multipart POST, HTTP basic auth).

    :param server_url: The server base URL; must be HTTPS (see :func:`normalize_server_url`).
    :param login: The account login.
    :param password: The account password (never logged).
    :param revision: What to upload; a documentation directory is compacted first.
    :return: 0 on success, 1 on failure.
    """
    import requests
    from requests.auth import HTTPBasicAuth
    from requests_toolbelt import MultipartEncoder

    destination = normalize_server_url(server_url)
    if destination is None:
        log.error("Publish: the server URL must use https.")
        return 1
    upload = revision.file
    if revision.rev_type == "d":
        if not upload.is_dir() or not (upload / "index.html").exists():
            log.error(f"Publish: documentation must be a directory holding index.html: {upload}.")
            return 1
        upload = compact_directory(upload, revision.branch)
    if not upload.is_file():
        log.error(f"Publish: nothing to upload at {upload}.")
        return 1
    endpoint = "api" if upload.stat().st_size < LARGE_FILE_THRESHOLD else "upload"
    try:
        with open(upload, "rb") as stream:
            encoder = MultipartEncoder(
                fields={
                    "action": "push_doc" if revision.rev_type == "d" else "push",
                    "hash": revision.hash,
                    "branch": revision.branch,
                    "name": revision.name,
                    "flavor_name": revision.flavor_name,
                    "date": revision.date,
                    "rev_type": revision.rev_type,
                    "package": (upload.name, stream, "application/octet-stream"),
                }
            )
            response = requests.post(
                f"{destination}/{endpoint}",
                auth=HTTPBasicAuth(login, password),
                data=encoder,
                headers={"Content-Type": encoder.content_type},
                timeout=UPLOAD_TIMEOUT,
            )
    except Exception as err:
        log.error(redact(f"Publish: upload to {destination} failed: {type(err).__name__}: {err}"))
        return 1
    body = redact(response.content.decode("utf-8", errors="replace")[:2000])
    if response.status_code == 201:
        log.warning(f"Publish: server answered {response.status_code} {response.reason}: {body}")
        return 0
    if response.status_code != 200:
        log.error(f"Publish: server answered {response.status_code} {response.reason}: {body}")
        return 1
    log.info(f"Publish: {upload.name} uploaded to {destination}/{endpoint}.")
    return 0


def get_project_version() -> str:
    """
    Read the project version from CMakeLists.txt.
    :return: The version string, or "Bad Version" if not found.
    """
    cmake_file = root / "CMakeLists.txt"
    if not cmake_file.exists():
        return "Bad Version"
    with open(cmake_file) as f:
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
    from subprocess import run

    try:
        ret = run(["git", "log", "-1", "--format=%h"], capture_output=True, text=True)
        if ret.returncode == 0:
            sha = ret.stdout.strip()[:7]
            if sha:
                return sha
        log.warning(f"git log returned {ret.returncode}, falling back to BUILD_VCS_NUMBER / .git/HEAD")
    except Exception as err:
        log.warning(f"git log raised {err}, falling back to BUILD_VCS_NUMBER / .git/HEAD")

    env_sha = _hash_from_teamcity_env()
    if env_sha:
        return env_sha[:7]

    ref_sha = _hash_from_git_refs(root)
    if ref_sha:
        return ref_sha[:7]

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
    arch = platform.machine().lower().replace("amd", "x").replace("86_", "").replace("arch", "rm").replace("v8", "64")
    return {"os": os_name, "arch": arch}
