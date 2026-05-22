"""
Action to check if the current build is for a draft PR, and abort it if so.
"""

import json
import urllib.error
import urllib.request
from typing import Optional

from ci import log
from ci.actions.base.action import BaseAction
from ci.utils.preset import PresetConfig


class CheckDraft(BaseAction):
    """
    Abort the build if the current PR is in draft state and this buildType
    is not opted in via ``--allow-draft=true``.

    Workaround for the lack of native ``ignoreDrafts`` enforcement and
    ``teamcity.pullRequest.isDraft`` exposure in the current TeamCity
    pull-requests plugin (build 222521 / TC 2026.1).

    Extra arguments (after ``--``):
      * ``--pr=N``           PR number (empty for non-PR builds, action no-ops)
      * ``--repo=owner/name`` GitHub repository (e.g. ``Silmaen/Owl``)
      * ``--token=TOKEN``    GitHub PAT with ``repo`` read scope
      * ``--allow-draft=true`` skip the check for draft-friendly buildTypes

    On draft PR + not allowed, emits a ``##teamcity[buildStop ...]`` service
    message so the build aborts immediately.

    Failure modes are strict: any inability to reach or authenticate against
    the GitHub PR API returns a non-zero exit code, failing the build. This
    is intentional — silently assuming non-draft would let unwanted builds
    run on draft PRs, defeating the purpose of the guard.
    """

    def run(self, preset: PresetConfig, extra_args: Optional[list[str]] = None) -> int:
        parsed = self.parse_extra_args(extra_args)

        pr = parsed.get("pr", "").strip()
        if not pr:
            log.info("Not a PR build (no pr number), skipping draft check.")
            return 0

        if parsed.get("allow-draft") == "true":
            log.info(f"PR #{pr}: this buildType is draft-friendly, continuing.")
            return 0

        token = parsed.get("token", "").strip()
        repo = parsed.get("repo", "").strip()
        if not token or token.startswith("%"):
            log.error("CheckDraft: GitHub token not provided or unresolved.")
            return 1
        if not repo:
            log.error("CheckDraft: repository not provided.")
            return 1

        url = f"https://api.github.com/repos/{repo}/pulls/{pr}"
        req = urllib.request.Request(url, headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        })
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = json.loads(resp.read())
        except urllib.error.HTTPError as e:
            log.error(f"CheckDraft: GitHub API returned HTTP {e.code} for PR #{pr}.")
            return 1
        except Exception as e:
            log.error(f"CheckDraft: failed to query GitHub PR API: {e}")
            return 1

        source_branch = data.get("head", {}).get("ref", "")
        is_draft = bool(data.get("draft", False))
        build_num = parsed.get("build-number", "").strip()

        # Enrich the UI: build number with source branch alongside, draft/ready tag.
        # We keep TC's sequential build counter (TC renders it with a leading
        # "#" automatically, so we don't add one).
        if source_branch and build_num:
            print(f"##teamcity[buildNumber '{build_num} {source_branch}']")
        print(f"##teamcity[addBuildTag '{'draft' if is_draft else 'ready'}']")

        if is_draft:
            log.info(f"PR #{pr} is draft, marking build as skipped.")
            # We do NOT use ##teamcity[buildStop ...] because that always
            # marks the build as cancelled — which GitHub renders as red
            # on the commit status, blocking the PR.
            # Instead we flip a flag and rely on every subsequent step
            # being gated by `equals('skip_pipeline', 'false')`, then set
            # the build status to SUCCESS with a descriptive text.
            print("##teamcity[setParameter name='skip_pipeline' value='true']")
            print(
                "##teamcity[buildStatus status='SUCCESS' "
                "text='OK (Skipped: draft PR)']"
            )
        else:
            log.info(f"PR #{pr} is ready for review, continuing.")
        return 0
