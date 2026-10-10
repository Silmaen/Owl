"""
Action reporting the dependencies that have a newer version (audit G-08).

Informative only: it never fails the build. Two sources:

- ConanCenter, for every package of the graph, transitive ones and build tools included: `conan graph outdated` on
  `conanfile.py` (every option on, so the editor, test and Tracy packages are checked too) resolves the whole graph,
  local recipes through the `owl-local` remote; then the versions of each package it flags are listed on ConanCenter
  and the newest real update is kept. Conan sorts `cci.<date>` snapshots and date versions above semantic versions,
  so its own "latest" can hide a real update (plutovg 1.3.2: `cci.20230205` above 1.3.3).
- Upstream, for the local recipes of `conan/recipes/` (absent or late on ConanCenter): the tags of the GitHub or
  GitLab repository of each source in `conandata.yml`, second sources included (nativefiledialog-extended's
  wayland-protocols).

A version counts as an update only within its scheme (semantic versions against semantic ones, dates against dates),
with the same flavour (`-docking`), and never as a pre-release unless the current version is one. The report is
logged, written to `output/dependency_report.json` and its count published as the TeamCity statistic
`OutdatedDependencies`.

Options (after `--`): `--profile=<path>` (default `conan/profiles/linux-clang`), `--output=<file.json>` (default
`output/dependency_report.json`), `--no-upstream` (skip the tags of the local recipes' repositories).
"""

from __future__ import annotations

import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.run import run_command, run_command_capture_output
from ci.utils.teamcity import report_statistic

_DATE_VERSION = re.compile(r"^(?:cci\.)?(\d{8})(?:[.+].*)?$")
_SEMANTIC_VERSION = re.compile(
    r"^(?P<release>\d+(?:\.\d+)*)(?P<letter>[a-z])?"
    r"(?:[-.]?(?P<pre>rc|alpha|beta|pre|dev)\.?(?P<pre_number>\d*))?"
    r"(?:-(?P<flavour>[A-Za-z][\w.]*))?(?:\+.*)?$"
)
_GITHUB_SOURCE = re.compile(
    r"https://github\.com/(?P<owner>[^/]+)/(?P<repo>[^/]+)/"
    r"(?:archive/(?:refs/tags/)?(?P<archive>[^/]+?)\.(?:tar\.gz|tar\.bz2|tar\.xz|tgz|zip)$|releases/download/(?P<release>[^/]+)/)"
)
_GITLAB_SOURCE = re.compile(
    r"https://(?P<host>[^/]*gitlab[^/]*)/(?P<project>.+?)/-/(?:releases|archive)/(?P<tag>[^/]+)/"
)
_COMMIT = re.compile(r"^[0-9a-f]{40}$")
_URL = re.compile(r"^\s*url:\s*[\"']?([^\"'\s]+)", re.MULTILINE)
_TAG_LINE = re.compile(r"^[0-9a-f]{40}\trefs/tags/(.+)$", re.MULTILINE)

# Versions that sort above the current one but are older (checked by hand); the only filter beyond the schemes.
KNOWN_FALSE_POSITIVES: dict[str, set[str]] = {
    # ImGuizmo tagged 1.83 (2021) before going back to 1.9 and 1.10.
    "imguizmo": {"1.83"},
}

CONANCENTER = "ConanCenter"
UPSTREAM = "upstream"


@dataclass(frozen=True)
class OutdatedDependency:
    """One dependency with a newer version."""

    name: str
    current: str
    latest: str
    source: str = CONANCENTER


@dataclass(frozen=True)
class _Version:
    """Comparable form of a semantic version."""

    key: tuple
    flavour: str
    prerelease: bool


def _version(ref: str) -> str:
    """Version part of a `name/version` reference (revision and timestamp dropped)."""
    version = ref.split("/", 1)[1] if "/" in ref else ref
    return version.split("#", 1)[0]


def _date(version: str) -> int | None:
    """The date of a `cci.<date>` or `<date>` version, None for another scheme."""
    match = _DATE_VERSION.match(version)
    return int(match.group(1)) if match else None


def _semantic(version: str) -> _Version | None:
    """The comparable form of a semantic version, None for another scheme."""
    match = _SEMANTIC_VERSION.match(version)
    if not match or _date(version) is not None:
        return None
    release = [int(part) for part in match.group("release").split(".")]
    while len(release) > 1 and release[-1] == 0:
        release.pop()
    prerelease = match.group("pre") is not None
    pre_number = int(match.group("pre_number") or 0)
    key = (tuple(release), match.group("letter") or "", 0 if prerelease else 1, pre_number)
    return _Version(key=key, flavour=match.group("flavour") or "", prerelease=prerelease)


