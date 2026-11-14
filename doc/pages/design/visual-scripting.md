# Visual scripting {#page-design-visual-scripting}

[TOC]

Design of the Blueprints-style visual scripting and of the typed Lua binding registry it rests on. User guide:
[Visual scripts](../visual-scripts.md).

## Typed Lua binding registry

- One declaration per binding (name, typed parameters, return, documentation) generates the Lua function and its
  reference page
- `script::getLuaBindings()` (private header `LuaBindings.h`) feeds the generated [Lua API reference](../lua-api.md),
  the node palette and the Luau type declarations

## Visual scripts

- Event graphs (`on_create`, `on_update`, triggers, collisions, UI callbacks) edited on a canvas of their own, in
  `.owlvs` files
- Graphs compile to Lua, so they run on the `ScriptInstance` path and pack like any script (`LuaScript`)
- Node palette generated from the typed binding registry, so every engine API is a node
- Colour and vector values, grouped pins split into components; maths, text, table and list nodes, `For Each`,
  `For Each Pair`, `While Loop`, `Break`, `Continue`, `Select`; list variables, internal variables, property bounds
- Functions, macros and library graphs (`GraphFunction`, `GraphParameter`; nodes `func.entry`, `func.return`,
  `func.call`), exported functions (dialogue conditions) and graphs for the script of a HUD, on the same palette and
  compiler
- Owl Nest editor: canvas, node library, inline values, variables, errors, the generated Lua, select-many, move-group,
  delete-group, copy / paste, undo
- Generic graphs wire events; state machines, behaviour trees, dialogues, sequences and PCG have their own graphs, and
  algorithms stay in Lua

## Custom Lua node {#visual-scripting-custom-lua}

Designers and developers script the same game, each in text or in graphs: a graph calls a Lua function, a Lua script
calls a graph function.

- Custom node (`CustomNode`, `custom.call`, `custom.pure`): its author gives the title, the typed input and output pins
  and the Lua code it runs; the compiler writes the code as a local function of the graph
- Library functions: a function of a `.lua` script, annotated with the types of its parameters and returns
  (`---@param`, `---@return`, `---@pure`), shows in the palette like a binding (`lua.call` / `lua.pure`); its code stays
  text, diffed and reviewed as code
- Owl Nest edits the title, the pins and the code of a custom node (inspector and code editor), one undo step each; the
  **Custom** tab of the palette lists the custom nodes and the project modules

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

## Luau {#visual-scripting-luau}

Scripts run on [Luau](https://luau.org), the Lua dialect of Roblox (MIT), derived from Lua 5.1: one VM for the
bindings, the sandbox, `require`, the quotas, the debugger and the console.

| Gain     | What it brings to Owl                                                                                     |
|----------|-----------------------------------------------------------------------------------------------------------|
| Types    | Gradual types (`--!strict` per file): typed pins of the custom nodes, completion and errors in the editor |
| Analysis | The type checker is an embeddable C++ library; type declarations generated from the binding registry      |
| Sandbox  | Built for untrusted code: interrupt callback, memory per category, for the mods                           |
| Speed    | Faster interpreter than Lua 5.x, optional native code on x64 and arm64; the interpreter alone on the Web  |
| Syntax   | Compound assignment, `continue`, `if` expressions, string interpolation, a native `vector` type           |

| Point      | How Owl handles it                                                                                         |
|------------|------------------------------------------------------------------------------------------------------------|
| Numbers    | Doubles only: scripts hold entity handles (`EntityHandle.h`), whole numbers cross through `LuaValues.h`    |
| Debugger   | No `lua_sethook`: breakpoints set per chunk with `lua_breakpoint`, hits on the `debugbreak` callback       |
| Time quota | The watchdog installs the `interrupt` callback on the overdue call, which raises the error at a safepoint  |
| Language   | Lua 5.1 base: no `goto`, no bitwise operators (`bit32`), no `__gc`; chunks compiled by `luau_compile`      |
| Errors     | A Lua error is a C++ exception: unwinding a binding runs the destructors of its objects                    |
| Releases   | Numbered without semantic versioning, a local recipe pinned by `conan.lock`                                |
| Editor     | Types checked in the Owl Nest code editor (`ScriptTypeChecker`, Analysis library)                          |

## Node-graph link waypoints

In the visual scripts, the PCG graphs and every `NodeCanvas` graph:

- Right-click on a link inserts a waypoint at the cursor, in the segment closest to it
- Drag to relocate, Delete to remove; the waypoints of a link whose two nodes move together follow them
- Waypoints persist with the link (`waypoints:` in `.owlvs`, `.owlpcg` and `.owlflow`), each edit one undo step
- The routing splits at each waypoint into cubic segments shaped like a plain link (`getNodeLinkSegments`)

## Script debugging aids

For Lua scripts and visual scripts:

- Breakpoints on Lua lines (gutter, F9) and on nodes (F9, context menu), held by `script::ScriptDebugger`; a hit does
  not stop the script mid-call: the call finishes, the game pauses and the **Script Debug** panel shows the script, the
  line, the entity and the locals, with Continue and Open
- Variable watch (`ScriptInstance::getWatchValues`) of the scene script, the HUD script and every entity script,
  filterable, selected entity first
- Wire values (`ScriptInstance::getProbeValues`, `visual::CompileMode::Debug`, `compileForDebug`, `getProbeKey`) drawn
  on the wires during Play; the node of the hit is outlined
