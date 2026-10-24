# Lua API: Input {#page-lua-api-input}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

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
