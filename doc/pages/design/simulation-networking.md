# Simulation and networking {#page-design-simulation-networking}

[TOC]

Design page for the deterministic simulation of v0.7.0 and the networking of v0.8.0, summarised in the [Roadmap](../roadmap.md). Networking stays before 1.0 and rests
on the deterministic simulation, the per-scene world (PR-33) and the open component registry (PR-37).

## Deterministic simulation (v0.7.0)

- Fixed-step simulation (physics, scripts, timers) independent from the frame rate: the clock and `on_tick` land in
  v0.4.0 (SIM1 of [Isometric worlds](isometric-worlds.md)), v0.7.0 makes every system run on it
- Replays record the v0.4.0 `ActionFrame` stream ([Windowing and input](windowing-input.md))
- Seeded random streams per system; no wall-clock reads in gameplay code
- Determinism test in CI: the same input log produces the same world hash on Linux and Windows

## Input recording and replay (v0.7.0)

- Record the input stream of a Play session, replay it frame-exact in the editor or the headless runner
- Replays double as regression tests

## Rewind in Play (v0.7.0)

- Scrub back in time during a Play session in Owl Nest (ring buffer of world snapshots + input log)

## Networking and multiplayer (v0.8.0)

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
