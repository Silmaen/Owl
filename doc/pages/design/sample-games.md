# Sample games {#page-design-sample-games}

[TOC]

Owl Engine ships two kinds of example content:

- **Engine Demo** (`engine_demo/`): an all-in-one showcase of every engine feature, not a game in itself.
- **Sample games** (`samples/`): small but real games, each one built on only what it needs, most of them mini remakes
  of known games. They prove that Owl Engine makes games, give a newcomer a project of a readable size to start from,
  and feed the two showcase games of v1.0.

Each sample game has its user page, listed in [Sample Games](../samples.md); this page holds the rules they follow
and the design decisions behind them.

## Rules

- One project per game, beside the Engine Demo, authored in Owl Nest; game rules in scripts, never in the engine.
- Visual scripts first ([Visual scripts in every game](#sample-games-visual-scripts)): everything that reads as a
  graph is a `.owlvs`; Lua stays for what a graph would hide (an undo stack, a level generator, a long table of data),
  as a Custom Lua node or a module called from the graph.
- Small: one screen or a handful of levels, finished (title, play, win / lose, settings), playable with keyboard,
  mouse and gamepad.
- CC0 or home-made art only (the reworked `textures/wolf3d/` of the Engine Demo included); a scenario test per game runs
  in CI, and the game packages with the runner.
- A sample game uses only what it needs: it is not a feature checklist, the Engine Demo is.
- Gameplay is settled with the maintainer at the first playthrough of each game.

## Common to every game {#sample-games-menus}

- Title, settings, victory and game over scenes; menus of its own, written by `tools/samples/<game>/generate_menus.py`
  (Lunar Lander, Asteroid Field, The Sunken Shrine and Bridge Builder on `tools/samples/menu_kit.py` and its
  `Layout`): its look, its title art, its buttons, its transitions
  (`scene.transition_to` in a colour) and an animated background. A How to play window on the title and the pause
  window are the only places that show the controls.
- `input_actions.yml` on the built-in actions (`move`, `look`, `jump`, `interact`, `primary`, `secondary`, `ui_*`)
  plus the game's own; `input/gamecontrollerdb.txt` copied.
- Sounds written by a generator in `tools/samples/<game>/`, a start jingle of its own per game; music and effects on
  the mixer buses with their sliders in the settings; the pause stops the scene clock (`time.set_paused`).
- The game state mirrored into `gamestate` keys (`<game>_score`, ...) for the scenarios.

## Layout

```
samples/<game>/
  owl_project.yml  game_settings.yml  input_actions.yml  input/gamecontrollerdb.txt
  scenes/  huds/  prefabs/  tilesets/  tilemaps/  scripts/  textures/  sounds/
tools/samples/<game>/                 art, sound, level and menu generators (neither exported nor installed)
test/unit/scenarios/*.owltest         scenario tests
```

- One self-contained project per game: the export packs one project folder, so a sample copies what it reuses from
  the Engine Demo rather than pointing at it. Each `owl_project.yml` has its own `name` (the save folder) and a title
  scene as `firstScene`.
- Scenarios live in `test/unit/scenarios/` (one CTest each, label `scenario`), named `<game>_<behaviour>.owltest`,
  with `scene: ../../../samples/<game>/scenes/<scene>.owl`; no test file is exported.
  `GameExportCoverage.EverySampleGameHasAScenario` fails for a sample whose scenes no scenario plays.
- Export: `GameExportEndToEnd.PlaysFromItsExportFolder` (`test/unit/export_tests/GameExport_test.cpp`) exports every
  sample and plays it headless ([Game export](game-export.md)).
- Install: `source/owlnest/CMakeLists.txt` installs `engine_demo/` and `samples/` (component `Nest`) side by side.
- Owl Nest: the File screen lists the sample games under **Sample games** (`panel::findSampleGames`).
- CI checks: `samples/` is in `_CODE_ROOTS` of `ci/utils/doc_identifiers.py`; `SampleHudLayout`
  (`DemoHudLayout_test.cpp`) applies the demo HUD rules ([HUD system](hud-system.md)) to every `.owlhud` of
  `samples/` and to every sample scene holding a `Canvas`.

## Engine Demo {#sample-games-demo-revamp}

The Engine Demo stays as small as possible and as complete as possible: every scene opens from the world map and
returns to it, none is reached any other way.

| Item        | What exists                                                                                                                                           |
|-------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| World map   | The only way in: the main menu starts the hub, no shortcut key leads to a scene                                                                       |
| Doors       | One script, `scripts/hub/place_door.owlvs`, with properties (target scene, transition, completion key, entry values) for every place                  |
| Progress    | Six places: platformer, raycast, iso town, voxel, tavern (cellar included), observatory; the portal wakes at six of six                               |
| Hub         | A `HudMarker` on each place left, a minimap, day and night with 2D lights, a reverb cave with adaptive music, Kenney Tiny Farm and Tiny Dungeon tiles |
| Tavern      | Kenney Mini Characters: a barkeeper with a tankard on a socket, a server walking a spline, two seated patrons, a dialogue each                        |
| Observatory | Its telescope opens `far_orbit`, flown from a cockpit HUD with a `ShipController`; the objective is to dock with the debris, then back to the hub     |
| Main menu   | A live diorama mixing styles: the 2D village in front, the 3D tavern behind, day and night; Start is the only way in                                  |
| Scenarios   | Every scene is played by a scenario, checked by `GameExportCoverage.EveryDemoSceneHasAScenario`                                                       |
| Rule        | Each release puts its features in the existing places instead of adding isolated scenes                                                               |

## Visual scripts in every game {#sample-games-visual-scripts}

Every entity, scene and menu script of the sample games is a graph (`.owlvs`): the rules of the level, the player,
the pick-ups, the menus and their transitions, the HUD handlers. The Lua left is the code a graph would hide: a
function holding a closure, a `pairs` walk over text patterns or a long computation stays a Custom Lua node of its
graph, and the shared rule modules stay `.lua` modules whose functions the graphs call as Lua Function nodes
(`---@param` annotations).

| Game            | Lua left, and why                                                                                                                                                             |
|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Breakout        | Custom node `read_events` of `game.owlvs`: it reads the queue of brick events with `string.gmatch`                                                                            |
| Sokoban         | The undo stack (`take_back`, `undo`, `restart` custom nodes of `sokoban.owlvs`); `levels.lua`, the level table and the progress keys, a module                                |
| Wolfenstein     | The fog of war of `explore.owlvs` (`update_fog`, `restore_seen`, `add_blocker`, the marker and minimap follow-up): ray walks over the 32×32 grid, rows spread over the frames |
| Tower defence   | The modules of `scripts/game/td/` (towers, enemies, waves, effects, camera, wheel, input, tutorial), called from `defence.owlvs`; the reveal animation of the world map       |
| Rail shooter    | The modules of `scripts/game/rs/` (field, foes, guns, effects), called from `rail.owlvs`; the `on_create` and `on_update` of `rail.owlvs`, which pass functions as callbacks  |
| Point-and-click | `adventure.lua`, the cursor, verbs, bag and dialogues, a module the room graphs call; the `act` handlers of the rooms, which hand closures to the act animation               |
| Lunar lander    | `lander.lua`, the vector maths, an annotated module                                                                                                                           |
| Asteroid field  | `combat.lua`, the lists of the fight                                                                                                                                          |
| Bridge builder  | `bridge.lua`, the lists of the build                                                                                                                                          |
| Sunken shrine   | `shrine.lua`, the block                                                                                                                                                       |

The level generators are Python tools (`tools/samples/<game>/`), the long data are data tables. The largest graphs
(`sokoban.owlvs`, `defence.owlvs`, `rail.owlvs`, about a thousand nodes each) are laid out one row per event or
function.

## Design notes per game

### Breakout

[Breakout](../sample_breakout.md), `samples/breakout/`.

- A broken brick is destroyed at once (a popping brick would keep its body); its burst
  (`prefabs/brick_burst.owlprefab`, tinted with `entity.set_tint`) and the power-up capsules are prefabs spawned with
  `scene.spawn_prefab` and destroyed when done.
- The wide paddle is a second paddle (a body cannot be resized from Lua); the paddle follows the mouse motion
  (`breakout_mouse_speed` setting).
- One scene per level; each level names the next one in the `next_scene` property of its `Game` script, so a new
  level is a duplicated scene in Owl Nest.

### Sokoban

[Sokoban](../sample_sokoban.md), `samples/sokoban/`.

- One `.owltilemap` per level: layer 0 floor and walls, 1 goals, 2 crates and the hero tile of the start; the rules
  read the cells with `isometric.get_cell` and keep the undo stack in a Lua table.
- Level editing in Owl Nest: paint the three layers in the tilemap editor; for a new level, duplicate a level scene,
  point its `Level` at a new tilemap, set the `level` property of its `Game` script, then add its row (scene, name) to
  `tables/levels.owltable`: the title, the level select, the victory screen and the level before it follow the table.
  `sokoban.owlvs` warns when crates and goals do not match.
- `tools/tests/test_sokoban_levels.py` replays a stored solution of each level text of `tools/samples/sokoban/`.

### Wolfenstein mini {#sample-games-wolfenstein-combat}

[Wolfenstein mini](../sample_wolfenstein.md), `samples/wolfenstein/`: three guarded floors to explore.

- Floors carved by `tools/samples/wolfenstein/generate_levels.py`, which refuses a floor with a cell out of reach (so
  100 % is always possible; a slid secret wall counts as a wall where it stops), then written as Owl Nest saves them.
- Map explored: the fog of `explore.owlvs` on a 32×32 minimap, set up 8 rows per frame (a single `on_create` loop
  overruns the script time quota on emulated arm64), kept in the gamestate across saves.
- Pick-up sparks: a `RendererParticles` layer with `Projection: Raycast` draws them in the first-person view.
- Guards: `guard.owlbt` (flinch, fire, chase when alerted, patrol), its leaves exported functions of `guard.owlvs`;
  line of sight by `physics.raycast`, chase over the grid; 50 health, 0.5 to 1 second of surprise once alerted, hits
  likelier the closer the player, shots heard within 10 cells through the walls.
- Art: *The Guard* and *FPS weapons overlay* (CC0, OpenGameArt), sliced by
  `tools/samples/wolfenstein/generate_combat.py` from the downloaded sheets; `place_combat.py` writes the guards, the
  gear, the `Arsenal` and `Status` entities into the floor scenes, replacing only those entities.
- Feedback: the status bar face and `status.owlvs` come from `generate_status.py`; the red overlays are frames of
  sprite sheets (`ui.set_image_frame`), the side of a hit the guard position `guard.owlvs` records (`wolf_hit_x`,
  `wolf_hit_y`), the shake a small yaw wobble `player.owlvs` adds from the `wolf_shake_yaw` game-state key.

### Tower defence {#sample-games-tower-defence}

[Tower defence](../sample_tower_defence.md), `samples/tower_defence/`: the Horde against human towers and a keep, ten
levels in 3D with relief, the scene that mixes the most rendering styles.

| Element                       | Rendered by                                    | How                                                                                                |
|-------------------------------|------------------------------------------------|----------------------------------------------------------------------------------------------------|
| Terrain, river, bridge, decor | `RendererMesh`                                 | Kenney Tower Defense Kit tiles, one entity per cell, raised and stacked for the plateaus           |
| Towers                        | `RendererMesh`                                 | Kit tower parts stacked by level (base, middle, top, weapon), power-ups by their tints and weapons |
| Keep, walls, gate, banners    | `RendererMesh`                                 | Kenney Castle Kit                                                                                  |
| Sky, sun, night               | `ProceduralSky`, `DayNightCycle`               | Sun with cascaded shadows; point `Light` torches on the keep and along the road at night           |
| Enemies                       | `Renderer2D` `sprites`: billboards, depth test | 8 directions x 9 frames (`creatures.py`), the frame picked from heading and view; the boss bigger  |
| Projectiles and effects       | `sprites`                                      | Arrows, shells, shards, bursts, fire; a `Light2D` on fire shells and torches                       |
| Build wheel, health bars      | `marks` over all; bars in `sprites`            | The wheel beside its pad on the screen, picked by tag                                              |
| Range ring, build cursor      | `Renderer2D` `ground`: flat, depth test        | Lies on the terrain, hidden by the towers in front                                                 |
| Minimap                       | `RenderTarget`                                 | Orthographic camera over the level into a `UiImage` of the HUD                                     |
| World map                     | `Renderer2D` `world`                           | The inked map, sites, banners, stars, road dashes, clouds, a 2D scene between the levels           |
| HUD and menus                 | `Canvas`                                       | Gold, lives, wave, minimap frame, spells, pause; parchment, wood and gold                          |

- Engine: `Renderer2D` layer keys `Billboard` (`None`, `Upright`, `Full`: the sprites turn to the camera, their 2D lights
  on the ground plane) and `DepthTest` (sprites tested against the 3D layers drawn before, sorted back to front), both
  following the light and exposure of the 3D scene; `scene.pick_entity` with a tag returns the tagged ancestor of the
  mesh hit and sees billboards as drawn.
- Data-driven levels: `tools/samples/tower_defence/maps.py` holds per level a height grid, the road, the river, the
  pads, the decor, the waves, the nights and the briefing; `generate_maps.py` writes one scene per level, its waves
  table and its briefing dialogue, editable in Owl Nest. The roads are the `Linear` splines of the `Road` and `Road B`
  entities; the towers and the enemies are the rows of `tables/towers.owltable` and `tables/enemies.owltable`.
- Each enemy is a `SteeringAgent` that arrives at its lane point, keeps apart (flock `horde`) and steps around the
  others; the steering moves only where an enemy stands, while its progress, the leaks and the towers' aim read the
  distance walked along the road, so the waves keep their timing.
- Water, ground and snow are material graphs (`td_water`, `td_ground`, `td_snow`), given to the tiles by
  `generate_materials.py`.
- Stars: the best of each level is kept and the upgrades screen spends their sum; replaying a level for more stars
  credits the difference; Refund all is free.

### Rail shooter {#sample-games-rail-shooter}

[Ring Runner](../sample_rail_shooter.md), `samples/rail_shooter/`.

- Rail coordinates: every rule works in (distance along the rail, across, up), placed through `spline.sample` and
  `spline.get_tangent` each frame, so a shift of the floating origin changes nothing; enemies fly in the frame of the
  carrier and keep pace with the ship.
- Aim assist: a shot close to the line of the reticles goes straight at the nearest target; enemy shots go wide in
  a fixed pattern, so a run plays the same each time and a weaving ship dodges them.
- Tempers: each row of the `enemies` table has an `accuracy` (the wide offset taken off, the shot led on the ship's
  motion) and a `pursuit` (the pattern drawn towards the ship until it closes in) beside its rate of fire.
- Two wingmen in formation on the ship fire in turn at the nearest enemy ahead; `line` and `vee` squadrons fly in the
  formation of their middle fighter and scatter when it is shot (`foes.lua`).

### Point-and-click {#sample-games-point-and-click}

[The Moon Lantern](../sample_point_and_click.md), `samples/point_and_click/`.

- 320 x 180 pixel-art backdrops with 3D skinned actors scaled by depth and walk-behind masks.
- The hero paths on `RoomNav`, a `NavMeshVolume` baked from the walk area of the room (`WalkAreas` sources); the actors
  keep their walk agents: the mesh gives the corners of the path, the agent walks from one to the next.
- No dead end: nothing is used up before its puzzle.

### Lunar lander {#sample-games-lunar-lander}

[Lunar Lander](../sample_lunar_lander.md), `samples/lunar_lander/`.

- A static `Mesh` moon with a spherical `GravitySource`; the hull and four legs on breakable fixed joints.
- Thrust by `physics.add_force` in `on_tick`, the stick as a turn rate (`physics.set_angular_velocity`).
- The predicted path is `physics.predict_trajectory` drawn as beads, cut by a ray cast where it meets the ground.
- A failed landing starts again from its scene (the rewind of
  [Simulation and networking](simulation-networking.md#simulation-rewind) has no Lua call).

### Asteroid field {#sample-games-asteroid-field}

[Asteroid Field](../sample_asteroid_field.md), `samples/asteroid_field/`.

- Wingmen and enemies are `SteeringAgent`s driven by behaviour trees (`wingman.owlbt`, `enemy.owlbt`) whose leaves are
  the exported functions of `mission.owlvs`. A tree starts at the tick after its spawn, so `combat.lua` writes the
  target of a new enemy once `bt.set_value` takes it.
- Aim: the mouse moves an aim circle (a cursor of `combat.lua` kept in the view, so it works with the cursor captured);
  the lasers leave toward the point of the circle's ray (`scene.get_view_ray`) at the range of the lead, within 34° of
  the nose. The lasers carry the fighter's velocity, so the lock's X solves the intercept in the fighter's frame:
  (w.w - s²) t² + 2 (r.w) t + r.r = 0 with r the target offset, w its velocity minus the fighter's, s the laser speed.
- The right button turns the fighter toward the circle through `entity.set_ship_turn`; the `ShipController` keeps a
  throttle (*Throttle Rate*, *Min Throttle*), cut by `entity.set_ship_throttle`.
- Freighters are `NavObstacle`s the enemies steer around; an enemy too close to the hull it attacks breaks off and
  holds its fire, so the damage of a freighter comes from shots crossing the gap.
- Lasers are ray-cast against the rocks (static spheres) and tested against the ships; rams and rebounds by sphere
  overlaps, the velocity reflected through `entity.set_ship_velocity`.

### Bridge builder {#sample-games-bridge-builder}

[Bridge Builder](../sample_bridge_builder.md), `samples/bridge_builder/`.

- A false 2D: a 3D side camera over bodies locked to the plane z = 0; parts become 2D bodies hinged at their nodes
  (`physics.add_joint`) when the test runs.
- A part body is a prefab with a `Tail` part holding the joint of its far end, one joint per entity; at a node the
  parts hinge to the anchor, or to the first part of the node.
- Loads: `physics.get_joint_load` against the break force of each kind, drawn as a colour on the part.
- **Replay the run** rebuilds the design and runs it again: the run is driven by the design alone, so it plays the same
  way.

### One-room Zelda-like {#sample-games-zelda-like}

[The Sunken Shrine](../sample_sunken_shrine.md), `samples/sunken_shrine/`.

- The creatures are `SteeringAgent`s in the plane on kinematic bodies, kept inside the walls of their room, on
  behaviour trees; no NavMesh.
- A door rule per room in its scene script: `key`, `braziers`, `block`, `boss`.
- An exit only leads on when the hero walks into it after a moment in the room: a creature pushing the hero does not
  send it out.

## Pool {#sample-games-pool}

Ideas without commitment, planned in no release. A release picks its sample games from the pool once its features are
done; every game of the pool is possible with 1.0, built or not. The two v1.0 showcase games (one 2D, one 3D) are
picked from the sample games and the pool.

| Game                  | Inspired by                       | Exercises                                                                          |
|-----------------------|-----------------------------------|------------------------------------------------------------------------------------|
| Mini Transport Tycoon | Mini Motorways / TTD              | isometric, roads, pathfinding, management HUD                                      |
| Mini Minecraft        | Minecraft                         | voxel, build and break, inventory                                                  |
| Marble or mini kart   | Marble Madness / Kart             | 3D, Jolt physics, shadows                                                          |
| Networked Bomberman   | Bomberman                         | tilemap, deterministic simulation, multiplayer                                     |
| Asteroids             | Asteroids                         | 2D inertia, screen wrap, particles                                                 |
| Station trader        | Taipan! / Elite                   | data tables, economy on the fixed tick, trade HUD                                  |
| Orbital golf          | Angry Birds Space / Gravitee Wars | 2D gravity fields, trajectory preview                                              |
| Visual-script puzzle  | Lemmings / pipe puzzles           | rules written only in visual scripts, tilemap, HUD                                 |
| Mini roguelike        | Rogue / Spelunky                  | levels from PCG graphs, tilemap, saves                                             |
| Mini launcher         | Model rocketry                    | parts on sockets, articulated bodies, gravity, orbit prediction, time warp, planet |

A plan is refined when its game is picked. First sketches:

- **Mini Transport Tycoon**: a 64×64 isometric map, two towns, four industries; roads, stations and trucks with a route
  each (`path.find`, `path.poll`); win on company value before year 5, lose on debt. Reuses the iso town of the Engine
  Demo (`scripts/iso/`, `huds/iso_town.owlhud`, `tilesets/iso_ground.owltileset`, `tools/iso_atlas`).
- **Mini Minecraft**: a finite island (about 96×96×48) authored in Owl Nest, a non-procedural `VoxelWorld` whose
  chunks a save keeps; break, collect and build from a 9-slot hotbar (`voxel.set_place_block`, `on_block_broken`);
  place 3 diamonds on the altar.
- **Mini launcher**: assemble a rocket from parts snapped on sockets, launch it and reach a target orbit, then a moon.

## Open gaps {#sample-games-gaps}

No engine gap is open: every game of the pool is reachable from scripts, the ones the audit of the pool against the
engine looked at included. Voxel building and its saves (Mini Minecraft), the isometric zoom (Mini Transport Tycoon),
3D bodies, gravity sources, trajectory prediction, joints and assemblies (marble, orbital golf, launcher), lockstep
multiplayer (Bomberman), data tables (station trader), the camera size and field of view (`entity.set_camera_size`,
`entity.set_camera_fov`), the tilemap switch (`tilemap.set_asset`, Sokoban in one scene) and procedural generation from
a seed (`tilemap.generate`, Mini roguelike) are all there.
