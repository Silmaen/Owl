# Owl Feature Demo

Engine Demo exercising all Owl engine features. This project serves as a
living showcase — it must be updated whenever new features are added to the engine.

## How to Open

1. Launch **Owl Nest**
2. **Open project** in the File screen and select `engine_demo/owl_project.yml`
3. The Main Menu scene loads automatically

## How to Play

The hub is a top-down town with four houses, one per level. Each level has a **victory objective** shown in its HUD;
completing it marks the level done (`level_<name>_done` in gamestate), returns to the hub and bumps the hub counter
"Levels completed: n / 4". When all four are done the **victory portal** (a purple teleporter) appears and answers.

1. **Main Menu** → press **Démarrer** to begin a new run, or **Continuer** if a pause snapshot exists. **O** (left
   bumper) opens the isometric town directly.
2. **Hub** (`world_map.owl`) — WASD to move (the hero has a walk cycle and mirrors toward the direction of motion),
   **E** at a door to enter a level. Hazards: lava patches and the water moat south of the first house cause
   **instant death** (the moat: damage over time). The houses from west to east: platformer, raycast, isometric town,
   voxel terrain. **ESC** pauses and returns to the Main Menu.
3. **Platformer** (`platformer_house.owl`) — A/D to run, W / Space / Up to jump; gamepad: left stick / D-pad, A, Start.
   A 176×32 level in three zones (meadow, farm, factory): spike traps and hits cost a third of the health bar (then a
   short invulnerability and a kick back), a fall into a water pit costs the same and respawns at the last checkpoint
   (flags), hearts heal, a secret cave hides behind the farm cliff. **Objective: reach the door** (gold checkered
   victory zone) at the top of the factory tower.
4. **Raycast level** (`raycast_demo.owl`) — Wolfenstein 3D E1M1 in first person. W/S walk, A/D turn, **E** opens
   doors and pushes the five secret walls, **Tab** (gamepad Y) opens the inventory window (arrows / D-pad between
   slots, Enter / A inspects, Escape / B closes, a journal list scrolls). Treasure (cross, chalice, chest, crown),
   food, first-aid kits, ammo clips and a machine gun are picked up on contact; secrets hide eleven treasures and
   more. **Objective: find an elevator** (the main one at the end of the west corridor, or the secret one behind
   the pushwalls) and press **E**. F6 / F9 quick-save and load slot 2 (the inventory comes back with the save).
5. **Isometric town** (`iso_town.owl`) — build roads with the mouse (left click, right click bulldozes, Q switches
   tool, WASD pans; keyboard cursor I J K L; pad: sticks, RB), watch the traffic. **Objective: the vehicles make 20
   trips**; keep the road graph connected.
6. **Voxel terrain** (`voxel_terrain.owl`) — WASD walk, mouse look, Space jump, Shift run (double-tap Space flies).
   **Objective: walk 100 blocks away from the landing point** (the HUD shows the distance). Escape returns to the
   hub without completing.
7. **ESC** in the hub or the platformer pauses the run and returns to the Main Menu, where **Démarrer** becomes
   **Continuer** and resumes exactly where you left off.

## Scenes

