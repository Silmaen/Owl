# Lua API: Scene, engine and log {#page-lua-api-scene}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `scene`

Entities of the running level and level changes.

| Function                                                                                                                          | Returns                             | Description                                                                                                                                                                                                                                                                                     |
|-----------------------------------------------------------------------------------------------------------------------------------|-------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `scene.find_entity(name: string)`                                                                                                 | `entity_id: entity`                 | First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, cache the result.                                                                                                                                                                                   |
| `scene.find_entities(name: string)`                                                                                               | `entity_ids: table`                 | Every entity of the scene with this tag, as an array of ids (empty when none); scans every entity, cache the result.                                                                                                                                                                            |
| `scene.create_entity(name: string)`                                                                                               | `entity_id: entity`                 | Create an empty entity.                                                                                                                                                                                                                                                                         |
| `scene.destroy_entity(entity_id: entity)`                                                                                         |                                     | Destroy an entity and its children at the end of the frame.                                                                                                                                                                                                                                     |
| `scene.spawn_prefab(prefab_path: string, [x: number], [y: number], [z: number])`                                                  | `entity_id: entity`                 | Instantiate a prefab asset (`.owlprefab`) at runtime, its root placed at a position when one is given; its scripts get `on_create` before or after the scripts of the frame. 0 on failure.                                                                                                      |
| `scene.pick_entity([x: number], [y: number], [tag: string])`                                                                      | `entity_id: entity`                 | Entity drawn under a viewport pixel (the mouse by default), nearest to the viewer: sprites and circles (2D, isometric or billboards) and meshes, not the UI; with a tag, only the entities of that tag, a hit on a descendant giving its tagged ancestor (a tower built of parts). 0 when none. |
| `scene.get_viewport_size()`                                                                                                       | `width: integer`, `height: integer` | Size of the viewport the scene is drawn into, in pixels (the window, or the game panel of Owl Nest); zeros before the first frame.                                                                                                                                                              |
| `scene.get_viewport_mouse()`                                                                                                      | `x: number`, `y: number`            | Mouse position in the viewport, in pixels from its top left (the pixel `scene.pick_entity` reads by default).                                                                                                                                                                                   |
| `scene.load_scene(level: string)`                                                                                                 |                                     | Load another level after this frame, keeping the game state (no transition).                                                                                                                                                                                                                    |
| `scene.transition_to(scene_path: string, [kind: string], [duration: number], [r: number], [g: number], [b: number], [a: number])` |                                     | Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default), in a colour (opaque black by default).                                                                                                                                                            |
| `scene.quit()`                                                                                                                    |                                     | Quit the game (stop Play in the editor) after this frame.                                                                                                                                                                                                                                       |

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
| `log.info(message: string)`  |         | Log a message at info level.    |
| `log.warn(message: string)`  |         | Log a message at warning level. |
| `log.error(message: string)` |         | Log a message at error level.   |
