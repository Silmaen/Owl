---
paths:
  - "source/owl/*/script/**"
  - "test/unit/script_tests/**"
  - "**/*.lua"
---

# Lua scripting (Lua 5.5)

User-facing reference: `doc/pages/scripting.md` (guide), `doc/pages/lua-api.md` (index) and the
`doc/pages/lua-api-<theme>.md` pages (both generated, never by hand).

- Public: `ScriptEngine` (static: quotas, property extraction; no scene, no Lua state), `ScriptInstance` (one isolated `lua_State` per entity),
  `ScriptProperty`. Private: `LuaEngine` (raw `lua_State*` wrapper), `LuaBindings`.
- Lua headers only through `source/owl/private/core/external/lua.h` (diagnostic suppression).
- **Typed binding registry** (`LuaBindings.h`): every binding is one `LuaBinding` in `declareBindings()`
  (`LuaBindings.cpp`: table, name, C function, description, typed params / returns); `registerBindings` registers
  them and `generateLuaReference()` writes the index `lua-api.md` and one page per `LuaReferencePage`
  (`getLuaReferencePages()`: a new table goes on one of them, a new page also into `doc/help.yml` under *Scripting*).
  Adding or changing a binding: edit the declaration, run `owl_script_tests_unit_test`, then copy the pages the
  failing `LuaBindingRegistry.ReferencePageMatchesTheRegistry` writes to `lua-api/` in its working directory
  (`<build>/test/lua-api/` under ctest) over `doc/pages/`. Every `table.fn(` in `scripting.md` must be bound (tested).
- Tables (`getLuaTables()`): `transform`, `physics`, `input`, `sound`, `scene`, `time`, `log`, `entity`, `ui`,
  `gamestate`, `save`, `settings`, `trigger`, `door`, `pushwall`, `isometric`, `path`, `engine`.
- Sandbox: only `base`, `table`, `string`, `math`, `utf8`, `coroutine`; `io`, `os`, `dofile`, `loadfile`,
  `string.dump` are removed, `load` is text-only, `setmetatable` refuses `__gc`, `collectgarbage` keeps
  `count` / `isrunning`. Never re-open them. Chunks load in mode `"t"` only (no bytecode).
- `require` is the sandbox's own (`LuaEngine::setModuleReader`, not `package`): dotted names of `[A-Za-z0-9_]` words
  only (no path, no `..`), read as `a/b.lua` from the pack then the asset folders, cached per state, under the
  script's quotas; the export packs the required modules. Require modules in `on_create`: Owl Nest runs a script's top
  level alone to read its properties.
- Quotas per `ScriptInstance` (`ScriptQuotas`: 64 MiB, 250 ms per call by default): allocator ceiling +
  watchdog thread (signal → count hook). Quota exceeded → instance disabled; plain error → logged, retried.
- Every host call into Lua goes through `LuaEngine::protectedCall` (pcall + traceback); host reads/writes
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
- Event callbacks: `on_collision(other_id)` (from `PhysicCommand::takeCollisionEvents(scene)`, dispatched by
  `Scene::dispatchCollisionEvents()` after the physics step), `on_trigger_enter(other_id)` /
  `on_trigger_exit(other_id)` / `on_triggered(other_id)`, `on_timer()`, `on_interact()`. Never dispatch to an
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
