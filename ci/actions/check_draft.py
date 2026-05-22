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
            log.warning("CheckDraft: no GitHub token provided, assuming non-draft.")
            return 0
        if not repo:
            log.warning("CheckDraft: no repo provided, assuming non-draft.")
            return 0

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
            log.warning(f"CheckDraft: GitHub API returned HTTP {e.code} — assuming non-draft.")
            return 0
        except Exception as e:
            log.warning(f"CheckDraft: failed to query GitHub PR API ({e}) — assuming non-draft.")
            return 0

        if data.get("draft", False):
            log.info(f"PR #{pr} is draft, aborting non-essential build.")
            print(
                "##teamcity[buildStop comment="
                "'Draft PR -- skipping non-essential build' "
                "readdToQueue='false']"
            )
        else:
            log.info(f"PR #{pr} is ready for review, continuing.")
        return 0
