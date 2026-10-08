"""
Action reporting the Conan dependencies that have a newer version on ConanCenter (audit G-08).

Informative only: it never fails the build. It runs `conan graph outdated` on `conanfile.py` (every option on, so the
editor, test and Tracy packages are checked too) against ConanCenter, drops the false positives (`cci.<date>`
snapshots and date versions older than a semantic one), logs the rest and publishes their count as the TeamCity
statistic `OutdatedDependencies`.

Options (after `--`): `--profile=<path>` (default `conan/profiles/linux-clang`), `--output=<file.json>` (default
`output/dependency_report.json`).
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command
from ci.utils.teamcity import report_statistic

_DATE_VERSION = re.compile(r"^(cci\.)?\d{8}([.+].*)?$")


@dataclass(frozen=True)
class OutdatedDependency:
    """One dependency with a newer version on the remote."""

    name: str
    current: str
    latest: str


def _version(ref: str) -> str:
    """Version part of a `name/version` reference."""
    return ref.split("/", 1)[1] if "/" in ref else ref


def is_false_positive(current: str, latest: str) -> bool:
    """
    Whether a remote "latest" version is not really newer than the current one.

    ConanCenter sorts `cci.<date>` snapshots and date versions (`20191104`) above semantic versions, so a package on
    `1.10` is reported "outdated" by a 2019 snapshot.

    :param current: The version in use.
    :param latest: The version the remote reports as the latest.
    :return: True when the report is to be ignored.
    """
    if latest == current:
        return True
    return bool(_DATE_VERSION.match(latest)) and not _DATE_VERSION.match(current)


def parse_outdated(data: dict) -> list[OutdatedDependency]:
    """
    Turn the JSON of `conan graph outdated --format=json` into the list of real updates.

    :param data: The decoded JSON (package name -> `current_versions`, `latest_remote`).
    :return: The outdated dependencies, sorted by name, false positives removed.
    """
    result: list[OutdatedDependency] = []
    for name, entry in data.items():
        latest_remote = entry.get("latest_remote") or {}
        latest = _version(latest_remote.get("ref", ""))
        if not latest:
            continue
        for current_ref in entry.get("current_versions", []):
            current = _version(current_ref)
            if not is_false_positive(current, latest):
                result.append(OutdatedDependency(name=name, current=current, latest=latest))
    return sorted(result, key=lambda d: d.name)


class DependencyReport(BaseAction):
    """
    Report the dependencies with a newer version on ConanCenter, without ever failing.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Run `conan graph outdated` and log the real updates.
        :param preset: The preset (unused: the report covers the whole `conanfile.py`).
        :param extra_args: Optional extra arguments, see the module docstring.
        :return: Always 0.
        """
        args = self.parse_extra_args(extra_args)
        profile = args.get("profile", str(root / "conan" / "profiles" / "linux-clang"))
        output = Path(args.get("output", str(root / "output" / "dependency_report.json")))
        output.parent.mkdir(parents=True, exist_ok=True)
        command = ["conan", "graph", "outdated", str(root), "--profile:all", profile]
        for option in ("testing", "nest", "tracy"):
            command += ["-o", f"&:{option}=True"]
        command += ["-r", "conancenter", "--format=json", f"--out-file={output}"]
        if run_command(command) != 0 or not output.exists():
            log.warning("DependencyReport: conan graph outdated failed, no report.")
            return 0
        outdated = parse_outdated(json.loads(output.read_text()))
        for dep in outdated:
            log.warning(f"DependencyReport: {dep.name} {dep.current} -> {dep.latest} on ConanCenter.")
        log.info(f"DependencyReport: {len(outdated)} dependencies have a newer version (report: {output}).")
        report_statistic("OutdatedDependencies", len(outdated))
        return 0
