# Sample games {#page-design-sample-games}

[TOC]

Design page of the sample games, summarised in the [Roadmap](../roadmap.md).

## Goal

Owl ships two kinds of example content:

- **Engine Demo** (today's `engine_demo/`): an all-in-one showcase of every engine feature, not a game in itself.
- **Sample games**: small but real games, each one built on only what it needs, most of them mini remakes of known
  games. They prove that Owl makes games, give a newcomer a project of a readable size to start from, and feed the
  two showcase games of v1.0.

## Rules

- One project per game, beside the Engine Demo, authored in Owl Nest; game rules in Lua, never in the engine.
- Small: one screen or a handful of levels, finished (title, play, win / lose, settings), playable with keyboard,
  mouse and gamepad.
- CC0 or home-made art only; a scenario test per game runs in CI, and the game packages with the runner.
- A sample game uses only what it needs: it is not a feature checklist, the Engine Demo is.

## Short list

| Game                             | Inspired by           | Exercises                                                   | Possible from |
|----------------------------------|-----------------------|-------------------------------------------------------------|---------------|
| Breakout                         | Breakout / Arkanoid   | 2D, physics, HUD, sound, gamepad                            | v0.4.0        |
| Sokoban                          | Sokoban               | tilemap, undo, saves, level editing in Owl Nest             | v0.4.0        |
| Mini Transport Tycoon            | Mini Motorways / TTD  | isometric, roads, pathfinding, management HUD               | v0.4.0        |
| Wolfenstein mini                 | Wolfenstein 3D        | raycast, doors, pick-ups, HUD                               | v0.4.0        |
| Mini Minecraft                   | Minecraft             | voxel, build and break, inventory                           | v0.4.0        |
| One-room Zelda-like              | The Legend of Zelda   | top-down, dialogue, enemy AI, 2D lights                     | v0.7.0        |
| Marble or mini kart              | Marble Madness / Kart | 3D, Jolt physics, shadows                                   | v0.7.0        |
| Networked Bomberman              | Bomberman             | tilemap, deterministic simulation, multiplayer              | v0.8.0        |

Breakout or Sokoban comes first (simple, quick to finish), then Mini Transport Tycoon, which carries Owl's main
argument. The two v1.0 showcase games (one 2D, one 3D) are picked from this list.