def is_update(current: str, candidate: str, name: str = "") -> bool:
    """
    Whether a candidate version is really newer than the current one.

    :param current: The version in use.
    :param candidate: A version available on the remote or upstream.
    :param name: The package name, to apply `KNOWN_FALSE_POSITIVES`.
    :return: True for a newer version of the same scheme and flavour, a pre-release only after a pre-release.
    """
    if candidate in KNOWN_FALSE_POSITIVES.get(name, set()):
        return False
    current_date, candidate_date = _date(current), _date(candidate)
    if current_date is not None or candidate_date is not None:
        return current_date is not None and candidate_date is not None and candidate_date > current_date
    current_version, candidate_version = _semantic(current), _semantic(candidate)
    if current_version is None or candidate_version is None:
        return False
    if candidate_version.flavour != current_version.flavour:
        return False
    if candidate_version.prerelease and not current_version.prerelease:
        return False
    return candidate_version.key > current_version.key


def is_false_positive(current: str, latest: str) -> bool:
    """
    Whether a remote "latest" version is not really newer than the current one.

    :param current: The version in use.
    :param latest: The version the remote reports as the latest.
    :return: True when the report is to be ignored.
    """
    return not is_update(current, latest)


def newest_update(name: str, current: str, candidates: list[str]) -> str | None:
    """
    The newest real update of a version among candidates.

    :param name: The package name.
    :param current: The version in use.
    :param candidates: The available versions, in any order.
    :return: The newest candidate that is an update, None when there is none.
    """
    best: str | None = None
    for candidate in candidates:
        if is_update(current, candidate, name) and (best is None or is_update(best, candidate, name)):
            best = candidate
    return best


def parse_outdated(data: dict, remote_versions: dict[str, list[str]] | None = None) -> list[OutdatedDependency]:
    """
    Turn the JSON of `conan graph outdated --format=json` into the list of real updates.

    :param data: The decoded JSON (package name -> `current_versions`, `latest_remote`).
    :param remote_versions: Every version of a package on ConanCenter, when listed; otherwise only the remote's
        "latest" is considered.
    :return: The outdated dependencies, sorted by name, false positives removed.
    """
    result: list[OutdatedDependency] = []
    for name, entry in data.items():
        latest_remote = entry.get("latest_remote") or {}
        candidates = list((remote_versions or {}).get(name, []))
        if latest := _version(latest_remote.get("ref", "")):
            candidates.append(latest)
        for current_ref in entry.get("current_versions", []):
            current = _version(current_ref)
            if update := newest_update(name, current, candidates):
                result.append(OutdatedDependency(name=name, current=current, latest=update))
    return sorted(result, key=lambda d: d.name)


def parse_conan_list(data: dict) -> list[str]:
    """
    The versions in the JSON of `conan list <name>/* -r <remote> --format=json`.

    :param data: The decoded JSON (remote name -> `name/version` -> details).
    :return: The versions, empty when the remote reports an error.
    """
    versions: list[str] = []
    for refs in data.values():
        if isinstance(refs, dict) and "error" not in refs:
            versions += [_version(ref) for ref in refs]
    return versions


def tag_version(tag: str, repo: str = "") -> str | None:
    """
    Version carried by a git tag.

    :param tag: The tag (`v1.4.1`, `1.49`, `glfw-3.5.1`, `release-1.2`).
    :param repo: The repository name, accepted as a tag prefix.
    :return: The version, None for a tag that is not one (`cpp14`, `vulkan-sdk-1.4.350.0`).
    """
    prefixes = ["v", "V", "release-"] + ([f"{repo}-"] if repo else [])
    for prefix in prefixes:
        if tag.startswith(prefix) and tag[len(prefix) : len(prefix) + 1].isdigit():
            return tag[len(prefix) :]
    return tag if tag[:1].isdigit() else None


def upstream_sources(conandata: str) -> list[tuple[str, str, str]]:
    """
    The git repositories and versions of the sources of a local recipe.

    :param conandata: The text of its `conandata.yml`.
    :return: `(repository name, git URL, version)` for each GitHub or GitLab source pinned to a tag, deduplicated
        (a pinned commit or another host is left out).
    """
    result: list[tuple[str, str, str]] = []
    for url in _URL.findall(conandata):
        if match := _GITHUB_SOURCE.match(url):
            repo, git = match.group("repo"), f"https://github.com/{match.group('owner')}/{match.group('repo')}.git"
            tag = match.group("archive") or match.group("release")
        elif match := _GITLAB_SOURCE.match(url):
            project = match.group("project")
            repo, git, tag = (
                project.rsplit("/", 1)[-1],
                f"https://{match.group('host')}/{project}.git",
                match.group("tag"),
            )
        else:
            continue
        if _COMMIT.match(tag):
            continue
        version = tag_version(tag, repo)
        if version is None:
            continue
        source = (repo, git, version)
        if source not in result:
            result.append(source)
    return result


