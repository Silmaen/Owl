# Lua API reference {#page-lua-api}

[TOC]

Every table and function a script can call. This page is generated from the binding registry (`getLuaBindings()` in `source/owl/private/script/LuaBindings.cpp`): do not edit it by hand. When a binding changes, `owl_script_tests_unit_test` fails and writes the new page to the temporary folder (the failure names the file): copy it over this one. See [Lua scripting](scripting.md) for the callbacks, the properties and the sandbox.

An `entity` is the integer UUID of an entity (`entity_id` in its own script, `scene.find_entity` for the others); `[name: type]` is optional.

## `transform`

Local transform of an entity.

| Function                                                                        | Returns                                  | Description                                                    |
|---------------------------------------------------------------------------------|------------------------------------------|----------------------------------------------------------------|
| `transform.get_position(entity_id: entity)`                                     | `x: number`, `y: number`, `z: number`    | Local position of the entity (zeros when it has no transform). |
| `transform.set_position(entity_id: entity, x: number, y: number, z: number)`    |                                          | Set the local position.                                        |
| `transform.get_rotation(entity_id: entity)`                                     | `rx: number`, `ry: number`, `rz: number` | Local rotation, in radians.                                    |
| `transform.set_rotation(entity_id: entity, rx: number, ry: number, rz: number)` |                                          | Set the local rotation, in radians.                            |
| `transform.get_scale(entity_id: entity)`                                        | `sx: number`, `sy: number`, `sz: number` | Local scale.                                                   |
| `transform.set_scale(entity_id: entity, sx: number, sy: number, sz: number)`    |                                          | Set the local scale.                                           |

## `physics`

Box2D body of an entity.

| Function                                                                           | Returns                    | Description                                               |
|------------------------------------------------------------------------------------|----------------------------|-----------------------------------------------------------|
| `physics.impulse(entity_id: entity, fx: number, fy: number)`                       |                            | Apply a linear impulse to the body.                       |
| `physics.get_velocity(entity_id: entity)`                                          | `vx: number`, `vy: number` | Linear velocity of the body (zeros without a body).       |
| `physics.set_velocity(entity_id: entity, vx: number, vy: number)`                  |                            | Set the linear velocity of the body.                      |
| `physics.set_transform(entity_id: entity, x: number, y: number, rotation: number)` |                            | Move the body to a world position and rotation (radians). |
| `physics.set_gravity_scale(entity_id: entity, scale: number)`                      |                            | Scale the world gravity for this body (0 = none).         |

## `input`

Actions of the game's input map, then the raw keyboard, mouse and gamepad state.

