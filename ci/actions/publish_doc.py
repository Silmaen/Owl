"""
Action to publish documentation to a remote server.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import (
    DEPLOY_PASSWORD_ENV,
    Revision,
    get_project_version,
    normalize_server_url,
    push_revision,
)
from ci.utils.secrets import get_secret, reject_secret_args


class PublishDoc(BaseAction):
    """
    Action to publish generated documentation to a remote server.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Publish documentation for the given preset.
        :param preset: The preset configuration.
        :param extra_args: Required: --url, --login (the password comes from
            OWL_DEPLOY_PASSWORD, never from the command line). Optional: --dry-run.
        :return: Exit code indicating success or failure.
        """
        log.info(f"Publishing documentation with preset: {preset.cmake_preset}")

        # Parse extra arguments
        params = self.parse_extra_args(extra_args)
        refused = reject_secret_args(params, {"password": DEPLOY_PASSWORD_ENV})
        if refused:
            log.error(refused)
            return 1
        url = params.get("url")
        login = params.get("login")
        password = get_secret(DEPLOY_PASSWORD_ENV)
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

        revision = Revision(rev_type="d", branch=version, file=doc_dir)

        log.info(f"Documentation info: {revision}, user={login}, url={url}")

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

        return push_revision(url, login, password, revision)
