# Tower defence

A sample game of Owl Engine after Kingdom Rush: hold the keep against eight waves on an isometric map. This is the
first map, in basic home-made art, to try the gameplay; more maps come once it plays well. Open
`samples/tower_defence` in Owl Nest (the **Sample games** list of the File screen) and play `scenes/title.owl`, or
package it with **Package the game**.

## How to play

Enemies come out of the gate and walk the road to the keep. Build towers on the stone pads beside the road:

| Tower  | Price | Does                                                         |
|--------|-------|--------------------------------------------------------------|
| Archer | 70    | Fast arrows at one enemy; armour stops most of them          |
| Cannon | 110   | Slow shells that burst on a group, aimed where it will be    |
| Frost  | 90    | Chills an enemy: slower for a while, a little damage         |

Runners are fast and weak, soldiers average, armoured enemies slow and tough. Every kill pays gold; every enemy that
reaches the keep costs one of the 20 lives. A countdown runs before each wave: calling the wave before its end pays
2 gold per second left. A tower sells back for half its price. Hold the eight waves with a life left and the map is
defended (kept between sessions, with the most lives kept).

## Controls

| Action             | Keyboard                         | Mouse                                 | Gamepad     |
|--------------------|----------------------------------|---------------------------------------|-------------|
| Open a pad / tower | Q / E or Tab to it, then Enter   | Click it                              | D-pad, A    |
| Build              | Enter on the badge, or 1 2 3     | Click a badge                         | D-pad, A    |
| Sell               | X                                | Click the coin badge                  | X           |
| Close the ring     | Backspace                        | Right click, or click elsewhere       | B           |
| Call the next wave | Space                            | **Call wave**                         | Y           |
| Speed ×2           | F                                | **Speed**                             | RB          |
| Pan                | WASD, arrows                     | Mouse on an edge of the view          | Left stick  |
| Pause              | Escape, P                        | **Menu**                              | Start       |
| Menus              | Arrows, Enter, Escape            | Point, click                          | D-pad, A, B |

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Inside

| Path                          | Content                                                                                       |
|-------------------------------|-----------------------------------------------------------------------------------------------|
| `scenes/`                     | `title`, `map_01`, `settings`, `victory` (end stats counted up), `game_over`                  |
| `tilemaps/map_01.owltilemap`  | The map: layer `ground` (grass), layer `roads` (the road the enemies walk, the pavement pads) |
| `prefabs/`                    | Enemies, towers, projectiles, health bar and puff, spawned by the script                      |
| `scripts/game/defence.lua`    | The rules: waves, enemies, towers, projectiles, the build ring, camera, pause and speed       |
| `scripts/ui/`                 | The menus; `parade.lua` marches a wave past a tower under every menu                          |
| `huds/play.owlhud`            | Gold, lives, wave and countdown, Call wave / Speed / Menu, the ring's info panel, pause       |

## Making a map

Duplicate `scenes/map_01.owl` in Owl Nest and point its `Ground` at a new tilemap. Paint the road in layer `roads`
with the road tiles (their connections make the path) from an edge of the map to a cell beside the keep, and the
pads with the pavement tile. Move the `Gate` onto the first road cell and the `Keep` beside the last one. On the
`Game` entity set `map_number`, `scene_path`, `next_scene` (and that of the map before) and edit the `waves` list: one
string per wave, groups `kind count gap` separated by `;` (`"runner 8 0.8; soldier 2 1.5"`). The values of the towers
and of the enemies are properties of `defence.lua`, the same for every map unless one overrides them.

The art, the sounds, the first map and the menus come from `tools/samples/tower_defence/` (`generate_assets.py`,
`generate_maps.py` from the map data of `maps.py`, `generate_menus.py`): running a generator again overwrites the edits
made in Owl Nest.
