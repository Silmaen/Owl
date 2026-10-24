# Tower defence

A sample game of Owl Engine in the spirit of Warcraft 1 and 2: the Horde marches on a human keep, through ten levels
in real 3D with relief, between visits to a hand-inked world map. Open `samples/tower_defence` in Owl Nest (the
**Sample games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

The world map traces the road from the capital to the next level: choose an open site and march, or spend the stars
won in the upgrades. A briefing card opens each level, a line and an icon for its nights, its modifiers and the
enemies it brings first (their portrait and how to beat them): a click, Enter or pad A starts it. The Horde comes out
of its gate and walks the road to the keep. Build towers on the stone pads beside it: click a pad and choose a badge
of the wheel that opens beside it.

The first level starts with a short tutorial, a parchment hint and an arrow by each target while the time runs slowly:
build a tower on the glowing pad, call a wave early, raise the tower, sell it, read the day and night of the HUD. Skip
tutorial on the briefing (pad B, Backspace) or in the pause window skips it for good; finished or skipped, it is not
shown again until **Reset tutorial** in the settings.

| #  | Level           | Land                          | Night waves        | Modifiers           | Brings                 |
|----|-----------------|-------------------------------|--------------------|---------------------|------------------------|
| 1  | Greenmarch Ford | Meadows, a ford               | 4, 8               |                     | Goblins, grunts, ogres |
| 2  | Thornwood Pass  | Woods between two plateaus    | 3, 6, 9, 10        | Fog                 | Wolf riders            |
| 3  | Stonebridge     | Two gates, a long bridge      | 3, 4, 7, 8, 10     |                     | Trolls                 |
| 4  | Mirefen Marsh   | Two rivers, four bridges      | 3, 6, 9, 10        | Fog                 | Goblin sappers         |
| 5  | Frostmere       | Snow, a plateau gate          | 3, 6, 9, 10        | Blizzard            | Ogre mages             |
| 6  | Highpeak Watch  | Snow, two ramps down a cliff  | 3, 4, 7, 8, 11     | Gale                | Orc berserkers         |
| 7  | Sunscorch Flats | Sand, two gates               | 5, 10              | Drought             |                        |
| 8  | Ashfall Ridge   | Ash, a ridge climbed and left | 2, 4, 6, 8, 10, 11 | Blood moon          |                        |
| 9  | Ravenhold       | Ash, two winding roads        | 3, 4, 7, 8, 11, 12 | Plunder, long night |                        |
| 10 | The Black Gate  | Ash, two gates                | 4, 8, 11, 12       | Blood moon          | The orc chieftain      |

| Tower  | Price | Does                                                    | Level 2                      | Level 3                    | By day       | By night          |
|--------|-------|---------------------------------------------------------|------------------------------|----------------------------|--------------|-------------------|
| Archer | 70    | Fast arrows at one enemy; ogres shrug most of them off  | Heavier arrows, longer range | Faster and heavier volleys | +10 % damage | −15 % range       |
| Cannon | 110   | Slow shells bursting on a group, aimed where it will be | Heavier shells               | Wider and heavier bursts   | +15 % bursts | Fires 13 % slower |
| Frost  | 90    | Chills an enemy: slower for a while, a little damage    | Colder, more damage          | Longer chill               | −20 % chill  | +40 % chill       |

A level costs 80 % (level 2) then 120 % (level 3) of the tower price. At level 3 a tower takes one power-up of two
(150 % of its price): piercing arrows or double shot, fire shells or shock wave, full freeze or vulnerability aura. A
pad on high ground gives its tower 15 % more range per step of height. Towers alone do not hold a level: raise them.

### Day and night

Day and night alternate within a level: between two waves the sky turns to dusk or to dawn, announced, for the waves
of the level fought at night, when the torches are lit. Every tower and every enemy has a bonus or a malus by day and
by night: the HUD shows a sun or a moon and when the phase changes next, the info scroll of a tower and the first
sight of an enemy give its day and night effects, the pause window lists them all.

| Enemy         | Is                                                     | By day             | By night           | Beat it with                            |
|---------------|--------------------------------------------------------|--------------------|--------------------|-----------------------------------------|
| Goblin        | Fast and frail                                         | −10 % speed        | +15 % speed        | Archers                                 |
| Orc grunt     | Sturdy                                                 | −10 % damage taken | +10 % speed        | Any tower                               |
| Ogre          | Slow and tough, arrows glance off                      | −15 % speed        | +15 % life         | Cannons, frost                          |
| Wolf rider    | Very fast                                              | +15 % damage taken | +20 % speed        | Frost towers                            |
| Troll         | Heals over time                                        | No healing         | Heals 50 % more    | Cannon bursts; fire stops the healing   |
| Ogre mage     | Wards the Horde around it against 45 damage, every 6 s | −20 % wards        | +50 % wards        | Kill it first; fire burns through wards |
| Goblin sapper | Fast, 3 lives at the keep, bursts when killed          | −10 % speed        | +20 % speed        | Shoot it early, in the crowd            |
| Orc berserker | Faster as it bleeds                                    | +10 % damage taken | +15 % speed        | Frost, then finish it fast              |
| Orc chieftain | Armoured, rallies the Horde, half slowed, never frozen | +10 % damage taken | −15 % damage taken | Cannons, the vulnerability aura         |

Every kill pays gold; every enemy reaching the keep strikes its gate and costs one of the 20 lives (3 for a sapper, 10
for the chieftain). A countdown runs before each wave: calling it before its end pays 2 gold per second left. A tower
sells back for half of what it cost. Hold every wave with a life left and the level is defended, with 3, 2 or 1 stars
for 18, 10 or fewer lives kept.

### Modifiers and unlocks

Some levels add a modifier, named on the world map, in the briefing and in the pause window:

| Modifier   | Effect                                                            |
|------------|-------------------------------------------------------------------|
| Fog        | Towers −15 % range                                                |
| Blizzard   | Frost chill +30 %, the Horde 12 % slower, towers fire 10 % slower |
| Blood moon | The Horde has 30 % more life at night                             |
| Drought    | Cannons cost 20 % less, frost towers 20 % more                    |
| Gale       | Arrows −15 % damage, cannon bursts +25 %                          |
| Plunder    | The Horde carries 25 % more gold                                  |
| Long night | At night the Horde is 10 % faster and towers see 10 % less far    |

| Level won        | Unlocks                                                                      |
|------------------|------------------------------------------------------------------------------|
| Greenmarch Ford  | The second power-up of each tower                                            |
| Thornwood Pass   | Rain of arrows: a spell striking the enemies around the one nearest the keep |
| Stonebridge      | Royal treasury: 60 more gold at the start of every level                     |
| Mirefen Marsh    | Fletchers' guild: archers cost 10 % less                                     |
| Frostmere        | Shield wall: a spell stopping the enemies around the one nearest the keep    |
| Highpeak Watch   | Watchtowers: every tower sees 5 % further                                    |
| Sunscorch Flats  | Harvest tithe: 10 % more gold per kill                                       |
| Ashfall Ridge    | Night watch: towers see 10 % further at night                                |
| Ravenhold        | Royal guard: 3 more lives on every level                                     |
| All ten, 3 stars | Keep walls: 5 more lives                                                     |

## Upgrades

The stars are a currency: **Upgrades** on the world map spends them on six families, four tiers each, bought in
order for 1, 2, 3 then 4 stars (30 stars to win, 60 to buy everything). A level replayed for more stars credits the
difference (its best is kept), and **Refund all** gives every star back for free.

| Family         | Tier 1                      | Tier 2                     | Tier 3            | Tier 4                  |
|----------------|-----------------------------|----------------------------|-------------------|-------------------------|
| Archers        | +15 % damage                | +12 % range                | Shoot 15 % faster | +10 % range             |
| Cannons        | Cost 15 % less              | 20 % wider bursts          | +20 % damage      | Shoot 15 % faster       |
| Frost          | The chill lasts 30 % longer | Chilled enemies are slower | +12 % range       | +20 % damage            |
| Rain of arrows | Ready 20 % sooner           | +30 % damage               | 25 % wider        | +30 % damage            |
| Shield wall    | Ready 20 % sooner           | Holds 2 seconds more       | 25 % wider        | Ready 20 % sooner again |
| Treasury       | +40 gold at the start       | +10 % gold per kill        | +2 lives          | +60 gold at the start   |

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

| Path                           | Content                                                                                                      |
|--------------------------------|--------------------------------------------------------------------------------------------------------------|
| `scenes/`                      | `title`, `world_map`, `upgrades`, `level_01` to `level_10`, `settings`, `victory`, `game_over`               |
| `models/td/`, `models/castle/` | Tiles (green and snow), towers and weapons of the Tower Defense Kit, keep of the Castle Kit (Kenney, CC0)    |
| `fonts/`                       | `Cinzel` (titles) and `MedievalSharp` (labels), OFL                                                          |
| `prefabs/`                     | Enemies, towers (one per level and power-up), projectiles, effects, spawned by the script                    |
| `scripts/game/defence.lua`     | The properties and the hooks of a level: it requires the rules of `scripts/game/td/`                         |
| `scripts/game/td/`             | The rules by subject: `towers`, `enemies`, `waves`, `effects`, `wheel`, `briefing`, `tutorial`, `economy`... |
| `scripts/ui/`                  | The menus, `world_map.lua`, `upgrades.lua`; `parade.lua` marches the Horde under the menus                   |
| `huds/play.owlhud`             | Gold, lives, wave, day or night, minimap, spells, Call wave / Speed / Menu, pause, briefing window           |

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
height. On the `Game` entity set `level_number`, `level_name`, `scene_path`, `night_waves` (the waves fought at night,
`"3 6 9 10"`), `modifiers` (keys of `defence.lua`: `fog`, `blizzard`, `blood_moon`, `drought`, `gale`, `plunder`,
`long_night`), `new_enemies` (named in the briefing) and edit the `waves` list: one string per wave, groups
`kind count gap [road]` separated by `;` (`"goblin 8 0.8; grunt 2 1.5 b"`). The day and night effects are the
`time_effects` list of `defence.lua` (`"archer night range 0.85"`: subject, phase, stat, factor).
`start_towers` lists the towers standing at the start (`"pad kind level [power]"`, pads counted row by row from the
north-west). The values of the towers and of the enemies are properties of `defence.lua`, the same for every level
unless one overrides them. Add the level to the `levels` and `level_scenes` lists of the `MapCanvas` script of
`scenes/world_map.owl`.

The art, the sounds, the levels and the menus come from `tools/samples/tower_defence/` (`generate_assets.py` with the
Horde of `creatures.py`, the parchment of `medieval_ui.py`, the world map of `world_map_art.py` and the menu scenery
of `scenery.py`, `generate_maps.py` from the level grids and themes of `maps.py`, `generate_menus.py` with the world
map and the upgrades screen): running a generator again overwrites the edits made in Owl Nest.
