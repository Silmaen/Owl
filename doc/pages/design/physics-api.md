# Physics API {#page-design-physics-api}

[TOC]

Design page for the physics work, summarised in the [Roadmap](../roadmap.md). Current behaviour is documented in
[Physics](../physics.md).

## Principle: one engine, one API, determinism first

**Jolt** is the single physics engine, for 3D and for 2D. A 2D body is a Jolt body locked to the XY plane
(`EAllowedDOFs::Plane2D`): 2D and 3D bodies live in the same world and collide with each other, which is what a scene
mixing rendering styles needs. Gameplay code, Lua bindings and the editor see one **backend-independent physics world
interface** (bodies, shapes, queries, joints, contact events, layers). Each scene owns its physics world, reached
through `PhysicCommand` with the scene.

Determinism outranks speed: the same inputs give the same world, bit for bit, on every target
([Simulation](simulation-networking.md#simulation-cross-platform)). No build option, CPU feature or platform may change
a simulation result.

## Decisions

| Topic               | Decision                                                                                                                       |
|---------------------|--------------------------------------------------------------------------------------------------------------------------------|
| Engine              | Jolt (MIT): cross-platform determinism, MinGW, Android and WASM, mutable compounds, constraints, state save                    |
| Rejected            | PhysX (determinism on one machine only, no MinGW nor Web), Bullet (maintenance mode), Rapier (Rust toolchain)                  |
| 2D                  | Jolt bodies locked to the XY plane; Box2D removed                                                                              |
| Lua API             | One `physics` table for 2D and 3D: the dimension follows the vector type (`vec2` or `vec3`)                                    |
| Precision           | Single precision; large worlds go through the floating origin ([Large worlds](large-worlds.md)), no `JPH_DOUBLE_PRECISION`     |
| Determinism         | `JPH_CROSS_PLATFORM_DETERMINISTIC`, no FMA, `-ffp-contract=off`                                                                |
| x86_64 instructions | One fixed level for every build: SSE4.2 (x86-64-v2); no AVX2 variant, no runtime dispatch                                      |
| arm64, Web          | NEON (mandatory on ARMv8-A); WASM SIMD128                                                                                      |
| Unsupported CPU     | An x86_64 CPU without SSE4.2 runs with physics off and a warning, never an illegal instruction                                 |
| Package             | Local recipe `conan/recipes/joltphysics/`: the ConanCenter one disables determinism and pushes `-mavx2 -mfma` to its consumers |
| Threads             | Jolt's job system on the Taskflow solver executor; results independent of the worker count                                     |
| Rewind and network  | Jolt `SaveState` / `RestoreState` for the snapshots of rewind and rollback                                                     |
| Check               | Frozen world hash of a Jolt scene, the same value on x86_64 and arm64, with 1 and several workers                              |

## v0.3.0 — correctness

- Fixed-step simulation: accumulator, bounded steps per frame, interpolated transforms, per-scene settings
  (PR-22: D-05) — done
- Multi-threaded Box2D solver on a dedicated Taskflow executor, automatic above 2 000 dynamic bodies
  (PR-22: P-12, D-14) — done; Box2D built with AVX2 on x86_64 (`OWL_PHYSICS_AVX2`) — done
- Bodies destroyed with their entity, no ghost collider (PR-13: C-08, D-04)
- `on_collision` implemented from Box2D contact events, with the other entity (D-07, I-02) — done

## v0.7.0 — Jolt and the physics interface

| Step                                                                                       | Status      |
|--------------------------------------------------------------------------------------------|-------------|
| Local Jolt recipe (deterministic, SSE4.2, no consumer flags), world-hash golden on Jolt    | Done        |
| Backend-independent physics world interface, Jolt behind it                                | Done        |
| 3D rigid body component and shapes, editor support                                         | Done        |
| 2D bodies on Jolt (plane-locked), the 2D scenes and the world-hash golden moved, Box2D out | Done        |
| Rewind snapshots through `SaveState` / `RestoreState`                                      | Planned     |

## v0.7.0 — 2D physics complete

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

## v0.7.0 — 3D physics

- 3D rigid body component on the same interface
- Collision shapes: box, sphere, capsule, mesh
- Raycasting queries for gameplay (line-of-sight, ground detection), sphere casts
- Joints and constraints
- Articulated bodies ([Large worlds](large-worlds.md))
    - Compound bodies rebuilt when parts join or leave; mass, centre of mass and inertia recomputed
    - Breakable joints with force and torque thresholds, an event when they break
    - Sub-stepping per scene for long chains of joints
    - Forces and torques applied at a point, from Lua and from systems
- Gravity fields (uniform, point, spherical) summed per body, in 2D and 3D, trajectory prediction, analytic propagation during
  time warp ([Large worlds](large-worlds.md))
