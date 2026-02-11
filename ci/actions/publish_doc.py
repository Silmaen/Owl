"""
Action to publish documentation to a remote server.
"""

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import (
    download_api_script,
    get_project_version,
    run_api_push,
)


class PublishDoc(BaseAction):
    """
    Action to publish generated documentation to a remote server.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Publish documentation for the given preset.
        :param preset: The preset configuration.
        :param extra_args: Required: --url, --login, --password. Optional: --dry-run.
        :return: Exit code indicating success or failure.
        """
        log.info(f"Publishing documentation with preset: {preset.cmake_preset}")

        # Parse extra arguments
        params = self.parse_extra_args(extra_args)
        url = params.get("url")
        login = params.get("login")
        password = params.get("password")
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

        # Determine documentation directory
        build_dir = preset.get_build_dir()
        if preset.release_preset not in [None, ""]:
            build_dir = preset.get_release_build_dir()
        doc_dir = build_dir / "Documentation" / "html"

        # Get version
        version = get_project_version()
        if version == "Bad Version":
            log.error("Could not determine project version from CMakeLists.txt.")
            return 1

        # Build info dict
        info = {
            "type": "d",
            "branch": version,
            "file": str(doc_dir),
            "user": login,
            "passwd": password,
            "url": url,
        }

        log.info(f"Documentation info: {info}")

        if dry_run:
            if not doc_dir.exists():
                log.warning(f"Documentation directory not found (dry-run): {doc_dir}")
            elif not (doc_dir / "index.html").exists():
                log.warning(f"index.html not found in documentation directory (dry-run): {doc_dir}")
            log.info("Dry-run mode: skipping actual publication.")
            return 0

        if not doc_dir.exists():
            log.error(f"Documentation directory not found: {doc_dir}")
            return 1
        if not (doc_dir / "index.html").exists():
            log.error(f"index.html not found in documentation directory: {doc_dir}")
            return 1

        # Download api.py from the server
        api_script = root / "ci" / "api.py"
        if not download_api_script(url, api_script):
            return 1

        # Push the documentation
        return run_api_push(api_script, info)
