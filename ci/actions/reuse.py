"""
Action telling a CI job that an earlier success of the same job already gave its verdict.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig


class Reuse(BaseAction):
    """
    Exit 0 when an earlier successful job of the same name ran on a commit with exactly the tree of this one, so this
    job can stop there; exit 1 when it has to run. For the jobs whose script is not `BuildAndTest` (Code Style, the
    analyses, the benchmarks), which call it first: ``ci_action.py Reuse <preset> && exit 0 || true``.

    Same rules as `BuildAndTest`: only in a pipeline a push started (a merge request pipeline included), never in one
    run by hand, nor for a job started by hand, nor with `OWL_CI_REUSE=false`. Outside GitLab it always says "run".

    Options:
      * ``--same_commit`` — only the very commit counts, not another one with the same tree (clang-tidy of a merge
        request: its verdict reads the diff from the merge base too).
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Look for an earlier success of the running job.
        :param preset: Unused, kept for the common action signature.
        :param extra_args: Optional extra arguments (``--same_commit``).
        :return: 0 when the earlier verdict stands, 1 when the job must run.
        """
        from ci.utils.gitlab import reuse_allowed, same_tree_success

        args = self.parse_extra_args(extra_args)
        if not reuse_allowed():
            return 1
        earlier = same_tree_success(same_commit=args.get("same_commit", "false") == "true")
        if earlier is None:
            log.info("Reuse: no earlier success on this tree, the job runs.")
            return 1
        log.info(f"Reuse: job #{earlier['id']} passed on {earlier['commit']['id'][:8]}, same tree: its verdict stands.")
        return 0
