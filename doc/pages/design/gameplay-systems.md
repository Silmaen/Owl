# Gameplay systems {#page-design-gameplay-systems}

[TOC]

Design of the gameplay systems: AI on the deterministic simulation
([Simulation and networking](simulation-networking.md)), audio, narrative and HUD markers. They are optional modules
on the phased systems, not inside `Scene`. Physics is described in [Physics API](physics-api.md), simulation level of
detail and time warp in [Large worlds](large-worlds.md).

## AI

- Navigation: navigation mesh baked from the scene geometry, A* on the mesh, dynamic obstacle avoidance; tile-graph
  pathfinding beside it ([2D complete](2d-complete.md))
- Behaviour trees: `.owlbt` assets edited on the node graph canvas of Owl Nest; sequence, selector, parallel,
  decorator, condition and action nodes; Lua leaves for custom actions and conditions
- Steering behaviours: seek, flee, arrive, wander, pursue, evade; flocking (separation, alignment, cohesion);
  composed by a behaviour tree or Lua; in open 3D space, obstacle avoidance between moving volumes and formations

User guide: [Pathfinding](../pathfinding.md#pathfinding-navmesh).

| Decision   | Choice                                                                                         |
|------------|------------------------------------------------------------------------------------------------|
| Library    | In-house bake and search: deterministic on every target, no dependency                         |
| Partition  | Monotone regions over voxels, stacked floors kept apart, contours simplified then triangulated |
| Storage    | The baked triangles are saved in the scene (`NavMeshVolume`), every target loads the same mesh |
| Bake input | Static colliders and meshes without body; what moves and hidden entities are skipped           |
| Search     | A* over the triangles and the links, then the funnel                                           |
| Avoidance  | ORCA: agents share half, obstacles none; a blocked agent retries bearing right                 |
| Obstacles  | `NavObstacle` cylinders are avoided, never carved out of the mesh                              |
| Links      | `NavLink` attached to the meshes at read time, no new bake                                     |
| Tick       | `owl.nav_agents` in `PrePhysics`, fixed tick only, agents in UUID order                        |
| Editor     | Inspector with a Bake button, `nav.bake` command, pickable markers, overlay                    |

User guide: [AI: steering and behaviour trees](../ai.md).

| Decision             | Choice                                                                                                               |
|----------------------|----------------------------------------------------------------------------------------------------------------------|
| Library              | In-house: no dependency, the same bits on every target                                                               |
| Asset                | `.owlbt` YAML: nodes with their canvas position, links parent to child, blackboard keys with their start value       |
| Run order            | `owl.behaviour_trees` in `Scripts` after `owl.scripts`, fixed tick, agents in UUID order; Running resumes the node   |
| Reactivity           | A reactive Selector tries the upper children again each tick and halts the running one; Guard checks a key each tick |
| Leaves               | Nodes of a fixed catalogue (`getNodeTypes()`); custom ones are Lua functions called with `(entity, dt)`              |
| Determinism          | Random draws from the scene streams `owl.behaviour_trees` and `owl.steering`                                         |
| Editor               | `.owlbt` tab on the node graph canvas, `bt.*` commands, live node status and blackboard in Play                      |
| Steering combination | Weighted sum of the behaviours, times a response rate, kept under `maxAcceleration` then `maxSpeed`                  |
| Avoidance            | Closest approach of moving spheres within `lookAhead`, other agents and `NavObstacle`s, in the three planes          |
| Formations           | A slot in the frame of the leader, reached with `arrive` plus the velocity of the leader                             |

## Audio

- Mixer: buses Master → Music / SFX / Ambient / Voice, per-bus volume, mute and solo, crossfades between music tracks
- Effects: reverb zones (a cave against the outdoors), low-pass and high-pass filters (underwater, behind walls),
  Doppler on moving 3D sources
- Adaptive music: layers and stingers switched by game states (calm, danger, combat), on the mixer crossfades

User guide: [Sound](../sound.md).

| Decision       | Choice                                                                                                  |
|----------------|---------------------------------------------------------------------------------------------------------|
| Mixer          | `sound::Mixer` over the backend: every play goes through it; gain = volume × fade × bus gain            |
| Bus choice     | The `category` of a `SoundSource` is the bus; a cue has a default bus                                   |
| Two volumes    | Designer mix saved in the scene (`Mixer:`), player volume from the `volume_*` settings, multiplied      |
| Solo           | While a bus is soloed only the soloed ones are heard; Master is never soloed                            |
| Crossfades     | Fades in the mixer, linear or equal power; one music track, `playMusic` crossfades to the next          |
| Reverb zones   | A `ReverbZone` component (box or sphere on the entity transform), weights blended at the listener       |
| Reverb backend | EFX: one auxiliary slot with the standard reverb, presets from `efx-presets.h`                          |
| Dry and wet    | Music and Master are dry; SFX, Ambient and Voice send to the reverb and take the zone filter            |
| Filters        | Three gains (whole, low, high): low-pass, high-pass or band-pass; source and zone filters multiply      |
| Doppler        | Velocities from world positions in double a frame apart, held at the speed of sound; factor in the mix  |
| Adaptive music | An `AdaptiveMusic` component: synced looping layers, states fading them, a stinger on entry             |
| State source   | `sound.set_music_state` in Lua, or a game state key the `owl.sound` system follows                      |
| Headless       | The Null backend keeps every source and listener value, so the whole logic is tested without audio      |
| Editor         | Mixer panel (`scene.set_mixer`, `sound.set_live_bus` in Play), zone volume drawn and sized by the gizmo |

## Narrative

Dialogue trees (`.owldlg`): lines, choices with Lua conditions, branches and events, drawn as a graph in Owl Nest with a
preview, played from Lua with `dialogue.*` and displayed by the game's HUD ([Dialogues](../dialogues.md)).

## HUD markers

HUD elements anchored to world entities (labels, brackets, health bars), clamped to the screen edge with an off-screen
indicator when the entity leaves the view ([HUD system](hud-system.md#world-markers)).
