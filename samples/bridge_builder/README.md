# Bridge Builder

A bridge building sample game of Owl Engine after Poly Bridge: six gaps of a mountain valley to span within a budget,
seen from the side. Lay roads and beams from node to node, then run the test: the bridge becomes real, the vehicles
drive across or the river takes them. Open `samples/bridge_builder` in Owl Nest (the **Sample games** list of the File
screen) and play `scenes/title.owl`, or package it with **Package the game**.

## How to play

Choose a gap in the level select (a gap opens once the one before is crossed). The red bolts on the cliffs and on the
rock pillars are the anchors; a part starts from an anchor or from a node already laid, and the end of a part is a new
node. Press **Test run**: every part becomes a body hinged to its nodes, and the vehicles waiting on the left cliff
drive to the right one.

- **Road** (100 a metre, up to 3 m) is the deck the vehicles drive on; **wood** (60, up to 4 m) and **steel** (160,
  up to 5 m, much stronger) are the supports, that no vehicle touches. A deck alone sags and snaps: brace it with
  triangles.
- During the run each part turns from green to red as its joints take the load; a joint pulled beyond its strength
  snaps. The run is won when every vehicle reaches the far side, lost when one falls in the river or the vehicles do
  not make it in 30 seconds; the plan comes back for another try. **Replay the run** on the result plays a won run
  again.
- The camera zooms and pans a little around the gap, and follows the cursor moved by the keys; **Grid** shows the
  points the parts snap to. The tool buttons carry an icon of their kind.
- A bridge that holds earns a star, two when it costs at most half way between the star budget and the budget, three
  under the star budget. The best result of each gap and its design are kept; the victory screen tallies the six.

The controls are shown in the **How to play** window of the title and in the pause window, never during play.

## Controls

| Action        | Keyboard                        | Mouse             | Gamepad           |
|---------------|---------------------------------|-------------------|-------------------|
| Cursor        | Arrows, W A S D                 | Point at the grid | D-pad, left stick |
| Lay a part    | Space (from a node, at its end) | Left click        | A                 |
| Erase or stop | X, Delete                       | Right click       | X                 |
| Undo          | Z, Backspace                    | **Undo** button   | B                 |
| Kind of part  | 1 2 3, Tab                      | The kind buttons  | LB / RB           |
| Test run      | R (again to edit)               | **Test run**      | Y                 |
| Zoom          | Q / E, Page Up / Page Down      | Wheel             | LT / RT           |
| Pan           | (follows the cursor)            | Middle drag       | Right stick       |
| Grid          | G                               | **Grid**          | Back              |
| Pause         | Escape, P                       |                   | Start             |
| Menus         | Arrows, Enter, Escape           | Point, click      | D-pad, A, B       |

The settings hold the master, music and effects volumes (the mixer buses), the interface size, the colour vision
correction, the reduced motion, the subtitles of the sounds, the colours of the loads and a test run at half speed.

## Inside

| Path                         | Content                                                                                 |
|------------------------------|-----------------------------------------------------------------------------------------|
| `scenes/level_1.owl` to `6`  | The gorge (cliffs and pillars, static 2D bodies), the anchors, the vehicles, the valley |
| `scenes/`                    | Also `title`, `level_select`, `settings`, `victory`, over the valley and a truss bridge |
| `scripts/game/level.owlvs`   | The scene script of a level: build, test run, win, stars, saves, pause                  |
| `scripts/game/vehicle.owlvs` | A vehicle drives across during the run, counts itself across or fallen                  |
| `scripts/game/part.owlvs`    | A part of the run reports a joint that snaps                                            |
| `scripts/game/bridge.lua`    | The lists of the build: nodes, parts, undo, cost, saved design, bodies and joints       |
| `scripts/ui/`                | The menus (`title`, `level_select`, `settings`, `result`) and `backdrop`                |
| `prefabs/`                   | The plan and the body of each kind of part, the nodes                                   |
| `huds/play.owlhud`           | Level, cost, kinds, Undo and Test run, the load, the result and pause windows           |

The bridge lives in the plane z = 0 and the camera looks at it from the side, straight along -Z (`bridge.view` keeps
it so when it zooms and pans, within the limits of `bridge.lua`); the bodies are 2D bodies locked to that plane, the
models deep blocks for the look. A part of the run is a prefab: a root body and a `Tail` part merged into
it, holding the joint of its far end; `physics.add_joint` hinges each end to the anchor of its node, or to the first
part of the node, with the break force of its kind, and `physics.get_joint_load` colours it. Each level scene holds
its budget, its star budget, the bounds of its grid and the next scene in the properties of its scene script: move the
cliffs, the pillars and the `Anchor` entities in Owl Nest, add or remove vehicles (each one with its two wheel parts),
and the level follows.

The models, the textures, the sounds, the music and the jingles are home-made by `tools/samples/bridge_builder/`
(`generate_models.py`, `generate_assets.py`, which also draws the icons of the tools); `generate_level.py` and
`generate_menus.py` wrote the first layout of the scenes, the prefabs, the HUD and the graphs.
