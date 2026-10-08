"""
Lint, type-check and tests of the CI's own Python code (`ci/`, `ci_action.py`) for the `CodeStyle` gate.

`ruff check`, `ruff format --check`, `mypy` and `pytest` (`ci/tests/`) read their configuration from
`pyproject.toml`. Their findings come
back as `(path, line, column, message)` tuples, so the gate prints them as GNU diagnostics like every other check.
"""

from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass

from ci import root

_RUFF_RE = re.compile(r"^(?P<path>[^:\s][^:]*):(?P<line>\d+):(?P<col>\d+): (?P<msg>.+)$")
_RUFF_FORMAT_RE = re.compile(r"^Would reformat: (?P<path>.+)$")
_MYPY_RE = re.compile(r"^(?P<path>[^:\s][^:]*):(?P<line>\d+): error: (?P<msg>.+)$")
_PYTEST_RE = re.compile(r"^FAILED (?P<path>[^:\s]+)::(?P<test>\S+)(?: - (?P<msg>.+))?$")


@dataclass(frozen=True)
class Finding:
    """One finding of a Python tool."""

    tool: str
    path: str
    line: int
    column: int
    message: str


def parse_ruff(output: str) -> list[Finding]:
    """
    Read the findings of `ruff check --output-format=concise`.

    :param output: The tool's standard output.
    :return: One finding per diagnostic line; summary lines are ignored.
    """
    return [
        Finding("ruff", m["path"], int(m["line"]), int(m["col"]), m["msg"])
        for m in map(_RUFF_RE.match, output.splitlines())
        if m
    ]


def parse_ruff_format(output: str) -> list[Finding]:
    """
    Read the files `ruff format --check` would rewrite.

    :param output: The tool's standard output.
    :return: One whole-file finding per file.
    """
    return [
        Finding("ruff-format", m["path"], 1, 1, "file is not formatted (run `poetry run ruff format`)")
        for m in map(_RUFF_FORMAT_RE.match, output.splitlines())
        if m
    ]


def parse_mypy(output: str) -> list[Finding]:
    """
    Read the errors of `mypy` (its notes are context, not findings).

    :param output: The tool's standard output.
    :return: One finding per error line.
    """
    return [
        Finding("mypy", m["path"], int(m["line"]), 1, m["msg"]) for m in map(_MYPY_RE.match, output.splitlines()) if m
    ]


def parse_pytest(output: str) -> list[Finding]:
    """
    Read the failed tests of `pytest -q -rf`.

    :param output: The tool's standard output.
    :return: One finding per failed test, on line 1 of its file.
    """
    return [
        Finding("pytest", m["path"], 1, 1, f"{m['test']} failed" + (f": {m['msg']}" if m["msg"] else ""))
        for m in map(_PYTEST_RE.match, output.splitlines())
        if m
    ]


def _run(args: list[str]) -> str:
    result = subprocess.run([sys.executable, "-m", *args], cwd=str(root), capture_output=True, text=True)
    return result.stdout or ""


def lint_python() -> list[Finding]:
    """
    Run the four tools over the CI code.

    :return: Every finding, empty when the code is clean.
    """
    findings = parse_ruff(_run(["ruff", "check", "--output-format=concise", "--no-fix"]))
    findings += parse_ruff_format(_run(["ruff", "format", "--check"]))
    findings += parse_mypy(_run(["mypy", "--no-error-summary"]))
    findings += parse_pytest(_run(["pytest", "-q", "-rf", "-p", "no:cacheprovider"]))
    return findings
