# Editor command API and MCP server {#page-design-mcp-server}

[TOC]

Design page for the editor automation work, summarised in the [Roadmap](../roadmap.md).

## v0.3.0 — editor command API

- Every editor mutation goes through a command executed by `UndoManager` (no direct scene writes from panels)
- Commands are addressable by name with typed arguments, so they can be issued by code, tests and tools
- The same API drives the headless runner and the `owlnest_tests` category
- Base of the MCP server below and of the v0.9.0 replay tooling
- Shipped as `commands::CommandRegistry` (library `OwlNestCommands`, see [Editor](../editor.md), Editor commands).
  Continuous edits previewed live (inspector fields, gizmo, brushes, node moves) stay recorded once the gesture
  ends; their named counterparts (`entity.set_transform`...) are what tools call

## v0.4.0 — MCP server in Owl Nest

- Built directly into Owl Nest: same process, no separate bridge executable
- Enabled by an option in the editor settings, disabled by default
- Listens over HTTP on `127.0.0.1` only (MCP "streamable HTTP" transport); it lives and dies with the editor
- Requests are queued and executed on the main thread during the frame
- Read resources: scene tree, components, assets, prefabs, documentation pages, performance statistics
- Action tools routed through the command API, hence undoable: create / modify / delete entities, instantiate a
  prefab, run Play for N frames and return a capture plus measurements
- Tests drive the server end to end against a headless editor session
