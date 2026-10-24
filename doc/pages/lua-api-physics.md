# Lua API: Physics {#page-lua-api-physics}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `physics`

Box2D body of an entity.

| Function                                                                           | Returns                    | Description                                               |
|------------------------------------------------------------------------------------|----------------------------|-----------------------------------------------------------|
| `physics.impulse(entity_id: entity, fx: number, fy: number)`                       |                            | Apply a linear impulse to the body.                       |
| `physics.get_velocity(entity_id: entity)`                                          | `vx: number`, `vy: number` | Linear velocity of the body (zeros without a body).       |
| `physics.set_velocity(entity_id: entity, vx: number, vy: number)`                  |                            | Set the linear velocity of the body.                      |
| `physics.set_transform(entity_id: entity, x: number, y: number, rotation: number)` |                            | Move the body to a world position and rotation (radians). |
| `physics.set_gravity_scale(entity_id: entity, scale: number)`                      |                            | Scale the world gravity for this body (0 = none).         |
