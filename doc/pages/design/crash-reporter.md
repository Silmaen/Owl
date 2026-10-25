# Crash reporter {#page-design-crash-reporter}

[TOC]

A crash in Owl Nest or an exported game leaves a report the user can pass on; nothing leaves the machine by itself. Planned for v0.8.0, summarised in the [Roadmap](../roadmap.md).

## Plan

| Item          | Plan                                                                                                       |
|---------------|------------------------------------------------------------------------------------------------------------|
| Report folder | Written locally on a crash: stack trace with symbols when available, the log, config, versions, OS and GPU |
| Dialog        | Shows where the folder is                                                                                  |
| GitHub issue  | A button opens a pre-filled issue page in the browser                                                      |
| Privacy       | Nothing is sent or uploaded automatically; the user attaches the report themselves                         |
