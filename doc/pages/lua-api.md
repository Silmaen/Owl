# Lua API reference {#page-lua-api}

[TOC]

Every table and function a script can call, one page per theme. These pages are generated from the binding registry (`getLuaBindings()` in `source/owl/private/script/LuaBindings.cpp`): do not edit them by hand. When a binding changes, `owl_script_tests_unit_test` fails and writes the new pages to `lua-api/` in its working directory (the failure names the folder): copy them over `doc/pages/`. See [Lua scripting](scripting.md) for the callbacks, the properties and the sandbox.

An `entity` is the integer UUID of an entity (`entity_id` in its own script, `scene.find_entity` for the others); `[name: type]` is optional.

## Pages

| Page                                                | Table       | Content                                                                                                              |
|-----------------------------------------------------|-------------|----------------------------------------------------------------------------------------------------------------------|
| [Entities and transforms](lua-api-entities.md)      | `entity`    | Entity queries.                                                                                                      |
|                                                     | `transform` | Local transform of an entity.                                                                                        |
| [Scene, engine and log](lua-api-scene.md)           | `scene`     | Entities of the running level and level changes.                                                                     |
|                                                     | `engine`    | The engine running the game.                                                                                         |
|                                                     | `log`       | Engine log.                                                                                                          |
| [Input](lua-api-input.md)                           | `input`     | Actions of the game's input map, then the raw keyboard, mouse and gamepad state.                                     |
| [Physics](lua-api-physics.md)                       | `physics`   | Box2D body of an entity.                                                                                             |
| [Sound](lua-api-sound.md)                           | `sound`     | Sound playback, by handle.                                                                                           |
| [Time and timers](lua-api-time.md)                  | `time`      | Frame and fixed-tick time.                                                                                           |
|                                                     | `trigger`   | Timer triggers.                                                                                                      |
| [HUD](lua-api-ui.md)                                | `ui`        | HUD widgets and screen transitions.                                                                                  |
| [Game state, saves and settings](lua-api-saves.md)  | `gamestate` | Values kept across levels and stored in saves.                                                                       |
|                                                     | `save`      | Save slots (applied after the frame).                                                                                |
|                                                     | `settings`  | Game settings: defaults from `game_settings.yml`, user overrides.                                                    |
| [Raycast doors and push-walls](lua-api-raycast.md)  | `door`      | Raycast doors.                                                                                                       |
|                                                     | `pushwall`  | Raycast push-walls.                                                                                                  |
| [Isometric terrain and paths](lua-api-isometric.md) | `isometric` | Terrain picking and tile editing of an isometric tilemap.                                                            |
|                                                     | `path`      | Shortest paths over the graph a tilemap's `navigation` rules describe (roads, tracks), searched over several frames. |
