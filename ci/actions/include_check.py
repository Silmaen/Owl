"""
Action compiling every header and source alone against strict libc++, without the precompiled header.
"""

from ci import log, root
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.preset import get_build_dir
from ci.utils.remote import configure_remote, parse_remote_args
from ci.utils.run import run_command, MODE_BY_COLOR, MODE_FOR_NINJA

CHECK_TARGET = "owl_include_check"
"""Umbrella target defined by `cmake/IncludeCheck.cmake` (headers + sources)."""


class IncludeCheck(BaseAction):
    """Configure a preset that enables `OWL_INCLUDE_CHECK` and build the `owl_include_check` target.

    Each header of the engine, the editor, the runner, the test helpers and the bench is compiled alone
    in a generated translation unit, and each source file is compiled again, against libc++ with
    `_LIBCPP_REMOVE_TRANSITIVE_INCLUDES` and without the precompiled header. A file that counts on a
    transitive standard include fails here, as it would with a recent libstdc++ (MSYS2 MinGW).
    Nothing is linked, so the check needs no libc++ build of the dependencies.

    Extra arguments (after ``--``): the DepManager remote flags of :class:`Build`
    (``--remote_url`` / ``--remote_login`` / ``--remote_passwd`` / ``--remote_name``) and
    ``--target=<name>`` to build only ``owl_header_check`` or ``owl_source_check``.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """Configure the preset and build the include-check target.

        :param preset: A preset with ``OWL_INCLUDE_CHECK=ON`` (``linux-include-check``).
        :param extra_args: Optional extra arguments, see the class docstring.
        :return: Exit code indicating success or failure.
        """
        cache = (preset.raw_config or {}).get("cacheVariables", {})
        if str(cache.get("OWL_INCLUDE_CHECK", "OFF")).upper() not in ("ON", "TRUE", "1"):
            log.error(f"IncludeCheck: preset '{preset.cmake_preset}' does not set OWL_INCLUDE_CHECK=ON.")
            return 1
        args = self.parse_extra_args(extra_args)
        remote_status = configure_remote(parse_remote_args(args))
        if remote_status != 0:
            log.error("IncludeCheck: DepManager remote registration failed; aborting before CMake configure.")
            return remote_status
        cmd = ["cmake", "--preset", preset.cmake_preset, "-S", str(root)]
        if preset.cmake_generator not in [None, ""]:
            cmd += ["-G", preset.cmake_generator]
        configure = run_command(cmd, detection_mode=MODE_BY_COLOR)
        if configure != 0:
            log.error("IncludeCheck: CMake configuration failed.")
            return configure
        build_dir = get_build_dir(preset.cmake_preset)
        detection = MODE_FOR_NINJA if preset.cmake_generator and "Ninja" in preset.cmake_generator else MODE_BY_COLOR
        target = args.get("target", CHECK_TARGET)
        # Keep going after the first failure so one run reports every offending file.
        cmd = ["cmake", "--build", str(build_dir), "--target", target]
        if detection == MODE_FOR_NINJA:
            cmd += ["--", "-k", "0"]
        result = run_command(cmd, detection_mode=detection)
        if result != 0:
            log.error("IncludeCheck: a header or a source does not include what it uses (see the errors above).")
            return result
        log.info("IncludeCheck: every header and source compiles on its own against strict libc++.")
        return 0
