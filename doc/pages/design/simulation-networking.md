# Simulation and networking {#page-design-simulation-networking}

[TOC]

Design page for the deterministic simulation of v0.7.0 and the networking of v0.9.0, summarised in the [Roadmap](../roadmap.md). Networking stays before 1.0 and rests
on the deterministic simulation, the per-scene world (PR-33) and the open component registry (PR-37).

## Deterministic simulation (v0.7.0) {#simulation-deterministic}

| Part                                                                                  | Status      |
|---------------------------------------------------------------------------------------|-------------|
| Every system (physics, scripts, timers) on the fixed tick of `SimulationClock`        | Done        |
| Seeded random streams per system (`math::RandomStream`, `Scene::getRandomStream`)     | Done        |
| Scene seed (`Simulation: seed:`), Scene Settings field, `scene.set_random_seed`       | Done        |
| UUIDs of the entities created in Play drawn from the `owl.entity` stream              | Done        |
| Lua `math.random` on the `lua` stream, fixed Lua string hash seed                     | Done        |
| World hash (`scene::computeWorldHash`), run-to-run test                               | Done        |
| No wall-clock read in gameplay code (the engine reads it only to time its logs)      | Done        |
| Frozen world hash of a physics and Lua scene, checked on every target (arm64 nightly) | Done        |
| Engine Demo: dice crates thrown from the seed (`platformer_dice.owltest`)              | Done        |
| World hash of a recorded input log compared across every target, with the replay      | Planned     |

- Replays record the `ActionFrame` stream ([Windowing and input](windowing-input.md))
- The frozen hash rests on the bit-exact rules below; it changes everywhere with the simulation or the scene format,
  on one target only with a determinism bug

## Cross-platform determinism (v0.7.0) {#simulation-cross-platform}

Always on, in the simulation only (rendering is not concerned): the same inputs give the same world, bit for bit, on
linux-x64, linux-arm64, windows-x64 (MinGW and clang-cl), android-arm64 and the Web.

| Source of divergence | Rule                                                                                                            | Status  |
|----------------------|-----------------------------------------------------------------------------------------------------------------|---------|
| FMA contraction      | `-ffp-contract=off` on every Owl target (Clang fuses `a*b+c` on arm64, GCC in GNU mode)                         | Done    |
| Maths library        | `math::sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `log`, `pow` (`math/elementaryFunctions.h`), never the system libm, in `math` and `scene` | Done    |
| Fast maths           | `-fno-fast-math` on every Owl target; denormals kept (no flush to zero)                                         | Done    |
| 2D physics           | Box2D 3 deterministic across platforms (its build sets `-ffp-contract=off`, its own trigonometry)               | Done    |
| 3D physics           | Jolt built with `JPH_CROSS_PLATFORM_DETERMINISTIC`, its cost measured in `test/bench/`, when Jolt lands          | Planned |
| Lua                  | Fixed string hash seed; `math.sin`… `math.pow` bound to the engine maths (the `^` operator keeps the libm)       | Done    |
| Lua iteration        | `table.sorted_keys(t)`: booleans, numbers, strings in a fixed order whatever the insertion history               | Done    |
| Test                 | Bit fingerprints of the engine maths checked on every target (`ElementaryFunctions` tests, arm64 nightly)        | Done    |
| Test                 | Frozen world hash of a Box2D and Lua scene (`WorldHashGoldenTest`), the same value on every target              | Done    |

## Input recording and replay (v0.7.0)

- Record the input stream of a Play session, replay it frame-exact in the editor or the headless runner
- Replays double as regression tests

## Rewind in Play (v0.7.0)

- Scrub back in time during a Play session in Owl Nest (ring buffer of world snapshots + input log)

## Fair play {#networking-fairness}

A game both multiplayer and multi-platform keeps its players equal; a platform may change the look, never the odds.

| Source of unfairness   | Rule                                                                                                     | Release |
|------------------------|----------------------------------------------------------------------------------------------------------|---------|
| Simulation rate        | The fixed tick never depends on the frame rate; inputs sampled per tick                                  | v0.7.0  |
| Visibility and quality | An entity tagged gameplay (cover, occluder, fog limiting the view) is never reduced by a quality setting | v0.7.0  |
| Field of view          | The game bounds the gameplay field of view, whatever the aspect ratio of the screen                      | v0.7.0  |
| Outcome                | The authoritative server decides; a platform difference only costs a prediction correction               | v0.9.0  |
| Input device           | The input type of each player is known to the lobby: matchmaking filter, optional crossplay              | v0.9.0  |
| Latency                | Server-side lag compensation (hitboxes rewound to the time of the shot)                                  | v0.9.0  |
| Performance floor      | A minimum hardware profile, checked when a multiplayer match starts                                      | v0.9.0  |

## Networking and multiplayer (v0.9.0)

- Two network models: an authoritative server with client prediction (action games), and lockstep on the
  cross-platform deterministic simulation (management games, strategy), with desync detection by world hash

- Platforms: desktop and Android; no networking on the Web in 1.0 (a browser opens no UDP socket; a WebSocket or
  WebRTC transport is evaluated after 1.0)

- Network transport layer
    - UDP-based reliable messaging (or integrate a library like ENet/GameNetworkingSockets)
    - Client-server model with authoritative server
    - Connection management: connect, disconnect, reconnect, timeout
- Entity replication
    - Mark components as replicated (sync from server to clients)
    - Interpolation and prediction for smooth movement
    - Authority model: server-owned vs client-owned entities
    - Interest management: relevance areas and priorities, a client receives only the entities near it
      ([Large worlds](large-worlds.md))
- RPC system
    - Lua API: `rpc_server(func, args)`, `rpc_client(func, args)`, `rpc_all(func, args)`
    - Reliable and unreliable RPC channels
- Lobby and session management
    - Host/join game, player list, ready state
    - Lua callbacks: `on_player_join`, `on_player_leave`
- Network debugging tools
    - Latency/packet-loss simulation in editor
    - Network stats overlay (ping, bandwidth, entity count)
