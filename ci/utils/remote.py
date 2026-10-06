"""
Helper for registering a DepManager remote on a CI agent.

The CI build steps run on freshly-provisioned agents that don't yet have a
`~/.edm/config.yaml`. TeamCity passes the remote URL and login as extra
arguments and the password through the ``OWL_REMOTE_PASSWORD`` environment
variable; the build / configure step calls into here to register the remote
before any CMake configure runs.

The registration goes through DepManager's Python API, in process: the
password never appears in a command line (visible to ``ps``) nor in the log.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Optional

from ci import log
from ci.utils.secrets import get_secret, reject_secret_args

REMOTE_PASSWORD_ENV = "OWL_REMOTE_PASSWORD"
"""Environment variable holding the DepManager remote password."""


@dataclass
class RemoteConfig:
    """Remote-registration parameters.

    All fields are optional; when ``url`` is empty the caller treats the
    configuration as a no-op (the agent already has a remote set up). When
    ``url`` is set, the remote is registered and marked default so subsequent
    CMake configures pull from it. ``error`` is set when the arguments are
    refused (a password passed on the command line).
    """

    url: str = ""
    login: str = ""
    passwd: str = ""
    name: str = "default"
    error: str = ""


def parse_remote_args(parsed: dict[str, str]) -> RemoteConfig:
    """Pull the remote-related keys out of a ``parse_extra_args`` dict.

    Accepted keys (any order, ``=`` syntax):
      * ``remote_url`` -- mandatory to trigger registration
      * ``remote_login`` -- optional login
      * ``remote_name`` -- optional alias (defaults to ``default``)

    The password is read from ``OWL_REMOTE_PASSWORD``; ``--remote_passwd`` is refused.

    :param parsed: The dict returned by ``BaseAction.parse_extra_args``.
    :return: A populated :class:`RemoteConfig`.
    """

    error = reject_secret_args(parsed, {"remote_passwd": REMOTE_PASSWORD_ENV})
    return RemoteConfig(
        url=parsed.get("remote_url", ""),
        login=parsed.get("remote_login", ""),
        passwd=get_secret(REMOTE_PASSWORD_ENV),
        name=parsed.get("remote_name", "default"),
        error=error or "",
    )


def _add_remote(config: RemoteConfig) -> int:
    """Register the remote through DepManager's Python API.

    :param config: The remote parameters.
    :return: 0 on success, non-zero on failure.
    """
    from depmanager.command.remote import RemoteCommand

    try:
        RemoteCommand().add(config.name, config.url, True, config.login, config.passwd)
    except SystemExit as err:
        # depmanager reports an invalid URL with log.fatal + exit().
        return err.code if isinstance(err.code, int) and err.code != 0 else 1
    except Exception as err:
        log.error(f"configure_remote: depmanager raised {type(err).__name__}: {err}")
        return 1
    return 0


def configure_remote(config: RemoteConfig) -> int:
    """Register the remote described by ``config`` via ``depmanager``.

    Idempotent on the URL side: adding a remote overwrites an existing entry of
    the same name, so a second invocation simply refreshes the URL /
    credentials. The remote is always marked default so
    ``cmake/Depmanager.cmake`` can pull from it without further hints.

    :param config: The parameters parsed from the action's extra args.
    :return: Exit code (0 on success, non-zero if the registration failed or
        the arguments were refused). When ``config.url`` is empty the call is
        a logged no-op so callers can chain it unconditionally.
    """

    if config.error:
        log.error(f"configure_remote: {config.error}")
        return 1
    if not config.url:
        log.info(
            "configure_remote: no remote_url supplied — skipping remote registration."
        )
        return 0

    safe_passwd = "<set>" if config.passwd else "<empty>"
    log.info(
        f"configure_remote: registering DepManager remote '{config.name}' "
        f"at {config.url} (login={config.login or '<empty>'}, passwd={safe_passwd})."
    )
    exit_code = _add_remote(config)
    if exit_code != 0:
        log.error(
            f"configure_remote: DepManager remote registration failed with exit code {exit_code}."
        )
        return exit_code
    log.info("configure_remote: remote registered successfully.")
    return 0


def configure_remote_from_args(
    extra_args: Optional[list[str]],
    parse_extra_args: Callable[[Optional[list[str]]], dict[str, str]],
) -> int:
    """Convenience wrapper: parse the extra args and register the remote.

    :param extra_args: The raw ``--`` arguments from the CLI.
    :param parse_extra_args: The static parser inherited from ``BaseAction``.
    :return: 0 on success or no-op; non-zero on registration failure.
    """

    return configure_remote(parse_remote_args(parse_extra_args(extra_args)))
