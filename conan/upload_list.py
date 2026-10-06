"""
Package list of the binaries a build should push to the Conan binary cache.

Called by cmake/Conan.cmake after `conan install`: keeps, from the local cache listing, only the binaries of
Owl's dependency graph that did not come from a remote (built here, now or by an earlier run). The agents'
Conan cache is shared with other projects, whose packages must not be pushed.

Usage: python upload_list.py <graph.json> <local.json> <out.json>
  graph.json  `conan graph info --format=json` of the build
  local.json  `conan list "*#*:*#latest" --format=json` of the local cache
  out.json    package list for `conan upload --list`
"""

import json
import sys
from pathlib import Path

# Binary statuses of `conan graph info` for a package not taken from a remote.
LOCAL_STATUSES = {"Build", "Missing", "Cache"}


def wanted_binaries(graph: dict) -> set[tuple[str, str]]:
    """
    Binaries of the graph that the build had to produce or found only in the local cache.

    :param graph: Parsed `conan graph info --format=json` output.
    :return: (name/version#rrev, package_id) pairs.
    """
    wanted: set[tuple[str, str]] = set()
    for node in graph.get("graph", {}).get("nodes", {}).values():
        ref, package_id = node.get("ref", ""), node.get("package_id")
        if "#" not in ref or not package_id or node.get("binary") not in LOCAL_STATUSES:
            continue
        wanted.add((ref.split("%")[0], package_id))
    return wanted


def filter_listing(listing: dict, wanted: set[tuple[str, str]]) -> dict:
    """
    Restrict a local cache listing to the wanted binaries.

    :param listing: Parsed `conan list --format=json` output.
    :param wanted: (name/version#rrev, package_id) pairs to keep.
    :return: Package list in the same format, holding only those binaries.
    """
    kept: dict = {}
    for ref, recipe in listing.get("Local Cache", {}).items():
        for rrev, revision in recipe.get("revisions", {}).items():
            packages = {
                package_id: package
                for package_id, package in revision.get("packages", {}).items()
                if (f"{ref}#{rrev}", package_id) in wanted and package.get("revisions")
            }
            if packages:
                kept.setdefault(ref, {"revisions": {}})["revisions"][rrev] = {**revision, "packages": packages}
    return {"Local Cache": kept}


def main(argv: list[str]) -> int:
    """
    Write the filtered package list.

    :param argv: Command-line arguments (graph, local listing, output).
    :return: 0 on success, 1 on wrong usage.
    """
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 1
    graph = json.loads(Path(argv[1]).read_text(encoding="utf-8"))
    listing = json.loads(Path(argv[2]).read_text(encoding="utf-8"))
    result = filter_listing(listing, wanted_binaries(graph))
    Path(argv[3]).write_text(json.dumps(result, indent=2), encoding="utf-8")
    count = sum(len(r["packages"]) for v in result["Local Cache"].values() for r in v["revisions"].values())
    print(f"Conan binary cache: {count} binaries of the graph to push.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
