# Asteroid Field

An escort sample game of Owl Engine after Star Fox and X-Wing: three freighters fly through an asteroid field to a
jump gate, and you escort them in a fighter with two wingmen, through waves of enemy fighters. Open
`samples/asteroid_field` in Owl Nest (the **Sample games** list of the File screen) and play `scenes/title.owl`, or
package it with **Package the game**.

## How to play

Your fighter flies in six directions: the throttle drives it along its nose, the side jets slide it, the keys or the
stick turn it, at once and only while held, and so does the right button, toward the aim circle. The throttle stays
where you leave it: W raises it, S lowers it down to 40 % reverse, Backspace cuts it, and the HUD shows it under the
hull. With the flight assist on (the default) the fighter flies where it points, reaches the speed of its throttle or
stops in about a second and levels its wings once the roll is released; off it drifts on. A briefing opens the mission; the objective line at the top gives the metres
the convoy still has to fly to the gate, the markers show the freighters and the gate. The convoy
keeps its route at its own speed; you do not have to stay with it, but the fighters of the waves go for it, or for
you.

- Seven waves of fighters and interceptors come out of the field ahead of the convoy as it goes; an enemy is down in
  two hits (a fighter) or four (an interceptor), and breaks off for a while when it is hurt.
- The cross at the centre of the view is your heading; the mouse moves the aim circle, and your gimballed lasers leave
  toward it (up to 34° off the nose). They also carry the speed of your fighter. Hold the right button and the fighter
  turns toward the circle, faster the farther it is from the cross (full rate a quarter of the screen away).
- Lock the enemy nearest the aim circle (the middle button; on empty space it drops the lock), the wheel moves the lock
  to the next or the previous enemy: its marker reads LOCKED and a violet X shows where to put the circle for your
  lasers to meet it, from its speed, yours and theirs. No X when it cannot be caught or is behind you.
- Order your wingmen to attack (each goes for the nearest enemy) or to escort (back in formation on you).
- Your fighter bounces off the rocks and the other ships. Rocks hurt your hull (15 a ram) and destroy the enemies that
  run into them; ramming an enemy hard costs you 4 and the enemy a hit; enemy lasers cost you 6, a freighter 3.
- The enemies make passes at the freighters: they never get inside one, break off when they close in and hold their
  fire at point blank.
- A hit flashes the ship it hits and throws sparks and a glow; a kill blows up in a fireball with an expanding flash and
  debris; a damaged fighter or freighter smokes; when you are hit the screen edges flash red and the view shakes (not
  with the reduced motion setting). In flight the aim circle is your only cursor, the system one shows in the pause window.
- The mission is won when the convoy reaches the gate and jumps, lost when every freighter or your fighter is
  destroyed. Kills, the freighters saved (1000 each) and your hull left make the score; the best one is kept.

The fighter turns like a first-person view (its `ShipController` has a *Max Pitch*): left and right turn about the
vertical, the nose stays within 69° of the horizon and the wings stay level unless you roll. It has no *Angular
Response* (0: no rotational inertia) and no *Mouse Steer*: the mouse is left to the aim of the mission graph, which
turns the fighter with `entity.set_ship_turn` while the right button is held. Its *Throttle Rate* (0.6 per second)
makes the throttle persistent, its *Min Throttle* (-0.4) bounds the reverse.

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Controls

| Action        | Keyboard              | Mouse                 | Gamepad           |
|---------------|-----------------------|-----------------------|-------------------|
| Pitch, yaw    | Arrows, A / D (yaw)   | Hold right button     | Left stick        |
| Aim           |                       | Move (click the view) |                   |
| Throttle      | W / S (kept)          |                       | RT / LT           |
| Cut throttle  | Backspace             |                       | Left stick click  |
| Roll          | Q / E                 |                       | LB / RB           |
| Slide         | X / C, R / V          |                       | Right stick       |
| Fire          | Space                 | Left button           | A                 |
| Lock target   | F                     | Middle button         | X                 |
| Next lock     | T (G: previous)       | Wheel                 | D-pad right, left |
| Wings attack  | 1                     |                       | Y                 |
| Wings escort  | 2                     |                       | B                 |
| Flight assist | Z                     |                       | Right stick click |
| Pause         | Escape, P             |                       | Start             |
| Menus         | Arrows, Enter, Escape | Point, click          | D-pad, A, B       |

The settings hold the master, music and effects volumes (the mixer buses), the interface size, the colour vision
correction, the reduced motion, the subtitles of the sounds, a sturdier convoy (it takes half the damage), a start
with the flight assist off and an inverted pitch.

## Inside

| Path                         | Content                                                                                           |
|------------------------------|---------------------------------------------------------------------------------------------------|
| `scenes/mission.owl`         | The route, the convoy, the player, the wingmen, the rocks, the gate, the planet                   |
| `scenes/`                    | Also `title`, `settings`, `victory`, `game_over`, over a view of the field                        |
| `scripts/game/mission.owlvs` | The scene script: the fight each tick, orders, lock, HUD, the end, pause; the leaves of the trees |
| `scripts/game/combat.lua`    | Lasers, hits, effects, rams, waves, freighters, lock, aim, orders: the lists of the fight         |
| `scripts/game/enemy.owlbt`   | An enemy: break off when hurt or too close, chase and fire when close, chase otherwise            |
| `scripts/game/wingman.owlbt` | A wingman: attack the nearest enemy or keep its slot, by the order                                |
| `tables/waves.owltable`      | The waves: distance of the convoy, count, kind, how far ahead, whom they go for                   |
| `prefabs/`                   | Fighter and interceptor (steering, tree, marker, trail), the lasers                               |
| `huds/play.owlhud`           | Hull, throttle, score, order, convoy, wave, route, aim, hit edges, messages, the pause window     |

The player flies with a `ShipController`; the wingmen and the enemies fly with a `SteeringAgent` in open space
(formation on the player, pursuit, flight, separation, avoidance), driven by a `BehaviourTreeAgent` whose Lua leaves
are exported functions of the mission graph. The freighters ride the `Route` spline with `SplineFollower`s. Move the
rocks, the route points and the gate in Owl Nest; edit the waves in the data table editor.

The ships, the rocks and the lasers are models of the Kenney Space Kit (CC0) converted for the rail shooter and
copied here with its starfield; the freighter, the gate, the title, the HUD art, the sounds, the music and the
jingles are home-made by `tools/samples/asteroid_field/generate_assets.py`; `generate_level.py` and
`generate_menus.py` wrote the first layout of the scenes, the prefabs, the trees, the table, the HUD and the graphs.
