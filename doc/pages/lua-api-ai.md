# Lua API: Steering and behaviour trees {#page-lua-api-ai}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `steer`

Steering behaviours of the `SteeringAgent`s: targets, weights, formations, on the 2D plane, the ground or in space.

| Function                                                                               | Returns                               | Description                                                                                                                                                                                                                                        |
|----------------------------------------------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `steer.set_target(entity_id: entity, target: entity)`                                  | `done: boolean`                       | Entity a `SteeringAgent` seeks, flees, reaches, pursues or evades; false when the entity has no agent or the target does not exist.                                                                                                                |
| `steer.set_target_point(entity_id: entity, x: number, y: number, z: number)`           | `done: boolean`                       | World point a `SteeringAgent` seeks, flees or reaches, in place of a target entity; false when the entity has no agent.                                                                                                                            |
| `steer.set_weight(entity_id: entity, behaviour: string, weight: number)`               | `done: boolean`                       | Weight of a behaviour of a `SteeringAgent` (`seek`, `flee`, `arrive`, `pursue`, `evade`, `wander`, `separation`, `alignment`, `cohesion`, `avoidance`, `formation`), 0 to leave it out; false for an unknown behaviour or an entity without agent. |
| `steer.get_weight(entity_id: entity, behaviour: string)`                               | `weight: number`                      | Weight of a behaviour of a `SteeringAgent`; nothing for an unknown behaviour or an entity without agent.                                                                                                                                           |
| `steer.get_velocity(entity_id: entity)`                                                | `x: number`, `y: number`, `z: number` | Velocity of a `SteeringAgent` at the last tick, in world units per second; nothing without agent.                                                                                                                                                  |
| `steer.set_max_speed(entity_id: entity, speed: number)`                                |                                       | Highest speed of a `SteeringAgent`, in world units per second.                                                                                                                                                                                     |
| `steer.set_leader(entity_id: entity, leader: entity, x: number, y: number, z: number)` | `done: boolean`                       | Put a `SteeringAgent` in the formation of a leader, at a slot in the frame of the leader (its weight `formation` must be above 0); false when the leader does not exist.                                                                           |

## `bt`

Behaviour trees of the `BehaviourTreeAgent`s: their blackboard, their status, the tree they run.

| Function                                                   | Returns          | Description                                                                                                                                                                                                        |
|------------------------------------------------------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `bt.set_value(entity_id: entity, key: string, value: any)` | `done: boolean`  | Write a value on the blackboard of a `BehaviourTreeAgent` (a boolean, a number, a string, a table `{x, y, z}`; an entity for a key holding one); false when the key holds another kind or the entity has no agent. |
| `bt.get_value(entity_id: entity, key: string)`             | `value: any`     | A value of the blackboard of a `BehaviourTreeAgent`, a vector as a table `{x, y, z}`; nothing when the key holds nothing.                                                                                          |
| `bt.get_status(entity_id: entity)`                         | `status: string` | What the tree of a `BehaviourTreeAgent` returned at the last tick: `idle`, `running`, `success` or `failure`.                                                                                                      |
| `bt.restart(entity_id: entity)`                            |                  | Stop the tree of a `BehaviourTreeAgent` and start it again from the root at the next tick, its blackboard back to its first values.                                                                                |
| `bt.set_tree(entity_id: entity, tree: string)`             | `done: boolean`  | Run another `.owlbt` tree on a `BehaviourTreeAgent` from the next tick; false when the entity has no agent.                                                                                                        |
