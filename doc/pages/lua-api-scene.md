# Lua API: Scene, engine and log {#page-lua-api-scene}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `scene`

Entities of the running level and level changes.

| Function                                                                                                                          | Returns                               | Description                                                                                                                                                                                                                                                                                     |
|-----------------------------------------------------------------------------------------------------------------------------------|---------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `scene.get_world_origin()`                                                                                                        | `x: number`, `y: number`, `z: number` | World position of the scene origin, which local positions are relative to.                                                                                                                                                                                                                      |
| `scene.find_entity(name: string)`                                                                                                 | `entity_id: entity`                   | First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, cache the result.                                                                                                                                                                                   |
| `scene.find_entities(name: string)`                                                                                               | `entity_ids: table`                   | Every entity of the scene with this tag, as an array of ids (empty when none); scans every entity, cache the result.                                                                                                                                                                            |
| `scene.create_entity(name: string)`                                                                                               | `entity_id: entity`                   | Create an empty entity.                                                                                                                                                                                                                                                                         |
| `scene.destroy_entity(entity_id: entity)`                                                                                         |                                       | Destroy an entity and its children at the end of the frame.                                                                                                                                                                                                                                     |
| `scene.spawn_prefab(prefab_path: string, [x: number], [y: number], [z: number])`                                                  | `entity_id: entity`                   | Instantiate a prefab asset (`.owlprefab`) at runtime, its root placed at a position when one is given; its scripts get `on_create` before or after the scripts of the frame. 0 on failure.                                                                                                      |
| `scene.pick_entity([x: number], [y: number], [tag: string])`                                                                      | `entity_id: entity`                   | Entity drawn under a viewport pixel (the mouse by default), nearest to the viewer: sprites and circles (2D, isometric or billboards) and meshes, not the UI; with a tag, only the entities of that tag, a hit on a descendant giving its tagged ancestor (a tower built of parts). 0 when none. |
| `scene.is_world_cell_loaded(x: integer, y: integer, z: integer)`                                                                  | `loaded: boolean`                     | Whether the world partition cell at integer coordinates is merged into the scene.                                                                                                                                                                                                               |
| `scene.get_loaded_world_cells()`                                                                                                  | `count: integer`                      | Number of world partition cells merged into the scene.                                                                                                                                                                                                                                          |
| `scene.set_world_streaming(enabled: boolean)`                                                                                     |                                       | Switch the streaming of the world partition on or off; off unloads every cell.                                                                                                                                                                                                                  |
| `scene.get_planet_altitude(planet_id: entity, entity_id: entity)`                                                                 | `altitude: number`                    | Height of an entity above the ground (or the sea) of a planet, in metres; negative below it, 0 when either entity is missing.                                                                                                                                                                   |
| `scene.get_viewport_size()`                                                                                                       | `width: integer`, `height: integer`   | Size of the viewport the scene is drawn into, in pixels (the window, or the game panel of Owl Nest); zeros before the first frame.                                                                                                                                                              |
| `scene.get_viewport_mouse()`                                                                                                      | `x: number`, `y: number`              | Mouse position in the viewport, in pixels from its top left (the pixel `scene.pick_entity` reads by default).                                                                                                                                                                                   |
| `scene.load_scene(level: string)`                                                                                                 |                                       | Load another level after this frame, keeping the game state (no transition).                                                                                                                                                                                                                    |
| `scene.transition_to(scene_path: string, [kind: string], [duration: number], [r: number], [g: number], [b: number], [a: number])` |                                       | Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default), in a colour (opaque black by default).                                                                                                                                                            |
| `scene.quit()`                                                                                                                    |                                       | Quit the game (stop Play in the editor) after this frame.                                                                                                                                                                                                                                       |

## `engine`

The engine running the game.

| Function           | Returns           | Description                                                     |
|--------------------|-------------------|-----------------------------------------------------------------|
| `engine.version()` | `version: string` | Version of the engine running the game, as `major.minor.patch`. |

## `log`

Engine log.

| Function                     | Returns | Description                     |
|------------------------------|---------|---------------------------------|
| `log.trace(message: string)` |         | Log a message at trace level.   |
| `log.debug(message: string)` |         | Log a message at debug level.   |
| `log.info(message: string)`  |         | Log a message at info level.    |
| `log.warn(message: string)`  |         | Log a message at warning level. |
| `log.error(message: string)` |         | Log a message at error level.   |

## `math`

Random values drawn from the scene seed and elementary functions with the same bits on every target, over the Lua standard `math` table.

| Function                                  | Returns         | Description                                                                                                                                                                       |
|-------------------------------------------|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `math.random([m: integer], [n: integer])` | `value: number` | Random value of the `lua` stream of the scene, the same on every run of the same seed: a number in [0, 1) without argument, an integer in [1, m] or [m, n], 64 random bits for 0. |
| `math.randomseed([seed: integer])`        |                 | Restart the `lua` stream from a seed, or from the seed of the scene without argument.                                                                                             |
| `math.sin(x: number)`                     | `value: number` | Sine of an angle in radians, the same bits on every target.                                                                                                                       |
| `math.cos(x: number)`                     | `value: number` | Cosine of an angle in radians, the same bits on every target.                                                                                                                     |
| `math.tan(x: number)`                     | `value: number` | Tangent of an angle in radians, the same bits on every target.                                                                                                                    |
| `math.asin(x: number)`                    | `angle: number` | Arc sine in radians, the same bits on every target.                                                                                                                               |
| `math.acos(x: number)`                    | `angle: number` | Arc cosine in radians, the same bits on every target.                                                                                                                             |
| `math.atan(y: number, [x: number])`       | `angle: number` | Arc tangent of y / x in radians, in the quadrant of (x, y) (x is 1 when omitted), the same bits on every target.                                                                  |
| `math.exp(x: number)`                     | `value: number` | e to the power x, the same bits on every target.                                                                                                                                  |
| `math.log(x: number, [base: number])`     | `value: number` | Logarithm of x, natural or in a base, the same bits on every target.                                                                                                              |
| `math.pow(x: number, y: number)`          | `value: number` | x to the power y, the same bits on every target, unlike the `^` operator which uses the platform's maths.                                                                         |

## `table`

Sorted iteration over the Lua standard `table` table.

| Function                      | Returns       | Description                                                                                                                                                                                                                                                    |
|-------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `table.sorted_keys(t: table)` | `keys: table` | Keys of a table in a fixed order (booleans, then numbers ascending, then strings bytewise), the same on every run whatever the insertion history: `for _, k in ipairs(table.sorted_keys(t))`. A key that is a table, a function or a userdata raises an error. |
