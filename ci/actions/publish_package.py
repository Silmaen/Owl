"""
Action to publish the packaged archives (Engine SDK, Owl Nest) to a remote server.
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
    get_platform_info,
    get_project_version,
    normalize_server_url,
    push_revision,
)
from ci.utils.secrets import get_secret, reject_secret_args

PACKAGE_TYPES: dict[str, str] = {"OwlEngine": "e", "OwlNest": "a"}
"""The archive base name of each CPack component and its revision type on the publication site."""


class PublishPackage(BaseAction):
    """
    Action to publish the packaged archives (Engine SDK, Owl Nest) to a remote server.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Publish the archive of every component CPack wrote in the preset tree.
        :param preset: The preset configuration.
        :param extra_args: Required: --url, --login (the password comes from
            OWL_DEPLOY_PASSWORD, never from the command line). Optional: --hash, --dry-run.
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

        if not preset.run_package:
            log.error(f"Preset '{preset.cmake_preset}' is not a packaged tree (vendor 'package' flag).")
            return 1

        version = get_project_version()
        if version == "Bad Version":
            log.error("Could not determine project version from CMakeLists.txt.")
            return 1
        git_hash = git_hash[:7] if git_hash else get_git_hash()
        if git_hash in ["0000000", "", None]:
            log.error("Could not determine git hash.")
            return 1
        plat = get_platform_info()

        ext = preset.archive_format or "tar.gz"
        archives = find_archives(preset.get_build_dir(), version, git_hash, ext)
        if not archives:
            log.error(f"No OwlEngine / OwlNest {version}-{git_hash} archive in {preset.get_build_dir()}.")
            return 1
        result = 0
        for base_name, package_file in archives:
            revision = Revision(
                rev_type=PACKAGE_TYPES[base_name],
                branch=version,
                file=package_file,
                hash=git_hash,
                name=" ".join(re.findall("[A-Z][^A-Z]*", base_name)),
                flavor_name=f"{plat['os']} {plat['arch']}",
                date=datetime.now().isoformat(),
            )
            log.info(f"Package info: {revision}, user={login}, url={url}")
            if dry_run:
                log.info("Dry-run mode: skipping actual publication.")
                continue
            result = push_revision(url, login, password, revision) or result
        return result


def find_archives(folder: Path, version: str, git_hash: str, ext: str) -> list[tuple[str, Path]]:
    """
    Find the archives CPack wrote for one version and commit, one per component.

    :param folder: The packaged build tree (or the folder the archives were downloaded into).
    :param version: The project version.
    :param git_hash: The abbreviated commit hash.
    :param ext: The archive extension (`tar.gz` or `zip`).
    :return: The package base name and the archive of each component found, Engine first.
    """
    found: list[tuple[str, Path]] = []
    for base_name in PACKAGE_TYPES:
        matches = sorted(folder.glob(f"{base_name}-{version}-{git_hash}-*.{ext}"))
        if matches:
            found.append((base_name, matches[-1]))
    return found
