# Breakout

A sample game of Owl after Breakout and Arkanoid: five levels on one screen, three lives, bricks of one to three hits
and two power-ups. Open `samples/breakout` in Owl Nest (the **Sample games** list of the File screen) and play
`scenes/title.owl`, or package it with **Package the game**.

## How to play

Keep the ball in play with the paddle and break every brick of a level to reach the next one; a ball below the field
costs a life. Where the ball lands on the paddle sets its angle, and it speeds up a little at each bounce.

- Grey bricks take three hits, silver ones two; a coloured brick breaks at once (10, 25 or 50 points).
- Some bricks drop a capsule: **W** widens the paddle for 15 seconds, **S** slows the ball for 12 (50 points each).
- The ball serves by itself after five seconds. The best score is kept between sessions.

## Controls

| Action | Keyboard              | Mouse        | Gamepad           |
|--------|-----------------------|--------------|-------------------|
| Move   | A / D, Left / Right   | Move         | Left stick, D-pad |
| Serve  | Space, Up             | Left click   | A                 |
| Pause  | Escape, P             |              | Start             |
| Menus  | Arrows, Enter, Escape | Point, click | D-pad, A, B       |

The mouse moves the paddle by its motion; its speed is a setting, with the master volume and the interface size.

## Inside

| Path                      | Content                                                                     |
|---------------------------|-----------------------------------------------------------------------------|
| `scenes/`                 | `title`, `level_1` to `level_5`, `settings`, `victory`, `game_over`         |
| `scripts/game/`           | `game.lua` (rules, power-ups, pause), `paddle.lua`, `ball.lua`, `brick.lua` |
| `scripts/ui/`             | The menus, copied from the Engine Demo                                      |
| `prefabs/brick.owlprefab` | A brick: static body, sprite, script; the levels hold its instances         |
| `huds/play.owlhud`        | Score, level, lives, messages and the pause window                          |

The art, the sounds and the first layout of the levels come from `tools/samples/breakout/` (`generate_assets.py`,
`generate_levels.py`); edit the levels in Owl Nest afterwards (move, add or delete bricks, change their properties).
The `next_scene` property of the `Game` entity names the level that follows (`scenes/victory.owl` after the last one):
to add a level, duplicate a level scene, set its `level` and `next_scene`, and point the level before at it.
