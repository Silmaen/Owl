# Tower defence

A sample game of Owl Engine in the spirit of Warcraft 1 and 2: the Horde marches on a human keep, through five levels
in real 3D with relief, between visits to a hand-inked world map: Greenmarch Ford, Thornwood Pass, Stonebridge,
Frostmere (in the snow) and The Black Gate (in the ash lands, with the orc chieftain). Open `samples/tower_defence` in Owl Nest (the
**Sample games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

The world map traces the road from the capital to the next level: choose an open site and march, or spend the stars
won in the upgrades. In a level, the Horde comes out of its gate (two gates from Stonebridge on) and walks the road to
the keep. Build towers on the stone pads beside
it: click a pad and choose a badge of the wheel that opens beside it.

| Tower  | Price | Does                                                    | Level 2                      | Level 3                    |
|--------|-------|---------------------------------------------------------|------------------------------|----------------------------|
| Archer | 70    | Fast arrows at one enemy; ogres shrug most of them off  | Heavier arrows, longer range | Faster and heavier volleys |
| Cannon | 110   | Slow shells bursting on a group, aimed where it will be | Heavier shells               | Wider and heavier bursts   |
| Frost  | 90    | Chills an enemy: slower for a while, a little damage    | Colder, more damage          | Longer chill               |

A level costs 80 % (level 2) then 120 % (level 3) of the tower price. At level 3 a tower takes one power-up of two
(150 % of its price): piercing arrows or double shot, fire shells or shock wave, full freeze or vulnerability aura. A
pad on high ground gives its tower 15 % more range per step of height. Towers alone do not hold a level: raise them.

Each level brings a new kind of the Horde, named with its counter the first time it marches:

| Enemy             | From level      | Is                                                          | Beat it with                            |
|-------------------|-----------------|-------------------------------------------------------------|-----------------------------------------|
| Goblin            | Greenmarch Ford | Fast and frail                                              | Archers                                 |
| Orc grunt         | Greenmarch Ford | Sturdy                                                      | Any tower                               |
| Ogre              | Greenmarch Ford | Slow and tough, arrows glance off                           | Cannons, frost                          |
| Wolf rider        | Thornwood Pass  | Very fast                                                   | Frost towers                            |
| Troll             | Stonebridge     | Heals over time                                             | Cannon bursts; fire stops the healing   |
| Ogre mage         | Frostmere       | Wards the Horde around it against 45 damage, every 6 s      | Kill it first; fire burns through wards |
| Goblin sapper     | Frostmere       | Fast, its keg costs 3 lives at the keep, bursts when killed | Shoot it early, in the crowd            |
| The orc chieftain | The Black Gate  | Armoured, rallies the Horde, half slowed, never frozen      | Cannons, the vulnerability aura         |

Every kill pays gold; every enemy reaching the keep strikes its gate and costs one of the 20 lives (3 for a sapper, 10
for the chieftain). A countdown runs before each
wave: calling it before its end pays 2 gold per second left. The last waves come at nightfall, when the torches are
lit. A tower sells back for half of what it cost. Hold every wave with a life left and the level is defended, with 3,
2 or 1 stars for 18, 10 or fewer lives kept.

| Level won       | Unlocks                                                                      |
|-----------------|------------------------------------------------------------------------------|
| Greenmarch Ford | The second power-up of each tower                                            |
| Thornwood Pass  | Rain of arrows: a spell striking the enemies around the one nearest the keep |
| Stonebridge     | Royal treasury: 60 more gold at the start of every level                     |
| Frostmere       | Shield wall: a spell stopping the enemies around the one nearest the keep    |
| All five, 3 ★   | Keep walls: 5 more lives                                                     |

## Upgrades

The stars are a currency: **Upgrades** on the world map spends them on six families, three tiers each, bought in
order for 1, 2 then 3 stars. A level replayed for more stars credits the difference (its best is kept), and
**Refund all** gives every star back for free.

| Family         | Tier 1                         | Tier 2                     | Tier 3                    |
|----------------|--------------------------------|----------------------------|---------------------------|
| Archers        | +15 % damage                   | +12 % range                | Shoot 15 % faster         |
| Cannons        | Cost 15 % less                 | 20 % wider bursts          | +20 % damage              |
| Frost          | The chill lasts 30 % longer    | Chilled enemies are slower | +12 % range               |
| Rain of arrows | Ready 20 % sooner              | +30 % damage               | 25 % wider                |
| Shield wall    | Ready 20 % sooner              | Holds 2 seconds more       | 25 % wider                |
| Treasury       | +40 gold at the start          | +10 % gold per kill        | +2 lives                  |

The spells are raised once a level has unlocked them.

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
| Upgrades           | Arrows, Enter, Escape          | **Buy**, **Refund all**         | D-pad, A, B              |

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Inside

| Path                           | Content                                                                                      |
|--------------------------------|----------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `world_map`, `upgrades`, `level_01` to `level_05`, `settings`, `victory`, `game_over` |
| `models/td/`, `models/castle/` | Tiles (green and snow), towers and weapons of the Kenney Tower Defense Kit, keep of the Castle Kit (CC0) |
| `fonts/`                       | `Cinzel` (titles) and `MedievalSharp` (labels), OFL                                          |
| `prefabs/`                     | Enemies, towers (one per level and power-up), projectiles, effects, spawned by the script    |
| `scripts/game/defence.lua`     | The rules: waves, roads, towers and upgrades, the build wheel, spells, camera, day and night |
| `scripts/ui/`                  | The menus, `world_map.lua`, `upgrades.lua`; `parade.lua` marches the Horde under the menus   |
| `huds/play.owlhud`             | Gold, lives, wave and countdown, minimap, spells, Call wave / Speed / Menu, pause            |

A level mixes the rendering styles of the engine: the terrain, the towers and the keep are 3D meshes lit by the sun
of a `DayNightCycle` under a `ProceduralSky`, with shadows; the Horde, the projectiles and the effects are 2D sprites
in a billboard layer (`sprites`, depth tested, following the light of the scene, 2D lights for torches and fire), the
range ring and the cursor lie on the ground (`ground`), the build wheel floats over everything (`marks`), the minimap
is a `RenderTarget`, and the world map is a 2D scene: an inked map at 80 pixels per unit (pixel for pixel on a
2560 x 1440 screen, scaled down to fit a narrower one), with waving banners, drifting clouds and glints of the sea.

## Making a level

Duplicate `scenes/level_01.owl` in Owl Nest. Move or replace the tiles of `Terrain` (models of `models/td/`), the
`Pad` entities under `Pads`, the `Gate` and the `Keep`, and the `Waypoint NN` entities under `Route` (`Waypoint B NN`
under `Route B` for a second gate): the enemies walk from waypoint to waypoint in the order of their numbers, at their
height. On the `Game` entity set `level_number`, `level_name`, `scene_path`, `night_from` and edit the `waves` list:
one string per wave, groups `kind count gap [road]` separated by `;` (`"goblin 8 0.8; grunt 2 1.5 b"`).
`start_towers` lists the towers standing at the start (`"pad kind level [power]"`, pads counted row by row from the
north-west). The values of the towers and of the enemies are properties of `defence.lua`, the same for every level
unless one overrides them. Add the level to the `levels` and `level_scenes` lists of the `MapCanvas` script of
`scenes/world_map.owl`.

The art, the sounds, the levels and the menus come from `tools/samples/tower_defence/` (`generate_assets.py` with the
Horde of `creatures.py`, the parchment of `medieval_ui.py` and the world map of `world_map_art.py`, `generate_maps.py`
from the level grids and themes of `maps.py`, `generate_menus.py` with the world map and the upgrades screen): running a generator again overwrites the edits made in Owl Nest.
