# Visual scripting {#page-design-visual-scripting}

[TOC]

Design page for the Blueprints-style visual scripting, summarised in the [Roadmap](../roadmap.md).

## v0.3.0 — typed Lua binding registry

- One declaration per binding (name, typed parameters, return, documentation) generates the Lua function and its
  reference page
- Documented-but-missing bindings either land or leave the docs (D-07); the unused shared `LuaEngine` goes (D-26)
- The registry is the source of the v0.7.0 node palette
- Shipped as `script::getLuaBindings()` (private header `LuaBindings.h`) and the generated
  [Lua API reference](../lua-api.md); it moves to the public API when the node palette needs it

## v0.7.0 — visual scripting MVP

- Event graphs (`on_create`, `on_update`, triggers, collisions, UI callbacks) edited on `NodeCanvas`
- Graphs compile to Lua, so they run on the existing `ScriptInstance` path and pack like any script
- Node palette generated from the typed binding registry, so every engine API is a node
- Debugging: live values shown on the wires during Play, breakpoints on nodes
- Full editor coverage: select-many, move-group, delete-group, copy / paste, undo

## Node-graph link waypoints (v0.7.0)

- Right-click on a coloured link in `NodeCanvas` (Scene Flow today, future graphs too) to insert an intermediate point
  at the cursor position
- Drag to relocate, Delete to remove — waypoints persist in the `.owlflow` YAML
- The Bezier routing splits at each waypoint into a series of cubic segments so the user can manually steer links
  around node clusters when the auto-deflection heuristic isn't enough

## Script debugging aids (v0.7.0)

- Breakpoint markers (visual only — log-based, not a step debugger); the underlying `TextEditor::AddMarker` API is
  already exposed by the `imgui_color_text_edit` package
- Live variable watch panel (read globals from running `ScriptInstance` via `lua_pushglobaltable` + `lua_next`); it
  shares the value-inspection UI of the visual-scripting debugger
