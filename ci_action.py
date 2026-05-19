#!/usr/bin/env python3
"""
Entry point for all the CI actions.
"""

# Force unbuffered stdout/stderr so TeamCity build logs receive lines as they
# happen rather than only on process exit. This replaces the `python3 -u`
# invocation that broke under `poetry run` 2.x (Poetry intercepts `-u` instead
# of forwarding it to the interpreter, producing
# `The option "-u" does not exist`). Setting `PYTHONUNBUFFERED` here means the
# CI command line can drop the flag entirely and still get streaming logs.
import os
import sys

os.environ.setdefault("PYTHONUNBUFFERED", "1")
try:
    sys.stdout.reconfigure(line_buffering=True)
    sys.stderr.reconfigure(line_buffering=True)
except AttributeError:
    pass


def main():
    from argparse import ArgumentParser
    from ci.actions import get_actions
    from ci import log, Path
    from ci.utils.preset import PresetConfig
    from ci.utils.cmake import list_cmake_presets

    action_list = get_actions()

    name = Path(__file__).name
    parser = ArgumentParser(
        f"{name}",
        description="Select and run a CI action for a given preset.",
        epilog=f"use `{name} Help help` to get more information about available actions and presets.",
    )
    parser.add_argument(
        "action", type=str, choices=action_list.keys(),
        help="The CI action to perform."
    )
    parser.add_argument(
        "preset", type=str, choices=list_cmake_presets(), help="The preset to check."
    )
    parser.add_argument(
        "-v", "--verbose", action="count", help="Enable verbose logging."
    )
    parser.add_argument(
        "-q", "--quiet", action="count", help="Reduce logging verbosity."
    )
    args, remaining = parser.parse_known_args()

    # Filter out the '--' separator if present, keep only --key=value or --flag args
    extra_args = [a for a in remaining if a != "--"]

    # Detect a corrupted / cross-arch Poetry venv before invoking CMake.  We run the check
    # unconditionally (not gated on TeamCity) because TC's Docker-based jobs don't propagate
    # `TEAMCITY_VERSION` into the container: the previous gate silently skipped the protection
    # and let broken venvs reach `poetry sync` (which then no-op'd on the already-synced
    # lockfile).  `needs_refresh` is cheap on the happy path — one `poetry env info --path`
    # call + one file read — and only spawns a full Python import test when the platform
    # signature doesn't match. See `ci/utils/venv.py`.
    from ci.utils.venv import needs_refresh, current_platform_signature
    import os
    if "OWL_CI_REFRESH_VENV" not in os.environ and needs_refresh():
        os.environ["OWL_CI_REFRESH_VENV"] = "1"
        log.info(
            "Poetry venv appears broken for this host — forcing refresh "
            f"(now: {current_platform_signature()})"
        )
        # Actions other than `Build` invoke `poetry run depmanager` (or another venv-resident
        # tool) before any CMake configure step runs — that means `cmake/Poetry.cmake`'s
        # in-CMake `poetry env remove --all` + `poetry sync` would arrive too late and the
        # current process would already have hit `ImportError` on `cryptography/_rust.abi3.so`.
        # Perform the refresh inline so every downstream `poetry run …` invocation in this
        # process sees a freshly-synced venv.  Best-effort: a failure here logs but doesn't
        # abort — CMake will retry the same operations and surface a clearer error on its own
        # path if the underlying problem is unrelated to platform drift.
        from ci.utils.run import run_command
        log.info("Refreshing Poetry venv inline (poetry env remove --all + poetry sync --no-root)...")
        run_command(["poetry", "env", "remove", "--all", "--quiet"])
        sync_status = run_command(["poetry", "sync", "--no-root"])
        if sync_status != 0:
            log.warning(
                f"Inline poetry sync returned {sync_status}; subsequent commands may still fail. "
                "Leaving OWL_CI_REFRESH_VENV=1 so CMake's Poetry.cmake will retry."
            )
        else:
            # Inline refresh succeeded — clear the env var so CMake's Poetry.cmake doesn't
            # run `poetry env remove --all` a second time and destroy the freshly-synced venv.
            os.environ.pop("OWL_CI_REFRESH_VENV", None)
            # Stamp the venv so the next ci_action.py run skips the import probe.
            from ci.utils.venv import get_poetry_venv_path, write_marker
            stamped = get_poetry_venv_path()
            if stamped is not None:
                write_marker(stamped)

    from logging import INFO

    log_level = INFO
    if args.verbose is not None:
        log_level -= 10 * args.verbose
    if args.quiet is not None:
        log_level += 10 * args.quiet
    log.setLevel(log_level)

    action_func = action_list[args.action]
    config = PresetConfig(args.preset)
    result = action_func(config, extra_args if extra_args else None)
    if result != 0:
        log.error(f"Action '{args.action}' failed with exit code: {result}")
    else:
        log.info(f"Action '{args.action}' completed with result: {result}")
    return result


if __name__ == "__main__":
    exit(main())
