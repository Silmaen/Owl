"""
Helper for registering a DepManager remote on a CI agent.

The CI build steps run on freshly-provisioned agents that don't yet have a
`~/.edm/config.yaml`. TeamCity passes the remote URL + credentials as build
parameters, and the build / configure step calls into here to register the
remote with `depmanager remote add` before any CMake configure runs.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from ci import log
from ci.utils.run import run_command


@dataclass
class RemoteConfig:
    """Remote-registration parameters parsed from extra args.

    All four fields are optional; when ``url`` is empty the caller treats the
    configuration as a no-op (the agent already has a remote set up). When
    ``url`` is set, the remote is registered with ``depmanager remote add``
    and marked default so subsequent CMake configures pull from it.
    """

    url: str = ""
    login: str = ""
    passwd: str = ""
    name: str = "default"


def parse_remote_args(parsed: dict[str, str]) -> RemoteConfig:
    """Pull the remote-related keys out of a ``parse_extra_args`` dict.

    Accepted keys (any order, ``=`` syntax):
      * ``remote_url`` -- mandatory to trigger registration
      * ``remote_login`` / ``remote_passwd`` -- optional credentials
      * ``remote_name`` -- optional alias (defaults to ``default``)

    :param parsed: The dict returned by ``BaseAction.parse_extra_args``.
    :return: A populated :class:`RemoteConfig`.
    """

    return RemoteConfig(
        url=parsed.get("remote_url", ""),
        login=parsed.get("remote_login", ""),
        passwd=parsed.get("remote_passwd", ""),
        name=parsed.get("remote_name", "default"),
    )


def configure_remote(config: RemoteConfig) -> int:
    """Register the remote described by ``config`` via ``depmanager``.

    Idempotent on the URL side: ``depmanager remote add`` overwrites an
    existing entry of the same name, so a second invocation simply refreshes
    the URL / credentials. The remote is always marked default so
    ``cmake/Depmanager.cmake`` can pull from it without further hints.

    :param config: The parameters parsed from the action's extra args.
    :return: Exit code (0 on success, non-zero if the depmanager invocation
        failed). When ``config.url`` is empty the call is a logged no-op so
        callers can chain it unconditionally.
    """

    if not config.url:
        log.info(
            "configure_remote: no remote_url supplied — skipping remote registration."
        )
        return 0

    safe_passwd = "<redacted>" if config.passwd else "<empty>"
    log.info(
        f"configure_remote: registering DepManager remote '{config.name}' "
        f"at {config.url} (login={config.login or '<empty>'}, passwd={safe_passwd})."
    )
    # Long-form flags only — older depmanager builds bundled on some CI agents
    # reject the short aliases ("-u", "-n", …) even though `--help` lists
    # them. The long forms have been supported since the first depmanager
    # release.
    cmd = [
        "poetry",
        "run",
        "depmanager",
        "remote",
        "add",
        "--name",
        config.name,
        "--url",
        config.url,
        "--default",
    ]
    if config.login:
        cmd += ["--login", config.login]
    if config.passwd:
        cmd += ["--passwd", config.passwd]
    exit_code = run_command(cmd)
    if exit_code != 0:
        log.error(
            f"configure_remote: 'depmanager remote add' failed with exit code {exit_code}."
        )
        return exit_code
    log.info("configure_remote: remote registered successfully.")
    return 0


def configure_remote_from_args(
    extra_args: Optional[list[str]],
    parse_extra_args: callable,
) -> int:
    """Convenience wrapper: parse the extra args and register the remote.

    Used by actions that wish to opportunistically auto-configure the remote
    before doing their primary work (Build does this so a TeamCity agent can
    boot fresh and still pull dependencies).

    :param extra_args: The raw ``--`` arguments from the CLI.
    :param parse_extra_args: The static parser inherited from ``BaseAction``.
    :return: 0 on success or no-op; non-zero on registration failure.
    """

    return configure_remote(parse_remote_args(parse_extra_args(extra_args)))
