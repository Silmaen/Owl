"""
GitLab Code Quality report: the channel through which a finding reaches the merge request.

Every CI action prints its findings as `path:line:column: level: message`; this module turns the same lines into the
JSON GitLab shows on the merge request (`output/code-quality/<name>.json`, collected by `.gitlab-ci.yml`).
"""

from __future__ import annotations

import json
import re
from hashlib import sha256
from pathlib import Path

from ci import log, root

FINDING = re.compile(
    r"^(?P<path>(?:[A-Za-z]:)?[^:]+):(?P<line>\d+):(?P<column>\d+): "
    r"(?P<level>warning|error|fatal error): (?P<message>.*)$"
)
"""The diagnostic shape; the optional drive letter keeps a Windows path in one piece."""

TRAILING_CHECK = re.compile(r"\[(?P<check>[^\]]+)\]$")
"""clang-tidy and clang-format end a diagnostic with the check that raised it."""

LEADING_CHECK = re.compile(r"^(?P<check>[a-z][a-z-]*): ")
"""CodeStyle starts the message with its sub-check."""

SEVERITY = {"fatal error": "critical", "error": "major", "warning": "minor"}
"""Code Quality severity of each diagnostic level."""


def report_dir() -> Path:
    """
    :return: The directory the pipeline collects the reports from.
    """
    return root / "output" / "code-quality"


def to_issues(tool: str, lines: list[str]) -> list[dict]:
    """
    Convert diagnostic lines to Code Quality issues.

    :param tool: The check name used when a finding does not carry its own.
    :param lines: Output lines; the ones not in the diagnostic shape (notes, progress) are ignored.
    :return: One issue per finding.
    """
    issues = []
    for line in lines:
        match = FINDING.match(line.strip())
        if match is None:
            continue
        path = Path(match["path"])
        if path.is_absolute() and path.is_relative_to(root):
            path = path.relative_to(root)
        check = TRAILING_CHECK.search(match["message"]) or LEADING_CHECK.match(match["message"])
        issues.append(
            {
                "description": match["message"],
                "check_name": check["check"] if check else tool,
                # Stable across pipelines, so GitLab tells a new finding from one the target branch already had.
                "fingerprint": sha256(f"{tool}:{line.strip()}".encode()).hexdigest(),
                "severity": SEVERITY[match["level"]],
                "location": {"path": path.as_posix(), "lines": {"begin": int(match["line"])}},
            }
        )
    return issues


def write_report(name: str, tool: str, lines: list[str]) -> None:
    """
    Write the findings as a Code Quality report.

    Written even when empty: GitLab compares it with the target branch's, and an empty one is what tells it the
    findings are resolved.

    :param name: The report file name, without extension.
    :param tool: The check name used when a finding does not carry its own.
    :param lines: The findings, in the diagnostic shape; other lines are ignored.
    """
    issues = to_issues(tool, lines)
    output = report_dir() / f"{name}.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(issues, indent=2), encoding="utf-8")
    log.info(f"Code Quality: {len(issues)} finding(s) written to {output}")
