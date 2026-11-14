# Physics API {#page-design-physics-api}

[TOC]

Design of the physics. Current behaviour is documented in [Physics](../physics.md).

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
| 2D                  | Jolt bodies locked to the XY plane                                                                                             |
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

## Features

- Fixed-step simulation: accumulator, bounded steps per frame, interpolated transforms, per-scene settings
- Bodies destroyed with their entity, no ghost collider
- Collision callbacks with the other entity: `on_collision`, `on_collision_exit`, `on_trigger_enter`,
  `on_trigger_exit`; collision layers and masks, sensors
- Queries: raycast, circle, box and sphere casts with a layer mask, overlaps; Lua `physics.raycast`; line-of-sight and
  ground detection for gameplay
- 3D shapes: box, sphere, capsule, cylinder, convex hull, mesh
- Joints: fixed, distance, hinge, slider, limits, motors; a visual joint editor in Owl Nest (viewport overlay and
  inspector)
- Gravity fields, trajectory prediction, time warp on rails ([Large worlds](large-worlds.md#large-worlds-gravity))

## Articulated bodies {#physics-api-articulated-bodies}

User reference: [Assemblies](../physics.md#assemblies).

- Compound bodies rebuilt when parts join or leave (`SceneBody::part`, `attach`, `detach`): one Jolt
  `StaticCompoundShape` replaced through `SetShape`, mass, centre of mass and inertia recomputed
- Breakable joints with force and torque thresholds, from the solver's impulses of the last collision step, checked in
  joint order (deterministic), an `on_joint_break` event
- Solver iterations per scene (`velocitySteps`, `positionSteps`) next to the collision steps, for long chains of joints
- Forces and torques applied at a point (`PhysicCommand::addForceAtPoint`), from Lua and from systems (thrust, drag,
  lift computed by the game)
