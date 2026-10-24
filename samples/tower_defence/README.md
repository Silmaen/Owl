# Tower defence

A sample game of Owl Engine in the spirit of Warcraft 1 and 2: the Horde (goblins, orc grunts, ogres, wolf riders,
trolls) marches on a human keep, through levels in real 3D with relief, between visits to a parchment world map. Three
levels are playable: Greenmarch Ford, Thornwood Pass and Stonebridge. Open `samples/tower_defence` in Owl Nest (the
**Sample games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

The world map traces the road from the capital to the next level: choose an open site and march. In a level, the Horde
comes out of its gate (two gates at Stonebridge) and walks the road to the keep. Build towers on the stone pads beside
it: click a pad and choose a badge of the wheel that opens beside it.

| Tower  | Price | Does                                                    | Level 2                      | Level 3                    |
|--------|-------|---------------------------------------------------------|------------------------------|----------------------------|
| Archer | 70    | Fast arrows at one enemy; ogres shrug most of them off  | Heavier arrows, longer range | Faster and heavier volleys |
| Cannon | 110   | Slow shells bursting on a group, aimed where it will be | Heavier shells               | Wider and heavier bursts   |
| Frost  | 90    | Chills an enemy: slower for a while, a little damage    | Colder, more damage          | Longer chill               |

A level costs 80 % (level 2) then 120 % (level 3) of the tower price. At level 3 a tower takes one power-up of two
(150 % of its price): piercing arrows or double shot, fire shells or shock wave, full freeze or vulnerability aura. A
pad on high ground gives its tower 15 % more range per step of height. Towers alone do not hold a level: raise them.

Goblins are fast and weak, wolf riders faster, grunts average, trolls heal over time, ogres slow and tough. Every kill
pays gold; every enemy reaching the keep strikes its gate and costs one of the 20 lives. A countdown runs before each
wave: calling it before its end pays 2 gold per second left. The last waves come at nightfall, when the torches are
lit. A tower sells back for half of what it cost. Hold every wave with a life left and the level is defended, with 3,
2 or 1 stars for 18, 10 or fewer lives kept.

| Level won       | Unlocks                                                                      |
|-----------------|------------------------------------------------------------------------------|
| Greenmarch Ford | The second power-up of each tower                                            |
| Thornwood Pass  | Rain of arrows: a spell striking the enemies around the one nearest the keep |
| Stonebridge     | Royal treasury: 60 more gold at the start of every level                     |

## Controls

| Action             | Keyboard                       | Mouse                           | Gamepad                  |
|--------------------|--------------------------------|---------------------------------|--------------------------|
| Open a pad / tower | Q / E or Tab to it, then Enter | Click it                        | D-pad left / right, A    |
| Build, raise       | Enter on the badge, or 1 2 3   | Click a badge                   | D-pad, A                 |
| Sell               | X                              | Click the coin badge            | X                        |
| Close the wheel    | Backspace                      | Right click, or click elsewhere | B                        |
| Call the next wave | Space                          | **Call wave**                   | Y                        |
| Spells             | 4 (rain), 5 (shield)           | Their buttons                   | Left / right stick press |
| Speed x2           | F                              | **Speed**                       | RB                       |
| Pan                | WASD, arrows                   | Mouse on an edge of the view    | Left stick               |
| Turn the view      | Z / C                          |                                 | LB, D-pad up / down      |
| Zoom               | R / V                          | Wheel                           | Right stick              |
| Pause              | Escape, P                      | **Menu**                        | Start                    |
| World map          | Q / E, Enter, Escape           | Click a site, **March!**        | D-pad, A, B              |

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Inside

| Path                           | Content                                                                                      |
|--------------------------------|----------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `world_map`, `level_01` to `level_03`, `settings`, `victory`, `game_over`           |
| `models/td/`, `models/castle/` | Tiles, towers and weapons of the Kenney Tower Defense Kit, keep of the Castle Kit (CC0)      |
| `fonts/`                       | `Cinzel` (titles) and `MedievalSharp` (labels), OFL                                          |
| `prefabs/`                     | Enemies, towers (one per level and power-up), projectiles, effects, spawned by the script    |
| `scripts/game/defence.lua`     | The rules: waves, roads, towers and upgrades, the build wheel, spells, camera, day and night |
| `scripts/ui/`                  | The menus and `world_map.lua`; `parade.lua` marches a wave past a tower under the menus      |
| `huds/play.owlhud`             | Gold, lives, wave and countdown, minimap, spells, Call wave / Speed / Menu, pause            |

A level mixes the rendering styles of the engine: the terrain, the towers and the keep are 3D meshes lit by the sun
of a `DayNightCycle` under a `ProceduralSky`, with shadows; the Horde, the projectiles and the effects are 2D sprites
in a billboard layer (`sprites`, depth tested, following the light of the scene, 2D lights for torches and fire), the
range ring and the cursor lie on the ground (`ground`), the build wheel floats over everything (`marks`), the minimap
is a `RenderTarget`, and the world map is a 2D scene.

## Making a level

Duplicate `scenes/level_01.owl` in Owl Nest. Move or replace the tiles of `Terrain` (models of `models/td/`), the
`Pad` entities under `Pads`, the `Gate` and the `Keep`, and the `Waypoint NN` entities under `Route` (`Waypoint B NN`
under `Route B` for a second gate): the enemies walk from waypoint to waypoint in the order of their numbers, at their
height. On the `Game` entity set `level_number`, `level_name`, `scene_path`, `night_from` and edit the `waves` list:
one string per wave, groups `kind count gap [road]` separated by `;` (`"goblin 8 0.8; grunt 2 1.5 b"`).
`start_towers` lists the towers standing at the start (`"pad kind level [power]"`, pads counted row by row from the
north-west). The values of the towers and of the enemies are properties of `defence.lua`, the same for every level
unless one overrides them. Add the level to the `levels` list of the `MapCanvas` script of `scenes/world_map.owl`.

The art, the sounds, the levels and the menus come from `tools/samples/tower_defence/` (`generate_assets.py` with the
Horde of `creatures.py` and the parchment of `medieval_ui.py`, `generate_maps.py` from the level grids of `maps.py`,
`generate_menus.py` with the world map): running a generator again overwrites the edits made in Owl Nest.
