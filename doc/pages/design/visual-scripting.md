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
| Maths, text, table and list nodes, `For Each`, `Select`; list variables                  | Done    |
| Internal variables, property bounds, exported functions (dialogue conditions)            | Done    |
| Engine Demo gameplay scripts as graphs, the algorithms left in Lua                       | Done    |

User guide: [Visual scripts](../visual-scripts.md).

## Custom Lua node (v0.7.0) {#visual-scripting-custom-lua}

Designers and developers script the same game, each in text or in graphs: a graph calls a Lua function, a Lua script
calls a graph function.

- Custom node: its author gives the title, the typed input and output pins and the Lua code it runs; the compiler
  inlines the code in the Lua of the graph
- Library functions: a function of a `.lua` script, annotated with the types of its parameters and returns, shows in the
  palette like a binding; its code stays text, diffed and reviewed as code
- Owl Nest edits the title, the pins and the code of a custom node (inspector and code editor), one undo step each
- Generic graphs wire events; state machines, behaviour trees, dialogues, sequences and PCG have their own graphs, and
  algorithms stay in Lua

| Part                                                                                           | Status |
|------------------------------------------------------------------------------------------------|--------|
| `CustomNode` (`custom.call`, `custom.pure`), compiled to a local function of the graph         | Done   |
| Annotated Lua functions (`---@param`, `---@return`, `---@pure`), nodes `lua.call` / `lua.pure` | Done   |
| Owl Nest: **Custom** tab, palette entries of the custom nodes and the project modules          | Done   |

## Generated Lua {#visual-scripting-codegen}

The compiler writes Lua a developer can read in the **Lua** view of Owl Nest:

- Locals and parameters take the names of their pins (`dt`, `other_id`, `result_2`)
- A pure node read by one input is written where it is read; one read twice goes to a local of the statement
- A value read only in the Lua function that writes it is a local of that function, declared at its top; one read
  from another event stays in the slot table `__vs`
- A statement gets a `do ... end` block only when it declares locals; a branch with one side writes `if` or `if not`
- The paths of a branch meet at the nearest node both run (its post-dominator): the branch is an `if` up to it, the
  node follows once
- A node several events reach is written once, as a function `__joinN` each path calls; inside a graph function the
  paths are written out, since the join could not see the parameters
- Two passes: the first records which function reads each value, the second places it; the debug build keeps every
  value in `__vs` for the wire probes

## Luau (v0.7.0) {#visual-scripting-luau}

[Luau](https://luau.org) is the Lua dialect of Roblox (MIT), derived from Lua 5.1. It replaces Lua 5.5 in v0.7.0, before
the mod API, so the scripting API freezes once at 1.0.

| Gain     | What it brings to Owl                                                                                      |
|----------|------------------------------------------------------------------------------------------------------------|
| Types    | Gradual types (`--!strict` per file): typed pins of the custom nodes, completion and errors in the editor  |
| Analysis | The type checker is an embeddable C++ library; type declarations generated from the binding registry       |
| Sandbox  | Built for untrusted code: interrupt callback, memory per category, for the mods                            |
| Speed    | Faster interpreter than Lua 5.x, optional native code on x64 and arm64; the interpreter alone on the Web   |
| Syntax   | Compound assignment, `continue`, `if` expressions, string interpolation, a native `vector` type            |

| Change      | How Owl handles it                                                                                          |
|-------------|-------------------------------------------------------------------------------------------------------------|
| Numbers     | Doubles only: scripts hold entity handles (`EntityHandle.h`), whole numbers cross through `LuaValues.h`     |
| Debugger    | No `lua_sethook`: breakpoints set per chunk with `lua_breakpoint`, hits on the `debugbreak` callback        |
| Time quota  | The watchdog installs the `interrupt` callback on the overdue call, which raises the error at a safepoint   |
| Language    | Lua 5.1 base: no `goto`, no bitwise operators (`bit32`), no `__gc`; chunks compiled by `luau_compile`       |
| Errors      | Luau built with `LUA_USE_LONGJMP`: a Lua error unwinds like in the reference Lua, never as a C++ exception |
| Releases    | Numbered 0.6xx to 0.7xx without semantic versioning, a local recipe pinned by `conan.lock`                  |

| Part                                                                              | Status  |
|-----------------------------------------------------------------------------------|---------|
| Entity handles in place of UUIDs                                                  | Done    |
| VM, bindings, sandbox, `require`, quotas, debugger and console on Luau 0.746      | Done    |
| Engine Demo and samples ported (bitwise masks, 64-bit generators)                 | Done    |
| `script/*` benchmarks against the Lua 5.5 baseline                                | Planned |
| Types checked in the Owl Nest code editor (Analysis library)                      | Done    |

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
