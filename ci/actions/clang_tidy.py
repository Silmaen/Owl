"""
Action running clang-tidy over the project's translation units.

The analysis is driven from `compile_commands.json` **after** the build rather
than hooked into the compiler through `CMAKE_CXX_CLANG_TIDY`, because that is
what makes the set of analysed translation units selectable. Two scopes:

* **full** — every C++ translation unit of the repository in `compile_commands.json`. Runs on
  `main`, on a manual run, and whenever the diff scope cannot be established
  with certainty.
* **diff** — only the translation units a pull request can change the verdict
  of: the `.cpp` files it touches, plus every `.cpp` that includes — directly
  or transitively — a header it touches. That closure comes from ninja's own
  dependency database (`ninja -t deps`), which records the complete include
  set of each object file as the compiler saw it, so a header edit is never
  missed because the `.cpp` consuming it lies outside the diff.

The diff base is the pull request's **merge base**, published by
teamcity-github-bridge as `teamcity.github.bridge.pullRequest.mergeBase`.
Diffing against the target branch's head instead would also pick up everything
that landed on `main` since the branch started, and report other people's
findings on this pull request.

Anything that makes the narrowing untrustworthy — no pull request, no merge
base, no git, no ninja dependency database, or a change to the build system or
to `.clang-tidy` itself — degrades to the full scope. Narrowing is an
optimisation; missing a finding is not an acceptable failure mode.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor
from logging import ERROR, INFO, WARNING
from pathlib import Path
from typing import Optional

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig

# Suffixes clang-tidy is invoked on. Mirrors what `CMAKE_CXX_CLANG_TIDY` used
# to cover: C++ translation units only.
TU_SUFFIXES: tuple[str, ...] = (".cpp", ".cc", ".cxx")

# Suffixes treated as "a header some translation unit may include".
HEADER_SUFFIXES: tuple[str, ...] = (".h", ".hpp", ".hxx", ".inl", ".ipp")

# How many selected translation units are named in the log before the list is
# summarised. The point is to show a reviewer what was analysed, which the first
# few dozen entries plus a count already do.
LISTED_UNITS: int = 40

# clang-tidy arguments per tool: the analyzer mode keeps only the Clang static
# analyzer checks and makes each of them an error.
TOOL_ARGS: dict[str, list[str]] = {
    "tidy": [],
    # optin.performance.Padding is a field-order suggestion, not a defect: it would
    # flag every translation unit including Scene.h and stays a performance topic.
    "analyzer": [
        "--checks=-*,clang-analyzer-*,-clang-analyzer-optin.performance.Padding",
        "--warnings-as-errors=clang-analyzer-*",
    ],
}
TOOL_LABELS: dict[str, str] = {"tidy": "clang-tidy", "analyzer": "clang static analyzer"}

# Repo-relative paths whose change invalidates the file-level mapping: compiler
# flags, dependency versions or the check list itself moved, so every
# translation unit needs re-analysing regardless of what else the diff touches.
FULL_SCOPE_FILES: tuple[str, ...] = (
    ".clang-tidy",
    "depmanager.yml",
)

# Same, as patterns: any CMake input.
FULL_SCOPE_PATTERNS: tuple[re.Pattern[str], ...] = (
    re.compile(r"(?:^|/)CMakeLists\.txt$"),
    re.compile(r"\.cmake$"),
    re.compile(r"(?:^|/)CMakePresets[^/]*\.json$"),
)


def _rel(path: Path | str) -> str:
    """
    Repo-relative form of a path, for log lines.

    :param path: Absolute or already-relative path.
    :return: The path relative to the repository root when it lies inside it,
        its string form otherwise.
    """
    try:
        return str(Path(path).relative_to(root))
    except ValueError:
        return str(path)


def _norm(path: str, base: Path) -> str:
    """
    Absolute, `..`-free form of a path from a dependency file.

    `os.path.normpath` and not `Path.resolve()` on purpose: this runs on every
    line of `ninja -t deps` (six figures on this project) and `resolve()` would
    make a `realpath` syscall for each. Both ends of every comparison go
    through this same function, so the two agree without needing symlinks
    expanded.

    :param path: Path as recorded by the compiler, absolute or relative to
        `base`.
    :param base: Directory a relative path is relative to (the build dir).
    :return: Normalised absolute path.
    """
    return os.path.normpath(path if os.path.isabs(path) else os.path.join(base, path))


# ────────────────────────────────────────────────────────────────────────────
# git
# ────────────────────────────────────────────────────────────────────────────


def _git(*args: str, quiet: bool = False) -> Optional[str]:
    """
    Run a git command in the repository and capture its stdout.

    :param args: Arguments passed to git.
    :param quiet: Do not log anything when git exits non-zero (for probes whose
        failure is an expected answer rather than a problem).
    :return: The command's stdout, or None when git is missing or failed.
    """
    try:
        proc = subprocess.run(
            ["git", "-C", str(root), *args],
            capture_output=True,
            text=True,
        )
    except OSError as err:
        log.warning(f"clang-tidy: cannot run git ({err}).")
        return None
    if proc.returncode != 0:
        if not quiet:
            log.warning(
                f"clang-tidy: `git {' '.join(args)}` exited {proc.returncode}: "
                f"{proc.stderr.strip()}"
            )
        return None
    return proc.stdout


def _is_commit(ref: str) -> bool:
    """
    Tell whether a ref or SHA resolves to a commit present in this checkout.

    :param ref: Ref name or SHA.
    :return: True when the object is available locally.
    """
    out = _git("rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", quiet=True)
    return out is not None and out.strip() != ""


def _changed_files(base: str) -> Optional[list[str]]:
    """
    List the files the current checkout changes with respect to `base`.

    Uses the three-dot form, so the comparison starts at the merge base of
    `base` and `HEAD` even if the checkout is a merge commit — a two-dot diff
    would then also report what came in from the target branch.

    :param base: Ref or SHA to compare against.
    :return: Repo-relative paths (deletions excluded), or None when git failed.
    """
    out = _git("diff", "--name-only", "--diff-filter=d", f"{base}...HEAD")
    if out is None:
        return None
    return [line.strip() for line in out.splitlines() if line.strip()]


# ────────────────────────────────────────────────────────────────────────────
# Translation units and their include closure
# ────────────────────────────────────────────────────────────────────────────


def _load_translation_units(build_dir: Path) -> Optional[dict[str, Path]]:
    """
    Read `compile_commands.json` and map each object file to its source.

    :param build_dir: Configured build directory.
    :return: Object path (normalised, absolute) → source path, or None when the
        database is missing or unreadable.
    """
    database = build_dir / "compile_commands.json"
    if not database.is_file():
        log.error(
            f"clang-tidy: {_rel(database)} not found — configure and build the "
            "preset first (the compilation database is what drives the analysis)."
        )
        return None
    try:
        entries = json.loads(database.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as err:
        log.error(f"clang-tidy: cannot read {_rel(database)}: {err}")
        return None

    units: dict[str, Path] = {}
    build_root = build_dir.resolve()
    for entry in entries:
        directory = entry.get("directory", str(build_dir))
        source = Path(entry.get("file", ""))
        if source.suffix not in TU_SUFFIXES or not source.is_file():
            continue
        # Third-party sources compiled into the build (Conan cache, copied imgui backends) are not ours to analyse.
        resolved = (Path(directory) / source).resolve()
        if not resolved.is_relative_to(root) or resolved.is_relative_to(build_root):
            continue
        output = entry.get("output")
        if not output:
            continue
        units[_norm(output, Path(directory))] = source
    return units


def _dependent_objects(build_dir: Path, headers: set[str]) -> Optional[set[str]]:
    """
    Find the object files whose recorded include closure contains one of
    `headers`.

    Reads ninja's dependency database, which holds the full transitive include
    list the compiler reported for every object built in this directory. This
    is what catches a header change reaching a `.cpp` that the diff does not
    touch, including through several intermediate headers.

    :param build_dir: Build directory holding `.ninja_deps`.
    :param headers: Normalised absolute paths of the changed headers.
    :return: Normalised absolute object paths, or None when the database could
        not be read (caller must then fall back to the full scope).
    """
    ninja = shutil.which("ninja")
    if ninja is None:
        log.warning("clang-tidy: ninja not found in PATH.")
        return None
    if not (build_dir / ".ninja_deps").is_file():
        log.warning(
            f"clang-tidy: {_rel(build_dir / '.ninja_deps')} not found — the "
            "preset must have been built before the analysis is scoped."
        )
        return None
    try:
        proc = subprocess.run(
            [ninja, "-C", str(build_dir), "-t", "deps"],
            capture_output=True,
            text=True,
        )
    except OSError as err:
        log.warning(f"clang-tidy: cannot run `ninja -t deps` ({err}).")
        return None
    if proc.returncode != 0:
        log.warning(
            f"clang-tidy: `ninja -t deps` exited {proc.returncode}: "
            f"{proc.stderr.strip()}"
        )
        return None

    objects: set[str] = set()
    current: Optional[str] = None
    for line in proc.stdout.splitlines():
        if not line:
            continue
        if line[0] in " \t":
            if current is None or current in objects:
                continue
            if _norm(line.strip(), build_dir) in headers:
                objects.add(current)
            continue
        current = _norm(line.split(":", 1)[0], build_dir)
    return objects


# ────────────────────────────────────────────────────────────────────────────
# Scope resolution
# ────────────────────────────────────────────────────────────────────────────


def _forces_full_scope(path: str) -> bool:
    """
    Tell whether a changed file invalidates the per-file narrowing.

    :param path: Repo-relative path of a changed file.
    :return: True when every translation unit must be re-analysed.
    """
    if path in FULL_SCOPE_FILES:
        return True
    return any(pattern.search(path) for pattern in FULL_SCOPE_PATTERNS)


def _resolve_base(parsed: dict[str, str]) -> Optional[str]:
    """
    Determine the commit the diff scope is computed against.

    Order: an explicit `--diff_base` (local use), then the pull request's merge
    base, then the target branch as a last resort — each candidate kept only if
    the commit is actually present in the checkout.

    :param parsed: Parsed extra arguments.
    :return: Ref or SHA to diff against, or None to run the full scope.
    """
    if parsed.get("full", "").lower() == "true":
        log.info("clang-tidy: --full requested, analysing every translation unit.")
        return None

    explicit = parsed.get("diff_base", "").strip()
    if explicit:
        if _is_commit(explicit):
            return explicit
        log.warning(
            f"clang-tidy: --diff_base='{explicit}' is not a commit in this "
            "checkout; falling back to the full scope."
        )
        return None

    if parsed.get("is_pull_request", "").strip().lower() != "true":
        log.info(
            "clang-tidy: no pull request context, analysing every translation unit."
        )
        return None

    candidates: list[str] = []
    merge_base = parsed.get("merge_base", "").strip()
    if merge_base:
        candidates.append(merge_base)
    else:
        log.warning(
            "clang-tidy: pull request build with no merge base published "
            "(the bridge's `mergeBase.enabled` is off, or the GitHub lookup "
            "failed) — deriving it locally from the target branch instead. "
            "That stays a merge-base comparison: the diff below uses the "
            "three-dot form, so git recomputes the divergence point itself."
        )
    target = parsed.get("target_branch", "").strip()
    if target:
        candidates += [f"origin/{target}", target]

    for candidate in candidates:
        if _is_commit(candidate):
            return candidate
        log.warning(
            f"clang-tidy: diff base '{candidate}' is not a commit in this checkout."
        )
    log.warning(
        "clang-tidy: no usable diff base, falling back to the full scope."
    )
    return None


def _select(
        units: dict[str, Path],
        base: str,
        build_dir: Path,
) -> Optional[set[Path]]:
    """
    Narrow the translation units to those the diff can change the verdict of.

    :param units: Object path → source path, from the compilation database.
    :param base: Commit the diff is computed against.
    :param build_dir: Build directory (for the ninja dependency database).
    :return: The selected sources, or None when the narrowing is not
        trustworthy and the caller must analyse everything.
    """
    changed = _changed_files(base)
    if changed is None:
        log.warning("clang-tidy: could not list the changed files.")
        return None
    if not changed:
        # A pull request always changes something, so an empty diff means the
        # base is not the one we think it is (a squashed or re-pointed branch,
        # a checkout that already contains the base). Analysing nothing would
        # silently pass the gate; analyse everything instead.
        log.warning(f"clang-tidy: no file changed against {base} — suspicious base.")
        return None

    log.info(f"clang-tidy: {len(changed)} file(s) changed against {base}.")
    forcing = [path for path in changed if _forces_full_scope(path)]
    if forcing:
        log.info(
            "clang-tidy: build configuration changed "
            f"({', '.join(sorted(forcing))}) — analysing every translation unit."
        )
        return None

    sources = set(units.values())
    selected = {
        path
        for path in (root / entry for entry in changed)
        if path.suffix in TU_SUFFIXES and path in sources
    }
    headers = {
        _norm(str(root / entry), root)
        for entry in changed
        if Path(entry).suffix in HEADER_SUFFIXES
    }
    if not headers:
        return selected

    objects = _dependent_objects(build_dir, headers)
    if objects is None:
        return None
    if not objects:
        log.warning(
            "clang-tidy: none of the changed headers appears in any translation "
            "unit's include closure — nothing includes them in this preset "
            f"({', '.join(sorted(_rel(header) for header in headers))})."
        )
    selected |= {units[obj] for obj in objects if obj in units}
    return selected


# ────────────────────────────────────────────────────────────────────────────
# Running clang-tidy
# ────────────────────────────────────────────────────────────────────────────


def _log_level(line: str) -> int:
    """
    Map one clang-tidy output line to a log level.

    Keeps the GNU/clang diagnostic shape intact — teamcity-github-bridge scans
    the build log for it to annotate the pull request's diff — while letting
    TeamCity colour the log the way the compiler steps do.

    :param line: One line of clang-tidy output.
    :return: The logging level to emit it at.
    """
    if ": error:" in line or ": fatal error:" in line:
        return ERROR
    if ": warning:" in line:
        return WARNING
    return INFO


def _available_cores() -> int:
    """Return the number of cores this process may run on.

    The scheduler affinity mask is preferred over ``os.cpu_count()``: in a
    container or under ``taskset`` it is the number of cores actually usable.

    :return: The usable core count, at least 1.
    """
    if hasattr(os, "sched_getaffinity"):
        try:
            return max(1, len(os.sched_getaffinity(0)))
        except OSError:
            pass
    return os.cpu_count() or 1


def _job_count(requested: str) -> int:
    """Return the number of parallel clang-tidy processes to run.

    :param requested: The ``--jobs`` value; empty, ``0`` or invalid means one
        job per available core.
    :return: The job count, at least 1.
    """
    if requested:
        try:
            jobs = int(requested)
        except ValueError:
            log.warning(f"clang-tidy: ignoring invalid --jobs={requested!r}.")
            jobs = 0
        if jobs > 0:
            return jobs
    return _available_cores()


def _analyse(
    executable: str, build_dir: Path, source: Path, tool_args: list[str]
) -> tuple[Path, int, str]:
    """
    Run clang-tidy on a single translation unit.

    :param executable: clang-tidy executable.
    :param build_dir: Build directory holding the compilation database.
    :param source: Source file to analyse.
    :param tool_args: Extra clang-tidy arguments selecting the check set.
    :return: The source, clang-tidy's exit code, and its combined output.
    """
    command = [
        executable,
        "-p",
        str(build_dir),
        "--quiet",
        "-extra-arg=-Wno-unknown-warning-option",
        *tool_args,
        str(source),
    ]
    try:
        proc = subprocess.run(command, capture_output=True, text=True)
    except OSError as err:
        return source, 1, f"failed to run clang-tidy: {err}"
    return source, proc.returncode, proc.stdout + proc.stderr


# ────────────────────────────────────────────────────────────────────────────
# Action
# ────────────────────────────────────────────────────────────────────────────


class ClangTidy(BaseAction):
    """Run clang-tidy over the preset's translation units.

    Extra arguments (after ``--``):
      * ``--is_pull_request`` / ``--merge_base`` / ``--target_branch`` — the
        pull request context, wired in TeamCity from
        ``teamcity.github.bridge.*``. With ``is_pull_request=true`` and a
        resolvable merge base, only the translation units the pull request can
        change the verdict of are analysed.
      * ``--diff_base=<ref>`` — analyse the diff against this ref instead.
        Meant for local use: ``-- --diff_base=main``.
      * ``--full`` — force the full scope even in a pull request.
      * ``--jobs=N`` — parallel clang-tidy processes (default: one per
        available core).
      * ``--tool=tidy|analyzer`` — ``tidy`` (default) runs the check set of
        ``.clang-tidy``; ``analyzer`` runs the same binary restricted to the
        Clang static analyzer checks (``clang-analyzer-*``), every finding an
        error.
      * ``--dry_run`` — list the translation units that would be analysed and
        stop. Cheap way to check the scoping without paying for the analysis.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """Analyse the preset's translation units with clang-tidy.

        :param preset: The preset to analyse. Must have been configured and
            built: the compilation database and ninja's dependency database are
            the inputs.
        :param extra_args: Optional extra arguments. See the class docstring.
        :return: Exit code indicating success or failure.
        """
        parsed = self.parse_extra_args(extra_args)
        # The preset's `binaryDir` is relative to the source tree; every path
        # comparison below is against absolute paths from the compilation
        # database, so anchor it once here.
        build_dir = preset.get_build_dir()
        if not build_dir.is_absolute():
            build_dir = root / build_dir

        units = _load_translation_units(build_dir)
        if units is None:
            return 1
        if not units:
            log.error(
                f"clang-tidy: no C++ translation unit found in {_rel(build_dir)}."
            )
            return 1

        base = _resolve_base(parsed)
        selected: Optional[set[Path]] = None
        if base is not None:
            selected = _select(units, base, build_dir)
        if selected is None:
            selected = set(units.values())
            log.info(f"clang-tidy: full scope, {len(selected)} translation unit(s).")
        else:
            log.info(
                f"clang-tidy: diff scope, {len(selected)} of {len(set(units.values()))} "
                "translation unit(s):"
            )
            listed = sorted(selected)
            for source in listed[:LISTED_UNITS]:
                log.info(f"  {_rel(source)}")
            if len(listed) > LISTED_UNITS:
                log.info(f"  ... and {len(listed) - LISTED_UNITS} more")

        if not selected:
            log.info("clang-tidy: nothing to analyse.")
            return 0

        if parsed.get("dry_run", "").lower() == "true":
            log.info("clang-tidy: --dry_run requested, stopping before the analysis.")
            return 0

        executable = shutil.which("clang-tidy")
        if executable is None:
            log.error(
                "clang-tidy: executable not found in PATH. It belongs to the "
                "build image — do not install it on the host."
            )
            return 1

        jobs = _job_count(parsed.get("jobs", ""))
        tool = parsed.get("tool", "tidy").lower() or "tidy"
        if tool not in TOOL_ARGS:
            log.error(f"clang-tidy: unknown --tool={tool!r}, expected tidy or analyzer.")
            return 1
        tool_args = TOOL_ARGS[tool]

        ordered = sorted(selected)
        log.info(
            f"Running {executable} on {len(ordered)} translation unit(s) "
            f"with {jobs} parallel job(s)."
        )
        failed: list[Path] = []
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            results = pool.map(
                lambda source: _analyse(executable, build_dir, source, tool_args), ordered
            )
            for index, (source, status, output) in enumerate(results, start=1):
                log.info(f"[{index}/{len(ordered)}] {_rel(source)}")
                for line in output.splitlines():
                    if line.strip():
                        log.log(_log_level(line), line)
                if status != 0:
                    failed.append(source)

        if failed:
            log.error(
                f"{TOOL_LABELS[tool]} reported findings in {len(failed)} translation unit(s): "
                f"{', '.join(_rel(source) for source in failed)}"
            )
            return 1
        log.info(f"{TOOL_LABELS[tool]}: no finding.")
        return 0
