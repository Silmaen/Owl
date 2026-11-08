"""
Action deciding whether the nightly pipeline has anything to build.
"""

import os

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig

TEMPLATES = root / "ci" / "gitlab" / "templates.yml"
"""The building blocks the nightly jobs extend: an artifact-generated pipeline cannot include the repository's files."""

NIGHTLY = root / "ci" / "gitlab" / "nightly.yml"
"""The nightly jobs, run as a child pipeline of the scheduled one."""

UNCHANGED = root / "ci" / "gitlab" / "nightly-unchanged.yml"
"""The child pipeline of a night with no new commit: a single job saying so (a child needs at least one)."""

OUTPUT = root / "output" / "nightly.yml"
"""The file the `nightly` trigger job includes."""


def nightly_pipeline(unchanged: bool) -> str:
    """
    The child pipeline of tonight.

    :param unchanged: True when the commit already passed the last nightly.
    :return: The YAML of the child pipeline: the templates followed by the nightly jobs, or the single no-change job.
    """
    if unchanged:
        return UNCHANGED.read_text(encoding="utf-8")
    return TEMPLATES.read_text(encoding="utf-8") + "\n" + NIGHTLY.read_text(encoding="utf-8")


class NightlyGate(BaseAction):
    """
    Write the child pipeline of the scheduled pipeline: the nightly jobs when the default branch moved since the last
    successful nightly, else a single job saying there is nothing new. Run by hand, it always writes the nightly jobs.

    A night without a reference (first nightly, no `OWL_GITLAB_API_TOKEN`, API unreachable) builds: skipping is an
    optimisation, missing a nightly is not.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Compare the commit with the last successful nightly and write `output/nightly.yml`.
        :param preset: Unused, kept for the common action signature.
        :param extra_args: Optional extra arguments (unused).
        :return: Always 0.
        """
        from ci.utils.gitlab import last_scheduled_sha

        commit = os.environ.get("CI_COMMIT_SHA", "")
        # A nightly run by hand always builds: it is there to check a branch.
        last = last_scheduled_sha() if os.environ.get("CI_PIPELINE_SOURCE", "schedule") == "schedule" else None
        unchanged = commit != "" and commit == last
        if unchanged:
            log.info(f"NightlyGate: {commit[:8]} already passed the last nightly, nothing to build.")
        else:
            log.info(f"NightlyGate: building {commit[:8] or 'HEAD'} (last nightly: {(last or 'none')[:8]}).")
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(nightly_pipeline(unchanged), encoding="utf-8")
        return 0
