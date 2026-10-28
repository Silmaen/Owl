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
- Node editing of other assets (scripts first) folds in here, on the same palette and compiler (decided at the v0.4.0
  kickoff)

### Status {#visual-scripting-status}

| Part                                                                                     | Status  |
|------------------------------------------------------------------------------------------|---------|
| `.owlvs` graph, palette from the binding registry, compiler to Lua, `LuaScript` and pack | Done    |
| Colour and vector values, grouped pins split into components                             | Done    |
| Owl Nest editor: canvas, node library, inline values, variables, errors, Lua, undo       | Done    |
| Debugging: live values on the wires, breakpoints on nodes                                | Planned |
| Node editing of other assets                                                             | Planned |

User guide: [Visual scripts](../visual-scripts.md).

### Lua to graph {#visual-scripting-lua-to-graph}

To evaluate after v0.6.0: build a graph from a Lua script, the reverse of the compiler. The subset to support (hooks,
binding calls, `if`, numeric `for`, property reads and writes) and what to do with the rest (one Lua node holding the
code, or a refusal) are the open questions.

## Node-graph link waypoints (v0.6.0)

- Right-click on a coloured link in `NodeCanvas` (visual scripts and any other graph) to insert an intermediate point
  at the cursor position
- Drag to relocate, Delete to remove — waypoints persist in the `.owlflow` YAML
- The Bezier routing splits at each waypoint into a series of cubic segments so the user can manually steer links
  around node clusters when the auto-deflection heuristic isn't enough

## Script debugging aids (v0.6.0)

- Breakpoint markers (visual only — log-based, not a step debugger); the underlying `TextEditor::AddMarker` API is
  already exposed by the `imgui_color_text_edit` package
- Live variable watch panel (read globals from running `ScriptInstance` via `lua_pushglobaltable` + `lua_next`); it
  shares the value-inspection UI of the visual-scripting debugger
