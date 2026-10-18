# Sokoban

A sample game of Owl after Sokoban: ten levels of rising difficulty, from one crate to five. Open `samples/sokoban` in
Owl Nest (the **Sample games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the
game**.

## How to play

Push every crate onto a goal (the red marks); a crate turns gold on its goal. Crates can only be pushed, one at a time,
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

The title opens the level select (every level is open, the done ones are marked) or continues with the first level
left; the settings hold the master volume, the interface size and a reset of the progress.

## Inside

| Path                           | Content                                                                                    |
|--------------------------------|--------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `level_select`, `level_01` to `level_10`, `settings`, `victory`                   |
| `tilemaps/level_NN.owltilemap` | A level: layer `floor` (floor, walls), `goals`, `boxes` (crates, the hero where he starts) |
| `tilesets/sokoban.owltileset`  | The tiles of `textures/sokoban_tiles.png`                                                  |
| `scripts/game/sokoban.lua`     | The rules: steps and pushes on the tilemap cells, the undo stack, the win, the pause       |
| `scripts/ui/`                  | The menus, after the Engine Demo ones; `level_select.lua` fills the list of levels         |
| `huds/level.owlhud`            | Level, moves, best, hints, Undo / Restart / Menu buttons and the pause window              |

## Editing a level

Open a level scene in Owl Nest, select the `Level` entity and paint its tilemap in the tilemap editor: walls and floor
in layer `floor`, goal marks in `goals`, crates and the hero tile (where the player starts) in `boxes`. Keep as many
crates as goals; the game warns in the log otherwise. To add a level, duplicate a level scene, point its `Level` at a
new tilemap, and add the scene to `LEVEL_SCENES` (and its name to `LEVEL_NAMES`) in `sokoban.lua`, `title.lua` and
`level_select.lua`.

The art, the sounds, the first version of the levels and the menus come from `tools/samples/sokoban/`
(`generate_assets.py`, `generate_levels.py`, `generate_menus.py`): running a generator again overwrites the edits made
in Owl Nest.
