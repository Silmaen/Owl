---
paths:
  - "source/owl/*/script/**"
  - "test/unit/script_tests/**"
  - "**/*.lua"
---

# Lua scripting (Luau)

User-facing reference: `doc/pages/scripting.md` (guide), `doc/pages/lua-api.md` (index) and the
`doc/pages/lua-api-<theme>.md` pages (both generated, never by hand).

- Public: `ScriptEngine` (static: quotas, property extraction; no scene, no Lua state), `ScriptInstance` (one isolated `lua_State` per entity),
  `ScriptProperty`. Private: `LuaEngine` (raw `lua_State*` wrapper), `LuaBindings`.
- Luau 0.746 (local Conan recipe, built with `LUA_USE_LONGJMP`: a Lua error unwinds like in the reference Lua, so the
  "every `luaL_check*` before an object with a destructor" rule below holds). Headers only through
  `source/owl/private/core/external/lua.h` (diagnostic suppression, two-argument `lua_pushcfunction` /
  `lua_pushcclosure`, `lua_pushglobaltable`).
- Numbers are doubles: whole numbers cross through `script/LuaValues.h` (`pushInteger`, `checkInteger`,
  `optInteger`, `toInteger`, `isInteger`, 64-bit, refusing a fraction), never `lua_pushinteger` (32-bit in Luau).
  Entities cross as handles (`script/EntityHandle.h`: registry id + 1, bit 32 for the HUD, 0 = none), never UUIDs:
  `entityAt` / `pushEntity` in the bindings, `ScriptInstance` converts the UUIDs the host passes.
- **Typed binding registry** (`LuaBindings.h`): every binding is one `LuaBinding` in `declareBindings()`
  (`LuaBindings.cpp`: table, name, C function, description, typed params / returns); `registerBindings` registers
  them and `generateLuaReference()` writes the index `lua-api.md` and one page per `LuaReferencePage`
  (`getLuaReferencePages()`: a new table goes on one of them, a new page also into `doc/help.yml` under *Scripting*).
  Adding or changing a binding: edit the declaration, run `owl_script_tests_unit_test`, then copy the pages the
  failing `LuaBindingRegistry.ReferencePageMatchesTheRegistry` writes to `lua-api/` in its working directory
  (`<build>/test/lua-api/` under ctest) over `doc/pages/`. Every `table.fn(` in `scripting.md` must be bound (tested).
- Tables (`getLuaTables()`): `transform`, `physics`, `input`, `sound`, `scene`, `time`, `log`, `entity`, `ui`,
  `gamestate`, `save`, `settings`, `locale`, `trigger`, `door`, `pushwall`, `isometric`, `path`, `walk`, `nav`, `steer`, `bt`, `animator`,
  `socket`, `spline`, `datatable`, `engine`, `math` (`random` / `randomseed` on the scene stream and the engine's
  elementary functions, over the standard table), `table` (`sorted_keys`, over the standard table).
- Sandbox: only `base`, `table`, `string`, `math`, `utf8`, `coroutine`, `bit32`, `buffer`; never `os` nor `debug`.
  `load` is the engine's (text only, `luau_compile`), `collectgarbage` keeps `count` / `isrunning`. Every chunk goes
  through `loadChunk` (compile, `luau_load`, kept for its breakpoints); no bytecode from a script or a file.
- `require` is the sandbox's own (`LuaEngine::setModuleReader`, not `package`): dotted names of `[A-Za-z0-9_]` words
  only (no path, no `..`), read as `a/b.lua` from the pack then the asset folders, cached per state, under the
  script's quotas; the export packs the required modules. Require modules in `on_create`: Owl Nest runs a script's top
  level alone to read its properties.
- Quotas per `ScriptInstance` (`ScriptQuotas`: 64 MiB, 250 ms per call by default): allocator ceiling (a full GC after
  a refused allocation) + watchdog thread installing the `interrupt` callback on the overdue call only (Luau calls a
  set callback at every loop iteration), which raises at the next safepoint; `protectedCall` removes it. Quota
  exceeded → instance disabled; plain error → logged, retried.
- Every host call into Lua goes through `LuaEngine::protectedCall` (pcall + traceback, the message handler kept in the
  registry: a Luau C function pushed anew allocates a closure); host reads/writes
  of globals are raw (`pushRawGlobal`), never `lua_getglobal` / `lua_getfield` on script tables.
- Registry bindings are light C functions `guarded<fn>` (`LuaEngine::callGuarded`, exception trampoline, no closure
  per state) registered by `LuaEngine::registerTable`; any other C function goes through
  `LuaEngine::registerGuardedTable`. The bound scene and the delta time live in the state's `LuaEngine::Quota`
  (`setHostPointer`, `setDeltaTime`), not in the Lua registry. In a binding,
  call every `luaL_check*` **before** creating any object with a non-trivial destructor: a Lua error
  `longjmp`s over the binding's frame. Throw a C++ exception instead when a check comes later.
