---
paths:
  - "source/owl/*/script/**"
  - "test/script_tests/**"
  - "**/*.lua"
---

# Lua scripting (Lua 5.5)

User-facing reference: `doc/pages/scripting.md` (guide) and `doc/pages/lua-api.md` (generated, never by hand).

- Public: `ScriptEngine` (static: quotas, property extraction; no scene, no Lua state), `ScriptInstance` (one isolated `lua_State` per entity),
  `ScriptProperty`. Private: `LuaEngine` (raw `lua_State*` wrapper), `LuaBindings`.
- Lua headers only through `source/owl/private/core/external/lua.h` (diagnostic suppression).
- **Typed binding registry** (`LuaBindings.h`): every binding is one `LuaBinding` in `declareBindings()`
  (`LuaBindings.cpp`: table, name, C function, description, typed params / returns); `registerBindings` registers
  them and `generateLuaReference()` writes `doc/pages/lua-api.md`. Adding or changing a binding: edit the
  declaration, then copy the page the failing `LuaBindingRegistry.ReferencePageMatchesTheRegistry` test writes to
  the temporary folder over `doc/pages/lua-api.md`. Every `table.fn(` in `scripting.md` must be bound (tested).
- Tables (`getLuaTables()`): `transform`, `physics`, `input`, `sound`, `scene`, `time`, `log`, `entity`, `ui`,
  `gamestate`, `save`, `settings`, `trigger`, `door`, `pushwall`.
- Sandbox: only `base`, `table`, `string`, `math`, `utf8`, `coroutine`; `io`, `os`, `dofile`, `loadfile`,
  `string.dump` are removed, `load` is text-only, `setmetatable` refuses `__gc`, `collectgarbage` keeps
  `count` / `isrunning`. Never re-open them. Chunks load in mode `"t"` only (no bytecode).
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
- **The `properties` table is editor metadata only.** At runtime the globals come from the component's
  serialized `properties:` block in the scene; a hand-written `LuaScript:` with only `scriptPath:` leaves
  them `nil`. Add `- name: x / type: float / value: N` entries in the scene YAML.
- Lua keycodes are raw GLFW integers (W=87, Space=32, LShift=340, arrows 262–265).
- Scripts load from the open `.owlpack` first, then the filesystem.
- A script must never trigger a scene load directly: set a request flag (see `save.load_game`) handled
  after `onUpdateRuntime()`.
