"""
Action to register a DepManager remote on a CI agent before any CMake step.
"""

from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.remote import configure_remote, parse_remote_args


class ConfigureRemote(BaseAction):
    """Register a DepManager remote from TeamCity-supplied parameters.

    Extra arguments (after ``--``):
      * ``--remote_url=<url>`` — required; e.g. ``srvs://package.argawaen.net``.
        When omitted the action logs a no-op and returns 0 so the step can be
        wired into every build without breaking those that don't need it.
      * ``--remote_login=<login>`` — optional username for authenticated pulls.
      * ``--remote_name=<name>`` — optional remote alias (default: ``default``).

    The optional password is read from the ``OWL_REMOTE_PASSWORD`` environment
    variable (encrypted at rest by depmanager in ``~/.edm/config.yaml``);
    ``--remote_passwd`` is refused, a secret never goes on a command line.

    Typical TeamCity invocation::

        OWL_REMOTE_PASSWORD='%remote_passwd%' \\
        poetry run python ci_action.py ConfigureRemote linux-clang-release \\
            -- --remote_url=%remote_url% --remote_login=%remote_login%

    The action ignores the ``preset`` argument — it is required only because
    every action takes one, and keeping the signature uniform lets TeamCity
    reuse the same wrapper script as every other step.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """Register the DepManager remote described by ``extra_args``.

        :param preset: Unused (kept for uniformity with other actions).
        :param extra_args: ``--remote_url`` / ``--remote_login`` /
            ``--remote_name`` parameters.
        :return: 0 on success or when no URL was supplied; non-zero when
            the registration failed or a password was passed as an argument.
        """

        _ = preset  # action is preset-agnostic
        config = parse_remote_args(self.parse_extra_args(extra_args))
        return configure_remote(config)
