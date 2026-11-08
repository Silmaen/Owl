"""
Action chaining every step a CI build job runs on one preset.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig

_OPEN_STEPS = {"Test", "Coverage"}
"""Steps whose log section is not folded in GitLab: the ones a reader opens the job for."""


class BuildAndTest(BaseAction):
    """
    Clean, build, test, coverage, documentation and package of one preset, in that order, stopping at the first
    failure.

    What runs is read from the preset (`OWL_TESTING`, `OWL_ENABLE_COVERAGE`, `OWL_ENABLE_DOCUMENTATION`, the
    `vendor.silmaen` block) and nowhere else: `.gitlab-ci.yml` only says which presets exist, where and when they run.
    The tests labelled ``slow`` are left out (the nightly slow suite runs them with `Test` directly). The archives are
    written on the default branch only, where the package jobs publish them.

    A job of a pipeline a push started stands on an earlier success of the same job on a commit with exactly the same
    tree (a merge of an up-to-date merge request, a push of the same content), and builds nothing; never a job that
    packages, nor one started by hand, nor with `OWL_CI_REUSE=false`.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Run every step the preset declares.
        :param preset: The preset to build.
        :param extra_args: Optional extra arguments (unused).
        :return: Exit code indicating success or failure.
        """
        from ci.actions.build import Build
        from ci.actions.clean import Clean
        from ci.actions.coverage import Coverage
        from ci.actions.documentation import Documentation
        from ci.actions.package import Package
        from ci.actions.test import Test
        from ci.utils.gitlab import on_default_branch, reuse_allowed, same_tree_success, section

        # The archives only come from a build of their own: a job that packages never stands on an earlier one.
        packages = preset.run_package and on_default_branch()
        if not packages and reuse_allowed() and (earlier := same_tree_success()) is not None:
            commit = earlier["commit"]["id"][:8]
            log.info(f"BuildAndTest: job #{earlier['id']} passed on {commit}, same tree: its verdict stands.")
            if earlier.get("coverage") is not None:
                # Read by the job's `coverage:` pattern, so the coverage reference of the default branch carries on.
                log.info(f"Coverage of job #{earlier['id']}: lines: {float(earlier['coverage']):.2f}%")
            return 0

        steps: list[tuple[BaseAction, list[str] | None]] = [(Clean(), None), (Build(), None)]
        if preset.run_tests:
            steps.append((Test(), ["--label_exclude=slow"]))
            if preset.run_coverage:
                steps.append((Coverage(), None))
        if preset.run_documentation:
            steps.append((Documentation(), None))
        if packages:
            steps.append((Package(), None))

        for action, action_args in steps:
            name = type(action).__name__
            # The results stay open (test failures, coverage), the long and usually quiet steps fold.
            with section(name.lower(), f"{name} {preset.cmake_preset}", collapsed=name not in _OPEN_STEPS):
                log.info(f"── {name} {preset.cmake_preset}")
                result = action(preset, action_args)
            if result != 0:
                return result
        return 0