| Scene                  | Purpose                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main_menu.owl`        | Logo, dynamic Démarrer/Continuer button, Settings, Delete Save, Quit                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `world_map.owl`        | Hub: top-down 48×24 town drawn with Tiny Town (Tiny Dungeon hero with a walk cycle, camera at a pixel-exact 3×): four houses (platformer, raycast, isometric town, voxel), stone roads, a moat with a bridge (water = damage), a lava meadow (death), the HUD counter of levels completed and the victory portal that wakes when all four are done Also an information board (E: a window with a scroll view of help text), a lever that opens a gate through an `EntityLink`, a villager walking over a grid navigation layer, two more villagers on Lua loops, drifting cloud shadows and chimney smoke. |
| `platformer_house.owl` | Level 1: side-scroller 176×32 in Pixel Platformer art with its farm and industrial expansions (pixel-exact 3×): meadow (x 0-55), farm (56-111, a climb to a plateau, a secret cave, a chasm) and factory (112-175, acid pits, a lift, a spiked catwalk, a tower), 92 coins (two `coin_pair` prefab instances, 11 in the secret cave), 4 hearts, 18 spikes, 6 water pits and a chasm, 5 kinematic moving platforms (4 h, 1 v), 5 checkpoint flags, animated water, 6 torches and 4 beacons, a 3-layer parallax backdrop, a camera that leads and shakes, a hit flash with invulnerability, coin pops and fades, a door at the top of the tower as the objective, an astronaut with idle / walk / jump frames |
| `raycast_demo.owl`     | Level 2: Wolfenstein 3D E1M1 (64×64, `RendererRaycast`): doors, five secret pushwalls, decorations, 20 pick-ups (11 treasures, food, first aid, ammo, a machine gun), two elevators (`ExitElevator`, the secret `SecretElevator`) and the inventory window of `huds/raycast.owlhud` A minimap (a 2D `ui` layer over the raycast one) shows the walls and the player. |
| `iso_town.owl`         | Level 3: isometric town (also **O** from the main menu): a 24×24 sloped map with a hill, an embankment, a ring road and two streets (road tiles from the neighbour mask), 23 buildings from `IsometricObject` slices, trees, three vehicles driving to their stops over the road graph and re-routing when a road is bulldozed (`navigation` rules of the Ground tilemap, `iso_town_traffic.lua`), a mouse / keyboard / gamepad build-road and bulldoze tool (`iso_town.lua`, `huds/iso_town.owlhud`), a 2D sky with drifting clouds, and the trips objective A one-way north street and a slow east side of the ring are navigation exceptions of the Ground tilemap. |
| `voxel_terrain.owl`    | Level 4: procedural voxel terrain (`RendererVoxel`, `VoxelPlayer`) with a HUD showing the distance objective (`huds/voxel.owlhud`, `voxel_explorer.lua`)                                                                                                                                                                                                                                                                                                                                                                                            |
| `settings_menu.owl`    | Volume + speed sliders, reset defaults, back button                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| `victory.owl`          | Win screen with score, menu button                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| `game_over.owl`        | Lose screen with score, retry (→ world map) + menu buttons                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

## Art

The sample uses the Kenney CC0 packs in `textures/kenney/` (see their `License.txt`); `textures/generate_kenney_atlases.py`
builds the two atlases from them.

| Where                                  | Pack                                                                                                                      |
|----------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| `world_map.owl` ground, houses, trees  | Tiny Town (`town_atlas.png`; Tiny Town has no liquid, so water, lava and bridge tiles are drawn in its palette)           |
| `world_map.owl` player                 | Tiny Dungeon (the hero of the atlas; the pack has no walk frames: four are drawn from the idle sprite by `generate_kenney_atlases.py`, tiles 168-171, and the sprite is mirrored toward the direction of motion) |
| `platformer_house.owl` tiles, props    | Pixel Platformer tiles (`platformer_atlas.png`) for the meadow; the farm and industrial expansions, the animated water, the checkpoint flag and the cave tiles (`platformer_ext_atlas.png`) |
| `platformer_house.owl` player, door    | Pixel Platformer characters (idle, two walk frames, jump) and door tiles, picked from `platformer_player.lua`             |
| `platformer_house.owl` backdrop        | Pixel Platformer backgrounds (`pixel_backdrop.owltileset` on `platformer_backdrop.png`: the sheet plus a keyed copy of its horizon row), three parallax layers |

The tilesets use the `Nearest` filter and the cameras show a whole number of screen pixels per art pixel
(`orthographicSize` 15 for the 16 px hub, 13.33 for the 18 px platformer at 720p: 3×), so the pixels stay sharp. Sprites
that share an atlas texture with a tileset share its filter.

## Tilesets

| Tileset                     | Atlas size      | Tiles            | Notable collidables / hazards                                                                                                                                                                                                                                                              |
|-----------------------------|-----------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `town_atlas.owltileset`     | 12×14 / 16px    | 168              | Tiny Town sheet (grass, dirt autotile, stepping stones, trees, houses, fences, sign; trees, houses and fences collidable) plus water, lava and bridge tiles drawn in its palette and the Tiny Dungeon characters; `Nearest` filter |
| `platformer_atlas.owltileset` | 20×16 / 18px | 320              | Pixel Platformer packed sheet (grass, dirt, orange and snow tops, bricks, crates collidable; water, spikes, ladder, door, coin, props) with its 24 px characters at the bottom (read as a 15×12 grid by the sprites); `Nearest` filter |
| `platformer_ext.owltileset` | 16×16 / 18px   | 256              | Farm and industrial expansions (orange and metal terrain, hay, girders, conveyors, hazard strips collidable), water surface phases, flag, cave and secret-room tiles; `Nearest` filter |
| `pixel_backdrop.owltileset` | 8×5 / 24px     | 32               | Pixel Platformer backgrounds (sky, clouds, hills, forest) and their keyed horizon bands; the platformer backdrop with three parallax layers (sky locked to the camera, far bands at 0.12, hills at 0.3); `Nearest` filter |
| `raycast_walls.owltileset`  | 4×4 / 64px      | 8                | gray brick, blue stone, wood, red banded wall variants — all collidable, sampled by `RendererRaycast` for wall stripes                                                                                                                                                                     |
| `iso_ground.owltileset`     | 16×31 / 64×48px | 26 flat + slopes | `kind: Isometric` diamond terrain (grass, sand, water, road, rock, snow, rail, …) with its slope table (18 sloped variants of 24 terrains, the 16 road shapes included) and the grass and road foundations, drawn by the isometric layer; generated by `tools/iso_atlas` (also `iso_ground_2x` and `iso_ground_half`) |

## Gameplay Features

- **Win condition**: complete the objective of the four levels (platformer door, E1M1 elevator, 20 town trips,
  100 voxel blocks), return to the hub each time, step on the victory portal that appears once all four are done → Victory.
- **Defeat condition**: HP bar depletes to 0 → Game Over.
- **World hazards**: water = damage-over-time (-0.6 HP/s), lava = instant death.
- **Platformer hazards**: spike traps = -0.34 HP per hit (1.2 s of invulnerability, knock-back, camera shake, red blink), water pits and the chasm = -0.34 HP and a respawn at the last checkpoint behind a fade (`platformer_checkpoint_x` / `_y` in the gamestate); hearts give 0.34 HP back.
- **Coins**: 92 collectibles along the level (11 of them in the secret room), each pops (scale up, fade) when picked; the score persists across the run via gamestate.
- **ESC pause + Continue**: ESC at any time saves a pause snapshot (`continue_*` keys
  in gamestate) and returns to the main menu; the primary button toggles between
  Démarrer (fresh run) and Continuer (resume exact scene + position + health).
- **Save system**: quick-save with F5 (legacy), slot 1 + slot 2 by `SaveManager`,
  Continue from menu re-loads slot 1.
- **Settings persistence**: master volume + player speed editable in-game, persisted
  across launches via `SettingsManager`.

## Animation Clips

The platformer's collectible coins are driven by an inline `AnimatedSpriteRenderer`
component (texture `animated_coin.png`, 6×3 grid, 18 frames). A reusable `.owlanim`
asset is bundled under `engine_assets/animations/coin.owlanim` for the Animation
editor and as a regression-test fixture, but the sample itself does not reference
it directly.

## Lua Scripts

| Script                   | Features exercised                                                                                                                                                                                        |
|--------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main_menu.lua`          | Dynamic Démarrer/Continuer button, save slot info, settings dispatch, fade transitions, `gamestate` continue snapshot, `save.has_save/list/delete`, `scene.load_scene`                                    |
| `world_player.lua`       | Top-down 4-direction movement (gravity scale = 0 + `set_velocity`), walk cycle through `isometric.set_sprite_frame`, camera-follow, ESC pause, levels counter (`level_<name>_done` to `levels_done`, HUD text) and victory portal gate, `physics.set_transform` position restore Still while the board is open. |
| `platformer_player.lua`  | Tight side-scroller controls (state machine grounded / rising / falling, coyote time, jump buffer), no air control, look-ahead camera with smoothing, pixel snapping and shake, hit flash and invulnerability (`entity.set_tint`), respawn behind `ui.transition_fade_out/in`, banners on the objective text, HUD update, ESC pause, `on_collision` trace |
| `house_door.lua`         | `Trigger.Type=Interaction`, gates entry on `house_<n>_done` flag, saves world-return position, loads platformer scene                                                                                     |
| `level_complete.lua`     | Victory zone of the platformer: sets `level_platformer_done`, returns to the hub                                                                                                                    |
| `teleporter.lua`         | `Trigger.Type=Interaction`, loads the victory scene once `levels_done` is 4 (visibility set by `world_player.lua`)                                                                                               |
| `lava_damage.lua`        | `Trigger.Type=LuaCallback`, continuous health drain via `time.delta()` (used by the world-map water trigger boxes)                                                                                        |
| `hazard.lua`             | `Trigger.Type=Target` + `on_trigger_enter`, spikes and pits leave a request in the gamestate that the player script applies                                                                                                                       |
| `coin.lua`               | `Trigger.Type=LuaCallback`, score increment, sprite spin, pick-up pop (`transform.set_scale`, `entity.set_tint`), sound + deferred entity destroy                                                                                                                          |
| `moving_platform_h.lua`  | Kinematic body driven by `physics.set_velocity` along X from `on_tick` (fixed simulation tick) — pushes the player                                                                                        |
| `moving_platform_v.lua`  | Same idea on Y — pushes the player vertically                                                                                                                                                             |
| `checkpoint.lua`         | `Trigger.Type=Target`, stores the respawn point in the gamestate, raises and waves the flag with `isometric.set_sprite_frame` |
| `heart.lua`              | `Trigger.Type=LuaCallback`, heals through the gamestate when the bar is not full, bob and pop                  |
| `flicker.lua`            | Torch flicker with `entity.set_tint` from `on_update`                                                           |
| `secret_room.lua`        | `Trigger.Type=Target`, opens a plug of decor tiles with `isometric.set_cell` (any tilemap), reveals hidden entities with `ui.set_visible` |
| `settings_menu.lua`      | `settings.get/set/save/load/apply/reset_all`, slider handling, fade transitions                                                                                                                           |
| `victory.lua`            | `gamestate.get`, fade transitions                                                                                                                                                                         |
| `game_over.lua`          | Retry → world map, Menu → main menu, gamestate clear                                                                                                                                                      |
| `raycast_house_door.lua` | `Trigger.Type=Interaction`, saves world-return position, loads `raycast_demo.owl`                                                                                                                         |
| `raycast_player.lua`     | First-person controls (W/S walk, A/D turn through the `walk` / `turn` actions), gravity scale 0, still while the inventory window is open, ESC returns to the hub with the pose kept in `raycast_return_*` gamestate keys |
| `pickup.lua`             | `Trigger.Type=LuaCallback`, string properties, gamestate as the inventory (`inv_<item>`), entity destruction, sound                                                                                       |
| `inventory.lua`          | An `.owlhud` of its own driven from Lua: slots (icon, count, tooltip), window opened by the `inventory` action, `ui.set_focus`, `onFocusCallback`, `onCloseCallback`, quick save / load (F6 / F9)         |
| `raycast_exit.lua`       | `Trigger.Type=Interaction` on an elevator set in a wall (`kind` property: main or secret), sets `level_raycast_done`, returns to the hub                                                                                    |
| `raycast_level.lua`      | `pushwall.get_state` polled on the five secret walls, treasure and secret counters of the HUD from the `inv_<item>` keys The minimap dot follows the player. |
| `voxel_explorer.lua`     | Script on a `VoxelPlayer` entity: distance from the landing point as objective, `level_voxel_done`, Escape back to the hub                                                                                                 |
| `iso_town.lua`           | Trips objective (`town_trips` to `level_iso_done`) and build-road and bulldoze tool: `isometric.pick_cell` / `get_cell` / `set_cell` / `get_slope` / `get_height`, actions `build`, `bulldoze`, `town_tool`, `cursor`, `ui.is_mouse_over_ui`, funds in gamestate |
| `iso_town_traffic.lua`   | Vehicles driving from stop to stop over the road graph (`path.find`, `poll`, `get`, `get_edge_cost`, `get_version`), re-routing when a road is bulldozed, on the terrain, one sprite per vehicle with eight headings, right-hand lanes and quarter-circle turns Drives slower on costly cells (`path.get_edge_cost`), never west along the one-way street. |
| `iso_cloud.lua`          | Parallax cloud of the 2D layer behind the isometric one                                                                                                                                                   |
| `info_board.lua`         | Interaction trigger opening a window of the hub HUD: `ui.set_visible`, `ui.raise_window`, `onCloseCallback`, `UiScrollView` scrolled by `ui.scroll_get` / `ui.scroll_set` |
| `hub_lever.lua`          | `entity.get_link` on a hidden `EntityLink` entity, `scene.destroy_entity`, `transform.set_rotation`, `isometric.set_sprite_frame`, toast |
| `hub_villager.lua`       | `path.find` / `poll` / `get` in `cells` mode over the hidden `walkable` layer of the hub tilemap (the hub's terrain-grid pathfinding). The F3 overlay draws over the isometric view only, so this route is not shown |
| `hub_wanderer.lua`       | Two-point pacing loop with script properties set per entity in the scene |
| `hub_ambient.lua`        | Chimney smoke and cloud shadows moved by one script (`CircleRenderer`, `transform.set_*`) |

## Engine Features Covered

### Components Used
- [x] Transform, Camera, Canvas, SpriteRenderer, CircleRenderer, AnimatedSpriteRenderer, TextRenderer
- [x] PhysicBody (dynamic + static), Player
- [x] Trigger (Death, Victory, Teleport, Timer, Interaction, LuaCallback, Target)
- [x] Hierarchy (parent-child), Visibility, Tag, ID
- [x] LuaScript (with properties)
- [x] SoundListener, BackgroundTexture
- [x] UiRect, UiText, UiButton, UiSlider, UiProgressBar, UiImage, UiPanel
- [x] UiWindow, UiLayout, UiTooltip, UiToolCursor (the "Controls" window of `huds/gameplay.owlhud`)
- [x] UiFocus (slots of the inventory window, `onFocusCallback`) and a window with a grid `UiLayout` (`huds/raycast.owlhud`)
- [x] UiScrollView (the information board of the hub: a window of help text, wheel / bar / `move` keys) and relative `UiRect` units (`sizeUnit` / `offsetUnit`: the levels counter and toast of `huds/world_map.owlhud` use ScreenWidth, ScreenHeight, ParentWidth and ParentHeight ratios)
- [x] EntityLink (the hub lever reads the link of its hidden `LeverLink` entity to find the gate it opens; editable in the inspector)
- [x] Tilemap navigation: `connections` mode with per-cell exceptions (a one-way street and a slow road in `iso_town.owl`) and `cells` mode on a hidden `walkable` layer of the hub tilemap (the villager)
- [x] Renderer stack mixing: the raycast level composes its `raycast_world` layer with a `Space: Screen` `ui` Renderer2D layer drawing a minimap (a Tilemap and a dot)
- [x] UiList (the journal of the inventory window: a model grown by the script, only the rows in view bound with `onBindCallback` and `ui.list_set_row`, `onSelectCallback`, wheel and bar)

### Lua API Coverage
- [x] `transform` (get/set position, rotation, scale)
- [x] `physics` (impulse, get_velocity, set_velocity, set_transform, set_gravity_scale)
- [x] `input` (action map: is_action_pressed, is_action_just_pressed, get_action_value, get_action_vector; get_mouse_x, get_mouse_y)
- [x] `scene` (find_entity, destroy_entity, create_entity, load_scene)
- [x] `entity` (has_component, get_name, get_link)
- [x] `ui` (scroll_get, scroll_set, raise_window, set_text, get_text, set_visible, set_progress, set_slider_value, get_slider_value, set_button_enabled, transition_fade_in/out, is_transition_active)
- [x] `gamestate` (set, get, remove, clear)
- [x] `save` (save_game, load_game, has_save, list_saves, delete_save)
- [x] `settings` (get, set, save, load, apply, reset, reset_all)
- [x] `sound` (play, stop, pause, resume, set_volume)
- [x] `path` (find, poll, get, release, is_walkable, get_edge_cost: vehicles of the iso town, the villager of the hub)
- [x] `log` (trace, info, warn, error)
- [x] `trigger` (start_timer, stop_timer, reset_timer)
- [x] Trigger callbacks (on_trigger_enter/exit, on_timer, on_interact, custom)
- [x] Collision callback (`on_collision(other_id)` from Box2D contacts)
- [x] Gamepad (`input.get_gamepad_stick`, `is_gamepad_button_just_pressed`) in the platformer, project mapping file
- [x] Action map (`input_actions.yml`: axis, button and vector actions on keys, mouse and gamepad, overriding the built-in ones)

### Gameplay Systems
- [x] Scene transitions (menu / world map / platformer / victory / game-over / settings)
      with fade effects
- [x] Victory / defeat flow with Victory and Death trigger zones (lava death, water
      damage tick, spike damage tick)
- [x] Health system with HUD (UiProgressBar + ScoreText)
- [x] Save system (quick-save F5, slot 1, list / delete saves from menu)
- [x] Continue from menu — restores `continue_scene` + position + health from gamestate
      after an ESC pause, so the user resumes exactly where they left off
- [x] Settings persistence (game defaults + user overrides + reset to defaults)
- [x] Trigger zones (Death, LuaCallback, Interaction, Teleport)
- [x] Tilemap system — world map (32×24, top-down), platformer (176×32, side-view, two tilemaps)
      and raycast demo (16×16, first-person via `RendererRaycast`) all use the
      same `Tilemap` component; per-renderer dispatch comes from the active
      `RenderLayer` type. Per-tile collidable flags from the matching
      `.owltileset` drive both the visual rendering and the static colliders.
- [x] Renderer stack — three project layers `[Renderer2D(world),
      RendererRaycast(raycast_world), Renderer2D(ui)]`. Most scenes use the
      2D world; the raycast demo flips `world` off and `raycast_world` on
      via `EnabledRenderers`. Every HUD canvas is drawn by the HUD pass, in
      pixels on top of the layers, at 1280×720 reference pixels scaled with
      the window height; the gameplay scenes name their HUD with `Hud:`
      (`huds/gameplay.owlhud` for the platformer, `world_map`, `raycast`,
      `voxel` and `iso_town` have their own), the menus keep their canvas.
- [x] Static + Kinematic platforms — `PhysicBody Static` for jumpable ledges,
      `PhysicBody Kinematic` driven by `physics.set_velocity` for moving platforms
      that push the player
- [x] AnimatedSpriteRenderer — 8 collectible coins on the platformer use the
      `animated_coin.png` 6×3 spritesheet (18 frames at 0.08 s, loop)
- [x] Sound effects (jump, coin pickup, victory, menu start)
- [x] UiImage (owl logo), UiPanel, UiButton, UiSlider, UiProgressBar, UiText —
      all 8 widget types in use across menu / settings / HUD
- [x] HUD windows — the platformer HUD holds a draggable, closable "Controls" window (column `UiLayout`, content
      clipped to the window, tooltip on hover); T (or gamepad X) toggles a `UiToolCursor` sprite following the mouse
- [x] Inventory — pick-ups of the raycast level, a slotted inventory window driven by the keyboard or the
      gamepad through the HUD focus, state in gamestate (kept in saves); scenario `test/scenarios/raycast_level.owltest`
- [x] Level progression — victory objectives, `level_<name>_done` flags, hub counter and gated victory portal;
      scenario `test/scenarios/hub_progress.owltest`
- [x] BackgroundTexture (sky gradient on world map, dark gradient inside the house)
- [x] Mouse input tracking — mouse coordinates displayed on the main menu

### Not Yet Demonstrated
- [ ] NativeScript (C++ scripting, only Lua exercised)
- [ ] EntityLink component
- [ ] Timer triggers (the original `hazard_timer.lua` was retired with the legacy
      gameplay scenes)
