# Lua API: Physics {#page-lua-api-physics}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `physics`

Physics body of an entity.

| Function                                                                                                                          | Returns                                  | Description                                                                                      |
|-----------------------------------------------------------------------------------------------------------------------------------|------------------------------------------|--------------------------------------------------------------------------------------------------|
| `physics.impulse(entity_id: entity, fx: number, fy: number, [fz: number])`                                                        |                                          | Apply a linear impulse to the body; fz for a 3D body.                                            |
| `physics.get_velocity(entity_id: entity)`                                                                                         | `vx: number`, `vy: number`, `vz: number` | Linear velocity of the body (zeros without a body); vz for a 3D body.                            |
| `physics.set_velocity(entity_id: entity, vx: number, vy: number, [vz: number])`                                                   |                                          | Set the linear velocity of the body; vz for a 3D body.                                           |
| `physics.set_transform(entity_id: entity, x: number, y: number, rotation_or_z: number, [rx: number], [ry: number], [rz: number])` |                                          | Move the body: x, y and its rotation (radians) in 2D; x, y, z and Euler angles rx, ry, rz in 3D. |
| `physics.set_gravity_scale(entity_id: entity, scale: number)`                                                                     |                                          | Scale the world gravity for this body (0 = none).                                                |
