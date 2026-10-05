---
paths:
  - "source/owl/*/script/**"
  - "test/script_tests/**"
  - "**/*.lua"
---

# Lua scripting (Lua 5.5)

User-facing reference: `doc/pages/scripting.md`. Update it with every binding change.

- Public: `ScriptEngine` (singleton), `ScriptInstance` (one isolated `lua_State` per entity),
  `ScriptProperty`. Private: `LuaEngine` (raw `lua_State*` wrapper), `LuaBindings`.
- Lua headers only through `source/owl/private/core/external/lua.h` (diagnostic suppression).
- Bound tables (`registerTable` in `LuaBindings.cpp`): `transform`, `physics`, `input`, `sound`, `scene`,
  `time`, `log`, `entity`, `ui`, `gamestate`, `save`, `settings`, `trigger`, `door`, `pushwall`.
- Sandbox: only `base`, `table`, `string`, `math`, `utf8`, `coroutine`; `io`, `os`, `dofile`, `loadfile`
  are removed. Never re-open them.
- Lifecycle: `ScriptEngine::init()` in `Scene::onStartRuntime()`; instances call `on_create` /
  `on_update` / `on_destroy`.
- Properties: `properties = { {name, type, default}, ... }`, parsed by
  `ScriptEngine::extractProperties()`, set as globals before `on_create`.
- **The `properties` table is editor metadata only.** At runtime the globals come from the component's
  serialized `properties:` block in the scene; a hand-written `LuaScript:` with only `scriptPath:` leaves
  them `nil`. Add `- name: x / type: float / value: N` entries in the scene YAML.
- Lua keycodes are raw GLFW integers (W=87, Space=32, LShift=340, arrows 262–265).
- Scripts load from the open `.owlpack` first, then the filesystem.
- A script must never trigger a scene load directly: set a request flag (see `save.load_game`) handled
  after `onUpdateRuntime()`.
