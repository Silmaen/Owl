# Owl Feature Demo

Sample project exercising all Owl engine features. This project serves as a
living showcase — it must be updated whenever new features are added to the engine.

## How to Open

1. Launch **Owl Nest**
2. **File > Open Project** and select `sample_project/owl_project.yml`
3. The Main Menu scene loads automatically

## How to Play (v0.2.0 flow)

The sample game has been rebuilt around the new tilemap system:

1. **Main Menu** → press **Démarrer** to begin a new run, or **Continuer** if a
   pause snapshot exists.
2. **World Map** (`world_map.owl`) — top-down tilemap view. WASD to move
   around a grass plain bordered by mountains. Hazards: lava patches and
   the water moat south of the house cause **instant death**.
   Cross the bridge to reach the central house and press **E** in front of
   its door to enter the platformer. **A second house** sits to the east
   (a wider 7×4 building, reached by following the stone path east from the
   first house) — press **E** in front of its door to enter the first-person
   Wolfenstein-style demo level.
3. **Inside the House** (`platformer_house.owl`) — side-scroller platformer.
   A/D to run, W / Space / Up to jump. Hazards: lava pits at floor level
   (instant death), spike traps (lose ~1/3 of your health bar each contact).
   Reach the gold checkered **victory zone** at the top-right to clear the
   level.
4. **Raycast Demo** (`raycast_demo.owl`) — first-person view of a 16×16
   Wolfenstein-inspired tilemap. **W/A/S/D** strafe, **Q/E** turn,
   **ESC** returns to the world map. The same `Tilemap` component drives
   both the 2D world view and this raycast view — only the active
   `RenderLayer` changes.
4. Clearing the level closes the house door (the door entity reports as
   already explored on subsequent visits) and bumps `houses_visited` in
   gamestate. When `houses_visited == houses_total`, a **purple teleporter**
   appears in the upper-left of the world map — walk over it to teleport
   to the **Victory** screen.
5. **ESC** at any point in gameplay pauses the run and returns to the Main
   Menu, where the **Démarrer** button label switches to **Continuer** and
   resumes you exactly where you left off (same scene, same position,
   same health).

## Scenes

| Scene                     | Purpose                                                                |
|---------------------------|------------------------------------------------------------------------|
| `main_menu.owl`           | Logo, dynamic Démarrer/Continuer button, Settings, Delete Save, Quit   |
| `world_map.owl`           | Top-down 32×24 tilemap world: plain + mountains + central house + hazards (water = damage, lava = death) + hidden teleporter |
| `platformer_house.owl`    | Side-scroller 60×16 inside the house: static platforms, kinematic moving platforms (h + v), lava pits (death), spike traps (damage), 8 collectible coins, victory portal |
| `settings_menu.owl`       | Volume + speed sliders, reset defaults, back button                    |
| `victory.owl`             | Win screen with score, menu button                                     |
| `game_over.owl`           | Lose screen with score, retry (→ world map) + menu buttons             |

## Tilesets

| Tileset                   | Atlas size | Tiles | Notable collidables / hazards |
|---------------------------|-----------|-------|-------------------------------|
| `world_topdown.owltileset` | 4×4 / 64px | 16    | mountain, tree, fence, sign, house wall/roof (collidable); water + lava (visual; hazard via separate trigger entities) |
| `world_platform.owltileset`| 4×2 / 64px | 8     | floor, platform, brick (collidable); lava + spikes (visual; hazard triggers overlay) |
| `raycast_walls.owltileset` | 4×4 / 64px | 8     | gray brick, blue stone, wood, red banded wall variants — all collidable, sampled by `RendererRaycast` for wall stripes |

## Gameplay Features

- **Win condition**: clear the platformer level inside the house, return to the world
  map (door reports as explored), step on the teleporter that appears once every house
  is done → Victory.
- **Defeat condition**: HP bar depletes to 0 → Game Over.
- **World hazards**: water = damage-over-time (-0.6 HP/s), lava = instant death.
- **Platformer hazards**: lava pits = instant death, spike traps = -0.34 HP per touch.
- **Coins**: 8 collectibles inside the house, score persists across the run via gamestate.
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

