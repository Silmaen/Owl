# Owl Feature Demo

Sample project exercising all Owl engine features. This project serves as a
living showcase — it must be updated whenever new features are added to the engine.

## How to Open

1. Launch **Owl Nest**
2. **File > Open Project** and select `sample_project/owl_project.yml`
3. The Main Menu scene loads automatically

## Scenes

### `scenes/main_menu.owl` -- Main Menu
- **Canvas UI**: title text (UIText), play button (UIButton), version label
- **Lua script** (`scripts/main_menu.lua`): button click callback, scene loading, save detection
- **Features tested**: Canvas, UIRect anchoring (TopCenter, Center, BottomRight), UIButton states,
  UIText, scene.load_scene, gamestate, save.has_save

### `scenes/gameplay.owl` -- Gameplay
- **Player**: SpriteRenderer + PhysicBody (dynamic) + Player component + LuaScript
  - WASD movement via `physics.impulse()`
  - Coin collection via `scene.find_entity()` + distance check + `scene.destroy_entity()`
  - Score stored in `gamestate` (persists across saves)
  - Auto-save to slot 1 when all coins collected
  - Quick-save with F5
- **PlayerHat**: child entity of Player — tests **hierarchy** (follows parent)
- **Ground + Walls**: static PhysicBody entities
- **Coins**: CircleRenderer — destroyed on collection
- **Moving Platform**: LuaScript with `transform.get/set_position`, oscillates via `math.sin`
- **HUD Canvas**: screen-space overlay
  - **ScoreText** (UIText, TopLeft): updated via `ui.set_text()` from player script
  - **HealthBar** (UIProgressBar, TopRight): animated via `ui.set_progress()` from HUD script
  - **Instructions** (UIText, TopCenter): static help text

## Lua Scripts

| Script | Features Exercised |
|--------|-------------------|
| `scripts/main_menu.lua` | UI callbacks, `scene.load_scene`, `gamestate.get/set`, `save.has_save`, `log.info` |
| `scripts/player.lua` | `input.is_key_pressed`, `physics.impulse`, `transform.get_position`, `scene.find_entity`, `scene.destroy_entity`, `gamestate.set`, `save.save_game`, properties |
| `scripts/moving_platform.lua` | `transform.get/set_position`, `transform.set_rotation`, `math.sin`, properties |
| `scripts/hud.lua` | `ui.set_progress`, `scene.find_entity` |

## Engine Features Covered

- [x] Lua scripting (lifecycle callbacks, properties, all API tables)
- [x] Canvas UI (screen-space overlay rendering)
- [x] UIRect (anchoring: TopLeft, TopCenter, TopRight, Center, BottomRight)
- [x] UIText (fontSize, color, aspect-ratio-preserved rendering)
- [x] UIButton (states, click callback via Lua)
- [x] UIProgressBar (animated value via Lua)
- [x] Scene transitions (`scene.load_scene`)
- [x] Physics (dynamic + static bodies, impulse, velocity)
- [x] Entity hierarchy (parent-child transform inheritance)
- [x] SpriteRenderer + CircleRenderer
- [x] Scene entity management (find, create, destroy)
- [x] Camera (orthographic)
- [x] SoundListener
- [x] Input system (keyboard: WASD + F5)
- [x] GameState (key-value store, cross-scene persistence)
- [x] Save/Load system (auto-save on completion, quick-save F5, save detection on menu)
