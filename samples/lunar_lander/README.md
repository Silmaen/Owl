# Lunar Lander

A landing sample game of Owl Engine after Lunar Lander: five landings on a small moon, from a hover above the first
pad to a descent from orbit into a crater of the far side. Open `samples/lunar_lander` in Owl Nest (the **Sample
games** list of the File screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

Bring the lander down on the lit pad, gently and upright. The moon pulls it towards its centre, harder near the
ground; the main engine pushes along the lander, so tilt it to steer, and the side jets slide it a little. Released,
the stick stops the lander turning; it keeps the attitude it has. The camera follows from behind and above; turn
it around the lander and zoom it as you like.

- The cyan beads show the path the lander is on for the next seconds, the red cross where that path meets the ground.
- The touchdown is the first foot pad on the ground; one faster than about 2 m/s snaps a leg; a leg broken, the hull
  touching the ground, a landing off the pad, the tanks empty in the air or the lander leaving the moon lose the
  landing. Retry starts that landing again.
- A landing scores 1000, 10 per unit of fuel left, up to 500 for landing near the centre of the pad and up to 400 for
  a soft touchdown. The victory screen lists the five landings; the best total is kept.
- The second and third landings start high and gliding towards the pad, the fourth and fifth in a circular orbit:
  turn the engine against the motion to brake, then come down.

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Controls

| Action      | Keyboard              | Mouse        | Gamepad     |
|-------------|-----------------------|--------------|-------------|
| Main engine | Space                 | Left button  | RT (or A)   |
| Tilt        | W A S D               |              | Left stick  |
| Side jets   | Arrows                |              | D-pad       |
| Turn view   | Q / E, R / F          | Right drag   | Right stick |
| Zoom        | Z / X                 | Wheel        | LB / RB     |
| Pause       | Escape, P             |              | Start       |
| Menus       | Arrows, Enter, Escape | Point, click | D-pad, A, B |

The settings hold the master, music and effects volumes (the mixer buses), the interface size, the colour vision
correction, the reduced motion, the subtitles of the sounds, the inverted tilt and the hidden predicted path.

## Inside

| Path                          | Content                                                                                 |
|-------------------------------|-----------------------------------------------------------------------------------------|
| `scenes/landing_1.owl` to `5` | The moon, its five pads, the Earth, the lander, the beads of the path, the chase camera |
| `scenes/`                     | Also `title`, `settings`, `victory`, `game_over`, over a live view of the moon          |
| `scripts/game/landing.owlvs`  | The scene script of a landing: flight, instruments, touchdown, crash, score, pause      |
| `scripts/game/hull.owlvs`     | The hull reports a broken leg or a crash of the hull                                    |
| `scripts/game/lander.lua`     | The flight maths the graphs call: thrust, attitude, telemetry, predicted path, camera   |
| `scripts/ui/`                 | The menus (`title`, `settings`, `result`) and `backdrop`, which turns the view          |
| `huds/play.owlhud`            | Landing, pad, score, fuel, altitude and speeds, messages and the pause window           |
| `models/`                     | The moon, the pad, the lander hull and leg, the plume, the beads and the impact ring    |

The moon is one mesh: its `MeshRenderer`, a static `Mesh` body and a spherical `GravitySource` that pulls the
lander. The lander is the `Lander` body and four `Leg` bodies, children of it in the hierarchy so that the editor
moves them together, each held by a fixed `PhysicJoint` with a break force: the leg breaks when a touchdown loads it
beyond. Each landing scene sets its pad, its fuel and the start velocity in the properties of its scene script;
move the lander, the pads or the camera in Owl Nest to make another one, and point the `next_scene` of the landing
before it at it.

The models, the textures, the sounds, the music and the jingles are home-made by `tools/samples/lunar_lander/`
(`generate_models.py`, `generate_assets.py`); the starfield is the one of the rail shooter; `generate_level.py` and
`generate_menus.py` wrote the first layout of the scenes, the HUD and the graphs.
