# Simulation and networking {#page-design-simulation-networking}

[TOC]

Design of the deterministic simulation, its replay and rewind, and of the planned networking, which rests on them.

## Deterministic simulation {#simulation-deterministic}

- Every system (physics, scripts, timers) runs on the fixed tick of `SimulationClock`
- Seeded random streams per system (`math::RandomStream`, `Scene::getRandomStream`) from the scene seed
  (`Simulation: seed:`, Scene Settings field, `scene.set_random_seed`)
- UUIDs of the entities created in Play are drawn from the `owl.entity` stream; Lua `math.random` uses the `lua`
  stream, with a fixed Lua string hash seed
- No wall-clock read in gameplay code (the engine reads it only to time its logs)
- World hash (`scene::computeWorldHash`): a run-to-run test, a frozen hash of a physics and Lua scene checked on every
  target, and the hash of a recorded input log compared across every target; the Engine Demo throws dice crates from
  the seed (`platformer_dice.owltest`)
- Replays record the device state of every frame ([Windowing and input](windowing-input.md)); the replay scenarios
  check the world hash on every target
- The frozen hash rests on the bit-exact rules below; it changes everywhere with the simulation or the scene format,
  on one target only with a determinism bug

## Cross-platform determinism {#simulation-cross-platform}

Always on, in the simulation only (rendering is not concerned): the same inputs give the same world, bit for bit, on
every target.

| Source of divergence | Rule                                                                                                                                                                    |
|----------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| FMA contraction      | `-ffp-contract=off` on every Owl target (Clang fuses `a*b+c` on arm64, GCC in GNU mode)                                                                                 |
| Maths library        | `math::sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `log`, `pow` (`math/elementaryFunctions.h`), never the system libm, in `math` and `scene`            |
| Fast maths           | `-fno-fast-math` on every Owl target; denormals kept (no flush to zero)                                                                                                 |
| Physics              | Jolt with `JPH_CROSS_PLATFORM_DETERMINISTIC`, no FMA, SSE4.2 on every x86_64 build, NEON on arm64, for 2D and 3D and every worker count ([Physics API](physics-api.md)) |
| Lua                  | Fixed string hash seed; `math.sin`… `math.pow` bound to the engine maths (the `^` operator keeps the libm)                                                              |
| Lua iteration        | `table.sorted_keys(t)`: booleans, numbers, strings in a fixed order whatever the insertion history                                                                      |
| Tests                | Bit fingerprints of the engine maths on every target (`ElementaryFunctions` tests); frozen world hash of a Jolt and Lua scene (`WorldHashGoldenTest`)                   |

## Input recording and replay {#simulation-replay}

`scene::Replay` and the versioned `.owlreplay` YAML file; `Scene::setRecordingReplay` / `takeRecordedReplay`,
`setPlaybackReplay`, `getReplayProgress`; `OwlRunner --record <file>` and `--replay <file>`, also with `--scenario`;
the scenario key `replay:` and the expectations `replay: synced` / `finished`, `world_hash:`; in Owl Nest the Record
button (`scene.record`) and replays played from the Content Browser. The Engine Demo has a recorded tour of the
platformer house, checked by `replay_platformer_house.owltest`.

- A replay holds, per frame, its exact duration, the device `InputState`, the cursor capture and the HUD pointer
  (`ReplayPointer`); and the scene seed, the tick rate, the world hash at the start and a world hash every
  `Replay::checkpointTicks` (60) ticks
- Playing a replay applies its seed and clock settings and ignores the time step of the caller and the devices; after
  the last frame the scene reads the devices again
- Lua raw input (`input.is_key_pressed`, mouse, gamepad) and the mouse-look of the first-person, ship and voxel
  controllers read the input frame of the scene, so they replay too
- A divergence is reported at the first checkpoint whose world hash differs (frame 0: the start state differs)
- A replay needs the same project: its action map, settings and texts are not stored
- Only the first scene of a session is recorded: a level transition ends the recording
- The systems that run per frame replay the recorded frame sequence, not a different frame rate
- `Scene::copy` keeps the entity and component order of its source, so the editor's Play simulates like the runner

## Rewind in Play {#simulation-rewind}

Every editor Play records its timeline in memory (the Record button only writes the file). A seek simulates the frames
again from the Play start with the recorded inputs, then pauses; it is bounded per editor frame, silent, without script
breakpoints. Play or Step after a seek branches the session from that frame (`Scene::takeOverReplay`). Owl Nest has a
timeline slider in the Play controls and `scene.rewind` (`F9`, one second back).

- A seek re-simulates instead of restoring snapshots: the Lua state of the scripts cannot be saved, and the
  bit-exact simulation gives the world of the frame from its inputs; the physics state (`PhysicCommand::saveState`)
  serves the trajectory prediction and the rollback of the network
- A seek costs the frames it simulates again: going back starts from the Play start, going forward goes on from the
  frame reached
- The timeline is the first scene of a Play: a level transition ends it, and a played `.owlreplay` is not rewound
- The timeline holds every frame of the Play (the device state and the HUD mouse of each frame)

## Fair play {#networking-fairness}

A game both multiplayer and multi-platform keeps its players equal; a platform may change the look, never the odds.

- **Simulation rate:** the fixed tick never depends on the frame rate and inputs are sampled per tick. The `Player`
  impulses run in the tick (`owl.player_input` has no frame update), and the first person, voxel and ship controllers
  move by the mean speed of each frame, so a jump, a fall or a burn covers the same distance at 30 or 240 frames per
  second.
- **Visibility:** an entity tagged gameplay (cover, occluder, fog limiting the view) is never reduced by a quality
  setting: `Visibility::gameplay` ([Scene](../scene.md)) keeps the shadows of gameplay lights within the slots every
  backend has, and every quality setting honours it.
- **Field of view:** the game bounds the gameplay field of view, whatever the aspect ratio of the screen
  (`SceneCamera::setMaxAspectRatio`, [Renderer](../renderer.md)).

Planned with the networking:

- **Outcome:** the authoritative server decides; a platform difference only costs a prediction correction
- **Input device:** the input type of each player is known to the lobby: matchmaking filter, optional crossplay
- **Latency:** server-side lag compensation (hitboxes rewound to the time of the shot)
- **Performance floor:** a minimum hardware profile, checked when a multiplayer match starts

## Networking and multiplayer

Planned. Two network models: an authoritative server with client prediction (action games), and lockstep on the
cross-platform deterministic simulation (management games, strategy), with desync detection by world hash. Desktop and
Android; no networking on the Web (a browser opens no UDP socket).

- Transport: UDP-based reliable messaging, client-server with an authoritative server; connect, disconnect,
  reconnect, timeout
- Replication: components marked as replicated, interpolation and prediction, server-owned and client-owned entities;
  interest management (relevance areas and priorities, a client receives only the entities near it)
- RPC: Lua `rpc_server(func, args)`, `rpc_client(func, args)`, `rpc_all(func, args)`; reliable and unreliable channels
- Lobby: host / join, player list, ready state; Lua `on_player_join`, `on_player_leave`
- Debugging: latency and packet-loss simulation in the editor, a network stats overlay (ping, bandwidth, entity count)
