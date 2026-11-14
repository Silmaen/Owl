"""
Action starting the full pipeline of the merge requests marked ready since their last pipeline.
"""

from ci import log
from ci.actions.base.action import BaseAction, PresetConfig


class ReadyPipelines(BaseAction):
    """
    Start a pipeline on every open merge request marked ready whose last pipeline is the draft one of the same commit:
    GitLab starts none when a draft is marked ready, so the jobs a draft leaves to their ▶ would never run before the
    merge. Run by the `ready-watch` schedule; the new pipeline stands on the draft's successes for the jobs both ran
    (`Reuse`, `BuildAndTest`), so only the jobs the draft skipped build.

    Needs `OWL_GITLAB_API_TOKEN` (`read_api`) to read and `OWL_GITLAB_PIPELINE_TOKEN` (`api`, Developer) to start a
    pipeline; without them it says so and does nothing.
    """

    def run(self, preset: PresetConfig, extra_args=None) -> int:
        """
        Find the merge requests to run again and start their pipelines.
        :param preset: Unused, kept for the common action signature.
        :param extra_args: Optional extra arguments (unused).
        :return: 1 when a pipeline could not be started, else 0.
        """
        from ci.utils.gitlab import merge_requests_to_rerun, start_merge_request_pipeline

        result = 0
        merge_requests = merge_requests_to_rerun()
        if not merge_requests:
            log.info("ReadyPipelines: no merge request marked ready since its draft pipeline.")
        for iid in merge_requests:
            pipeline = start_merge_request_pipeline(iid)
            if pipeline is None:
                log.error(f"ReadyPipelines: cannot start the pipeline of !{iid} (OWL_GITLAB_PIPELINE_TOKEN, `api`).")
                result = 1
            else:
                log.info(f"ReadyPipelines: !{iid} marked ready, pipeline #{pipeline} started.")
        return result
