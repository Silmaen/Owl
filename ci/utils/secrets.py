"""
Central handling of CI secrets: reading them from the environment and masking them in logs.

Rules enforced here (see `.claude/rules/python-ci.md`):

* a secret never travels in a command line (argv is visible to `ps` on the agent);
  it is read from an environment variable with :func:`get_secret`;
* every value obtained through :func:`get_secret` or :func:`register_secret` is
  replaced by :data:`MASK` in every record that goes through the `ci` logger
  (a :class:`SecretFilter` is attached to the root handlers by `setup_logging`);
* a command is always logged through :func:`redact_command`, which also masks
  the value of any sensitive flag (``--passwd``, ``--password=…``, …) even when
  the value was never registered.
"""

from __future__ import annotations

import logging
import os
import re

MASK = "******"
"""Replacement text for a masked secret."""

SENSITIVE_FLAG_RE = re.compile(r"(passw(or)?d|secret|token|api[_-]?key|credential)", re.IGNORECASE)
"""Flag names (without leading dashes) whose value is always masked in a logged command."""

_MIN_SECRET_LENGTH = 3
"""Shorter values are not registered: masking them would shred unrelated log text."""

_secrets: set[str] = set()
"""Values registered for masking, process-wide."""


def register_secret(value: str | None) -> None:
    """
    Register a value so that it is masked in every subsequent log record.

    :param value: The secret value; empty or very short values are ignored.
    """
    if value and len(value) >= _MIN_SECRET_LENGTH:
        _secrets.add(value)


def clear_secrets() -> None:
    """Forget every registered secret (used by the tests)."""
    _secrets.clear()


def get_secret(env_var: str) -> str:
    """
    Read a secret from the environment and register it for masking.

    :param env_var: Name of the environment variable holding the secret.
    :return: The secret value, or an empty string when the variable is unset.
    """
    value = os.environ.get(env_var, "")
    register_secret(value)
    return value


def redact(text: str) -> str:
    """
    Replace every registered secret found in ``text`` by :data:`MASK`.

    :param text: Any text about to be logged.
    :return: The text with the registered secrets masked.
    """
    # Longest first, so a secret containing another one is masked whole.
    for secret in sorted(_secrets, key=len, reverse=True):
        if secret in text:
            text = text.replace(secret, MASK)
    return text


def _is_sensitive_flag(arg: str) -> bool:
    """Tell whether a command-line token names a sensitive option."""
    return arg.startswith("-") and SENSITIVE_FLAG_RE.search(arg.lstrip("-")) is not None


def redact_command(command: list[str] | str) -> str:
    """
    Render a command for the log with every secret masked.

    Masks the value of sensitive flags in both ``--flag=value`` and ``--flag value``
    forms, then every registered secret wherever it appears.

    :param command: The command, as a list of arguments or a single string.
    :return: The command as one printable string, safe to log.
    """
    args = command.split() if isinstance(command, str) else [str(a) for a in command]
    out: list[str] = []
    mask_next = False
    for arg in args:
        if mask_next:
            out.append(MASK)
            mask_next = False
            continue
        if _is_sensitive_flag(arg):
            if "=" in arg:
                out.append(arg.split("=", 1)[0] + "=" + MASK)
            else:
                out.append(arg)
                mask_next = True
            continue
        out.append(arg)
    return redact(" ".join(out))


def reject_secret_args(params: dict[str, str], env_vars: dict[str, str]) -> str | None:
    """
    Refuse secrets passed as command-line extra arguments.

    :param params: The parsed extra arguments of an action.
    :param env_vars: Map of forbidden argument name to the environment variable to use instead.
    :return: An error message when a forbidden argument is present, ``None`` otherwise.
    """
    for arg, env_var in env_vars.items():
        if arg in params:
            register_secret(params[arg])
            return f"--{arg} is not accepted on the command line (visible in ps and logs); set {env_var} instead."
    return None


class SecretFilter(logging.Filter):
    """Logging filter masking every registered secret in the formatted message."""

    def filter(self, record: logging.LogRecord) -> bool:
        """
        Rewrite the record message with the secrets masked.

        :param record: The log record.
        :return: Always True (the record is kept).
        """
        if not _secrets:
            return True
        try:
            message = record.getMessage()
        except (TypeError, ValueError):
            return True
        masked = redact(message)
        if masked != message:
            record.msg = masked
            record.args = None
        return True