def parse_tags(ls_remote: str, repo: str = "") -> list[str]:
    """
    The versions of the tags listed by `git ls-remote --tags --refs`.

    :param ls_remote: The command output.
    :param repo: The repository name, accepted as a tag prefix.
    :return: The versions of the tags that carry one.
    """
    versions = (tag_version(tag, repo) for tag in _TAG_LINE.findall(ls_remote))
    return [version for version in versions if version is not None]


def _normalized(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


class DependencyReport(BaseAction):
    """
    Report the dependencies with a newer version on ConanCenter or upstream, without ever failing.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Run `conan graph outdated`, check the local recipes upstream and log the real updates.
        :param preset: The preset (unused: the report covers the whole `conanfile.py`).
        :param extra_args: Optional extra arguments, see the module docstring.
        :return: Always 0.
        """
        args = self.parse_extra_args(extra_args)
        profile = args.get("profile", str(root / "conan" / "profiles" / "linux-clang"))
        output = Path(args.get("output", str(root / "output" / "dependency_report.json")))
        output.parent.mkdir(parents=True, exist_ok=True)
        outdated = self._conancenter(profile, output)
        if outdated is None:
            return 0
        if args.get("no-upstream") != "true":
            outdated = sorted(outdated + self._upstream(), key=lambda d: d.name)
        for dep in outdated:
            log.warning(f"DependencyReport: {dep.name} {dep.current} -> {dep.latest} ({dep.source}).")
        output.write_text(json.dumps([asdict(dep) for dep in outdated], indent=2) + "\n")
        log.info(f"DependencyReport: {len(outdated)} dependencies have a newer version (report: {output}).")
        report_statistic("OutdatedDependencies", len(outdated))
        return 0

    @staticmethod
    def _conancenter(profile: str, output: Path) -> list[OutdatedDependency] | None:
        """The real updates on ConanCenter, None when the graph cannot be resolved."""
        # Same registration as cmake/Conan.cmake: the graph resolves the local recipes whatever the cache holds.
        local = ["conan", "remote", "add", "owl-local", str(root / "conan"), "--type=local-recipes-index", "--force"]
        if run_command(local + ["--index", "0"]) != 0:
            log.warning("DependencyReport: cannot register the owl-local remote, no report.")
            return None
        output.unlink(missing_ok=True)
        command = ["conan", "graph", "outdated", str(root), "--profile:all", profile]
        for option in ("testing", "nest", "tracy"):
            command += ["-o", f"&:{option}=True"]
        command += ["-r", "owl-local", "-r", "conancenter", "--format=json", f"--out-file={output}"]
        if run_command(command) != 0 or not output.exists():
            log.warning("DependencyReport: conan graph outdated failed, no report.")
            return None
        data = json.loads(output.read_text())
        listing = output.with_name(f"{output.stem}_versions.json")
        remote_versions: dict[str, list[str]] = {}
        for name in data:
            listing.unlink(missing_ok=True)
            list_command = ["conan", "list", f"{name}/*", "-r", "conancenter", "--format=json"]
            if run_command(list_command + [f"--out-file={listing}"]) == 0 and listing.exists():
                remote_versions[name] = parse_conan_list(json.loads(listing.read_text()))
        listing.unlink(missing_ok=True)
        return parse_outdated(data, remote_versions)

    @staticmethod
    def _upstream() -> list[OutdatedDependency]:
        """The real updates in the upstream repositories of the local recipes."""
        result: list[OutdatedDependency] = []
        for conandata in sorted((root / "conan" / "recipes").glob("*/all/conandata.yml")):
            recipe = conandata.parent.parent.name
            for repo, git, current in upstream_sources(conandata.read_text()):
                status, ls_remote = run_command_capture_output(["git", "ls-remote", "--tags", "--refs", git])
                if status != 0:
                    log.warning(f"DependencyReport: cannot list the tags of {git}, {recipe} not checked upstream.")
                    continue
                name = recipe if _normalized(repo) == _normalized(recipe) else f"{recipe} ({repo})"
                if update := newest_update(recipe, current, parse_tags(ls_remote, repo)):
                    result.append(OutdatedDependency(name=name, current=current, latest=update, source=UPSTREAM))
        return result
