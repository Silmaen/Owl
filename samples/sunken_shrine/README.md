# The Sunken Shrine

A Zelda-like sample game of Owl Engine: a small dungeon of six rooms, one screen each, seen from above. A key, a
sword, a dark crypt, a block puzzle and a guardian stand between the hero and the heart of the shrine. Open
`samples/sunken_shrine` in Owl Nest (the **Sample games** list of the File screen) and play `scenes/title.owl`, or
package it with **Package the game**.

## How to play

The old man of the hall tells the way. North of the hall, the slime pit hides the key in its chest; east, the
armoury keeps the sword on its stand and a door the key opens. Beyond it, the dark crypt opens when its four braziers
burn; then the stone block of the sinking floor must be pushed onto its plate; and the guardian waits in its den.

- Walk up to something and use it: talk to the old man (use again for the next line), open the chest, take the
  sword, light a brazier, push the block one cell away from you.
- The hero shows its back, its front or its side the way it faces; a stroke sweeps the blade across that way.
- Once you hold the sword, strike: a slime takes two blows, a bat or a rat one, the guardian ten. Hit, a creature
  backs off; the guardian grows faster as it weakens.
- A creature touching you costs a heart (the guardian two), pushes you back with a thud, flashes you red and shakes
  the room (not with the reduced motion); you then blink while no creature can hurt you. The hearts sit large on a
  dark plate in the top left corner. With no heart left the game is over, and **Retry**
  wakes you at the door of the room, your hearts full again. What you found and opened stays found and open.
- The quest is saved on entering each room and on each find; **Continue** on the title takes it up again.
- Touch the heart the guardian guarded to end the quest. The tally gives the time, the creatures and the falls; a
  fast quest with few falls makes the best score, kept between sessions.

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Controls

| Action    | Keyboard              | Mouse        | Gamepad           |
|-----------|-----------------------|--------------|-------------------|
| Walk      | W A S D, arrows       |              | Left stick, D-pad |
| Sword     | Space                 | Left button  | A                 |
| Use, talk | E, Enter              | Right button | X                 |
| Pause     | Escape, P             |              | Start             |
| Menus     | Arrows, Enter, Escape | Point, click | D-pad, A, B       |

The settings hold the master, music and effects volumes (the mixer buses), the interface size, the colour vision
correction, the reduced motion, the subtitles of the sounds, gentler creatures (slower, and a longer grace after a
hit) and a brighter dark crypt.

## Inside

| Path                          | Content                                                                                  |
|-------------------------------|------------------------------------------------------------------------------------------|
| `scenes/room_1.owl` to `6`    | A room: its tilemap, the hero, the exits, the door, the things and creatures, the lights |
| `scenes/`                     | Also `title`, `settings`, `victory`, `game_over`, over the hall lit by its torches       |
| `scripts/game/room.owlvs`     | The scene script of a room: entry, memory, save, door, old man, hearts, fall, end        |
| `scripts/game/player.owlvs`   | The hero: walk, sword, use, hurt                                                         |
| `scripts/game/creature.owlvs` | A creature: blows taken, room bounds; `guardian.owlvs` adds the phases                   |
| `scripts/game/*.owlbt`        | The creature, bat and guardian trees: back off when hit, chase when close, wander        |
| `scripts/game/exit.owlvs`     | An exit: the next room, entered by the facing door                                       |
| `scripts/game/shrine.lua`     | The block, the names of things, the room bounds, the hero's pose, push, blink and shake  |
| `tilemaps/`, `tilesets/`      | The rooms, painted with the Kenney Tiny Dungeon sheet; walls and torches collide         |
| `dialogues/old_man.owldlg`    | What the old man says                                                                    |
| `huds/play.owlhud`            | Hearts, key and sword, room name, messages, dialogue box, pause window                   |

A room is a scene: paint its walls in the tilemap editor of Owl Nest, move its things and creatures, and set the
rule of its door (`key`, `braziers`, `block`, `boss` or `none`) in the properties of its scene script; an exit is a
sensor with `exit.owlvs`, the scene it leads to and the spawn point it enters by.

The tiles, the hero, the old man, the creatures and the sword are the Kenney Tiny Dungeon sheet (CC0) of the Engine
Demo, copied; the back and the sides of the hero are drawn in its colours; the hearts, the key, the stroke, the plate, the title, the sounds, the music and the jingles are
home-made by `tools/samples/sunken_shrine/generate_assets.py`; `generate_level.py` and `generate_menus.py` wrote the
first layout of the rooms, the trees, the dialogue, the HUD and the graphs.
