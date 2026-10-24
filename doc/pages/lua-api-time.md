# Lua API: Time and timers {#page-lua-api-time}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `time`

Frame and fixed-tick time.

| Function                           | Returns           | Description                                                                                                                                                      |
|------------------------------------|-------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `time.delta()`                     | `seconds: number` | Duration of the current frame, or of the fixed tick inside `on_tick`, in seconds.                                                                                |
| `time.real_delta()`                | `seconds: number` | Real duration of the current frame, in seconds, before the time scale and the pause (menus).                                                                     |
| `time.set_scale(scale: number)`    |                   | Speed of the scene clock (0 to 16, 1 by default): `time.delta`, the ticks, the physics, the animations and the behaviours follow it; the UI keeps the real time. |
| `time.get_scale()`                 | `scale: number`   | Speed of the scene clock set by `time.set_scale`.                                                                                                                |
| `time.set_paused(paused: boolean)` |                   | Pause or resume the scene clock: paused, `on_update` still runs with a zero `time.delta`, no tick, physics step nor animation runs.                              |
| `time.is_paused()`                 | `paused: boolean` | Whether the scene clock is paused.                                                                                                                               |
| `time.tick_alpha()`                | `alpha: number`   | Progress of the frame between the last two fixed ticks, in [0, 1), to blend what is drawn.                                                                       |

## `trigger`

Timer triggers.

| Function                                 | Returns | Description                                     |
|------------------------------------------|---------|-------------------------------------------------|
| `trigger.start_timer(entity_id: entity)` |         | Start or restart a Timer trigger.               |
| `trigger.stop_timer(entity_id: entity)`  |         | Stop a Timer trigger.                           |
| `trigger.reset_timer(entity_id: entity)` |         | Reset the elapsed time of a Timer trigger to 0. |
