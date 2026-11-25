# Sokoban

A sample game of Owl Engine after Sokoban: ten levels of rising difficulty, from one crate to five. Open `samples/sokoban` in
Owl Nest (the **Sample games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the
game**.

## How to play

Push every crate onto a goal (the red marks); a crate turns gold on its goal and sparkles, a push raises dust. Crates can only be pushed, one at a time,
never pulled: a crate in a corner stays there, so undo is never far. The moves and the pushes are counted; the fewest
moves of each level are kept, with the levels done, between sessions (the settings of the game).

## Controls

| Action  | Keyboard                      | Mouse           | Gamepad           |
|---------|-------------------------------|-----------------|-------------------|
| Step    | Arrows, WASD (hold to repeat) |                 | D-pad, left stick |
| Undo    | Z, U, Backspace               | **Undo** button | B                 |
| Restart | R                             | **Restart**     | Y                 |
| Pause   | Escape, P                     | **Menu**        | Start             |
| Menus   | Arrows, Enter, Escape         | Point, click    | D-pad, A, B       |

The title opens the level select (every level is open, the done ones are marked), continues with the first level left
or shows the rules and the controls in its **How to play** window; the controls are shown there and in the pause
window, never during play. The settings hold the master, music and effects volumes, the interface size and a reset
of the progress.

## Inside

| Path                           | Content                                                                                                |
|--------------------------------|--------------------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `level_select`, `level` (every level), `settings`, `victory`                                  |
| `tilemaps/level_NN.owltilemap` | A level: layer `floor` (floor, walls), `goals`, `boxes` (crates, the hero where he starts)             |
| `tilesets/sokoban.owltileset`  | The tiles of `textures/sokoban_tiles.png`                                                              |
| `tables/levels.owltable`       | The levels in play order: the tilemap and the name of each one                                         |
| `scripts/game/sokoban.owlvs`   | The rules: steps and pushes on the tilemap cells, the undo stack, the win, the pause                   |
| `scripts/game/levels.lua`      | Reads the level table and the progress settings for every screen, shows the level to play in the scene |
| `scripts/ui/`                  | The wooden menus; `level_select.owlvs` fills the list, `warehouse.owlvs` animates the corridor         |
| `huds/level.owlhud`            | Level, moves, best, Undo / Restart / Menu buttons, the pause window (with the controls)                |

## Editing a level

Open a `tilemaps/level_NN.owltilemap` in the tilemap editor of Owl Nest and paint it: walls and floor in layer
`floor`, goal marks in `goals`, crates and the hero tile (where the player starts) in `boxes`. Keep as many crates as
goals; the game warns in the log otherwise. To add a level, save a new tilemap and add a row for it (its tilemap and
its name) to `tables/levels.owltable` in the data table editor: the title, the level select and the level before it
follow the table. `scenes/level.owl` plays them all: the game gives its `Level` entity the tilemap of the level to
play (`tilemap.set_asset`) and sizes the camera to it (`entity.set_camera_size`); played on its own, the scene starts
the level of the `level` property of its `Game` entity.

The art, the sounds, the first version of the levels and the menus come from `tools/samples/sokoban/`
(`generate_assets.py`, `generate_levels.py`, `generate_menus.py`): running a generator again overwrites the edits made
in Owl Nest.

The menus look like a warehouse: a crate per letter of the title, a row of crates with stencilled names for buttons,
the progress, the settings and the level list typed on a clipboard, a corridor at the bottom where the hero pushes a
crate onto its goal, and wipes in the brown of the crates between the screens.
