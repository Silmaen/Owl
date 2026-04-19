"""
Poetry virtualenv integrity checks.

The CI may reuse a venv across jobs to save the 30–60 s cost of `poetry sync`, but the cache
path isn't arch-aware: if two agents (x86_64 + ARM64) share `$HOME/.cache/pypoetry/` — or a
bind-mounted workspace holding the venv — the ARM64 job ends up loading wheels compiled for
x86_64 and crashes at import (classic case: `cryptography/_rust.abi3.so: cannot open shared
object file`).

This module detects the situation by attempting the *exact* import that depmanager's boot
chain requires.  Running inside the target venv (we're invoked via `poetry run python3
ci_action.py`), an `ImportError` proves the venv is broken for this host — `ci_action.py`
then exports `OWL_CI_REFRESH_VENV=1`, which `cmake/Poetry.cmake` consumes to `poetry env
remove --all` + `poetry sync` and (re)stamp the venv with a platform marker.

The marker is informational — a fast visual cue in the venv directory about what host built
it — and lets `needs_refresh` answer without importing anything when it already matches.
"""
from __future__ import annotations

import platform
from pathlib import Path


MARKER_FILENAME = ".owl_platform"


def current_platform_signature() -> str:
    """
    :return: A short platform fingerprint — `<arch>-<os>-<impl>-<pyver>` — suitable for diffing
        venvs across runs.
    """
    return "-".join([
        platform.machine() or "unknown-arch",
        platform.system() or "unknown-os",
        platform.python_implementation(),
        platform.python_version(),
    ])


def get_poetry_venv_path() -> Path | None:
    """
    :return: The current Python's venv path when running inside a virtualenv, else None.
        Derived from `sys.prefix` vs `sys.base_prefix` so it doesn't spawn `poetry` (which can
        take tens of seconds to start on a cold ARM64 container).
    """
    import sys

    prefix = Path(sys.prefix)
    base = Path(sys.base_prefix)
    if prefix == base:
        return None
    return prefix


def platform_matches_marker(venv_path: Path) -> bool:
    """
    :param venv_path: The venv root (from :func:`get_poetry_venv_path`).
    :return: True when a marker file exists inside `venv_path` and records the current platform
        signature — in which case the venv is known-good for this host and no further check is
        needed.  False when the marker is missing, unreadable, or records a different platform.
    """
    marker = venv_path / MARKER_FILENAME
    if not marker.is_file():
        return False
    try:
        recorded = marker.read_text(encoding="utf-8").strip()
    except OSError:
        return False
    return recorded == current_platform_signature()


def venv_import_broken() -> bool:
    """
    Try the exact import that depmanager's boot chain fails on when the venv contains wheels
    built for a different architecture.  We run the import in the *current* Python process
    (which is already inside the target venv thanks to `poetry run python3 ci_action.py`), so
    the check is instantaneous and free of subprocess timeouts.

    :return: True when the import raises `ImportError` (venv is broken for this host), False
        when it succeeds.
    """
    try:
        import cryptography.fernet  # noqa: F401
    except ImportError:
        return True
    return False


def write_marker(venv_path: Path) -> None:
    """
    Persist the current platform signature inside the venv so the next run's
    :func:`platform_matches_marker` can short-circuit without attempting the import.

    Called by `cmake/Poetry.cmake` after every successful `poetry sync --no-root`.  Silent on
    I/O errors — a missing marker just means the next run will fall back to the import test.
    """
    try:
        (venv_path / MARKER_FILENAME).write_text(current_platform_signature(), encoding="utf-8")
    except OSError:
        pass


def needs_refresh() -> bool:
    """
    Decide whether the existing Poetry venv should be purged and re-synced.

    1. Not inside a venv → False (nothing to refresh; `poetry sync` will create one).
    2. Marker recorded and matches the current platform → False (fast path, no import tried).
    3. Otherwise, attempt the critical import in this very process: on `ImportError` the venv
       is broken for this host and we signal the caller to force a refresh.

    :return: True when the caller should set `OWL_CI_REFRESH_VENV=1` so `cmake/Poetry.cmake`
        runs `poetry env remove --all` before the next sync.
    """
    venv_path = get_poetry_venv_path()
    if venv_path is None:
        return False
    if platform_matches_marker(venv_path):
        return False
    return venv_import_broken()


def write_marker(venv_path: Path) -> None:
    """
    Persist the current platform signature into the venv so the next run can compare against it.
    Silent on I/O errors — a missing marker just means the next run will refresh defensively.
    """
    try:
        (venv_path / MARKER_FILENAME).write_text(current_platform_signature(), encoding="utf-8")
    except OSError:
        pass
