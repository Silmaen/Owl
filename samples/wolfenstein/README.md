# Wolfenstein mini

An exploration game in the spirit of Wolfenstein 3D, made with Owl Engine: three floors of 32×32 cells drawn by the raycast
renderer, with treasure, food, keys and locked doors, secret walls and an exit elevator on each floor, held by guards
who patrol, hear the shots, see you and fire back. You fight with a knife, a pistol, a machine gun and a chain gun.
The plan of the game is in [Sample games](../../doc/pages/design/sample-games.md).

## How to play

Open the folder in Owl Nest (**File > Sample games > Wolfenstein mini**, or **Open project**) and press Play, or export
it as a game. Find the elevator switch of each floor and press **E** on it. While you play, the status bar along the
bottom of the screen shows the floor, the score, your face, the health, the ammo, the keys and the weapon in hand, as in
Wolfenstein 3D; a key and the elevator switch get a marker (name, distance, an arrow on the screen edge)
once you have seen them, and every pick-up bursts into sparks. At the switch the elevator ride turns you round to its
door, which closes, and the elevator climbs (E, Enter or Escape skips it); then the tally counts up what you found,
one line after the other (Enter, Space, a click or pad A shows it all at once):

- **Items**: every pick-up taken (treasure, food, keys), over the pick-ups of the floor.
- **Secrets**: the secret walls found. A secret wall looks like the others: press **E** against the walls.
- **Map explored**: the floor cells you have seen, over all the floor cells (the minimap fills as you look around; it
  keeps its share of the screen at any resolution).
- **Kills**: the guards shot, over the guards of the floor, shown with the floor score at the end of the count.

Each percent of the four ratios adds 10 points to the score.

## Combat

You start with the knife, the pistol and 8 rounds; the weapon in your hands stands on the status bar. A hit flashes the
view red and lights the edge of the view on the side the shot came from, the face on the bar winces and the view
shakes; the face gets bloodier as the health drops, and below 25 % the edges of the view turn red and pulse.
Hold **Ctrl** (or **F**, the right trigger, pad B) to fire: the pistol fires once per press, the machine gun and the
chain gun fire as long as the button is held. A shot hits the nearest guard in sight in the middle of the view; the
knife, silent, only one within reach. Every shot is heard by the guards around (about ten cells, through the walls),
and a guard also sees you when you stand in front of it with no wall between you.

- **Guards** (100 points each) stand guard or walk their patrol. Once alerted, after a moment of surprise, they run
  after you over the floor, opening the doors (not the locked ones), and fire whenever they see you within nine
  cells: the closer, the likelier the hit. Two pistol shots kill one; it drops an ammo clip (4 rounds).
- **Food** heals as in Wolf3D: a roast dinner +10 %, dog food +4 %, left on the floor while you are at full health.
- **Gear:** ammo boxes (8 rounds), first aid kits (+25 %, left on the floor while you are at full health), the
  machine gun (floor 1, in a secret room) and the chain gun (floor 3, in a secret room), 6 rounds with each. A new
  weapon is taken in hand at once; **1** to **4** or the left bumper switch weapons, an empty gun clicks and leaves
  the knife in hand.
- **Death:** at 0 % the floor starts again from the save made when it started, with full health, the pistol and 8
  rounds. The best ratios of each floor and the best final score
are kept and shown on the service record of the title screen, beside its **How to play** window; the controls are
shown there and in the pause window (Escape), never during play. While the pause window is open the game clock stops:
you stand still and no door nor secret wall moves, whatever you press or click.

The settings set the master, music and effects volumes, the mouse turn and the interface size, and the accessibility:
a colour vision correction (protanopia, deuteranopia, tritanopia), reduced motion (the title view stands still, the
elevator ride does not turn, a hit does not shake the view, the red of low health does not pulse) and subtitles (the secret walls and the elevator).

