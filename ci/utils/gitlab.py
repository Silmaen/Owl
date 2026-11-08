"""
GitLab CI integration: the pipeline context of a job and the few API reads the actions need.

Nothing here is required outside GitLab: every function answers "unknown" (None, False) when the job variables are
absent, and the caller decides what that means.
"""

from __future__ import annotations

import json
import os
import sys
import time
from collections.abc import Iterator
from contextlib import contextmanager
from urllib.error import URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

from ci import log
from ci.utils.secrets import get_secret

TOKEN_VARIABLE = "OWL_GITLAB_API_TOKEN"
"""CI/CD variable holding a project access token with `read_api`: `CI_JOB_TOKEN` cannot list the project's jobs."""

_PAGES = 5
"""Pages of 100 successful jobs searched for a reference: older than that, the reference is stale anyway."""


def in_gitlab_ci() -> bool:
    """
    :return: True inside a GitLab CI job.
    """
    return "GITLAB_CI" in os.environ


def on_default_branch() -> bool:
    """
    :return: True when the job builds a commit of the default branch (a push, a schedule or a run by hand on it).
    """
    branch = os.environ.get("CI_COMMIT_BRANCH", "")
    return branch != "" and branch == os.environ.get("CI_DEFAULT_BRANCH", "main")


@contextmanager
def section(name: str, header: str, collapsed: bool = False) -> Iterator[None]:
    """
    A collapsible section of the job log, timed by GitLab; nothing outside GitLab CI.

    The markers go to stderr, where the logger writes, so that they stay in order with the lines around them.

    :param name: Section id: letters, digits, `_`, `.` and `-` only.
    :param header: Title shown on the folded line.
    :param collapsed: Fold the section from the start.
    """
    if not in_gitlab_ci():
        yield
        return
    option = "[collapsed=true]" if collapsed else ""
    sys.stderr.flush()
    sys.stderr.write(f"\x1b[0Ksection_start:{int(time.time())}:{name}{option}\r\x1b[0K{header}\n")
    sys.stderr.flush()
    try:
        yield
    finally:
        sys.stdout.flush()
        sys.stderr.write(f"\x1b[0Ksection_end:{int(time.time())}:{name}\r\x1b[0K\n")
        sys.stderr.flush()


def _api_get(path: str) -> object | None:
    """
    Read one GitLab API endpoint of this project.

    :param path: The path after `/projects/:id/`, query string included.
    :return: The decoded JSON, or None when the context, the token or the server is missing.
    """
    api = os.environ.get("CI_API_V4_URL", "")
    project = os.environ.get("CI_PROJECT_ID", "")
    token = get_secret(TOKEN_VARIABLE)
    if not api or not project or not token:
        return None
    request = Request(f"{api}/projects/{quote(project, safe='')}/{path}", headers={"PRIVATE-TOKEN": token})
    try:
        with urlopen(request, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))
    except (URLError, TimeoutError, ValueError) as error:
        log.warning(f"GitLab API: {path.split('?')[0]} unreadable ({error}).")
        return None


def reference_coverage(job_name: str | None = None) -> float | None:
    """
    The line coverage of the last successful job of the same name on the default branch, the reference a coverage
    drop is measured against.

    :param job_name: The job to look for; default the running job (`CI_JOB_NAME`).
    :return: Its coverage in percent, or None when there is none or the API cannot be read.
    """
    name = job_name or os.environ.get("CI_JOB_NAME", "")
    branch = os.environ.get("CI_DEFAULT_BRANCH", "main")
    if not name:
        return None
    for page in range(1, _PAGES + 1):
        jobs = _api_get(f"jobs?scope[]=success&per_page=100&page={page}")
        if not isinstance(jobs, list):
            return None
        for job in jobs:
            if job.get("name") == name and job.get("ref") == branch and job.get("coverage") is not None:
                return float(job["coverage"])
        if len(jobs) < 100:
            break
    return None


def last_scheduled_sha() -> str | None:
    """
    The commit the last successful scheduled pipeline of the default branch built.

    :return: Its SHA, or None when there is none or the API cannot be read.
    """
    branch = quote(os.environ.get("CI_DEFAULT_BRANCH", "main"), safe="")
    pipelines = _api_get(f"pipelines?ref={branch}&source=schedule&status=success&per_page=1")
    if not isinstance(pipelines, list) or not pipelines:
        return None
    return str(pipelines[0].get("sha", "")) or None


_REUSE_SOURCES = {"merge_request_event", "push"}
"""Pipelines whose build jobs may stand on an earlier success: the ones a push starts, never a run asked for."""

_REUSE_CANDIDATES = 5
"""Earlier commits compared with this one, the most recent first: each comparison is one API call."""


def reuse_allowed() -> bool:
    """
    Whether this job may stand on an earlier success instead of building: in a pipeline a push started, unless the job
    was started by hand or `OWL_CI_REUSE` is `false`.

    :return: True when an earlier success may be reused.
    """
    return (
        in_gitlab_ci()
        and os.environ.get("CI_PIPELINE_SOURCE", "") in _REUSE_SOURCES
        and os.environ.get("CI_JOB_MANUAL", "") != "true"
        and os.environ.get("OWL_CI_REUSE", "true") != "false"
    )


def same_tree_success(job_name: str | None = None) -> dict | None:
    """
    An earlier successful job of the same name whose commit has exactly the tree of this one: its verdict is this
    job's verdict.

    :param job_name: The job to look for; default the running job (`CI_JOB_NAME`).
    :return: The job (`id`, `coverage`, `commit.id`), or None when there is none or the API cannot be read.
    """
    name = job_name or os.environ.get("CI_JOB_NAME", "")
    sha = os.environ.get("CI_COMMIT_SHA", "")
    if not name or not sha:
        return None
    candidates: dict[str, dict] = {}
    for page in range(1, _PAGES + 1):
        jobs = _api_get(f"jobs?scope[]=success&per_page=100&page={page}")
        if not isinstance(jobs, list):
            return None
        for job in jobs:
            commit = (job.get("commit") or {}).get("id", "")
            if job.get("name") == name and commit and commit not in candidates:
                candidates[commit] = job
        if len(candidates) >= _REUSE_CANDIDATES or len(jobs) < 100:
            break
    for commit, job in list(candidates.items())[:_REUSE_CANDIDATES]:
        # Straight: the two trees against each other, whatever their history. No diff, same tree.
        compare = _api_get(f"repository/compare?from={commit}&to={sha}&straight=true")
        if isinstance(compare, dict) and compare.get("diffs") == []:
            return job
    return None
