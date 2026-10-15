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

| Function                                                                      | Returns             | Description                                                                                                   |
|-------------------------------------------------------------------------------|---------------------|---------------------------------------------------------------------------------------------------------------|
| `scene.find_entity(name: string)`                                             | `entity_id: entity` | First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, cache the result. |
| `scene.create_entity(name: string)`                                           | `entity_id: entity` | Create an empty entity.                                                                                       |
| `scene.destroy_entity(entity_id: entity)`                                     |                     | Destroy an entity and its children at the end of the frame.                                                   |
| `scene.load_scene(level: string)`                                             |                     | Load another level after this frame, keeping the game state (no transition).                                  |
| `scene.transition_to(scene_path: string, [kind: string], [duration: number])` |                     | Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default).                 |
| `scene.quit()`                                                                |                     | Quit the game (stop Play in the editor) after this frame.                                                     |

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

| Function                                                     | Returns        | Description                                                                                                                                      |
|--------------------------------------------------------------|----------------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| `entity.has_component(entity_id: entity, component: string)` | `has: boolean` | Whether the entity has a component: `Transform`, `PhysicBody`, `SpriteRenderer`, `Camera`, `Text`, `SoundSource`, `Canvas` or a `Ui*` component. |
| `entity.get_name(entity_id: entity)`                         | `name: string` | Tag of the entity (empty when unknown).                                                                                                          |

## `ui`

HUD widgets and screen transitions.

| Function                                                                                                 | Returns             | Description                                                                                                                                          |
|----------------------------------------------------------------------------------------------------------|---------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ui.find(name: string)`                                                                                  | `entity_id: entity` | First widget with this tag, searched in the scene's HUD asset then in the scene (0 when none); cache the result.                                     |
| `ui.set_text(entity_id: entity, text: string)`                                                           |                     | Set the text of a `UiText`.                                                                                                                          |
| `ui.get_text(entity_id: entity)`                                                                         | `text: string`      | Text of a `UiText`.                                                                                                                                  |
| `ui.set_visible(entity_id: entity, visible: boolean)`                                                    |                     | Show or hide the entity in the game.                                                                                                                 |
| `ui.set_progress(entity_id: entity, value: number)`                                                      |                     | Set the value of a `UiProgressBar` (0 to 1).                                                                                                         |
| `ui.raise_window(entity_id: entity)`                                                                     |                     | Draw a `UiWindow` over its siblings (a click on it does the same).                                                                                   |
| `ui.set_window_title(entity_id: entity, title: string)`                                                  |                     | Set the title of a `UiWindow`.                                                                                                                       |
| `ui.set_tooltip(entity_id: entity, text: string)`                                                        |                     | Set the text of a `UiTooltip`.                                                                                                                       |
| `ui.set_tool_cursor(entity_id: entity, active: boolean)`                                                 |                     | Show or hide a `UiToolCursor`, the sprite following the mouse over the HUD.                                                                          |
| `ui.get_slider_value(entity_id: entity)`                                                                 | `value: number`     | Value of a `UiSlider`.                                                                                                                               |
| `ui.set_slider_value(entity_id: entity, value: number)`                                                  |                     | Set the value of a `UiSlider`.                                                                                                                       |
| `ui.set_focus(entity_id: entity)`                                                                        | `focused: boolean`  | Give the keyboard and gamepad focus to a `UiButton` or a `UiSlider` and show it (false when the widget cannot take it); its `UiFocus` callback runs. |
| `ui.get_focus()`                                                                                         | `entity_id: entity` | Widget holding the keyboard and gamepad focus (0 when none).                                                                                         |
| `ui.set_button_enabled(entity_id: entity, enabled: boolean)`                                             |                     | Enable or disable a `UiButton`.                                                                                                                      |
| `ui.transition_fade_in(duration: number)`                                                                |                     | Start a fade-in screen transition.                                                                                                                   |
| `ui.transition_fade_out(duration: number)`                                                               |                     | Start a fade-out screen transition.                                                                                                                  |
| `ui.transition_play(kind: string, duration: number, [r: number], [g: number], [b: number], [a: number])` |                     | Start a screen transition of a kind (see Transition kinds), opaque black by default.                                                                 |
| `ui.is_transition_active()`                                                                              | `active: boolean`   | Whether a screen transition is running.                                                                                                              |

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