| Function                                                                     | Returns                    | Description                                                                                                                               |
|------------------------------------------------------------------------------|----------------------------|-------------------------------------------------------------------------------------------------------------------------------------------|
| `input.has_action(action: string)`                                           | `exists: boolean`          | Whether the game's action map defines an action (a misspelt name reads as released).                                                      |
| `input.is_action_pressed(action: string)`                                    | `pressed: boolean`         | Whether an action is held (a button down, an axis or vector past half deflection).                                                        |
| `input.is_action_just_pressed(action: string)`                               | `pressed: boolean`         | Whether an action went down since the previous tick (in `on_tick`) or frame (in `on_update`).                                             |
| `input.is_action_just_released(action: string)`                              | `released: boolean`        | Whether an action went up since the previous tick (in `on_tick`) or frame (in `on_update`).                                               |
| `input.get_action_value(action: string)`                                     | `value: number`            | Reading of a button (0 or 1) or axis action, in [-1, 1].                                                                                  |
| `input.get_action_vector(action: string)`                                    | `x: number`, `y: number`   | Reading of a vector action in the unit disc (x right, y down).                                                                            |
| `input.is_key_pressed(keycode: integer)`                                     | `pressed: boolean`         | Deprecated, use `input.is_action_pressed`: whether a key is held (GLFW key code: 65 = A, 87 = W, 32 = Space).                             |
| `input.is_mouse_button_pressed(button: integer)`                             | `pressed: boolean`         | Deprecated, use `input.is_action_pressed`: whether a mouse button is held (0 = left, 1 = right, 2 = middle).                              |
| `input.get_mouse_x()`                                                        | `x: number`                | Mouse X position in the window, in pixels.                                                                                                |
| `input.get_mouse_y()`                                                        | `y: number`                | Mouse Y position in the window, in pixels.                                                                                                |
| `input.is_key_just_pressed(keycode: integer)`                                | `pressed: boolean`         | Deprecated, use `input.is_action_just_pressed`: whether a key went down since the previous tick (in `on_tick`) or frame (in `on_update`). |
| `input.is_key_just_released(keycode: integer)`                               | `released: boolean`        | Deprecated, use `input.is_action_just_released`: whether a key went up since the previous tick (in `on_tick`) or frame (in `on_update`).  |
| `input.is_mouse_button_just_pressed(button: integer)`                        | `pressed: boolean`         | Deprecated, use `input.is_action_just_pressed`: whether a mouse button went down since the previous tick or frame.                        |
| `input.is_mouse_button_just_released(button: integer)`                       | `released: boolean`        | Deprecated, use `input.is_action_just_released`: whether a mouse button went up since the previous tick or frame.                         |
| `input.get_mouse_wheel()`                                                    | `dx: number`, `dy: number` | Wheel offset scrolled since the previous tick or frame (horizontal, vertical).                                                            |
| `input.is_gamepad_connected(pad: integer)`                                   | `connected: boolean`       | Whether a gamepad is connected in a slot (0 to 15).                                                                                       |
| `input.get_gamepad_name(pad: integer)`                                       | `name: string`             | Name of the gamepad of a slot, empty when none is connected.                                                                              |
| `input.is_gamepad_button_pressed(pad: integer, button: integer)`             | `pressed: boolean`         | Whether a gamepad button is held (0 = A, 1 = B, 2 = X, 3 = Y, 7 = Start, 11-14 = D-pad).                                                  |
| `input.is_gamepad_button_just_pressed(pad: integer, button: integer)`        | `pressed: boolean`         | Whether a gamepad button went down since the previous tick or frame.                                                                      |
| `input.is_gamepad_button_just_released(pad: integer, button: integer)`       | `released: boolean`        | Whether a gamepad button went up since the previous tick or frame.                                                                        |
| `input.get_gamepad_axis(pad: integer, axis: integer)`                        | `value: number`            | Raw gamepad axis (0-3 = sticks in [-1, 1], y down; 4, 5 = triggers in [0, 1]).                                                            |
| `input.get_gamepad_stick(pad: integer, stick: integer, [dead_zone: number])` | `x: number`, `y: number`   | Gamepad stick (0 = left, 1 = right) through a radial dead zone (0.15 by default), y down.                                                 |

## `sound`

Sound playback, by handle.

| Function                                            | Returns           | Description                                                                               |
|-----------------------------------------------------|-------------------|-------------------------------------------------------------------------------------------|
| `sound.play(asset_path: string)`                    | `handle: integer` | Play a sound asset (loaded on first use); an invalid handle when sound is off or missing. |
| `sound.stop(handle: integer)`                       |                   | Stop a playing sound.                                                                     |
| `sound.pause(handle: integer)`                      |                   | Pause a playing sound.                                                                    |
| `sound.resume(handle: integer)`                     |                   | Resume a paused sound.                                                                    |
| `sound.set_volume(handle: integer, volume: number)` |                   | Set the volume of a sound (0.0 to 2.0).                                                   |

## `scene`

Entities of the running level and level changes.

| Function                                                                                                                          | Returns             | Description                                                                                                                          |
|-----------------------------------------------------------------------------------------------------------------------------------|---------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| `scene.find_entity(name: string)`                                                                                                 | `entity_id: entity` | First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, cache the result.                        |
| `scene.find_entities(name: string)`                                                                                               | `entity_ids: table` | Every entity of the scene with this tag, as an array of ids (empty when none); scans every entity, cache the result.                 |
| `scene.create_entity(name: string)`                                                                                               | `entity_id: entity` | Create an empty entity.                                                                                                              |
| `scene.destroy_entity(entity_id: entity)`                                                                                         |                     | Destroy an entity and its children at the end of the frame.                                                                          |
| `scene.load_scene(level: string)`                                                                                                 |                     | Load another level after this frame, keeping the game state (no transition).                                                         |
| `scene.transition_to(scene_path: string, [kind: string], [duration: number], [r: number], [g: number], [b: number], [a: number])` |                     | Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default), in a colour (opaque black by default). |
| `scene.quit()`                                                                                                                    |                     | Quit the game (stop Play in the editor) after this frame.                                                                            |

