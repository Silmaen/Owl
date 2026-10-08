"""
Dead `NOLINT` markers for the `CodeStyle` gate: a `NOLINT(check)` naming a check `.clang-tidy` does not enable
suppresses nothing and hides what the code really needs.

The active set comes from `clang-tidy --list-checks`; `clang-analyzer-*` checks count as active since the
`ClangTidy --tool=analyzer` mode runs them. A bare `NOLINT` (no check list) is left alone.
"""

from __future__ import annotations

import fnmatch
import re
import shutil
import subprocess
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

_NOLINT_RE = re.compile(r"NOLINT(?:NEXTLINE|BEGIN|END)?\(([^)]*)\)")


@dataclass(frozen=True)
class DeadSuppression:
    """A check named by a `NOLINT` marker that clang-tidy never runs."""

    path: Path
    line: int
    check: str


def active_checks(source_root: Path) -> set[str] | None:
    """
    Checks clang-tidy enables for the repository.

    :param source_root: The repository root (holds `.clang-tidy`).
    :return: The enabled check names, or None when clang-tidy is not available.
    """
    tool = shutil.which("clang-tidy")
    if tool is None:
        return None
    probe = source_root / "source" / "owl" / "private" / "core" / "Log.cpp"
    result = subprocess.run([tool, "--list-checks", str(probe), "--"], capture_output=True, text=True)
    return {line.strip() for line in result.stdout.splitlines() if line.startswith("    ") and line.strip()}


def is_active(check: str, active: set[str]) -> bool:
    """
    Tell whether a check named by a `NOLINT` runs.

    :param check: The check name, possibly a glob (`*-magic-numbers`).
    :param active: The enabled checks.
    :return: True when the check (or one check of the glob) is enabled, or it is a static-analyzer check.
    """
    if check.startswith("clang-analyzer-"):
        return True
    if "*" in check:
        return any(fnmatch.fnmatch(name, check) for name in active)
    return check in active


def dead_suppressions(files: Iterable[Path], active: set[str]) -> list[DeadSuppression]:
    """
    Find the `NOLINT` checks no configuration enables.

    :param files: The C++ files to scan.
    :param active: The enabled checks.
    :return: One entry per dead check name, in file and line order.
    """
    found: list[DeadSuppression] = []
    for path in files:
        try:
            lines = path.read_text(errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, start=1):
            for match in _NOLINT_RE.finditer(line):
                for check in (name.strip() for name in match.group(1).split(",")):
                    if check and not is_active(check, active):
                        found.append(DeadSuppression(path, number, check))
    return found
