"""
The game runner archives of a release and the list of them each Owl Nest package carries (`runners.json`).

A runner archive holds, at its root, the files the exporter takes from a runner directory: the runner and its shared
libraries (desktop), the page, script and module of the Web runner, or the stripped libraries of the Android APK
(`lib/arm64-v8a/`). Owl Nest downloads the ones of its own release and checks them against `runners.json`.
"""

import hashlib
import tarfile
import zipfile
from pathlib import Path

RUNNER_BASE_NAME = "OwlGameRunner"
"""The base name of a runner archive: `OwlGameRunner-<version>-<hash>-<target>.<ext>`."""

TARGETS: dict[str, str] = {
    "linux-x64": "tar.gz",
    "linux-arm64": "tar.gz",
    "windows-x64": "tar.gz",
    "android-arm64": "tar.gz",
    "web": "tar.gz",
}
"""Every export target and the format of its runner archive: tar.gz, the one format Owl Nest unpacks."""

MANIFEST_NAME = "runners.json"
"""The list of the runner archives of a release, in the `bin/` folder of each Owl Nest package."""

WEB_RUNNER_FILES = ("OwlGameRunner.html", "OwlGameRunner.js", "OwlGameRunner.wasm")
"""The files of the Web runner (`GameExporter`)."""

DOWNLOAD_TIMEOUT = 300.0
"""The timeout in seconds of the download of a runner archive."""

ANDROID_LIBRARIES = Path("android-runner")
"""The folder of the build tree where `OwlGameRunnerLibraries` writes the stripped libraries of the APK."""


def archive_name(version: str, git_hash: str, target: str) -> str:
    """
    Name of the runner archive of a target.

    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :param target: One of :data:`TARGETS`.
    :return: `OwlGameRunner-<version>-<hash>-<target>.<ext>`.
    """
    return f"{RUNNER_BASE_NAME}-{version}-{git_hash}-{target}.{TARGETS[target]}"


def download_runner(
    server: str, version: str, git_hash: str, target: str, dest_dir: Path, timeout: float = DOWNLOAD_TIMEOUT
) -> Path | None:
    """
    Download the runner archive of a target from the delivery server (public GET, no authentication).

    :param server: The delivery server base URL.
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :param target: One of :data:`TARGETS`.
    :param dest_dir: The folder receiving the archive.
    :param timeout: The request timeout in seconds.
    :return: The downloaded archive, or None when the download failed.
    """
    import requests

    from ci import log
    from ci.utils.secrets import redact

    name = archive_name(version, git_hash, target)
    url = f"{server.rstrip('/')}/runners/{version}/{git_hash}/{name}"
    try:
        # The server answers 403 to any user agent but the one of Owl Nest.
        response = requests.get(url, headers={"User-Agent": f"OwlNest/{version}"}, timeout=timeout)
    except Exception as err:
        log.error(redact(f"Runners: download of {url} failed: {type(err).__name__}: {err}"))
        return None
    if response.status_code != 200:
        log.error(redact(f"Runners: {url} answered {response.status_code} {response.reason}."))
        return None
    dest_dir.mkdir(parents=True, exist_ok=True)
    archive = dest_dir / name
    archive.write_bytes(response.content)
    return archive


def target_of(archive: Path, version: str, git_hash: str) -> str | None:
    """
    Target of a runner archive, read from its name.

    :param archive: A runner archive.
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :return: The target, or None when the name is not the one of a runner archive of this release.
    """
    for target in TARGETS:
        if archive.name == archive_name(version, git_hash, target):
            return target
    return None


def is_shared_library(path: Path) -> bool:
    """
    Whether a file of a desktop runner directory goes with the runner, as `GameExporter` copies them.

    :param path: A file of the `bin/` folder.
    :return: True for a `.so`, `.so.N` or `.dll` that is not the shader compiler of Owl Nest.
    """
    name = path.name
    if not (path.suffix in (".so", ".dll") or ".so." in name):
        return False
    return not (name.startswith("slang") or name.startswith("libslang"))


