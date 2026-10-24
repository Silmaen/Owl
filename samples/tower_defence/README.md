# Tower defence

A sample game of Owl Engine in the spirit of Warcraft 1 and 2: the Horde (goblins, orc grunts, ogres) marches on a
human keep, through levels in real 3D with relief. Level 1, Greenmarch Ford, is playable; the next levels and the
world map between them follow. Open `samples/tower_defence` in Owl Nest (the **Sample games** list of the File
screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

The Horde comes out of its gate and walks the road to the keep. Build towers on the stone pads beside it:

| Tower  | Price | Does                                                    | Level 2        | Level 3        |
|--------|-------|---------------------------------------------------------|----------------|----------------|
| Archer | 70    | Fast arrows at one enemy; ogres shrug most of them off  | Longer range   | Faster volleys |
| Cannon | 110   | Slow shells bursting on a group, aimed where it will be | Heavier shells | Wider bursts   |
| Frost  | 90    | Chills an enemy: slower for a while, a little damage    | Colder         | Longer chill   |

A level costs 80 % (level 2) then 120 % (level 3) of the tower price. At level 3 a tower takes one power-up of two
(150 % of its price): piercing arrows or double shot, fire shells or shock wave, full freeze or vulnerability aura.
The second power-up of each tower is unlocked by winning Greenmarch Ford. A pad on high ground gives its tower 15 %
more range per step of height.

Goblins are fast and weak, grunts average, ogres slow and tough. Every kill pays gold; every enemy reaching the keep
costs one of the 20 lives. A countdown runs before each wave: calling it before its end pays 2 gold per second left.
The last wave comes at nightfall, when the torches are lit. A tower sells back for half of what it cost. Hold every
wave with a life left and the level is defended, with 3, 2 or 1 stars for 18, 10 or fewer lives kept.

## Controls

| Action             | Keyboard                       | Mouse                           | Gamepad               |
|--------------------|--------------------------------|---------------------------------|-----------------------|
| Open a pad / tower | Q / E or Tab to it, then Enter | Click it                        | D-pad left / right, A |
| Build, upgrade     | Enter on the badge, or 1 2 3   | Click a badge                   | D-pad, A              |
| Sell               | X                              | Click the coin badge            | X                     |
| Close the ring     | Backspace                      | Right click, or click elsewhere | B                     |
| Call the next wave | Space                          | **Call wave**                   | Y                     |
| Speed x2           | F                              | **Speed**                       | RB                    |
| Pan                | WASD, arrows                   | Mouse on an edge of the view    | Left stick            |
| Turn the view      | Z / C                          |                                 | LB, D-pad up / down   |
| Zoom               | R / V                          | Wheel                           | Right stick           |
| Pause              | Escape, P                      | **Menu**                        | Start                 |

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Inside

| Path                           | Content                                                                                            |
|--------------------------------|----------------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `level_01`, `settings`, `victory` (end stats counted up), `game_over`                     |
| `models/td/`, `models/castle/` | Tiles, towers and weapons of the Kenney Tower Defense Kit, keep of the Kenney Castle Kit (CC0)     |
| `prefabs/`                     | Enemies, towers (one per level and power-up), projectiles, effects, spawned by the script          |
| `scripts/game/defence.lua`     | The rules: waves, enemies, towers and upgrades, projectiles, the build ring, camera, day and night |
| `scripts/ui/`                  | The menus; `parade.lua` marches a wave past a tower under every menu                               |
| `huds/play.owlhud`             | Gold, lives, wave and countdown, minimap, Call wave / Speed / Menu, the ring's info panel, pause   |

A level mixes the rendering styles of the engine: the terrain, the towers and the keep are 3D meshes lit by the sun
of a `DayNightCycle` under a `ProceduralSky`, with shadows; the enemies, projectiles and effects are 2D sprites in a
billboard layer (`sprites`, depth tested, lit by 2D lights at night), the range ring and the cursor lie on the ground
(`ground`), the badges of the build ring float over everything (`marks`), and the minimap is a `RenderTarget`.

## Making a level

Duplicate `scenes/level_01.owl` in Owl Nest. Move or replace the tiles of `Terrain` (models of `models/td/`), the
`Pad` entities under `Pads`, the `Gate` and the `Keep`, and the `Waypoint NN` entities under `Route`: the enemies walk
from waypoint to waypoint in the order of their numbers, at their height. On the `Game` entity set `level_number`,
`level_name`, `scene_path`, `next_scene` (and that of the level before), `night_from` and edit the `waves` list: one
string per wave, groups `kind count gap` separated by `;` (`"goblin 8 0.8; grunt 2 1.5"`). `start_towers` lists the
towers standing at the start (`"pad kind level [power]"`, pads counted row by row from the north-west). The values of
the towers and of the enemies are properties of `defence.lua`, the same for every level unless one overrides them.

The art, the sounds, the levels and the menus come from `tools/samples/tower_defence/` (`generate_assets.py` with the
8-direction Horde of `creatures.py`, `generate_maps.py` from the level grids of `maps.py`, `generate_menus.py`):
running a generator again overwrites the edits made in Owl Nest.