## `engine`

The engine running the game.

| Function           | Returns           | Description                                                     |
|--------------------|-------------------|-----------------------------------------------------------------|
| `engine.version()` | `version: string` | Version of the engine running the game, as `major.minor.patch`. |

## `time`

Frame and fixed-tick time.

| Function            | Returns           | Description                                                                                |
|---------------------|-------------------|--------------------------------------------------------------------------------------------|
| `time.delta()`      | `seconds: number` | Duration of the current frame, or of the fixed tick inside `on_tick`, in seconds.          |
| `time.tick_alpha()` | `alpha: number`   | Progress of the frame between the last two fixed ticks, in [0, 1), to blend what is drawn. |

## `log`

Engine log.

| Function                     | Returns | Description                     |
|------------------------------|---------|---------------------------------|
| `log.trace(message: string)` |         | Log a message at trace level.   |
| `log.info(message: string)`  |         | Log a message at info level.    |
| `log.warn(message: string)`  |         | Log a message at warning level. |
| `log.error(message: string)` |         | Log a message at error level.   |

## `entity`

Entity queries.

| Function                                                                           | Returns                                            | Description                                                                                                                                                                                                               |
|------------------------------------------------------------------------------------|----------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `entity.has_component(entity_id: entity, component: string)`                       | `has: boolean`                                     | Whether the entity has a component: `Transform`, `PhysicBody`, `SpriteRenderer`, `MeshRenderer`, `Light`, `Light2D`, `Camera`, `Text`, `SoundSource`, `Canvas` or a `Ui*` component.                                      |
| `entity.get_name(entity_id: entity)`                                               | `name: string`                                     | Tag of the entity (empty when unknown).                                                                                                                                                                                   |
| `entity.get_link(entity_id: entity)`                                               | `linked_id: entity`                                | Entity an `EntityLink` of this entity points at (0 when it has none or the target is missing).                                                                                                                            |
| `entity.set_tint(entity_id: entity, r: number, g: number, b: number, [a: number])` | `done: boolean`                                    | Set the colour multiplied into a `SpriteRenderer`, `AnimatedSpriteRenderer` or `MeshRenderer` (hit flash, fade); false when the entity has none of them. `a` is 1 when omitted.                                           |
| `entity.get_tint(entity_id: entity)`                                               | `r: number`, `g: number`, `b: number`, `a: number` | Colour multiplied into a `SpriteRenderer`, `AnimatedSpriteRenderer` or `MeshRenderer`; nothing when the entity has none of them.                                                                                          |
| `entity.set_mesh(entity_id: entity, path: string)`                                 | `done: boolean`                                    | Draw another mesh file (`.gltf`, `.glb`, `.obj`, asset path) with the entity's `MeshRenderer`, loaded before the next frame; false when the entity has no `MeshRenderer`.                                                 |
| `entity.get_mesh(entity_id: entity)`                                               | `path: string`                                     | Asset path of the mesh file of the entity's `MeshRenderer`; nothing when it has none.                                                                                                                                     |
| `entity.set_material(entity_id: entity, slot: number, path: string)`               | `done: boolean`                                    | Give a submesh slot (0-based, -1 for every slot) of the entity's `MeshRenderer` a `.owlmat` material (asset path, empty for the file's own material); false when the entity has no `MeshRenderer` or the slot is invalid. |
| `entity.get_material(entity_id: entity, slot: number)`                             | `path: string`                                     | Asset path of the `.owlmat` of a submesh slot (0-based) of the entity's `MeshRenderer`, empty for the file's own material; nothing when it has no `MeshRenderer`.                                                         |
| `entity.set_light_color(entity_id: entity, r: number, g: number, b: number)`       | `done: boolean`                                    | Set the colour (sRGB, 0 to 1) of the entity's `Light` or `Light2D`; false when it has none.                                                                                                                               |
| `entity.get_light_color(entity_id: entity)`                                        | `r: number`, `g: number`, `b: number`              | Colour of the entity's `Light` or `Light2D`; nothing when it has none.                                                                                                                                                    |
| `entity.set_light_intensity(entity_id: entity, intensity: number)`                 | `done: boolean`                                    | Set the intensity (at least 0) of the entity's `Light` or `Light2D`; false when it has none.                                                                                                                              |
| `entity.get_light_intensity(entity_id: entity)`                                    | `intensity: number`                                | Intensity of the entity's `Light` or `Light2D`; nothing when it has none.                                                                                                                                                 |
| `entity.set_light_enabled(entity_id: entity, enabled: boolean)`                    | `done: boolean`                                    | Switch the entity's `Light` or `Light2D` on or off (it keeps its settings); false when it has none.                                                                                                                       |
| `entity.is_light_enabled(entity_id: entity)`                                       | `enabled: boolean`                                 | Whether the entity's `Light` or `Light2D` shines; nothing when it has none.                                                                                                                                               |
| `entity.set_light_range(entity_id: entity, range: number)`                         | `done: boolean`                                    | Set the range (world units, at least 0.01) of the entity's point or spot `Light`, or the radius of its `Light2D`; false when it has neither.                                                                              |

