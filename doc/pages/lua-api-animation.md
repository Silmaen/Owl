# Lua API: Animation and sockets {#page-lua-api-animation}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `animator`

State machine of the `Animator` of a skinned mesh: parameters and states.

| Function                                                             | Returns         | Description                                                                                                  |
|----------------------------------------------------------------------|-----------------|--------------------------------------------------------------------------------------------------------------|
| `animator.set_float(entity_id: entity, name: string, value: number)` | `done: boolean` | Set a number parameter of the entity's `Animator` state machine; false when it has no such parameter.        |
| `animator.set_bool(entity_id: entity, name: string, value: boolean)` | `done: boolean` | Set a flag parameter of the entity's `Animator` state machine; false when it has no such parameter.          |
| `animator.trigger(entity_id: entity, name: string)`                  | `done: boolean` | Fire a trigger parameter of the entity's `Animator`: it stays set until a transition uses it.                |
| `animator.get_parameter(entity_id: entity, name: string)`            | `value: number` | Value of a parameter of the entity's `Animator` (1 or 0 for a flag); nothing when it has none.               |
| `animator.get_state(entity_id: entity)`                              | `state: string` | Name of the current state of the entity's `Animator`; nothing before its first frame.                        |
| `animator.play(entity_id: entity, state: string, [fade: number])`    | `done: boolean` | Cross-fade the entity's `Animator` to a state, whatever the transitions, in `fade` seconds (0.2 by default). |
| `animator.set_playing(entity_id: entity, playing: boolean)`          | `done: boolean` | Run or hold the entity's `Animator` (held, the mesh keeps its pose); false when it has none.                 |

## `socket`

Entities snapped on the sockets of a mesh or of an entity.

| Function                                                          | Returns                               | Description                                                                                                                                  |
|-------------------------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| `socket.attach(entity_id: entity, owner: entity, socket: string)` | `done: boolean`                       | Snap the entity on a socket of `owner`: it becomes its child and follows the socket (and its bone); false when the owner has no such socket. |
| `socket.detach(entity_id: entity)`                                | `done: boolean`                       | Release the entity from its socket: it keeps its parent and stays where it is; false when it was attached to none.                           |
| `socket.get_position(entity_id: entity, socket: string)`          | `x: number`, `y: number`, `z: number` | World position of a socket of the entity, following its bone; nothing when it has no such socket.                                            |