- Lifecycle: `Scene::onStartRuntime()` creates each instance bound to its scene (`ScriptInstance::setScene`,
  read by the bindings through `getBoundScene(L)`, never a global); instances call `on_create` /
  `on_update` / `on_destroy`.
- Event callbacks: `on_collision(other_id)` / `on_collision_exit(other_id)` (from `PhysicCommand::takeCollisionEvents(scene)`, dispatched by
  `Scene::dispatchCollisionEvents()` after the physics step), `on_trigger_enter(other_id)` /
  `on_trigger_exit(other_id)` (also from `PhysicBody` sensors) / `on_triggered(other_id)`, `on_timer()`, `on_interact()`. Never dispatch to an
  entity for which `Scene::isPendingDestructionInTree()` is true; destruction from a callback is deferred.
- Properties: `properties = { {name, type, default}, ... }`, parsed by
  `ScriptEngine::extractProperties()`, set as globals before `on_create`.
- At runtime the globals come from the component's serialized `properties:` block in the scene; a property the
  scene does not list gets the `default` of the script (`ScriptInstance::getDeclaredProperties`, applied by
  `loadScriptInstance`). The globals are set after the chunk ran: top-level code cannot read them, only
  `on_create` and later; a script copies them into its locals there.
- Lua keycodes are raw GLFW integers (W=87, Space=32, LShift=340, arrows 262–265).
- Scripts load from the open `.owlpack` first, then the filesystem.
- A script must never trigger a scene load directly: set a request flag (see `save.load_game`) handled
  after `onUpdateRuntime()`.
- Visual scripts (`.owlvs`, `script::visual`): the graph model, the node palette (built-ins + one node per binding,
  `visual/BindingNodes.cpp`, empty without the script module) and the compiler to Lua. `LuaEngine::loadScript` /
  `loadBuffer` compile a `.owlvs` before loading it, so every script path accepts one; a new binding is a new node
  with no other edit. Owl Nest edits them in `VisualScriptDocument` (own `UndoManager<VisualScript>`), drawn by
  `VisualScriptCanvas`, which returns each gesture as an edit and never writes the graph.
- Debugging: `ScriptDebugger` (static: breakpoints per script and line, the pending `BreakpointHit`) is applied by the LuaEngine to each loaded chunk (`lua_breakpoint`, hit on the `debugbreak` callback); a hit never stops mid-call, the game pauses after it. `ScriptInstance::getWatchValues` / `getProbeValues` /
  `hasFunction` feed the Script Debug panel; `visual::compileForDebug` (`CompileMode::Debug`) adds the wire probes
  (`getProbeKey`), never used by the game.
- Graph variables: exposed ones are the `properties` (with `min` / `max` / `step`), the others (`exposed: false`) globals
  set at the top of the chunk. An `exported` `GraphFunction` is a global the engine calls (dialogue conditions), never a
  macro. No emitted statement starts with `(` (Luau would read it as a call of the line before).
- Visual functions and macros: `GraphFunction` / `GraphParameter`, nodes `func.entry`, `func.return`, `func.call`. A call
  with `library` set reads another `.owlvs`, whose functions are compiled into the caller's Lua (no variables, no call
  to another library); `AssetScanner` packs those files.
- Custom nodes (`CustomNode`, `custom.call` / `custom.pure`, value `custom`) compile to a `local function` of the graph;
  annotated Lua functions (`parseLuaFunctions`, `findLuaModule` through the library reader, nodes `lua.call` /
  `lua.pure` with values `module`, `function`) compile to `require("module").name(...)`. The compiler runs twice
  (`compileGraph`): pass one records the function reading each value, pass two keeps single-function values in locals,
  the rest in `__vs`; a branch is written as `if … else … end` up to its merge (the nearest post-dominator of
  both paths, `mergeOf`), then the merge once; a node reached otherwise by several exec paths outside a graph
  function becomes a `__joinN` function.
- HUD script: `.owlhud` key `SceneScript`, `Scene::getHudScript`; a widget callback is looked up in the widget's
  scripts, the HUD script and the scene script, then the entity scripts.
- Type checking: `ScriptTypeChecker` (`script/ScriptTypeChecker.h`, Luau `Analysis`, old solver in non-strict mode)
  checks a script against `getScriptDefinitions()` (one `declare` per registry table, `math` / `table` merged into the
  standard tables, the engine globals of `g_engineGlobals`): a global the engine sets goes there, a binding's types in
  its `LuaValue`s. `ScriptTypeChecker.ShippedScriptsHaveNoTypeError` keeps every game script of the repository clean
  and `EveryGlobalOfTheEngineIsDeclared` every global of a state declared. Owl Nest runs it in `CodeEditorDocument`.
- `ScriptConsole` (`script/ScriptConsole.h`): a Lua state apart from the game's (sandbox rules, no engine binding) for
  the editor REPL and editor scripts; the host registers the functions.
