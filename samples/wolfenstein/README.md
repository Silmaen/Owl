# Wolfenstein mini

An exploration game in the spirit of Wolfenstein 3D, made with Owl: three floors of 32×32 cells drawn by the raycast
renderer, with treasure, food, keys and locked doors, secret walls and an exit elevator on each floor. There are no
guards nor weapons yet: they come once their art exists. The plan of the game is in
[Sample games](../../doc/pages/design/sample-games.md).

## How to play

Open the folder in Owl Nest (**File > Sample games > Wolfenstein mini**, or **Open project**) and press Play, or export
it as a game. Find the elevator switch of each floor and press **E** on it. While you play, the HUD shows the floor,
the score and the keys only; when the floor is done, the tally counts up what you found, one line after the other
(Enter, Space, a click or pad A shows it all at once):

- **Items**: every pick-up taken (treasure, food, keys), over the pick-ups of the floor.
- **Secrets**: the secret walls found. A secret wall looks like the others: press **E** against the walls.
- **Map explored**: the floor cells you have seen, over all the floor cells (the minimap fills as you look around).

Each percent of the three ratios adds 10 points to the score. The best ratios of each floor and the best final score
are kept and shown on the service record of the title screen, beside its **How to play** window; the controls are
shown there and in the pause window (Escape), never during play.

| Action            | Keyboard            | Mouse                           | Gamepad                     |
|-------------------|---------------------|---------------------------------|-----------------------------|
| Walk              | W / S, Up / Down    |                                 | Left stick, D-pad up / down |
| Turn              | A / D, Left / Right | Move with the right button held | Right or left stick, D-pad  |
| Run               | Left Shift          |                                 | Stick click, right bumper   |
| Open, push, use   | E, Space            | Left button                     | A, X                        |
| Large map         | Tab                 |                                 | Y                           |
| Quick save / load | F6 / F9             |                                 |                             |
| Pause             | Escape              |                                 | Start                       |

The floor is saved when it starts (slot 1) and on F6 (slot 2); **Continue** on the title screen loads the newer of the
two. The menus work with the mouse, the arrows and Enter, or the D-pad and A / B.

## Floors

| Floor | Name             | Keys       | Secrets | Pick-ups |
|-------|------------------|------------|---------|----------|
| 1     | The cellars      | Gold       | 3       | 18       |
| 2     | The prison       | Gold, blue | 3       | 17       |
| 3     | The headquarters | Gold, blue | 3       | 20       |

## Project

| Folder      | Content                                                                                                |
|-------------|--------------------------------------------------------------------------------------------------------|
| `scenes/`   | `title`, `level_1` to `level_3`, `tally`, `settings`, `victory`                                        |
| `scripts/`  | `game/`: the rules (player, pick-ups, locked doors, secrets, fog and map explored, exit); `ui/`: menus |
| `huds/`     | `play.owlhud`: floor, score, keys, messages, the pause window (with the controls)                      |
| `tilemaps/` | The walls of each floor and its minimap                                                                |
| `textures/` | The walls and props of the Engine Demo (`wolf3d/`), the minimap tiles, the menu plates (`ui/`)         |
| `sounds/`   | Doors, secret walls, pick-ups, elevator and ambience of the Engine Demo; jingle and tally (`ui/`)      |

The floors are carved by `tools/samples/wolfenstein/generate_levels.py` (it checks that every cell can be reached), then
edited in Owl Nest like any scene: a pick-up is counted by its tag (`Cross`, `Chalice`, `Chest`, `Crown`, `Food`,
`DogFood`, `GoldKey`, `BlueKey`), a secret wall by the tag `Secret`, so duplicating one in the editor adds it to the
tally. The `next_scene` property of the `ExitElevator` script names the floor that follows (`scenes/victory.owl` after
the last one): a new floor is a duplicated scene whose previous floor points at it. Running the generator again
overwrites such edits.

The menus are dark stone and military grey (`tools/samples/wolfenstein/generate_menus.py`, art and menu sounds by
`generate_assets.py`): a raycast view of the courtyard of floor 3 turns slowly behind the title, the title and the
buttons are riveted steel plates, and every change of screen fades through dark red.
