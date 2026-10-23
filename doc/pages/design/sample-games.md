# Sample games {#page-design-sample-games}

[TOC]

Design page of the sample games, summarised in the [Roadmap](../roadmap.md).

## Goal

Owl Engine ships two kinds of example content:

- **Engine Demo** (today's `engine_demo/`): an all-in-one showcase of every engine feature, not a game in itself.
- **Sample games**: small but real games, each one built on only what it needs, most of them mini remakes of known
  games. They prove that Owl Engine makes games, give a newcomer a project of a readable size to start from, and feed the
  two showcase games of v1.0.

## Rules

- One project per game, beside the Engine Demo, authored in Owl Nest; game rules in Lua, never in the engine.
- Small: one screen or a handful of levels, finished (title, play, win / lose, settings), playable with keyboard,
  mouse and gamepad.
- CC0 or home-made art only (the reworked `textures/wolf3d/` of the Engine Demo included); a scenario test per game runs in CI, and the game packages with the runner.
- A sample game uses only what it needs: it is not a feature checklist, the Engine Demo is.

## Short list

| Game                  | Inspired by           | Exercises                                       | Possible from |
|-----------------------|-----------------------|-------------------------------------------------|---------------|
| Breakout              | Breakout / Arkanoid   | 2D, physics, HUD, sound, gamepad                | v0.4.0        |
| Sokoban               | Sokoban               | tilemap, undo, saves, level editing in Owl Nest | v0.4.0        |
| Wolfenstein mini      | Wolfenstein 3D        | raycast, doors, pick-ups, HUD                   | v0.4.0        |
| Tower defence         | Kingdom Rush          | isometric, waves, tile-graph pathfinding, HUD   | v0.5.0        |
| Mini Minecraft        | Minecraft             | voxel, build and break, inventory               | v0.4.0        |
| One-room Zelda-like   | The Legend of Zelda   | top-down, dialogue, enemy AI, 2D lights         | v0.7.0        |
| Marble or mini kart   | Marble Madness / Kart | 3D, Jolt physics, shadows                       | v0.7.0        |
| Networked Bomberman   | Bomberman             | tilemap, deterministic simulation, multiplayer  | v0.8.0        |
| Lunar lander          | Lunar Lander          | 3D, thrust and gravity, planetary terrain, legs | v0.7.0        |
| Asteroid field        | Star Fox / X-Wing     | 6DoF flight, 3D steering AI, trails, HUD marks  | v0.7.0        |

Breakout, Wolfenstein mini and Sokoban shipped in v0.4.0; a tower defence comes next, in v0.5.0, on the Lua gaps
closed for it and for Mini Transport Tycoon ([Order](#sample-games-order)), which waits in the
[Pool](#sample-games-pool). The two v1.0 showcase games (one 2D, one 3D) are picked from this
list, and so are the games of the [Pool](#sample-games-pool) once they enter it.

## Pool {#sample-games-pool}

Ideas without commitment: a game of the pool enters the short list when a release has room for it, or is dropped.

| Game                  | Inspired by                       | Exercises                                          | Possible from |
|-----------------------|-----------------------------------|----------------------------------------------------|---------------|
| Mini Transport Tycoon | Mini Motorways / TTD              | isometric, roads, pathfinding, management HUD      | v0.5.0        |
| Asteroids             | Asteroids                         | 2D inertia, screen wrap, particles                 | v0.4.0        |
| Station trader        | Taipan! / Elite                   | data tables, economy on the fixed tick, trade HUD  | v0.6.0        |
| Rail shooter          | Star Fox                          | splines, 3D, particles, HUD markers                | v0.7.0        |
| Orbital golf          | Angry Birds Space / Gravitee Wars | 2D gravity fields, trajectory preview              | v0.7.0        |
| Bridge builder        | Poly Bridge / Bad Piggies         | parts snapped on sockets, breakable joints, replay | v0.7.0        |

## v0.4.0 games

Feasibility checked against `main` on 2026-10-18 (Lua bindings, components, Engine Demo): all five games fit the
v0.4.0 engine. Breakout and Mini Transport Tycoon need no engine change, Sokoban one Lua binding
(`isometric.get_size`, added), Wolfenstein mini art, Mini Minecraft two small engine fixes ([Gaps](#sample-games-gaps)).
Breakout, Sokoban and a Wolfenstein mini without combat shipped with v0.4.0; the Lua gaps G1, G3, G7, G8 and G15 are
closed in v0.5.0, Mini Minecraft is planned in v0.6.0; Mini Transport Tycoon waits in the pool.

Common to every game:

- Title, settings, victory and game over scenes started from the Engine Demo (`scenes/main_menu.owl`,
  `scenes/settings_menu.owl`, `scenes/victory.owl`, `scenes/game_over.owl`, `scripts/ui/*.lua`): the menus already
  work with keyboard, mouse and gamepad (`UiFocus`, scenario `main_menu_focus.owltest`). Since the first playthrough,
  each game has menus of its own, written by `tools/samples/<game>/generate_menus.py`: its look, its title art, its
  buttons, its transitions (`scene.transition_to` in a colour) and an animated background; a How to play window on
  the title and the pause window are the only places that show the controls.
- `input_actions.yml` on the built-in actions (`move`, `look`, `jump`, `interact`, `primary`, `secondary`, `ui_*`)
  plus the game's own; `input/gamecontrollerdb.txt` copied.
- Sounds written by a generator in `tools/samples/<game>/`, like `tools/demo/generate_town_sounds.py` (the Kenney
  audio packs cannot be fetched from the build machine), a start jingle of its own per game;
  `sounds/ui/victory.wav` copied.
- The game state mirrored into `gamestate` keys (`<game>_score`, ...) for the scenarios.

### Breakout

Done in v0.4.0: `samples/breakout/` (how to play: its `README.md`).

| Item     | Made                                                                                                             |
|----------|------------------------------------------------------------------------------------------------------------------|
| Play     | One screen, 5 levels, 3 lives; bricks of 1 to 3 hits; 2 power-ups (wide paddle 15 s, slow ball 12 s)             |
| Win/lose | Every brick of the 5 levels / no life left; best score kept; the ball serves by itself after 5 s                 |
| Controls | `paddle` axis (A / D, arrows, left stick, D-pad) or the mouse; `serve` (Space, pad A, left click); `pause`       |
| Scenes   | `title`, `level_select`, `level_1` to `level_5` (one scene each), `settings`, `victory`, `game_over`             |
| Menus    | Arcade neon (`generate_menus.py`): a brick wall lit in a wave, an attract ball, neon buttons, magenta wipes      |
| Entities | `Paddle` and `PaddleWide` (Kinematic), `Ball` (Dynamic, restitution 1, friction 0), three `Wall` (Static)        |
| Bricks   | Instances of `prefabs/brick.owlprefab` (Static body, `SpriteRenderer`, `brick.lua` with its properties)          |
| Lua      | `game.lua` (rules, pools, pause), `paddle.lua`, `ball.lua` (speed kept with `physics.set_velocity`), `brick.lua` |
| HUD      | `huds/play.owlhud`: score, level, lives, a message line, the pause window (Resume, Restart, Quit, controls)      |
| Art      | Home-made bricks, paddle, ball, capsules, walls and backdrop, chip sounds and music (`tools/samples/breakout/`)  |
| Saves    | No slot; best score and levels cleared with their best through `settings.save` (`breakout_best`, `_done_N`)      |

- Reuse: the menus, built from the Engine Demo ones, have a
  look of their own. A broken brick is destroyed at once (a popping brick
  would keep its body), its burst shown from a pool instead of the pop of `coin.lua`.
- Gaps met: G5 (the paddle follows the mouse motion, `breakout_mouse_speed` setting), G1 (pools of hidden entities:
  power-ups, bursts, and a second paddle for the wide one, a body cannot be resized from Lua), G14 (one scene per
  level), G9 (master volume only), G8 (the pause stops the scene clock, `time.set_paused`). Each level names the next
  one in the `next_scene` property of its `Game` script (G16 fixed): a new level is a duplicated scene in Owl Nest.
- Scenarios: `breakout_serve` (serve, 300 frames, `breakout_bricks` below 40), `breakout_lose` (no input at all:
  the automatic serve, `breakout_lives` below 3), `breakout_gamepad` (dead zone, the stick moves the paddle, A serves),
  `breakout_level_select` (down selects level 2, accept starts a fresh run there), `breakout_how_to` (the How to play
  window opens and closes with the keyboard focus).
- The levels were first laid out by `tools/samples/breakout/generate_levels.py` (text grids) and are edited in Owl Nest
  from then on; `tools/tests/test_breakout_levels.py` checks the grids.

### Sokoban

| Item     | Done                                                                                                          |
|----------|---------------------------------------------------------------------------------------------------------------|
| Play     | 10 levels of 1 to 5 crates (2 to 38 pushes); push the crates onto the goals; undo, restart, moves and pushes  |
| Win/lose | Every crate on a goal; no lose; each level done and its fewest moves are kept                                 |
| Controls | `step_*` (WASD, arrows, D-pad) one cell per press, repeated when held; the left stick as four directions      |
|          | Z, U, Backspace or pad B undo; R or pad Y restart; Esc, P or Start pause; the mouse in menus and HUD buttons  |
| Scenes   | `title`, `level_select` (`UiList` with done marks), `level_01` to `level_10`, `settings`, `victory`           |
| Tilemap  | One `.owltilemap` per level: layer 0 floor and walls, 1 goals, 2 crates and the hero tile of the start        |
| Entities | `Level` (`Tilemap`), player (`AnimatedSpriteRenderer`, `isometric.set_sprite_frame`), `MovingBox`, music      |
| Lua      | `sokoban.lua` (rules on `isometric.get_cell`, undo stack in a Lua table, win), `level_select.lua`, menus      |
| HUD      | `huds/level.owlhud`: level, moves and pushes, best, Undo / Restart / Menu buttons, pause window with controls |
| Menus    | Wooden warehouse (`generate_menus.py`): crate letters, plank buttons, a progress log, an animated corridor    |
| Art      | `sokoban_tiles.png`: Tiny Dungeon floor, wall and hero (walk and push frames), home-made goal mark and crates |
| Saves    | No slot: `sokoban_done_NN` and `sokoban_best_NN` settings saved at once; Continue starts the first level left |

- Level editing in Owl Nest: paint the three layers in the tilemap editor (the hero tile of layer 2 places the player);
  for a new level, duplicate a level scene, point its `Level` at a new tilemap, set the `level`, `level_name` and
  `next_scene` properties of its `Game` script (and `next_scene` of the level before), then add the scene to
  `LEVEL_SCENES` of `title.lua` and `level_select.lua`, which list every level for the progress and the level select.
  `sokoban.lua` warns when crates and goals do not match.
- Generators in `tools/samples/sokoban/`: `generate_assets.py` (tile sheet, sounds), `generate_levels.py` (tileset,
  tilemaps, level scenes, HUD, from the texts of `levels.py`), `generate_menus.py`; `tools/tests/test_sokoban_levels.py`
  replays a stored solution of each level text.
- Reuse: the walk frames of `tools/demo/generate_kenney_atlases.py`, `scripts/hub/hub_water.lua` (`get_cell` /
  `set_cell` on a 2D tilemap), the journal list of `scripts/raycast/inventory.lua`; the menus, first copied from the
  Engine Demo, have a look of their own since the first playthrough.
- Gaps: G6 (tile bindings named `isometric`), G14 (one scene per level), G9 (master volume only). Lua had no way to
  read the size of a tilemap: `isometric.get_size` was added for it (width, height, cell size).
- Scenarios: `sokoban_push` (`sokoban_on_goal` grows), `sokoban_undo` (undo and restart put back crate and counters),
  `sokoban_wall` (no step into a wall nor a blocked crate), `sokoban_win` (level 1 solved, `sokoban_done_01` set, level 2
  opens), `sokoban_gamepad` (D-pad, B, stick dead zone), `sokoban_level_select` (`ui_down`, `ui_accept`).
- Size: **S**, done.

### Tower defence

First map only, basic art, to validate the gameplay; at least five maps once it plays well.

| Item     | Plan                                                                                                                          |
|----------|-------------------------------------------------------------------------------------------------------------------------------|
| Play     | One isometric map of 24×24, 8 waves; enemies walk a road from the gate to the keep, towers on fixed build pads beside it     |
| Win/lose | Every wave survived with at least one life / 20 lives lost (one per enemy reaching the keep)                                 |
| Waves    | Timed: a countdown before each wave; the player calls it early for a gold bonus proportional to the time left               |
| Towers   | Archer (fast, single target), cannon (slow, splash), frost (slows, small damage); no upgrade yet, sold for half its cost     |
| Enemies  | Runner (fast, weak), soldier (average), armoured (slow, resists arrows); gold on each kill                                    |
| Controls | Mouse: left on a pad opens the build ring or selects a tower, right cancels; Space or pad A calls the next wave early        |
|          | F or RB fast forward (×2); Esc, P or Start pause (controls shown there only); WASD, stick or edge scroll pans, wheel zooms   |
| Scenes   | `title`, `map_01`, `settings`, `victory` (end stats animated: kills, lives kept, gold, time), `game_over`                    |
| Tilemap  | `Ground` (isometric terrain, road overlays with `navigation` rules as in `iso_town.owl`), build pads as overlay tiles        |
| Entities | Prefabs `enemy_*`, `tower_*`, `projectile_*` spawned by Lua (G1); keep, gate, a few decor objects, camera                    |
| Lua      | `waves.lua` (wave table, countdown, early call, spawns, `path.find` gate → keep), `towers.lua` (targeting, cooldowns, sell), |
|          | `enemies.lua` (follow the cells, slow, armour, death and gold), `game.lua` (lives, gold, pause and fast forward via G8)      |
| HUD      | Gold, lives, wave n / N with the countdown and a call-early button, speed toggle; build ring on a pad (choice, cost, range)  |
| Menus    | Own look, kept simple (`generate_menus.py`)                                                                                   |
| Art      | Basic home-made shapes from `generate_assets.py` (towers, enemies with a heading, projectiles); terrain from `tools/iso_atlas` |
| Sounds   | Short home-made neutral effects (build, shot, hit, wave horn)                                                                |
| Saves    | No slot: `tower_done_01` setting                                                                                              |

- Gaps: G1 (enemies, towers, projectiles from prefabs), G8 (pause, fast forward), G15 (click a tower), G7 (ring icons).
- Scenarios: `tower_build` (gold spent, tower on a pad only), `tower_wave` (no tower: enemies reach the keep, lives down),
  `tower_call_early` (wave starts at once, bonus gold), `tower_defend` (towers kill wave 1, gold up),
  `tower_fast_forward` (time scale 2, pause stops enemies), `tower_win` (8 waves survived, `tower_done_01` set).
- Size: **S–M**, about 1 week: the iso town gives the terrain, roads and path graph; waves, towers and HUD are new.

### Mini Transport Tycoon

| Item     | Plan                                                                                                                                                                                            |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Play     | 64×64 map, two towns, four industries (farm, mine, factory, harbour); roads, stations, trucks with a route each                                                                                 |
| Win/lose | Company value at the target before year 5 / money below the loan limit for 3 months                                                                                                             |
| Controls | Mouse tools (left builds, right bulldozes), WASD or stick pans, keyboard cursor I J K L, pad RB switches tool                                                                                   |
| Scenes   | `title`, `play` (one generated map), `settings`, `victory`, `game_over`                                                                                                                         |
| Tilemap  | `Ground` (isometric, road overlays, `navigation` rules as in `iso_town.owl`); stations as overlay tiles                                                                                         |
| Entities | Towns and industries (`IsometricObject` slices), a pool of 24 hidden trucks (8 headings), trees, camera                                                                                         |
| Lua      | `build.lua` (roads, bulldozer, costs), `stations.lua`, `vehicles.lua` (`path.find`, `path.poll`, load, unload), `economy.lua` (production, pay by distance and days, running costs, loan, date) |
| HUD      | Toolbar, money and date, vehicle list (`UiList`), station and finance windows (`UiWindow`), tooltips                                                                                            |
| Art      | `tools/iso_atlas` (terrain, roads, buildings, vehicles); map from `city_map.py` with another seed and two towns                                                                                 |
| Saves    | Slots from the pause menu; the roads and stations laid in game are tilemap cells the save keeps (G3)                                                                                            |

- Reuse: `scripts/iso/iso_town.lua` (build tool, road shapes, funds), `scripts/iso/iso_town_traffic.lua` (route
  following, headings, waits), `scripts/iso/iso_camera.lua`, `scripts/iso/iso_town_life.lua`, `huds/iso_town.owlhud`,
  `tilesets/iso_ground.owltileset`, `textures/iso/`, `sounds/town/`, and the small map of the scenarios
  (`test/unit/test_helper/assets/scenarios/iso_town_small.owl`).
- Gaps, closed in v0.5.0: G1 (trucks spawned from a prefab), G3 (runtime roads kept by a save), G8 (pause, fast
  forward), G15 (click a vehicle, `scene.pick_entity`), G7.
- Scenarios: `tycoon_route` (the mouse lays a road from a farm to a factory, two stations, buys a truck;
  `tycoon_deliveries` above 0, money up), `tycoon_bankrupt` (no income, fast forward, game over),
  `tycoon_save` (save, bulldoze, load: the road is back).
- Size: **L**, about 3 weeks (the economy and its HUD are new, the rest comes from the demo).

### Wolfenstein mini

Shipped in `samples/wolfenstein/` (how to play: its `README.md`). An exploration game first: the guards, the weapons
and the combat come in a later release, once their art exists.

| Item     | As shipped                                                                                                                                                              |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Play     | 3 floors of 32×32 (cellars, prison, headquarters): treasure, food, gold and blue keys with their locked doors, 3 secret walls each, an exit elevator                    |
| Win      | The elevator of floor 3; after each floor a tally counted up line by line (skippable): items, secrets and map explored in %, 10 points per percent as a bonus           |
| Controls | `walk`, `turn`, `run`, `interact` (doors, secret walls, elevator), `map` (Tab, pad Y: large map); keyboard, mouse (right button turns, left opens) and gamepad          |
| Scenes   | `title`, `level_1` to `level_3` (`RendererRaycast` + a `Space: Screen` `Renderer2D` for the minimap), `tally`, `settings`, `victory`                                    |
| Entities | Player (`PhysicBody`, `Camera`), wall `Tilemap`, `RaycastDoor` (locked: empty `interactionAction`, `door.activate` with the key), `RaycastPushWall`, pick-ups, props    |
| Lua      | `player.lua`, `pickup.lua`, `locked_door.lua`, `secret.lua` (counts each wall once), `explore.lua` (fog, minimap, cells seen over floor cells), `level.lua`, `exit.lua` |
| HUD      | `huds/play.owlhud`: floor, score, keys, messages, a pause window with the controls (no ratio during play); the minimap is a tilemap of the overlay layer                |
| Menus    | Dark stone and steel (`generate_menus.py`): a raycast view turning behind the title, steel plate title and buttons, red fades                                           |
| Art      | The walls and props of `textures/wolf3d/`, reworked by the maintainer; the sounds of `sounds/raycast/` of the Engine Demo; guards and the weapon view later (G11)       |
| Saves    | Autosave in slot 1 at each floor start, F6 / F9 in slot 2, Continue loads the newer; best ratios per floor and best score in the settings                               |

- Floors carved by `tools/samples/wolfenstein/generate_levels.py`, which refuses a floor with a cell out of reach (so
  100 % is always possible; a slid secret wall counts as a wall where it stops, which cut off a room of floors 2 and
  3 until the first playthrough), then written as Owl Nest saves them: editable in Owl Nest, a pick-up counted by
  its tag.
- Map explored: the fog of `raycast_level.lua` on a 32×32 minimap, set up 8 rows per frame (a single `on_create` loop
  overruns the script time quota on emulated arm64), kept in the gamestate across saves. A slid secret wall shows,
  and stops the fog, in the cell where it stopped (it showed as floor there and the fog saw the room behind it).
- Reuse: `scripts/raycast/raycast_player.lua`, `scripts/raycast/raycast_level.lua`, `scripts/raycast/pickup.lua`,
  `scripts/raycast/raycast_exit.lua`, `tilesets/raycast_w3d_walls.owltileset`, `tilesets/minimap.owltileset`,
  `sounds/raycast/`; the menus, built from the Engine Demo ones, have a look of their own, and the ratios show
  on the tally, not on the play HUD.
- Gaps: G9, G17; G16 fixed (the `next_scene` property of `exit.lua` names the next floor). Later, with combat: G11
  (art), G10 (line of sight), G7 (weapon view frames), G1 (dropped ammo).
- Scenarios: `wolf_pickup` (walk onto a cross, `wolf_items` and `wolf_score` rise), `wolf_locked_door` (closed without
  the key, opens with it), `wolf_secret` (push a secret wall, `wolf_secrets` equals 1), `wolf_explore` (`wolf_seen`
  and `wolf_explored` rise while turning, Tab enlarges the map), `wolf_exit` (the tally scene opens, counts up,
  then floor 2), `wolf_tally` (accept during the count shows every line at once, the next accept leaves), `wolf_pause`
  (Escape opens the pause window, the player stands still, a door in reach stays closed, Resume closes it).

### Mini Minecraft

| Item     | Plan                                                                                                                                                              |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Play     | A finite island (about 96×96×48) authored in Owl Nest; break blocks, collect them, build from a 9-slot hotbar                                                     |
| Win/lose | Place 3 diamonds on the altar / health 0 (falls, lava); no mobs                                                                                                   |
| Controls | Built-in `VoxelPlayer` (WASD or stick, mouse or right stick look, `jump`, `sprint`); `primary` breaks, `secondary` places; 1 to 9, wheel or bumpers pick the slot |
| Scenes   | `title`, `island` (`RendererVoxel`, `VoxelWorld` not procedural), `settings`, `victory`, `game_over`                                                              |
| Entities | `VoxelWorld` (`voxel_blocks.owltileset`), `VoxelPlayer` with its camera, altar trigger, ambience `SoundSource`                                                    |
| Lua      | `hotbar.lua` (selected block), `inventory.lua` (counts from breaks and places), `health.lua` (fall damage from the height lost), `goal.lua`                       |
| HUD      | Crosshair (built in), hotbar (9 `UiImage` slots and counts), inventory window (`UiList`), health bar                                                              |
| Art      | `textures/atlases/voxel_blocks.png` (home-made by `tools/demo/generate_atlases.py`) plus coal and diamond blocks, icons                                           |
| Saves    | Slots: a non-procedural world writes its chunks into the save, so the edits come back                                                                             |

- Reuse: `scenes/voxel_terrain.owl`, `huds/voxel.owlhud`, `scripts/voxel/voxel_explorer.lua`,
  `tilesets/voxel_blocks.owltileset`, the Voxel Palette and structures of Owl Nest for the island,
  `scripts/raycast/inventory.lua` (window pattern).
- Gaps: G2 (Lua voxel bindings, blocking), G13 (edits need a captured cursor, blocking), G4 (procedural edits not
  saved, avoided by a finite world), G7 (hotbar icons), G1.
- Scenarios: `minecraft_break` (aim down, `primary`, `minecraft_inv_dirt` equals 1), `minecraft_place`
  (`secondary`, back to 0), `minecraft_hotbar` (bumpers change `minecraft_slot`), `minecraft_win`.
- Size: **M**, about 1.5 weeks, after G2 and G13 (about 4 days of engine work).

### Order {#sample-games-order}

| Step | Game                  | Release | Why                                                                                                |
|------|-----------------------|---------|----------------------------------------------------------------------------------------------------|
| 1    | Breakout              | v0.4.0  | Smallest, no blocking gap; sets up `samples/`, the scenarios and the export test for the others    |
| 2    | Wolfenstein mini      | v0.4.0  | Exploration without combat (score, items, secrets, map explored); combat once its art exists (G11) |
| 3    | Sokoban               | v0.4.0  | No blocking gap; proves level editing in Owl Nest, undo and saves                                  |
| 4    | Tower defence         | v0.5.0  | Waves spawned from prefabs (G1), pause (G8), the tile graph of the iso town; the Lua gaps closed  |
| 5    | Mini Minecraft        | v0.6.0  | After a small engine PR for G2 and G13                                                             |

## v0.7.0 games

Planned, not yet checked against the engine: their plans are refined when v0.7.0 starts.

### Lunar lander

| Item     | Plan                                                                                                       |
|----------|------------------------------------------------------------------------------------------------------------|
| Play     | Land a small craft on marked pads of a moon, from orbit to touchdown, fuel limited                         |
| Win/lose | Every pad reached / a hard landing breaks the legs or the craft                                            |
| Engine   | Planetary terrain layer, gravity field, trajectory prediction, breakable joints, HUD markers on the pads   |
| Size     | **M**, once the v0.7.0 physics and terrain are in                                                          |

### Asteroid field

| Item     | Plan                                                                                                       |
|----------|------------------------------------------------------------------------------------------------------------|
| Play     | Fly a fighter through an asteroid field, escort a convoy and fight waves of enemy fighters                 |
| Win/lose | The convoy reaches the gate / the convoy or the player is destroyed                                        |
| Engine   | 6DoF flight on Jolt, 3D steering and formations, particle trails, emissive lights, HUD markers, Doppler    |
| Size     | **M**, once the v0.7.0 AI and audio are in                                                                 |

## Layout

```
samples/
  breakout/  sokoban/  wolfenstein/       (tycoon/ and minecraft/ to come)
    owl_project.yml  game_settings.yml  input_actions.yml  input/gamecontrollerdb.txt
    scenes/  huds/  prefabs/  tilesets/  tilemaps/  scripts/  textures/  sounds/
tools/samples/<game>/                 art and sound generators (neither exported nor installed)
test/unit/scenarios/*.owltest         scenario tests
```

- One self-contained project per game beside `engine_demo/`: the export packs one project folder, so a sample copies
  what it reuses from the Engine Demo (the Kenney sheets weigh about 50 KB) rather than pointing at it. Each
  `owl_project.yml` has its own `name` (the save folder) and a title scene as `firstScene`.
- Scenarios stay in `test/unit/scenarios/`, already globbed by `test/unit/CMakeLists.txt` (one CTest each, label
  `scenario`), named `<game>_<behaviour>.owltest` (`breakout_serve`, `wolf_pickup`), with
  `scene: ../../../samples/<game>/scenes/<scene>.owl`; no test file is exported.
  `GameExportCoverage.EverySampleGameHasAScenario` fails for a sample whose scenes no scenario plays.
- Export: `GameExportEndToEnd.PlaysFromItsExportFolder` (one case per project) (`test/unit/export_tests/GameExport_test.cpp`)
  exports `engine_demo` and every `samples/*/owl_project.yml` (`OwlNest --export`, then
  `launch.sh --headless --smoke-test 60`): every scene and script is packed, and no warning is logged.
- Install: `source/owlnest/CMakeLists.txt` installs `engine_demo/` and `samples/` (component `Nest`) side by side.
- Owl Nest: the File screen lists the sample games under **Sample games** (`panel::findSampleGames`, the
  `samples/<game>/` folders beside the Engine Demo found by `findSampleProject`), one row each (G12).
- CI checks: `samples/` is in `_CODE_ROOTS` of `ci/utils/doc_identifiers.py`; `SampleHudLayout`
  (`DemoHudLayout_test.cpp`) applies the demo HUD rules to every `.owlhud` of `samples/` and to every sample scene
  holding a `Canvas` (its menus).

## Gaps {#sample-games-gaps}

| Id  | Gap                                                                                                                 | Size | Games                                    | Blocking?                                                   |
|-----|---------------------------------------------------------------------------------------------------------------------|------|------------------------------------------|-------------------------------------------------------------|
| G1  | No Lua call to spawn a prefab or add a component (fixed in v0.5.0: `scene.spawn_prefab`, `entity.add_component`)    | M    | Breakout, Tycoon, Wolfenstein, Minecraft | No: pools of authored entities hidden with `ui.set_visible` |
| G2  | No Lua `voxel` table: get and set a block, aimed block, `placeBlock`, break and place events                        | M    | Minecraft                                | **Yes**: no hotbar, no inventory                            |
| G3  | A save kept the `tilemapPath` of a tilemap, not its cells (fixed in v0.5.0: the changed cells are saved)            | M    | Tycoon                                   | No: replay the edits from a `gamestate` string              |
| G4  | A procedural `VoxelWorld` does not serialise its chunks: player edits are lost on save                              | M    | Minecraft                                | No: a finite, non-procedural world                          |
| G5  | No screen-to-world conversion nor viewport size in Lua for a 2D camera                                              | S    | Breakout                                 | No: the paddle follows the mouse delta                      |
| G6  | Tile and sprite-frame bindings live in `isometric` although they work on any tilemap                                | S    | Sokoban, Wolfenstein                     | No: naming only (a `tilemap` alias)                         |
| G7  | No `ui.set_image` in Lua (fixed in v0.5.0: `ui.set_image`, `ui.set_image_frame` on a sprite sheet grid)             | S    | Wolfenstein, Minecraft, Tycoon           | No: several `UiImage` toggled visible                       |
| G8  | No pause nor time scale in Lua (fixed in v0.5.0: `time.set_paused`, `time.set_scale`, `ui.is_world_blocked`)        | S    | Tycoon, all (pause)                      | No: Lua scales its own delta, bodies stopped by hand        |
| G9  | No music and effects mixing: `SoundSource` loops ignore `volume_music` and have no Lua volume                       | S    | All                                      | No: master volume only                                      |
| G10 | No line-of-sight or ray query in Lua                                                                                | S    | Wolfenstein                              | No: grid DDA in Lua, as in `raycast_level.lua`              |
| G11 | No guard nor weapon-view sprites                                                                                    | M    | Wolfenstein                              | No: exploration first, combat later                         |
| G12 | Owl Nest finds `engine_demo` only, and only `engine_demo/` is installed (fixed in v0.4.0: `samples/`, File screen)  | S    | All                                      | No: File > Open project                                     |
| G13 | `VoxelPlayer` edits fire only with a captured cursor: a gamepad player and headless scenarios cannot break or place | S    | Minecraft                                | **Yes**: gamepad rule and CI scenario                       |
| G14 | Lua cannot switch the tilemap asset of an entity: one scene per level                                               | S    | Sokoban, Breakout                        | No: duplicate the level scene                               |
| G15 | No Lua pick of the entity under the mouse (fixed in v0.5.0: `scene.pick_entity`, 2D, isometric, 3D)                 | S    | Tycoon                                   | No: compare `isometric.pick_cell` with the vehicle cells    |
| G16 | The export follows scene paths quoted in scripts, not those in script properties (fixed in v0.4.0: both followed)   | S    | Breakout, Wolfenstein                    | No: list the scenes in the script                           |
| G17 | A save keeps neither a slid `RaycastPushWall` nor an open door: after a load they are back in place                 | S    | Wolfenstein                              | No: pushed again, a secret counts once                      |
