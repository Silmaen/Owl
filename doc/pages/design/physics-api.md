# Physics API {#page-design-physics-api}

[TOC]

Design page for the physics work, summarised in the [Roadmap](../roadmap.md). Current behaviour is documented in
[Physics](../physics.md).

## Principle: one API, open to 3D

The physics API is designed once for 2D and 3D: a **backend-independent physics world interface** (bodies, shapes,
queries, joints, contact events, layers) with **Box2D** behind it for 2D and **Jolt** added later for 3D. Gameplay
code, Lua bindings and the editor only see the interface. One physics world per scene (PR-33) replaces the global
`PhysicCommand`.

## v0.3.0 — correctness

- Fixed-step simulation, multi-threaded Box2D solver on Taskflow (PR-22: D-05, P-12, D-14)
- Bodies destroyed with their entity, no ghost collider (PR-13: C-08, D-04)
- `on_collision` implemented from Box2D contact events, with the other entity (D-07, I-02)

## v0.5.0 — 2D physics complete

- Backend-independent physics world interface introduced, Box2D as its first backend
- Collision callbacks
    - `on_collision_enter`, `on_collision_exit`, `on_trigger_enter`, `on_trigger_exit`
    - Collision layers and masks for filtering
- Physics queries
    - Raycast, box cast, circle cast with filtering
    - Overlap queries (find all entities in area)
    - Lua API: `physics.raycast(origin, direction, distance)`
- Joints and constraints
    - Distance, revolute, prismatic, weld joints
    - Motor joints for vehicles/mechanisms
    - Visual joint editor in Owl Nest

## v0.8.0 — 3D physics (Jolt)

- 3D rigid body component on the same interface (2D stays on Box2D)
- Collision shapes: box, sphere, capsule, mesh
- Raycasting queries for gameplay (line-of-sight, ground detection), sphere casts
- Joints and constraints