| Script                       | Features exercised                                                                                                                                                                |
|------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main_menu.lua`              | Dynamic Démarrer/Continuer button, save slot info, settings dispatch, fade transitions, `gamestate` continue snapshot, `save.has_save/list/delete`, `scene.load_scene`            |
| `world_player.lua`           | Top-down 4-direction movement (gravity scale = 0 + `set_velocity`), camera-follow, ESC pause, teleporter visibility gate, world-return position restore                          |
| `platformer_player.lua`      | Tight side-scroller controls (state machine grounded / rising / falling, coyote time, jump buffer), no air control, camera-follow, HUD update, ESC pause                          |
| `house_door.lua`             | `Trigger.Type=Interaction`, gates entry on `house_<n>_done` flag, saves world-return position, loads platformer scene                                                              |
| `level_complete.lua`         | Marks the visited house as done, increments `houses_visited`, returns to the world map                                                                                            |
| `teleporter.lua`             | `Trigger.Type=Interaction`, loads the victory scene (visibility currently controlled by `world_player.lua`)                                                                       |
| `lava_damage.lua`            | `Trigger.Type=LuaCallback`, continuous health drain via `time.delta()` (used by the world-map water trigger boxes)                                                                |
| `spike_damage.lua`           | `Trigger.Type=LuaCallback`, instantaneous HP step on each entry (platformer spikes)                                                                                                |
| `coin.lua`                   | `Trigger.Type=LuaCallback`, score increment, sprite spin, sound + entity destroy                                                                                                  |
| `moving_platform_h.lua`      | Kinematic body driven by `physics.set_velocity` along X — pushes the player horizontally                                                                                          |
| `moving_platform_v.lua`      | Same idea on Y — pushes the player vertically                                                                                                                                      |
| `hud.lua`                    | Passive HUD root (HealthBar + ScoreText drawn under it, kept in sync by player scripts)                                                                                            |
| `settings_menu.lua`          | `settings.get/set/save/load/apply/reset_all`, slider handling, fade transitions                                                                                                   |
| `victory.lua`                | `gamestate.get`, fade transitions                                                                                                                                                  |
| `game_over.lua`              | Retry → world map, Menu → main menu, gamestate clear                                                                                                                              |
| `raycast_house_door.lua`     | `Trigger.Type=Interaction`, saves world-return position, loads `raycast_demo.owl`                                                                                                  |
| `raycast_player.lua`         | First-person controls (Q/E rotate yaw, WASD strafe relative to facing), gravity scale 0, ESC returns to world with pose persisted via `raycast_return_*` gamestate keys           |

## Engine Features Covered

### Components Used
- [x] Transform, Camera, Canvas, SpriteRenderer, CircleRenderer, AnimatedSpriteRenderer, TextRenderer
- [x] PhysicBody (dynamic + static), Player
- [x] Trigger (Death, Victory, Teleport, Timer, Interaction, LuaCallback, Target)
- [x] Hierarchy (parent-child), Visibility, Tag, ID
- [x] LuaScript (with properties)
- [x] SoundListener, BackgroundTexture
- [x] UiRect, UiText, UiButton, UiSlider, UiProgressBar, UiImage, UiPanel

### Lua API Coverage
- [x] `transform` (get/set position, rotation, scale)
- [x] `physics` (impulse, get_velocity, set_velocity, set_transform, set_gravity_scale)
- [x] `input` (is_key_pressed, is_mouse_button_pressed, get_mouse_x, get_mouse_y)
- [x] `scene` (find_entity, destroy_entity, create_entity, load_scene)
- [x] `entity` (has_component, get_name)
- [x] `ui` (set_text, get_text, set_visible, set_progress, set_slider_value, get_slider_value, set_button_enabled, transition_fade_in/out, is_transition_active)
- [x] `gamestate` (set, get, remove, clear)
- [x] `save` (save_game, load_game, has_save, list_saves, delete_save)
- [x] `settings` (get, set, save, load, apply, reset, reset_all)
- [x] `sound` (play, stop, pause, resume, set_volume)
- [x] `log` (trace, info, warn, error)
- [x] `trigger` (start_timer, stop_timer, reset_timer)
- [x] Trigger callbacks (on_trigger_enter/exit, on_timer, on_interact, custom)

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
- [x] Tilemap system — world map (32×24, top-down), platformer (60×16, side-view)
      and raycast demo (16×16, first-person via `RendererRaycast`) all use the
      same `Tilemap` component; per-renderer dispatch comes from the active
      `RenderLayer` type. Per-tile collidable flags from the matching
      `.owltileset` drive both the visual rendering and the static colliders.
- [x] Renderer stack — three project layers `[Renderer2D(world),
      RendererRaycast(raycast_world), Renderer2D(ui)]`. Most scenes use the
      2D world; the raycast demo flips `world` off and `raycast_world` on
      via `EnabledRenderers`. The HUD canvas in every scene carries
      `RendererTag: { Name: ui }` so it lands on the dedicated 2D pass.
- [x] Static + Kinematic platforms — `PhysicBody Static` for jumpable ledges,
      `PhysicBody Kinematic` driven by `physics.set_velocity` for moving platforms
      that push the player
- [x] AnimatedSpriteRenderer — 8 collectible coins on the platformer use the
      `animated_coin.png` 6×3 spritesheet (18 frames at 0.08 s, loop)
- [x] Sound effects (jump, coin pickup, victory, menu start)
- [x] UiImage (owl logo), UiPanel, UiButton, UiSlider, UiProgressBar, UiText —
      all 8 widget types in use across menu / settings / HUD
- [x] BackgroundTexture (sky gradient on world map, dark gradient inside the house)
- [x] Mouse input tracking — mouse coordinates displayed on the main menu

### Not Yet Demonstrated
- [ ] NativeScript (C++ scripting, only Lua exercised)
- [ ] EntityLink / PrefabLink components (no prefab in the current sample)
- [ ] Timer triggers (the original `hazard_timer.lua` was retired with the legacy
      gameplay scenes)
