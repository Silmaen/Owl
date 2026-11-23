"""
Action writing the game runner archive of a preset's target, next to the CPack archives.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import get_git_hash, get_project_version
from ci.utils.runners import archive_name, runner_files, write_archive


class PackageRunner(BaseAction):
    """
    Action writing the game runner archive of a preset's target, next to the CPack archives.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Write `OwlGameRunner-<version>-<hash>-<target>.<ext>` in the build tree of a preset with a runner target.
        :param preset: A built preset whose vendor `runner` names its target.
        :param extra_args: Optional: --hash.
        :return: Exit code indicating success or failure.
        """
        if not preset.runner_target:
            log.error(f"PackageRunner: preset '{preset.cmake_preset}' has no runner target (vendor 'runner').")
            return 1
        params = self.parse_extra_args(extra_args)
        git_hash = (params.get("hash") or get_git_hash())[:7]
        version = get_project_version()
        build_dir = preset.get_build_dir()
        files = runner_files(build_dir / "bin", preset.runner_target)
        if not files:
            log.error(f"PackageRunner: no {preset.runner_target} runner in {build_dir}.")
            return 1
        output = build_dir / archive_name(version, git_hash, preset.runner_target)
        write_archive(files, output)
        log.info(f"PackageRunner: {output.name}, {len(files)} files, {output.stat().st_size / 1e6:.1f} MB.")
        return 0
