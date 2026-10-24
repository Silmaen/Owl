# Lua API: Raycast doors and push-walls {#page-lua-api-raycast}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

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
