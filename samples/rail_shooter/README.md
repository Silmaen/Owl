# Ring Runner

A rail shooter sample game of Owl Engine after Star Fox: one lane of about three minutes through the asteroid ring of
a planet, squadrons of fighters, and the Warden at the end. Open `samples/rail_shooter` in Owl Nest (the **Sample
games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

The ship rides the rail on its own: steer it inside the frame and aim with the two reticles. Shoot the rocks in your
way and the fighters that come at you; a barrel roll dashes aside and turns their shots away while it lasts. The hull
recharges slowly when nothing hit it for a few seconds; a rock costs 15 points of hull, a shot 6, a ramming fighter 12.
A shot close to the line of the reticles goes straight at the nearest target.

- Small rocks break at the first hit (50 points), big ones take three (150); a fighter takes two (200), an interceptor
  five (400), the Warden 140 (5000). The hull left at the end is worth 30 points per percent.
- The Warden slows the flight down and weaves ahead of the ship, firing spreads; destroying it clears the lane.
- The victory screen counts the score, the enemies down, the rocks, the hit ratio and the hull up one line after the
  other, then the medal: bronze, silver from 22 000 points, gold from 30 000. The best score and medal are kept.

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Controls

| Action | Keyboard              | Mouse        | Gamepad     |
|--------|-----------------------|--------------|-------------|
| Steer  | W A S D               |              | Left stick  |
| Aim    | Arrows                | Move         | Right stick |
| Fire   | Space                 | Left button  | RT          |
| Roll   | Q / E                 |              | LB / RB     |
| Pause  | Escape, P             |              | Start       |
| Menus  | Arrows, Enter, Escape | Point, click | D-pad, A, B |

Once the mouse moved, the far reticle follows it; the aim stick or the arrows take it back around the nose of the ship.
The settings hold the master, music and effects volumes (the mixer buses), the interface size and the inverted
steering.

## Inside

| Path                     | Content                                                                                      |
|--------------------------|----------------------------------------------------------------------------------------------|
| `scenes/level.owl`       | The rail, the carrier (camera, ship, reticles, sequence players), the rocks, the planet      |
| `scenes/`                | Also `title`, `settings`, `victory`, `game_over`: live 3D backdrops behind their canvases    |
| `scripts/game/rail.lua`  | The scene script of the level: properties, hooks, sequence events, pause                     |
| `scripts/game/rs/`       | `core` (state, rail coordinates), `field` (rocks), `foes` (waves, boss), `guns` (lasers)     |
| `scripts/ui/`            | The menus; `backdrop.lua` flies their camera through the ring                                |
| `tables/`                | `waves`: distance along the rail, kind, count, pattern, place; `enemies`: the kinds' stats   |
| `sequences/`             | `intro`, `boss` (its arrival) and `outro`, played on the camera of the carrier               |
| `prefabs/`               | Lasers, enemy shots, fighter, interceptor, boss (HUD markers and engine trails)              |
| `huds/play.owlhud`       | Score, hits, hull, boss bar, messages, distance and the pause window (with the controls)     |

Everything lives in rail coordinates: the distance along the `Rail` spline, across it and up from it; the
`RailCarrier` rides the rail with a `SplineFollower`, and the floating origin of the scene keeps the camera near zero
while the planet of the planet layer turns below. Every rock is an entity of the scene: move, add or remove them in
Owl Nest; the waves and the kinds of enemies are rows of the data tables. Set the `startDistance` of the carrier's `SplineFollower` to try a
stretch of the lane from where it starts (the waves left behind are skipped).

The ships and the rocks are models of the Kenney Space Kit (CC0) converted by
`tools/samples/rail_shooter/generate_models.py`; the sky, the HUD art, the title, the sounds, the music and the start
jingle are home-made by `generate_assets.py`; `generate_level.py` and `generate_menus.py` wrote the first layout of
the scenes.
