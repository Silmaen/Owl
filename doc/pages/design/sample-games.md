# Sample games {#page-design-sample-games}

[TOC]

Design page of the sample games, summarised in the [Roadmap](../roadmap.md).

## Goal

Owl ships two kinds of example content:

- **Engine Demo** (today's `engine_demo/`): an all-in-one showcase of every engine feature, not a game in itself.
- **Sample games**: small but real games, each one built on only what it needs, most of them mini remakes of known
  games. They prove that Owl makes games, give a newcomer a project of a readable size to start from, and feed the
  two showcase games of v1.0.

## Rules

- One project per game, beside the Engine Demo, authored in Owl Nest; game rules in Lua, never in the engine.
- Small: one screen or a handful of levels, finished (title, play, win / lose, settings), playable with keyboard,
  mouse and gamepad.
- CC0 or home-made art only (the reworked `textures/wolf3d/` of the Engine Demo included); a scenario test per game runs in CI, and the game packages with the runner.
- A sample game uses only what it needs: it is not a feature checklist, the Engine Demo is.

## Short list

| Game                             | Inspired by           | Exercises                                                   | Possible from |
|----------------------------------|-----------------------|-------------------------------------------------------------|---------------|
| Breakout                         | Breakout / Arkanoid   | 2D, physics, HUD, sound, gamepad                            | v0.4.0        |
| Sokoban                          | Sokoban               | tilemap, undo, saves, level editing in Owl Nest             | v0.4.0        |
| Mini Transport Tycoon            | Mini Motorways / TTD  | isometric, roads, pathfinding, management HUD               | v0.4.0        |
| Wolfenstein mini                 | Wolfenstein 3D        | raycast, doors, pick-ups, HUD                               | v0.4.0        |
| Mini Minecraft                   | Minecraft             | voxel, build and break, inventory                           | v0.4.0        |
| One-room Zelda-like              | The Legend of Zelda   | top-down, dialogue, enemy AI, 2D lights                     | v0.7.0        |
| Marble or mini kart              | Marble Madness / Kart | 3D, Jolt physics, shadows                                   | v0.7.0        |
| Networked Bomberman              | Bomberman             | tilemap, deterministic simulation, multiplayer              | v0.8.0        |

Breakout or Sokoban comes first (simple, quick to finish), then Mini Transport Tycoon, which carries Owl's main
argument. The two v1.0 showcase games (one 2D, one 3D) are picked from this list.

## v0.4.0 games

Feasibility checked against `main` on 2026-10-18 (the bindings of `LuaBindings.cpp`, the components of
`source/owl/public/scene/component/`, the Engine Demo). All five can be made with the v0.4.0 engine: Breakout,
Sokoban and Mini Transport Tycoon need no engine change, Wolfenstein mini needs art, Mini Minecraft two small engine
fixes (see [Gaps](#sample-games-gaps)). The roadmap keeps the first three in v0.5.0 and the other two in v0.6.0;
pulling them forward is the maintainer's call.

Common to every game:

- Title, settings, victory and game over scenes copied from the Engine Demo (`scenes/main_menu.owl`,
  `scenes/settings_menu.owl`, `scenes/victory.owl`, `scenes/game_over.owl`, `scripts/ui/*.lua`): the menus already
  work with keyboard, mouse and gamepad (`UiFocus`, scenario `main_menu_focus.owltest`).
- `input_actions.yml` on the built-in actions (`move`, `look`, `jump`, `interact`, `primary`, `secondary`, `ui_*`)
  plus the game's own; `input/gamecontrollerdb.txt` copied.
- Sounds written by a generator in `tools/samples/<game>/`, like `tools/demo/generate_town_sounds.py` (the Kenney
  audio packs cannot be fetched from the build machine); `sounds/ui/start.wav` and `sounds/ui/victory.wav` copied.
- The game state mirrored into `gamestate` keys (`<game>_score`, ...) for the scenarios.

### Breakout

| Item     | Plan                                                                                                            |
|----------|-----------------------------------------------------------------------------------------------------------------|
| Play     | One screen, 5 levels, 3 lives; bricks of 1 to 3 hits; 2 power-ups (wide paddle, slow ball)                      |
| Win/lose | Every brick of the 5 levels / no life left; best score kept                                                     |
| Controls | `move` axis (keys, stick, D-pad) or the mouse; Space, pad A or left click serves                                |
| Scenes   | `title`, `level_1` to `level_5` (one scene each, `scene.transition_to`), `settings`, `victory`, `game_over`     |
| Entities | Paddle (`PhysicBody` Kinematic), ball (Dynamic, restitution 1, friction 0, gravity scale 0), walls (Static)     |
| Bricks   | Instances of a `brick.owlprefab` (Static body, `SpriteRenderer`), laid out in Owl Nest                          |
| Lua      | `paddle.lua`, `ball.lua` (`on_collision`, speed kept with `physics.set_velocity`), `brick.lua`, `game.lua`      |
| HUD      | `huds/play.owlhud`: score, lives, level, pause window                                                           |
| Art      | Home-made bricks, paddle and ball (`tools/samples/breakout/`); backdrop from `pixel_platformer_backgrounds.png` |
| Saves    | No slot; best score through `settings.set` and `settings.save`                                                  |

- Reuse: `scripts/platformer/flicker.lua` (hit flash), `scripts/platformer/coin.lua` (pop and fade before destroy),
  `huds/gameplay.owlhud` (corner layout of score and lives), `prefabs/coin_pair.owlprefab` (prefab pattern).
- Gaps: G5 (mouse to world), G1 (power-ups from a pool), G14 (one scene per level), G9 (music volume).
- Scenarios: `breakout_serve` (serve, 300 frames, `breakout_bricks` below its start value), `breakout_lose` (no
  input, `breakout_lives` drops by one), `breakout_gamepad` (the stick moves the paddle).
- Size: **S**, about 4 days.

### Sokoban

| Item     | Plan                                                                                                                   |
|----------|------------------------------------------------------------------------------------------------------------------------|
| Play     | 10 levels; push the boxes onto the goals; undo, restart, move counter                                                  |
| Win/lose | Every box on a goal; no lose, the best move count of each level is kept                                                |
| Controls | `move` (keys, D-pad, stick: one cell per press); Z, Backspace or pad B undo; R or pad Y restart                        |
| Scenes   | `title`, `level_select` (`UiList` with done marks), `level_01` to `level_10`, `settings`, `victory`                    |
| Tilemap  | One `.owltilemap` per level: layer 0 floor and walls, 1 goals, 2 boxes (moved with `isometric.set_cell`)               |
| Entities | Player (`AnimatedSpriteRenderer`, frames through `isometric.set_sprite_frame`), camera, music `SoundSource`            |
| Lua      | `sokoban.lua` (rules on `isometric.get_cell`, undo stack in a Lua table, win check), `level_select.lua`                |
| HUD      | `huds/level.owlhud`: level, moves, best, undo and restart hints                                                        |
| Art      | Tiny Dungeon (`tiny_dungeon.png`: walls, floor, hero), a Pixel Platformer crate; the Kenney Sokoban pack if downloaded |
| Saves    | Slot 1 written when a level is done (`sokoban_done_NN`, `sokoban_best_NN`); Continue on the title loads it             |

- Level editing in Owl Nest: paint the three layers in the tilemap editor, duplicate a level scene and point its
  `Tilemap` at the new file; `sokoban.lua` checks at start that boxes and goals match.
- Reuse: `scripts/hub/world_player.lua` (walk cycle, mirrored sprite), `scripts/hub/hub_water.lua` (`get_cell` /
  `set_cell` on a 2D tilemap), `scripts/raycast/inventory.lua` (`UiList` with gamepad focus).
- Gaps: G6 (tile bindings named `isometric`), G14 (one scene per level).
- Scenarios: `sokoban_push` (moves, `sokoban_on_goal` grows), `sokoban_undo` (undo puts back the box and the move
  count), `sokoban_wall` (no move into a wall), `sokoban_win` (level 1 solved, `sokoban_done_01` set).
- Size: **S**, about 5 days with the levels.

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
| Saves    | Slots from the pause menu; roads and stations replayed from a `gamestate` string on load (G3)                                                                                                   |

- Reuse: `scripts/iso/iso_town.lua` (build tool, road shapes, funds), `scripts/iso/iso_town_traffic.lua` (route
  following, headings, waits), `scripts/iso/iso_camera.lua`, `scripts/iso/iso_town_life.lua`, `huds/iso_town.owlhud`,
  `tilesets/iso_ground.owltileset`, `textures/iso/`, `sounds/town/`, and the small map of the scenarios
  (`test/unit/test_helper/assets/scenarios/iso_town_small.owl`).
- Gaps: G1 (trucks from a pool), G3 (runtime roads lost by a save), G8 (pause, fast forward), G15 (click a
  vehicle), G7.
- Scenarios: `tycoon_route` (the mouse lays a road from a farm to a factory, two stations, buys a truck;
  `tycoon_deliveries` above 0, money up), `tycoon_bankrupt` (no income, fast forward, game over),
  `tycoon_save` (save, bulldoze, load: the road is back).
- Size: **L**, about 3 weeks (the economy and its HUD are new, the rest comes from the demo).

### Wolfenstein mini

| Item     | Plan                                                                                                                                                                                                                                |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Play     | 3 levels of 32×32; guards that patrol, chase and shoot; pistol and machine gun; keys, treasure, food, ammo                                                                                                                          |
| Win/lose | The elevator of level 3 / health 0 with no life left; score, kills and secrets per level                                                                                                                                            |
| Controls | `move` (walk, turn), Ctrl, left click or pad RT fires, `interact` opens doors and pushwalls, 1, 2 or pad X weapon                                                                                                                   |
| Scenes   | `title`, `level_1` to `level_3` (`RendererRaycast`), `settings`, `victory`, `game_over`                                                                                                                                             |
| Entities | Player (`PhysicBody`), wall `Tilemap`, `RaycastDoor` (locked: empty `interactionAction`, `door.activate` with the key), `RaycastPushWall`, pick-ups, guards (`AnimatedSpriteRenderer` billboards, `PhysicBody`)                     |
| Lua      | `player.lua`, `weapon.lua` (hitscan: grid DDA to the first wall, nearest guard in a cone), `guard.lua` (states, 8 view frames through `isometric.set_sprite_frame`, sight by DDA, chase with `path.find`), `pickup.lua`, `exit.lua` |
| HUD      | `huds/play.owlhud`: health, ammo, lives, score, keys, weapon view, face                                                                                                                                                             |
| Art      | Guards and the weapon view to draw (G11); the walls and props of `textures/wolf3d/`, reworked by the maintainer, are kept                                                                                                           |
| Saves    | F6 / F9 slot as in the demo, autosave at each level start                                                                                                                                                                           |

- Reuse: `scripts/raycast/raycast_player.lua`, `scripts/raycast/raycast_level.lua` (DDA over the wall grid, fog
  minimap), `scripts/raycast/pickup.lua`, `scripts/raycast/inventory.lua`, `scripts/raycast/raycast_exit.lua`,
  `huds/raycast.owlhud`, `tilesets/raycast_w3d_walls.owltileset`, `sounds/raycast/`, scenario
  `raycast_level.owltest`.
- Gaps: G11 (art, blocking), G10 (line of sight), G7 (weapon view frames), G1 (dropped ammo from a pool), G9.
- Scenarios: `wolf_shoot` (face a guard, fire, `wolf_kills` equals 1), `wolf_locked_door` (closed without the key,
  opens with it), `wolf_hurt` (in a guard's sight, `wolf_health` drops), `wolf_exit`.
- Size: **M**, about 2 weeks plus the art.

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

### Order

| Step | Game                  | Why                                                                                             |
|------|-----------------------|-------------------------------------------------------------------------------------------------|
| 1    | Breakout              | Smallest, no blocking gap; sets up `samples/`, the scenarios and the export test for the others |
| 2    | Sokoban               | No blocking gap; proves level editing in Owl Nest, undo and saves                               |
| 3    | Mini Transport Tycoon | Owl's main argument; cleaner once G1, G3 and G8 are fixed                                       |
| 4    | Mini Minecraft        | After a small engine PR for G2 and G13                                                          |
| 5    | Wolfenstein mini      | Waits for its art (G11)                                                                         |

## Layout

```
samples/
  breakout/  sokoban/  tycoon/  wolfenstein/  minecraft/
    owl_project.yml  game_settings.yml  input_actions.yml  input/gamecontrollerdb.txt
    scenes/  huds/  prefabs/  tilesets/  tilemaps/  scripts/  textures/  sounds/
tools/samples/<game>/                 art and sound generators (neither exported nor installed)
test/unit/scenarios/<game>_*.owltest  scenario tests
```

- One self-contained project per game beside `engine_demo/`: the export packs one project folder, so a sample copies
  what it reuses from the Engine Demo (the Kenney sheets weigh about 50 KB) rather than pointing at it. Each
  `owl_project.yml` has its own `name` (the save folder) and a title scene as `firstScene`.
- Scenarios stay in `test/unit/scenarios/`, already globbed by `test/unit/CMakeLists.txt` (one CTest each, label
  `scenario`), with `scene: ../../../samples/<game>/scenes/<scene>.owl` and `project: ../../../samples/<game>`; no
  test file is exported.
- Export: `GameExportEndToEnd.SampleProjectPlaysFromItsExportFolder` (`test/unit/export_tests/GameExport_test.cpp`)
  exports `engine_demo` only; make it a parameterised test over `engine_demo` and every `samples/*/owl_project.yml`
  (`OwlNest --export`, then `launch.sh --headless --smoke-test 60`).
- Install: `source/owlnest/CMakeLists.txt` installs `engine_demo/` (component `Nest`); the same rule for `samples/`
  ships them in the packages.
- Owl Nest: `findSampleProject` (`Backstage.cpp`) looks for `engine_demo` only; the File screen lists the samples too
  (G12).
- CI checks: `samples/` added to `_CODE_ROOTS` of `ci/utils/doc_identifiers.py`; `DemoHudLayout_test.cpp` run over
  the HUDs of the samples too.

## Gaps {#sample-games-gaps}

| Id  | Gap                                                                                                                 | Size | Games                                    | Blocking?                                                        |
|-----|---------------------------------------------------------------------------------------------------------------------|------|------------------------------------------|------------------------------------------------------------------|
| G1  | No Lua call to spawn a prefab or add a component (`scene.create_entity` makes an empty entity)                      | M    | Breakout, Tycoon, Wolfenstein, Minecraft | No: pools of authored entities hidden with `ui.set_visible`      |
| G2  | No Lua `voxel` table: get and set a block, aimed block, `placeBlock`, break and place events                        | M    | Minecraft                                | **Yes**: no hotbar, no inventory                                 |
| G3  | A save keeps the `tilemapPath` of a tilemap, not its cells: runtime `set_cell` edits are lost on load               | M    | Tycoon                                   | No: replay the edits from a `gamestate` string                   |
| G4  | A procedural `VoxelWorld` does not serialise its chunks: player edits are lost on save                              | M    | Minecraft                                | No: a finite, non-procedural world                               |
| G5  | No screen-to-world conversion nor viewport size in Lua for a 2D camera                                              | S    | Breakout                                 | No: the paddle follows the mouse delta                           |
| G6  | Tile and sprite-frame bindings live in `isometric` although they work on any tilemap                                | S    | Sokoban, Wolfenstein                     | No: naming only (a `tilemap` alias)                              |
| G7  | No `ui.set_image` (texture or frame of a `UiImage`) in Lua                                                          | S    | Wolfenstein, Minecraft, Tycoon           | No: several `UiImage` toggled visible                            |
| G8  | No pause nor time scale in Lua (scene clock, physics)                                                               | S    | Tycoon, all (pause)                      | No: Lua scales its own delta, bodies stopped by hand             |
| G9  | No music and effects mixing: `SoundSource` loops ignore `volume_music` and have no Lua volume                       | S    | All                                      | No: master volume only                                           |
| G10 | No line-of-sight or ray query in Lua                                                                                | S    | Wolfenstein                              | No: grid DDA in Lua, as in `raycast_level.lua`                   |
| G11 | No guard nor weapon-view sprites                                                                                    | M    | Wolfenstein                              | **Yes** (content): draw them                                     |
| G12 | Owl Nest finds `engine_demo` only, and only `engine_demo/` is installed                                             | S    | All                                      | No: File > Open project                                          |
| G13 | `VoxelPlayer` edits fire only with a captured cursor: a gamepad player and headless scenarios cannot break or place | S    | Minecraft                                | **Yes**: gamepad rule and CI scenario                            |
| G14 | Lua cannot switch the tilemap asset of an entity: one scene per level                                               | S    | Sokoban, Breakout                        | No: duplicate the level scene                                    |
| G15 | No Lua pick of the entity under the mouse                                                                           | S    | Tycoon                                   | No: compare `isometric.pick_cell` with the vehicle cells         |
