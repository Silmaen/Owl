# Owl Feature Demo

Sample project exercising all Owl engine features. This project serves as a
living showcase — it must be updated whenever new features are added to the engine.

## How to Open

1. Launch **Owl Nest**
2. **File > Open Project** and select `sample_project/owl_project.yml`
3. The Main Menu scene loads automatically

## How to Play

- **Main Menu**: Play, Continue (loads last save), Settings, Delete Save, or Quit
- **Gameplay**: WASD to move, Space to jump, collect all 3 coins before health runs out
  - Coins heal you (+30% health each)
  - Health slowly decreases (hazard simulation)
  - Press **E** near the checkpoint circle to save (also stops hazard timer)
  - **M** to toggle music pause/resume
  - **F5** to quick-save anywhere
  - **Escape** to return to main menu
  - Level 1: reach the portal to advance to level 2
  - Level 2: collect all coins, then reach the victory zone
- **Victory**: victory screen with final score and fade transition
- **Game Over**: health reaches 0 → game over with retry/menu options
- **Settings**: adjust master volume and player speed, Reset Defaults button

## Scenes (6)

| Scene               | Purpose                                                             |
|---------------------|---------------------------------------------------------------------|
| `main_menu.owl`     | Logo, title, Play/Continue/Settings/Quit, save info, mouse coords   |
| `gameplay.owl`      | Level 1: player, coins, platforms, triggers, HUD, portal to level 2 |
| `level2.owl`        | Level 2: harder layout, victory zone, hazard timer                  |
| `settings_menu.owl` | Volume + speed sliders, reset defaults, back button                 |
| `victory.owl`       | Win screen with score, menu button                                  |
| `game_over.owl`     | Lose screen with score, retry + menu buttons                        |

## Gameplay Features

- **Victory condition**: collect all coins + reach victory zone (level 2) or auto-win (level 1 via portal)
- **Defeat condition**: health depletes to 0 → game over scene
- **Health system**: slowly decreases, healed by coin pickups
- **Checkpoint**: Interaction trigger (press E) saves and stops hazard timer
- **Hazard timer**: Timer trigger, stopped by checkpoint, restarts via marker entity
- **Death zone**: Death trigger below the ground
- **Music toggle**: M key pauses/resumes background music
- **Escape to menu**: Escape key returns to main menu
- **Quick-save**: F5 saves to slot 1
- **Continue**: main menu loads save slot 1
- **Delete save**: main menu can delete save data
- **Retry**: game over screen offers restart
- **Mouse tracking**: mouse coordinates displayed on main menu

## Prefabs

- `prefabs/coin.owlprefab` — Reusable coin template (CircleRenderer + Trigger + LuaScript)

## Animation Clips

- `animations/coin.owlanim` — Spritesheet animation clip (6×3 grid, 18 frames, Smooth
  speed curve). Open with the new Animation editor (double-click in the Content
  Browser) to scrub the timeline, tweak the speed curve, or change the frame range.

## Lua Scripts (11)

