"""
Action to publish the packaged archives (Engine SDK, Owl Nest, game runner) to a remote server.
"""

import re
from datetime import datetime
from pathlib import Path

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import (
    DEPLOY_PASSWORD_ENV,
    Revision,
    get_git_hash,
    get_project_version,
    normalize_server_url,
    push_revision,
)
from ci.utils.runners import RUNNER_BASE_NAME, TARGETS, archive_name, target_of
from ci.utils.secrets import get_secret, reject_secret_args

PACKAGE_TYPES: dict[str, str] = {"OwlEngine": "e", "OwlNest": "a", RUNNER_BASE_NAME: "r"}
"""The archive base name of each CPack component and its revision type on the publication site."""

ARCHIVE_EXTENSIONS = ("tar.gz", "zip")
"""The formats CPack writes: tar.gz on Linux, zip on Windows."""


class PublishPackage(BaseAction):
    """
    Action to publish the packaged archives (Engine SDK, Owl Nest, game runner) to a remote server.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Publish the archive of every component CPack wrote in the preset tree.
        :param preset: The preset configuration.
        :param extra_args: Required: --url, --login (the password comes from
            OWL_DEPLOY_PASSWORD, never from the command line). Optional: --hash, --dry-run, --folder (where the
            archives are, the preset tree by default), --components (comma separated base names, all by default).
        :return: Exit code indicating success or failure.
        """
        log.info(f"Publishing package with preset: {preset.cmake_preset}")

        # Parse extra arguments
        params = self.parse_extra_args(extra_args)
        refused = reject_secret_args(params, {"password": DEPLOY_PASSWORD_ENV})
        if refused:
            log.error(refused)
            return 1
        url = params.get("url")
        login = params.get("login")
        password = get_secret(DEPLOY_PASSWORD_ENV)
        git_hash = params.get("hash")
        dry_run = params.get("dry-run") == "true"

        # Validate required parameters
        if not url:
            log.error("Missing required parameter: --url")
            return 1
        if normalize_server_url(url) is None:
            log.error("The publication URL must use https.")
            return 1
        if not login:
            log.error("Missing required parameter: --login")
            return 1
        if not password and not dry_run:
            log.error(f"Missing publication password: set {DEPLOY_PASSWORD_ENV}.")
            return 1

        if not params.get("folder") and not preset.run_package and not preset.runner_target:
            log.error(f"Preset '{preset.cmake_preset}' is not a packaged tree (vendor 'package' or 'runner').")
            return 1
        folder = Path(params["folder"]) if params.get("folder") else preset.get_build_dir()
        components = params["components"].split(",") if params.get("components") else list(PACKAGE_TYPES)
        if unknown := [name for name in components if name not in PACKAGE_TYPES]:
            log.error(f"Unknown components: {', '.join(unknown)}.")
            return 1

        version = get_project_version()
        if version == "Bad Version":
            log.error("Could not determine project version from CMakeLists.txt.")
            return 1
        git_hash = git_hash[:7] if git_hash else get_git_hash()
        if git_hash in ["0000000", "", None]:
            log.error("Could not determine git hash.")
            return 1
        archives = [found for found in find_archives(folder, version, git_hash) if found[0] in components]
        if not archives:
            log.error(f"No {' / '.join(components)} {version}-{git_hash} archive in {folder}.")
            return 1
        result = 0
        for base_name, package_file in archives:
            revision = Revision(
                rev_type=PACKAGE_TYPES[base_name],
                branch=version,
                file=package_file,
                hash=git_hash,
                name=" ".join(re.findall("[A-Z][^A-Z]*", base_name)),
                flavor_name=archive_flavor(package_file, base_name, version, git_hash),
                date=datetime.now().isoformat(),
            )
            log.info(f"Package info: {revision}, user={login}, url={url}")
            if dry_run:
                log.info("Dry-run mode: skipping actual publication.")
                continue
            result = push_revision(url, login, password, revision) or result
        return result


def find_archives(folder: Path, version: str, git_hash: str) -> list[tuple[str, Path]]:
    """
    Find the archives CPack wrote for one version and commit, one per component.

    :param folder: The packaged build tree (or the folder the archives were downloaded into).
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :return: The package base name and the archive of each component found, the runners first, then Engine; a
        folder holding the archives of several package jobs gives one archive per platform.
    """
    found: list[tuple[str, Path]] = []
    for target in TARGETS:
        if (runner := folder / archive_name(version, git_hash, target)).is_file():
            found.append((RUNNER_BASE_NAME, runner))
    for base_name in ("OwlEngine", "OwlNest"):
        for ext in ARCHIVE_EXTENSIONS:
            found.extend(
                (base_name, match) for match in sorted(folder.glob(f"{base_name}-{version}-{git_hash}-*.{ext}"))
            )
    return found


def archive_flavor(archive: Path, base_name: str, version: str, git_hash: str) -> str:
    """
    Platform of an archive as the publication site names it, read from the archive name.

    CPack names the archive after the target (`linux-glibc_2.43-arm64`), which differs from the machine running the
    publication when the tree is cross compiled. A runner archive keeps its target as it is (`android-arm64`).

    :param archive: The archive CPack wrote.
    :param base_name: The package base name (`OwlEngine`, `OwlNest`).
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :return: The platform words of the archive name, e.g. `linux glibc_2.43 arm64`.
    """
    if base_name == RUNNER_BASE_NAME:
        return target_of(archive, version, git_hash) or ""
    platform_part = archive.name.removeprefix(f"{base_name}-{version}-{git_hash}-")
    for ext in ARCHIVE_EXTENSIONS:
        platform_part = platform_part.removesuffix(f".{ext}")
    return platform_part.replace("-", " ")
