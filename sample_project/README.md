# Owl Feature Demo

Sample project exercising the main Owl engine features.

## How to Open

1. Launch **Owl Nest**
2. **File > Open Project** and select `sample_project/owl_project.yml`
3. The Main Menu scene loads automatically

## Scenes

### `scenes/main_menu.owl` -- Main Menu
- **Canvas UI**: title text (UIText), play button (UIButton), version label
- **Lua script** (`scripts/main_menu.lua`): button click callback, screen transition fade-out
- **Features tested**: Canvas, UIRect anchoring (TopCenter, Center, BottomRight), UIButton states, UIText alignment, ScreenTransition

### `scenes/gameplay.owl` -- Gameplay
- **Player** (entity 201): SpriteRenderer + PhysicBody (dynamic) + Player component + LuaScript
  - WASD movement via `physics.impulse()`
  - Coin collection via `scene.find_entity()` + distance check + `scene.destroy_entity()`
  - **Script properties**: `speed` (float), `score` (int)
- **PlayerHat** (entity 202): child entity of Player -- tests **hierarchy** (follows parent)
- **Ground + Walls**: static PhysicBody entities
- **Coins** (entities 206-208): CircleRenderer -- destroyed on collection
- **Moving Platform** (entity 209): LuaScript with `transform.get/set_position`, oscillates via `math.sin`
- **Instruction Text** (entity 210): world-space Text component
- **HUD Canvas** (entity 211): screen-space overlay
  - **ScoreText** (UIText, TopLeft anchor): updated via `ui.set_text()` from Lua
  - **HealthBar** (UIProgressBar, TopRight anchor): updated via `ui.set_progress()` from Lua

## Lua Scripts

| Script | Features Exercised |
|--------|-------------------|
| `scripts/main_menu.lua` | UI callbacks, `ui.transition_fade_out`, `log.info` |
| `scripts/player.lua` | `input.is_key_pressed`, `physics.impulse`, `transform.get_position`, `scene.find_entity`, `scene.destroy_entity`, properties table |
| `scripts/moving_platform.lua` | `transform.get/set_position`, `transform.set_rotation`, `math.sin`, `time.delta`, properties |
| `scripts/hud.lua` | `ui.set_text`, `ui.set_progress`, `scene.find_entity`, `entity.get_name`, `time.delta` |

## Engine Features Covered

- [x] Lua scripting (lifecycle callbacks, properties, all API tables)
- [x] Canvas UI (screen-space overlay rendering)
- [x] UIRect (anchoring: TopLeft, TopCenter, TopRight, Center, BottomRight)
- [x] UIText (alignment, fontSize, color)
- [x] UIButton (states, click callback)
- [x] UIProgressBar (animated value)
- [x] ScreenTransition (fade-out)
- [x] Physics (dynamic + static bodies, impulse)
- [x] Entity hierarchy (parent-child transform inheritance)
- [x] SpriteRenderer + CircleRenderer
- [x] World-space Text
- [x] Scene entity management (find, create, destroy)
- [x] Camera (orthographic)
- [x] SoundListener
- [x] Input system (keyboard)
