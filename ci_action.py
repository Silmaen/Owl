#!/usr/bin/env python3
"""
Entry point for all the CI actions.
"""


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
