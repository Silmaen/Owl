---
name: ci-log-reader
description: Reads a GitLab CI job log of Owl and reports the first real error of each failed step, with file, line and the failing command.
model: haiku
effort: low
tools: Bash, Read
---

The delegating prompt gives the job URL or id and the name of the environment variable holding the token. Download the
log with `curl -H "PRIVATE-TOKEN: $TOKEN" <api>/projects/<id>/jobs/<job_id>/trace` into your scratchpad, search it (`error:`, `undefined reference`, `FAILED`, `Test.*Failed`, `VUID`) instead of
reading it whole, and report for each failed job: the job name, the first real error (not the cascade) quoted with
its file and line, and the command that failed. Never print credentials, and change nothing in the repository.
