# Visual scripting {#page-design-visual-scripting}

[TOC]

Design page for the Blueprints-style visual scripting, summarised in the [Roadmap](../roadmap.md).

## v0.3.0 — typed Lua binding registry

- One declaration per binding (name, typed parameters, return, documentation) generates the Lua function and its
  reference page
- Documented-but-missing bindings either land or leave the docs (D-07); the unused shared `LuaEngine` goes (D-26)
- The registry is the source of the v0.6.0 node palette
- Shipped as `script::getLuaBindings()` (private header `LuaBindings.h`) and the generated
  [Lua API reference](../lua-api.md); it moves to the public API when the node palette needs it

## v0.6.0 — visual scripting MVP

- Event graphs (`on_create`, `on_update`, triggers, collisions, UI callbacks) edited on a canvas of their own
- Graphs compile to Lua, so they run on the existing `ScriptInstance` path and pack like any script
- Node palette generated from the typed binding registry, so every engine API is a node
- Debugging: live values shown on the wires during Play, breakpoints on nodes
- Full editor coverage: select-many, move-group, delete-group, copy / paste, undo
- Functions, macros and library graphs (`GraphFunction`, `GraphParameter`; nodes `func.entry`, `func.return`,
  `func.call`) and graphs for the script of a HUD fold in here, on the same palette and compiler (decided at the v0.4.0
  kickoff)

### Status {#visual-scripting-status}

| Part                                                                                     | Status  |
|------------------------------------------------------------------------------------------|---------|
| `.owlvs` graph, palette from the binding registry, compiler to Lua, `LuaScript` and pack | Done    |
| Colour and vector values, grouped pins split into components                             | Done    |
| Owl Nest editor: canvas, node library, inline values, variables, errors, Lua, undo       | Done    |
| Debugging: live values on the wires, breakpoints on nodes                                | Done    |
| Functions, macros and library graphs                                                     | Done    |
| HUD graphs                                                                               | Done    |

User guide: [Visual scripts](../visual-scripts.md).

### Lua to graph {#visual-scripting-lua-to-graph}

To evaluate after v0.6.0: build a graph from a Lua script, the reverse of the compiler. The subset to support (hooks,
binding calls, `if`, numeric `for`, property reads and writes) and what to do with the rest (one Lua node holding the
code, or a refusal) are the open questions.

## Node-graph link waypoints (v0.6.0)

Shipped, in the visual scripts, the PCG graphs and every `NodeCanvas` graph:

- Right-click on a link inserts a waypoint at the cursor, in the segment closest to it
- Drag to relocate, Delete to remove; the waypoints of a link whose two nodes move together follow them
- Waypoints persist with the link (`waypoints:` in `.owlvs`, `.owlpcg` and `.owlflow`), each edit one undo step
- The routing splits at each waypoint into cubic segments shaped like a plain link (`getNodeLinkSegments`)

## Script debugging aids (v0.6.0)

Shipped, for Lua scripts and visual scripts:

- Breakpoints on Lua lines (gutter, F9) and on nodes (F9, context menu), held by `script::ScriptDebugger`; a hit does
  not stop the script mid-call: the call finishes, the game pauses and the **Script Debug** panel shows the script, the
  line, the entity and the locals, with Continue and Open
- Variable watch (`ScriptInstance::getWatchValues`) of the scene script, the HUD script and every entity script,
  filterable, selected entity first
- Wire values (`ScriptInstance::getProbeValues`, `visual::CompileMode::Debug`, `compileForDebug`, `getProbeKey`) drawn
  on the wires during Play; the node of the hit is outlined