| Script                | Features Exercised                                                                                                                                                                                                                                                               |
|-----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main_menu.lua`       | `save.has_save/load_game/list_saves/delete_save`, `ui.set_button_enabled/set_visible/get_text`, `input.get_mouse_x/y`, `ui.transition_fade_in/out`, `scene.load_scene`, `gamestate`                                                                                              |
| `player.lua`          | `input.is_key_pressed`, `physics.impulse/get_velocity`, `entity.has_component/get_name`, `sound.play/stop/pause/resume/set_volume`, `ui.set_visible/set_text/set_progress`, `scene.find/destroy/create_entity`, `settings.get`, `gamestate`, `save`, `ui.transition_fade_in/out` |
| `moving_platform.lua` | `transform.get/set_position/rotation`, `math.sin`, properties                                                                                                                                                                                                                    |
| `hud.lua`             | HUD initialization                                                                                                                                                                                                                                                               |
| `settings_menu.lua`   | `settings.get/set/save/load/apply/reset_all`, `ui.set_text/set_slider_value/get_slider_value`, `ui.transition_fade_in/out`                                                                                                                                                       |
| `coin.lua`            | LuaCallback trigger, `gamestate`, `scene.destroy_entity`, `sound.play`, rotation animation                                                                                                                                                                                       |
| `hazard_timer.lua`    | Timer trigger (repeating), `trigger.start_timer`, `scene.find/destroy_entity`, `transform.set_scale`                                                                                                                                                                             |
| `checkpoint.lua`      | Interaction trigger, `save.save_game`, `trigger.stop_timer/reset_timer`, `scene.create_entity`, `entity.get_name`, `physics.set_transform`                                                                                                                                       |
| `portal.lua`          | Teleport trigger, `sound.play`, visual animation                                                                                                                                                                                                                                 |
| `victory.lua`         | `gamestate.get/remove`, `ui.set_text`, `ui.transition_fade_in/out`                                                                                                                                                                                                               |
| `game_over.lua`       | `gamestate.get/clear`, `ui.transition_fade_in/out`, retry/menu buttons                                                                                                                                                                                                           |
| `death_zone.lua`      | LuaCallback trigger, `sound.play`, `scene.load_scene`                                                                                                                                                                                                                            |

## Engine Features Covered

### Components Used
- [x] Transform, Camera, Canvas, SpriteRenderer, CircleRenderer, AnimatedSpriteRenderer, TextRenderer
- [x] PhysicBody (dynamic + static), Player
- [x] Trigger (Death, Victory, Teleport, Timer, Interaction, LuaCallback, Target)
- [x] Hierarchy (parent-child), Visibility, Tag, ID
- [x] LuaScript (with properties)
- [x] SoundListener, BackgroundTexture
- [x] UIRect, UIText, UIButton, UISlider, UIProgressBar, UIImage, UIPanel

### Lua API Coverage
- [x] `transform` (get/set position, rotation, scale)
- [x] `physics` (impulse, get_velocity, set_transform)
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
- [x] Scene transitions (6 scenes interconnected) with fade effects
- [x] Victory / defeat flow with Victory and Death trigger zones
- [x] Health system with HUD (progress bar + score text)
- [x] Save system (quick-save, checkpoint, auto-save, continue, delete save, list saves)
- [x] Settings persistence (game defaults + user overrides + reset to defaults)
- [x] Entity hierarchy (PlayerHat follows Player)
- [x] Trigger zones (all 7 types demonstrated)
- [x] Prefab template (coin.owlprefab)
- [x] AnimatedSpriteRenderer (coin rotation spritesheet, 18 frames; level 2 coins use a `speedCurve` to pulse between half- and 2.5x-speed)
- [x] Sound effects + music with pause/resume/volume control
- [x] UIImage (owl logo), UIPanel (button backdrop), all 8 widget types
- [x] BackgroundTexture (sky gradient)
- [x] Mouse input tracking
- [x] UTF-8 / Latin-1 text rendering — main menu subtitle "Démo des fonctionnalités v0.1.1 - caractères éàüÇ"
      exercises the renderer's UTF-8 decoding path and the MSDF atlas's full Latin-1 charset.
- [x] AnimatedSpriteRenderer speed curve — level 2 coins remap playback time via a Smooth
      `math::Curve` (slow at the loop boundary, fast in the middle).
- [x] Reusable `.owlanim` asset — `animations/coin.owlanim` mirrors the level 2 coin
      animation (texture, 6×3 grid, 18 frames at 0.08 s, loop, Smooth speed curve).
      Double-click it in the Content Browser to open it in the new Animation editor
      (live preview, properties panel, frame-range timeline backed by ImSequencer).
- [x] Runtime entity creation (session/checkpoint markers)
- [x] Dynamic entity inspection (has_component, get_name)
- [x] Timer control from scripts (start/stop/reset)

### Not Yet Demonstrated (engine-level, no Lua API)
- [ ] NativeScript (C++ scripting, only Lua demonstrated)
- [ ] EntityLink / PrefabLink components (editor-level features)