## `ui`

HUD widgets and screen transitions.

| Function                                                                                                 | Returns                           | Description                                                                                                                                                                          |
|----------------------------------------------------------------------------------------------------------|-----------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ui.find(name: string)`                                                                                  | `entity_id: entity`               | First widget with this tag, searched in the scene's HUD asset then in the scene (0 when none); cache the result.                                                                     |
| `ui.set_text(entity_id: entity, text: string)`                                                           |                                   | Set the text of a `UiText`.                                                                                                                                                          |
| `ui.get_text(entity_id: entity)`                                                                         | `text: string`                    | Text of a `UiText`.                                                                                                                                                                  |
| `ui.set_visible(entity_id: entity, visible: boolean)`                                                    |                                   | Show or hide the entity in the game.                                                                                                                                                 |
| `ui.set_progress(entity_id: entity, value: number)`                                                      |                                   | Set the value of a `UiProgressBar` (0 to 1).                                                                                                                                         |
| `ui.raise_window(entity_id: entity)`                                                                     |                                   | Draw a `UiWindow` over its siblings (a click on it does the same).                                                                                                                   |
| `ui.set_window_title(entity_id: entity, title: string)`                                                  |                                   | Set the title of a `UiWindow`.                                                                                                                                                       |
| `ui.set_tooltip(entity_id: entity, text: string)`                                                        |                                   | Set the text of a `UiTooltip`.                                                                                                                                                       |
| `ui.set_tool_cursor(entity_id: entity, active: boolean)`                                                 |                                   | Show or hide a `UiToolCursor`, the sprite following the mouse over the HUD.                                                                                                          |
| `ui.get_slider_value(entity_id: entity)`                                                                 | `value: number`                   | Value of a `UiSlider`.                                                                                                                                                               |
| `ui.set_slider_value(entity_id: entity, value: number)`                                                  |                                   | Set the value of a `UiSlider`.                                                                                                                                                       |
| `ui.set_focus(entity_id: entity)`                                                                        | `focused: boolean`                | Give the keyboard and gamepad focus to a `UiButton` or a `UiSlider` and show it (false when the widget cannot take it); its `UiFocus` callback runs.                                 |
| `ui.get_focus()`                                                                                         | `entity_id: entity`               | Widget holding the keyboard and gamepad focus (0 when none).                                                                                                                         |
| `ui.set_button_enabled(entity_id: entity, enabled: boolean)`                                             |                                   | Enable or disable a `UiButton`.                                                                                                                                                      |
| `ui.scroll_get(entity_id: entity)`                                                                       | `x: number`, `y: number`          | Scroll offset of a `UiScrollView` (a `UiList` has `y` only), in reference pixels from the top left of the content.                                                                   |
| `ui.scroll_set(entity_id: entity, x: number, y: number)`                                                 |                                   | Scroll a `UiScrollView` or a `UiList` to an offset, in reference pixels; it is kept inside the content when the HUD is laid out.                                                     |
| `ui.list_set_count(entity_id: entity, count: integer)`                                                   |                                   | Set the number of rows of a `UiList`; the rows kept stay filled, a selection past the end is dropped.                                                                                |
| `ui.list_get_count(entity_id: entity)`                                                                   | `count: integer`                  | Number of rows of a `UiList`.                                                                                                                                                        |
| `ui.list_set_row(entity_id: entity, index: integer, text: string, [detail: string], [icon: string])`     |                                   | Fill a row of a `UiList` (1-based): its text, a right-aligned detail and an icon texture path. Only the rows in view need it, from the list's `onBindCallback` (reads `_row_index`). |
| `ui.list_invalidate(entity_id: entity)`                                                                  |                                   | Forget what the rows of a `UiList` show: the ones in view are bound again.                                                                                                           |
| `ui.list_get_selected(entity_id: entity)`                                                                | `index: integer`                  | Selected row of a `UiList`, 1-based (0 for none).                                                                                                                                    |
| `ui.list_set_selected(entity_id: entity, index: integer)`                                                |                                   | Select a row of a `UiList`, 1-based (0 or out of range for none) and scroll it into view; `onSelectCallback` does not run.                                                           |
| `ui.list_get_visible(entity_id: entity)`                                                                 | `first: integer`, `last: integer` | Rows of a `UiList` in view at the last frame drawn, 1-based and inclusive (0, 0 when none).                                                                                          |
| `ui.transition_fade_in(duration: number)`                                                                |                                   | Start a fade-in screen transition.                                                                                                                                                   |
| `ui.transition_fade_out(duration: number)`                                                               |                                   | Start a fade-out screen transition.                                                                                                                                                  |
| `ui.transition_play(kind: string, duration: number, [r: number], [g: number], [b: number], [a: number])` |                                   | Start a screen transition of a kind (see Transition kinds), opaque black by default.                                                                                                 |
| `ui.is_transition_active()`                                                                              | `active: boolean`                 | Whether a screen transition is running.                                                                                                                                              |
| `ui.is_mouse_over_ui()`                                                                                  | `over: boolean`                   | Whether the mouse is over a HUD widget or window that takes it (a click there is not for the scene).                                                                                 |

## `gamestate`

Values kept across levels and stored in saves.

| Function                                     | Returns      | Description                                                                         |
|----------------------------------------------|--------------|-------------------------------------------------------------------------------------|
| `gamestate.set(key: string, value: any)`     |              | Store a value (integer, number, string or boolean) kept across levels and in saves. |
| `gamestate.get(key: string, [default: any])` | `value: any` | Stored value, or `default` (nil when absent) when the key is missing.               |
| `gamestate.remove(key: string)`              |              | Remove a key.                                                                       |
| `gamestate.clear()`                          |              | Remove every key.                                                                   |

## `save`

Save slots (applied after the frame).

| Function                          | Returns           | Description                                                                        |
|-----------------------------------|-------------------|------------------------------------------------------------------------------------|
| `save.save_game(slot: integer)`   |                   | Save the level and the game state to a slot after this frame.                      |
| `save.load_game(slot: integer)`   |                   | Load a slot after this frame; the level keeps running when the slot does not load. |
| `save.has_save(slot: integer)`    | `exists: boolean` | Whether a slot holds a save.                                                       |
| `save.delete_save(slot: integer)` |                   | Delete the save of a slot.                                                         |
| `save.list_saves()`               | `saves: table`    | Every save, as a list of `{slot, timestamp, scene}`.                               |

## `settings`

Game settings: defaults from `game_settings.yml`, user overrides.

| Function                                    | Returns       | Description                                                                        |
|---------------------------------------------|---------------|------------------------------------------------------------------------------------|
| `settings.get(key: string, [default: any])` | `value: any`  | Setting value: user override, else game default, else `default` (nil when absent). |
| `settings.set(key: string, value: any)`     |               | Set a user override (integer, number, string or boolean).                          |
| `settings.save()`                           | `ok: boolean` | Write the user overrides to `settings.yml`.                                        |
| `settings.load()`                           |               | Reload the user overrides from `settings.yml`.                                     |
| `settings.reset(key: string)`               |               | Remove a user override (back to the game default).                                 |
| `settings.reset_all()`                      |               | Remove every user override.                                                        |
| `settings.apply()`                          |               | Apply the built-in keys to the window and the sound.                               |

## `trigger`

Timer triggers.

| Function                                 | Returns | Description                                     |
|------------------------------------------|---------|-------------------------------------------------|
| `trigger.start_timer(entity_id: entity)` |         | Start or restart a Timer trigger.               |
| `trigger.stop_timer(entity_id: entity)`  |         | Stop a Timer trigger.                           |
| `trigger.reset_timer(entity_id: entity)` |         | Reset the elapsed time of a Timer trigger to 0. |

## `door`

Raycast doors.

| Function                            | Returns         | Description                                                        |
|-------------------------------------|-----------------|--------------------------------------------------------------------|
| `door.activate(entity_id: entity)`  |                 | Open a closed raycast door.                                        |
| `door.close(entity_id: entity)`     |                 | Close an open or opening raycast door.                             |
| `door.is_open(entity_id: entity)`   | `open: boolean` | Whether the raycast door is fully open.                            |
| `door.get_state(entity_id: entity)` | `state: string` | State of the raycast door: `idle`, `opening`, `open` or `closing`. |

## `pushwall`

Raycast push-walls.

| Function                                | Returns          | Description                                          |
|-----------------------------------------|------------------|------------------------------------------------------|
| `pushwall.activate(entity_id: entity)`  |                  | Start pushing an idle raycast push-wall.             |
| `pushwall.has_moved(entity_id: entity)` | `moved: boolean` | Whether the push-wall reached its final position.    |
| `pushwall.get_state(entity_id: entity)` | `state: string`  | State of the push-wall: `idle`, `moving` or `final`. |

## `isometric`

Terrain picking and tile editing of an isometric tilemap.

| Function                                                                                                                      | Returns                                                  | Description                                                                                                                                                                                               |
|-------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `isometric.pick_cell(entity_id: entity, [x: number], [y: number])`                                                            | `cell_x: integer`, `cell_y: integer`                     | Terrain cell under a viewport pixel (the mouse by default), slope-aware; nothing when it is off the map.                                                                                                  |
| `isometric.get_cell(entity_id: entity, layer: integer, cell_x: integer, cell_y: integer)`                                     | `terrain: integer`, `overlay: integer`                   | Terrain and overlay tile of a cell of a layer of the tilemap (layers from 0, -1 for none or outside).                                                                                                     |
| `isometric.set_cell(entity_id: entity, layer: integer, cell_x: integer, cell_y: integer, terrain: integer, overlay: integer)` | `done: boolean`                                          | Set the terrain and overlay tile of a cell of a layer (-1 for none), without undo; false outside the grid or when the cell already held them.                                                             |
| `isometric.get_size(entity_id: entity)`                                                                                       | `width: integer`, `height: integer`, `cell_size: number` | Grid size of the tilemap of an entity, in cells, and the world size of a cell; nothing when the entity has no tilemap.                                                                                    |
| `isometric.get_slope(entity_id: entity, cell_x: integer, cell_y: integer)`                                                    | `slope: integer`                                         | Slope of a cell, from its corner heights (0 is flat, 1 to 18 are the other slopes of the tileset).                                                                                                        |
| `isometric.get_height(entity_id: entity, x: number, y: number)`                                                               | `z: number`                                              | World height of the terrain surface at a world position (the map's own height outside the map).                                                                                                           |
| `isometric.set_sprite_frame(entity_id: entity, frame: integer)`                                                               | `done: boolean`                                          | Show one frame (from 0, row by row) of the sheet of an `AnimatedSpriteRenderer` and stop its animation there, so one entity picks its picture from its heading; false when the entity has no such sprite. |
| `isometric.get_sprite_frame(entity_id: entity)`                                                                               | `frame: integer`                                         | Frame an `AnimatedSpriteRenderer` shows now; nothing when the entity has no such sprite.                                                                                                                  |

## `path`

Shortest paths over the graph a tilemap's `navigation` rules describe (roads, tracks), searched over several frames.

| Function                                                                                                                             | Returns                        | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
|--------------------------------------------------------------------------------------------------------------------------------------|--------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `path.find(entity_id: entity, from_x: integer, from_y: integer, to_x: integer, to_y: integer, [mode: string], [options: table])`     | `request: integer`             | Queue a shortest-path query between two cells of the tilemap's graph (its `navigation` rules); 0 when the entity has no graph. The path is searched over the next frames, `mode` is `auto`, `flat` or `hierarchical`. `options` (a table, which may replace `mode`) is `{no_reverse = true, heading_x = dx, heading_y = dy}`: the route never goes back to the cell it just left, except from a dead end (a cell with a single exit), and its first move goes on in the heading (an axis direction) when there is one; the search is then always flat. |
| `path.find_now(entity_id: entity, from_x: integer, from_y: integer, to_x: integer, to_y: integer, [mode: string], [options: table])` | `cells: table`, `cost: number` | Search a shortest path at once (a long query on a big map costs frame time, prefer `path.find`): the cells as `{x1, y1, x2, y2, ...}` and the cost in cells, nothing when no path joins the cells. `options` as in `path.find`.                                                                                                                                                                                                                                                                                                                        |
| `path.poll(entity_id: entity, request: integer)`                                                                                     | `status: string`               | State of a request: `pending`, `found`, `not_found` or `unknown` (released or never made).                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| `path.get(entity_id: entity, request: integer)`                                                                                      | `cells: table`, `cost: number` | Path of a found request: the cells as `{x1, y1, x2, y2, ...}` (start and goal included) and the cost in cells; nothing otherwise.                                                                                                                                                                                                                                                                                                                                                                                                                      |
| `path.release(entity_id: entity, request: integer)`                                                                                  |                                | Forget a request, pending or finished.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `path.is_valid(entity_id: entity, request: integer, [from_cell: integer])`                                                           | `valid: boolean`               | Whether every step of a found path still exists (false after a road of the path was bulldozed), from its `from_cell`-th cell (1 for the whole path).                                                                                                                                                                                                                                                                                                                                                                                                   |
| `path.get_edge_cost(entity_id: entity, from_x: integer, from_y: integer, to_x: integer, to_y: integer)`                              | `cost: number`                 | Cost, in cells, of the move between two neighbouring cells; nothing when the move does not exist.                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| `path.is_walkable(entity_id: entity, cell_x: integer, cell_y: integer)`                                                              | `walkable: boolean`            | Whether a cell is a node of the graph.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `path.get_version(entity_id: entity)`                                                                                                | `version: integer`             | Counter that grows each time the graph changes (a road built or bulldozed): compare it to know when to search again.                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| `path.set_budget(nodes: integer)`                                                                                                    |                                | Node expansions the scene spends on path requests each frame (4000 by default).                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| `path.set_debug(visible: boolean)`                                                                                                   |                                | Show or hide the navigation overlay over the scene: the graph of every tilemap (green two-way moves, orange one-way moves with an arrow) and the routes given to `path.show_route`. Drawn in the editor's Play view and in the game.                                                                                                                                                                                                                                                                                                                   |
| `path.get_debug()`                                                                                                                   | `visible: boolean`             | Whether the navigation overlay is shown, by a script or by the editor.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `path.show_route(entity_id: entity, route_id: integer, [cells: table], [r: number], [g: number], [b: number])`                       |                                | Give the overlay a route to draw over the tilemap, from its first cell to its last, in a colour (components 0 to 1, white by default): `route_id` names it (a vehicle), a new route with the same id replaces the old one and no cells removes it. Cells are `{x1, y1, x2, y2, ...}` as `path.get` returns them.                                                                                                                                                                                                                                       |
| `path.clear_routes(entity_id: entity)`                                                                                               |                                | Remove every route given to `path.show_route` for the tilemap.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
