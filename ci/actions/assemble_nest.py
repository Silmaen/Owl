"""
Action completing the Owl Nest packages of a release with the runners: `runners.json` and the Web runner in each.
"""

import json
import tempfile
from pathlib import Path

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig
from ci.utils.publish import get_git_hash, get_project_version
from ci.utils.runners import (
    MANIFEST_NAME,
    TARGETS,
    add_to_nest,
    archive_name,
    download_runner,
    make_manifest,
    web_runner_of,
)


class AssembleNest(BaseAction):
    """
    Action completing the Owl Nest packages of a release with the runners: `runners.json` and the Web runner in each.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Read the runner and Owl Nest archives of one release in a folder, write the completed Nest archives in another.
        :param preset: Unused (the archives come from every package job).
        :param extra_args: Required: --input (the archives of the package jobs), --output. Optional: --hash, --url
            (the delivery server the runners are downloaded from, else they are read in --input).
        :return: Exit code indicating success or failure.
        """
        params = self.parse_extra_args(extra_args)
        if not params.get("input") or not params.get("output"):
            log.error("AssembleNest: --input and --output are required.")
            return 1
        source = Path(params["input"])
        output = Path(params["output"])
        git_hash = (params.get("hash") or get_git_hash())[:7]
        version = get_project_version()
        url = params.get("url")

        # CPack's staging folder holds a copy of each archive.
        nests = sorted(
            path
            for path in source.rglob(f"OwlNest-{version}-{git_hash}-*")
            if path.is_file() and "_CPack_Packages" not in path.parts
        )
        if not nests:
            log.error(f"AssembleNest: no Owl Nest {version}-{git_hash} archive in {source}.")
            return 1

        with tempfile.TemporaryDirectory() as download:
            runners = []
            for target in TARGETS:
                if url:
                    found = download_runner(url, version, git_hash, target, Path(download))
                    if found is None:
                        log.error(f"AssembleNest: cannot download the {target} runner {version}-{git_hash}.")
                        return 1
                else:
                    matches = sorted(source.rglob(archive_name(version, git_hash, target)))
                    if not matches:
                        log.error(f"AssembleNest: no {target} runner {version}-{git_hash} in {source}.")
                        return 1
                    found = matches[0]
                runners.append(found)
            manifest = json.dumps(make_manifest(runners, version, git_hash), indent=2).encode()
            additions = [(manifest, f"bin/{MANIFEST_NAME}"), *web_runner_of(runners[list(TARGETS).index("web")])]
            output.mkdir(parents=True, exist_ok=True)
            (output / MANIFEST_NAME).write_bytes(manifest)
            for nest in nests:
                add_to_nest(nest, additions, output / nest.name)
                log.info(f"AssembleNest: {nest.name} with {MANIFEST_NAME} and the Web runner.")
        return 0