| Action            | Keyboard            | Mouse                           | Gamepad                     |
|-------------------|---------------------|---------------------------------|-----------------------------|
| Walk              | W / S, Up / Down    |                                 | Left stick, D-pad up / down |
| Turn              | A / D, Left / Right | Move with the right button held | Right or left stick, D-pad  |
| Run               | Left Shift          |                                 | Stick click, right bumper   |
| Open, push, use   | E, Space            | Left button                     | A, X                        |
| Fire              | Ctrl, F             |                                 | Right trigger, B            |
| Weapon            | 1 to 4              |                                 | Left bumper (next)          |
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

| Folder       | Content                                                                                                |
|--------------|--------------------------------------------------------------------------------------------------------|
| `scenes/`    | `title`, `level_1` to `level_3`, `tally`, `settings`, `victory`                                        |
| `scripts/`   | `game/`: the rules (player, pick-ups, locked doors, secrets, fog, markers and map explored, exit, the  |
|              | elevator ride), the guards (`guard.owlvs`, `guard.owlbt`), the weapons and the health (`arsenal`),     |
|              | the face and the red of the hits and of low health (`status`), `tactics.lua` (line of sight, chase     |
|              | over the grid); `ui/`: menus                                                                           |
| `prefabs/`   | `clip` and `guard_corpse`, left by a guard shot dead                                                   |
| `huds/`      | `play.owlhud`: the status bar, the red of the hits and of low health, messages, the pause window       |
| `sequences/` | `elevator.owlseq`: the elevator ride at the end of a floor (`scripts/game/elevator.owlvs`)             |
| `tilemaps/`  | The walls of each floor and its minimap                                                                |
| `textures/`  | The walls and props of the Engine Demo (`wolf3d/`), the minimap tiles, the menu and status bar art     |
|              | (`ui/`: the face, the weapon icons, the red sheets of the hits and of low health), the                 |
|              | guard (`guard/`, *The Guard*, CC0) and the weapons in hand (`weapons/`, *FPS weapons overlay*, CC0)    |
| `sounds/`    | Doors, secret walls, pick-ups, elevator and ambience of the Engine Demo; jingle and tally (`ui/`); the |
|              | home-made music of the menus and of the floors (`music/`); shots, guards and pick-ups (`combat/`)      |

The floors are carved by `tools/samples/wolfenstein/generate_levels.py` (it checks that every cell can be reached), then
edited in Owl Nest like any scene: a pick-up is counted by its tag (`Cross`, `Chalice`, `Chest`, `Crown`, `Food`,
`DogFood`, `GoldKey`, `BlueKey`), a secret wall by the tag `Secret`, so duplicating one in the editor adds it to the
tally; the sparks of the pick-ups are drawn by the `raycast_fx` particle layer, in the raycast view. The `next_scene` property of the `ExitElevator` script names the floor that follows (`scenes/victory.owl` after
the last one): a new floor is a duplicated scene whose previous floor points at it. Running the generator again
overwrites such edits.

The guards, the gear and the `Arsenal` entity of each floor are placed by
`tools/samples/wolfenstein/place_combat.py`, which only replaces those entities and can run again after an edit of the
floor in Owl Nest; `generate_combat.py` slices the two CC0 sheets of OpenGameArt (*The Guard*, *FPS weapons
overlay*) and synthesises the combat sounds.

The status bar, its face, its weapon icons, the red sheets of the hits and of low health and `status.owlvs` are written
by `tools/samples/wolfenstein/generate_status.py`, the bar itself is laid out in `huds/play.owlhud` by
`generate_menus.py`.

The menus are the DOS menus of 1992 (`tools/samples/wolfenstein/generate_menus.py`, menu sounds and music by
`generate_assets.py`): a raycast view of the courtyard of floor 3 turns slowly behind the riveted steel title, the menus
are blood red windows with grey pixel entries and a skull beside the selected one, and every change of screen fades
through dark red.
