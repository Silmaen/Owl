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

Pull requests MCP0 to MCP4 of [Isometric worlds](isometric-worlds.md).

- Built directly into Owl Nest: same process, no separate bridge executable
- Enabled by an option in the editor settings, disabled by default; port and status shown in the status bar
- Listens over HTTP on `127.0.0.1` only (MCP "streamable HTTP" transport); it lives and dies with the editor
- Requests are queued and executed on the main thread during the frame; a job waits while an inspector or gizmo
  gesture or a modal is in progress; pending jobs fail cleanly when the server stops
- Read resources: scene tree, components (YAML), assets, prefabs, documentation pages, performance statistics
- Action tools routed through the command API, hence undoable: create / modify / delete entities, set a component,
  instantiate a prefab, save, undo / redo, run Play for N frames and return a capture plus measurements
- Tests drive the server end to end against a headless editor session

### Command API v2 (MCP0)

Done. At v0.3.0 the registry had 9 commands, arguments without defaults nor choices, and results limited to an
entity id; it now has 13 (see [Editor](../editor.md), Editor commands).

- Argument types `Vec2`, `Vec4`, `Color` and `Enum`; `ArgSpec::defaultValue` (filled in before the command runs)
  and `ArgSpec::choices` (checked), so tool schemas are precise
- `CommandResult::data` carries structured fields (`entity`, `component`, `path`, `can_undo`...)
- New commands: `component.set` (merges a YAML / JSON fragment, recorded as a `ModifyEntityCommand`), `scene.save`,
  `edit.undo`, `edit.redo`; a `CommandSpec` holds either a factory (undo entry) or a runner (save, undo, redo)
- `CommandContext` (scene, undo history, play state, scene file, save callback) is what a command runs on;
  `EditorLayer::runCommand` builds it from the active document. While it plays, every command without
  `allowedInPlay` is refused with `RefusedDuringPlay`
- Every editor document adds its own named commands (the HUD editor first)

### Editor session (MCP1)

Done. `session::EditorSession` (`source/owlnest/sources/session/`, library `OwlNestCommands`, see [Editor](../editor.md),
Editor session) — scene and file, undo, registry and `execute` with the context the session builds, bounded play,
capture, statistics — implemented by `EditorLayer` (delegation to the active scene document) and by `HeadlessSession`
(no window, no ImGui) on the Null renderer for the tests.

- `PlayRequest`: `frames` and / or `ticks`, `timestepMs` (default one tick per frame), held `keys`, `stopAtEnd`,
  `capture`; a shared `PlayDriver` steps it one frame at a time (editor main loop) or in a loop (headless)
- `PlayReport`: frames, ticks, CPU time, draw calls, last phase timings, entity count, optional `FrameCapture`
  (`encodePng` through the engine's `encodeImagePng`); the Null renderer reads back a blank image of the right size
- Held keys go through the input injection, hence the Null input only; input actions in the editor come with IN3 / MCP4
- `EditorLayer::runCommand` is gone: the editor runs commands through `EditorSession::execute`; the scenario runner keeps
  its own registry and undo history (it edits the playing scene on purpose) and shares `findProjectDirectory`

### Protocol and transport (MCP2, MCP3)

- `OwlNestMcp` library: a socket-free JSON-RPC dispatcher (`handle(json) -> json`), `tools/list` generated from the
  registry in its deterministic order, `tools/call`, `resources/list`, `resources/read`, `resources/templates/list`;
  command errors map to tool results with `isError`
- Protocol revision 2025-11-25 (initialize handshake, `Mcp-Session-Id`, JSON responses, no SSE at first), with a
  stateless core that also accepts the 2026-07-28 revision
- `cpp-httplib` (ConanCenter, header-only, no TLS) under the `nest` option; `nlohmann_json` promoted to a direct
  dependency
- Security: bound to `127.0.0.1`; `Host` must be `127.0.0.1:port` or `localhost:port` and any `Origin` loopback
  (DNS rebinding); `Content-Type: application/json` required (no browser simple POST); a 256-bit bearer token
  regenerated per session, stored in a `0600` file next to the editor settings, never logged; body limited to 1 MiB,
  bounded queue and jobs per frame; path arguments canonicalised and kept inside the project; no Lua eval nor shell
  tool; resources returned as plain data, never as instructions

### Play and capture (MCP4)

`play.run { frames, timestep_ms, actions, capture }` plays the scene headlessly at a fixed step with injected input
actions and returns a PNG capture plus phase timings. `doc/pages/mcp.md` documents the client setup
(`claude mcp add --transport http`). The same play state machine drives the v0.9.0 replay tooling.
