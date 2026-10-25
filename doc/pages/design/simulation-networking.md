# Simulation and networking {#page-design-simulation-networking}

[TOC]

Design page for the deterministic simulation of v0.7.0 and the networking of v0.9.0, summarised in the [Roadmap](../roadmap.md). Networking stays before 1.0 and rests
on the deterministic simulation, the per-scene world (PR-33) and the open component registry (PR-37).

## Deterministic simulation (v0.7.0)

- Fixed-step simulation (physics, scripts, timers) independent from the frame rate: the clock and `on_tick` land in
  v0.4.0 (SIM1 of [Isometric worlds](isometric-worlds.md)), v0.7.0 makes every system run on it
- Replays record the v0.4.0 `ActionFrame` stream ([Windowing and input](windowing-input.md))
- Seeded random streams per system; no wall-clock reads in gameplay code
- Determinism test in CI: the same input log produces the same world hash on every target

## Cross-platform determinism (v0.7.0) {#simulation-cross-platform}

Always on, in the simulation only (rendering is not concerned): the same inputs give the same world, bit for bit, on
linux-x64, linux-arm64, windows-x64 (MinGW and clang-cl), android-arm64 and the Web.

| Source of divergence | Rule                                                                                                   |
|----------------------|--------------------------------------------------------------------------------------------------------|
| FMA contraction      | `-ffp-contract=off` on the simulation code (Clang fuses `a*b+c` on arm64, not on baseline x64)         |
| Maths library        | `sin`, `cos`, `exp`, `pow`, `atan2`… from a deterministic library of the engine, never the system libm |
| Fast maths           | `-ffast-math` and its parts banned; denormals kept (no flush to zero)                                  |
| 2D physics           | Box2D cross-platform determinism                                                                       |
| 3D physics           | Jolt built with `JPH_CROSS_PLATFORM_DETERMINISTIC`, its cost measured in `test/bench/`                 |
| Lua                  | Fixed string hash seed, sorted iteration helper for tables, `math.*` bound to the deterministic maths  |
| Test                 | World hash of the same input log compared across every target at night                                 |

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
