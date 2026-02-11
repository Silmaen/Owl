"""
Action to publish built packages to a remote server.
"""
import re
from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import (
    download_api_script,
    get_project_version,
    get_git_hash,
    get_platform_info,
    run_api_push,
)
from datetime import datetime


class PublishPackage(BaseAction):
    """
    Action to publish built packages to a remote server.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Publish a built package for the given preset.
        :param preset: The preset configuration.
        :param extra_args: Required: --url, --login, --password. Optional: --hash, --dry-run.
        :return: Exit code indicating success or failure.
        """
        log.info(f"Publishing package with preset: {preset.cmake_preset}")

        # Parse extra arguments
        params = self.parse_extra_args(extra_args)
        url = params.get("url")
        login = params.get("login")
        password = params.get("password")
        git_hash = params.get("hash")
        dry_run = params.get("dry-run") == "true"

        # Validate required parameters
        if not url:
            log.error("Missing required parameter: --url")
            return 1
        if not login:
            log.error("Missing required parameter: --login")
            return 1
        if not password:
            log.error("Missing required parameter: --password")
            return 1

        # Validate that preset has OWL_PACKAGE_NAME
        if not preset.run_package:
            log.error(
                f"Preset '{preset.cmake_preset}' does not have OWL_PACKAGE_NAME set.")
            return 1

        # Determine package type from preset name
        preset_name = preset.cmake_preset
        if "app" in preset_name:
            pkg_type = "a"
        elif "engine" in preset_name:
            pkg_type = "e"
        else:
            log.error(f"Cannot determine package type from preset name: {preset_name}")
            return 1

        # Get version and hash
        version = get_project_version()
        if version == "Bad Version":
            log.error("Could not determine project version from CMakeLists.txt.")
            return 1

        if not git_hash:
            git_hash = get_git_hash()
        else:
            git_hash = git_hash[:7]
        if git_hash in ["0000000", "", None]:
            log.error("Could not determine git hash.")
            return 1

        # Get platform info
        plat = get_platform_info()

        # Get package name from cache variables
        base_name = preset.raw_config["cacheVariables"]["OWL_PACKAGE_NAME"]
        friendly_name = " ".join(re.findall("[A-Z][^A-Z]*", base_name))

        # Build package filename
        ext = preset.archive_format or "tar.gz"
        os_str = plat["os"].replace(" ", "-")
        filename = f"{base_name}-{version}-{git_hash}-{os_str}-{plat['arch']}.{ext}"

        # Check that the package file exists
        packages_folder = preset.get_build_dir()
        package_file = packages_folder / filename

        # Build info dict
        info = {
            "type": pkg_type,
            "hash": git_hash,
            "branch": version,
            "name": friendly_name,
            "flavor_name": f"{plat['os']} {plat['arch']}",
            "date": datetime.now().isoformat(),
            "file": str(package_file),
            "user": login,
            "passwd": password,
            "url": url,
        }

        log.info(f"Package info: {info}")

        if dry_run:
            if not package_file.exists():
                log.warning(f"Package file not found (dry-run): {package_file}")
            log.info("Dry-run mode: skipping actual publication.")
            return 0

        if not package_file.exists():
            log.error(f"Package file not found: {package_file}")
            return 1

        # Download api.py from the server
        api_script = root / "ci" / "api.py"
        if not download_api_script(url, api_script):
            return 1

        # Push the package
        return run_api_push(api_script, info)