def runner_files(bin_dir: Path, target: str) -> list[tuple[Path, str]]:
    """
    Files of the runner of a target in a build tree, with their path in the archive.

    :param bin_dir: The `bin/` folder of the build tree.
    :param target: One of :data:`TARGETS`.
    :return: The files and their archive names, empty when the runner is missing.
    """
    if target == "web":
        files = [bin_dir / name for name in WEB_RUNNER_FILES]
        return [(file, file.name) for file in files] if all(file.is_file() for file in files) else []
    if target == "android-arm64":
        stripped = bin_dir.parent / ANDROID_LIBRARIES
        return [(file, file.relative_to(stripped).as_posix()) for file in sorted(stripped.rglob("*.so"))]
    runner = bin_dir / (f"{RUNNER_BASE_NAME}.exe" if target == "windows-x64" else RUNNER_BASE_NAME)
    if not runner.is_file():
        return []
    libraries = sorted(
        file for file in bin_dir.iterdir() if (file.is_file() or file.is_symlink()) and is_shared_library(file)
    )
    return [(runner, runner.name)] + [(file, file.name) for file in libraries]


def write_archive(files: list[tuple[Path, str]], output: Path) -> None:
    """
    Write an archive, a tar.gz or a zip after its extension; symbolic links stay links in a tar.gz.

    :param files: The files and their names in the archive.
    :param output: The archive to write.
    """
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.name.endswith(".zip"):
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
            for file, name in files:
                archive.write(file, name)
        return
    with tarfile.open(output, "w:gz") as archive:
        for file, name in files:
            archive.add(file, name, recursive=False)


def sha256_of(path: Path) -> str:
    """
    SHA-256 of a file.

    :param path: The file.
    :return: The hexadecimal digest.
    """
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def make_manifest(archives: list[Path], version: str, git_hash: str) -> dict:
    """
    The `runners.json` of a release: name, size and SHA-256 of each runner archive, by target.

    :param archives: The runner archives of the release.
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :return: The manifest, ready for `json.dump`.
    """
    runners = {}
    for archive in archives:
        target = target_of(archive, version, git_hash)
        if target is not None:
            runners[target] = {"file": archive.name, "size": archive.stat().st_size, "sha256": sha256_of(archive)}
    return {"version": version, "hash": git_hash, "runners": dict(sorted(runners.items()))}


def nest_root(archive: Path) -> str:
    """
    The top folder of an Owl Nest archive, as CPack writes it (the archive name without its extension).

    :param archive: An Owl Nest archive.
    :return: The name of its top folder.
    """
    name = archive.name
    return name.removesuffix(".zip") if name.endswith(".zip") else name.removesuffix(".tar.gz")


def add_to_nest(nest: Path, additions: list[tuple[bytes, str]], output: Path) -> None:
    """
    Copy an Owl Nest archive with more files in it.

    :param nest: The Owl Nest archive CPack wrote.
    :param additions: The contents and their path under the top folder of the archive.
    :param output: The archive to write (another path than `nest`).
    """
    root = nest_root(nest)
    output.parent.mkdir(parents=True, exist_ok=True)
    if nest.name.endswith(".zip"):
        with zipfile.ZipFile(nest) as source, zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as target:
            for item in source.infolist():
                target.writestr(item, source.read(item.filename))
            for content, name in additions:
                target.writestr(f"{root}/{name}", content)
        return
    import io

    with tarfile.open(nest, "r:gz") as source, tarfile.open(output, "w:gz") as target:
        for member in source.getmembers():
            target.addfile(member, source.extractfile(member) if member.isfile() else None)
        for content, name in additions:
            info = tarfile.TarInfo(f"{root}/{name}")
            info.size = len(content)
            info.mode = 0o644
            target.addfile(info, io.BytesIO(content))


def web_runner_of(archive: Path) -> list[tuple[bytes, str]]:
    """
    The files of the Web runner archive, to put in the `bin/web/` folder of an Owl Nest package.

    :param archive: The Web runner archive.
    :return: The contents and their path under the top folder of a Nest archive.
    """
    files = []
    with tarfile.open(archive, "r:gz") as source:
        for name in WEB_RUNNER_FILES:
            member = source.extractfile(name)
            if member is None:
                raise FileNotFoundError(f"{name} in {archive}")
            files.append((member.read(), f"bin/web/{name}"))
    return files
