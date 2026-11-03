# Sample Games {#page-samples}

[TOC]

**Level:** Beginner. **Read first:** [Getting Started](getting_started.md).

Six small but complete games made with Owl Engine, one project folder each under `samples/`, beside the Engine
Demo. Open one from the **Sample games** list of the Owl Nest File screen, or with **Open project**, and play its
`scenes/title.owl`; every one can be packaged with **Package the game**.

Each project is self-contained (its own `owl_project.yml` and a title scene as `firstScene`): what a sample reuses
from the Engine Demo is copied, never pointed at. The rules are Lua scripts, the art and the sounds are CC0 or
home-made, and each game has at least one scenario test.

| Game             | After                  | Main style                       | Page                                          |
|------------------|------------------------|----------------------------------|-----------------------------------------------|
| Breakout         | Breakout, Arkanoid     | 2D, neon lights                  | [Breakout](sample_breakout.md)                |
| Sokoban          | Sokoban                | 2D tilemap                       | [Sokoban](sample_sokoban.md)                  |
| Wolfenstein mini | Wolfenstein 3D         | Raycast                          | [Wolfenstein mini](sample_wolfenstein.md)     |
| Tower defence    | Warcraft 1 and 2       | 3D with 2D sprites and particles | [Tower defence](sample_tower_defence.md)      |
| Ring Runner      | Star Fox               | 3D on a rail, planet             | [Ring Runner](sample_rail_shooter.md)         |
| The Moon Lantern | The Legend of Kyrandia | 2D backdrops, 3D actors          | [The Moon Lantern](sample_point_and_click.md) |

| ![Breakout](../images/showcase/breakout.png)             | ![Sokoban](../images/showcase/sokoban.png)          | ![Wolfenstein mini](../images/showcase/wolfenstein.png)     |
|----------------------------------------------------------|-----------------------------------------------------|-------------------------------------------------------------|
| ![Tower defence](../images/showcase/tower_sunscorch.png) | ![Ring Runner](../images/showcase/rail_shooter.png) | ![The Moon Lantern](../images/showcase/point_and_click.png) |

## What each game shows

| Feature                                  | Breakout | Sokoban | Wolfenstein | Tower defence | Ring Runner | Moon Lantern |
|------------------------------------------|----------|---------|-------------|---------------|-------------|--------------|
| 2D sprites and 2D lights                 | yes      |         |             | yes           |             |              |
| Tilemaps                                 |          | yes     | yes         |               |             |              |
| Raycast renderer                         |          |         | yes         |               |             |              |
| 3D meshes                                |          |         |             | yes           | yes         | yes          |
| Physics bodies                           | yes      |         | yes         |               |             |              |
| Particles                                | yes      | yes     | yes         | yes           | yes         |              |
| Splines                                  |          |         |             | yes           | yes         |              |
| Data tables                              |          | yes     |             | yes           | yes         |              |
| Dialogues                                |          |         |             | yes           |             | yes          |
| Sequences                                |          |         | yes         |               | yes         | yes          |
| Localisation                             |          |         |             |               |             | yes          |
| Menus, settings, volumes, saved progress | yes      | yes     | yes         | yes           | yes         | yes          |

No sample uses a visual script or the pathfinding tile graph: [Visual Scripts](visual-scripts.md) and
[Pathfinding](pathfinding.md) are shown by the Engine Demo.

## Common ground

- **Menus and HUD:** every game has a title, a settings screen and a HUD (`huds/play.owlhud` or a canvas) with a
  pause window listing the controls; the controls are never shown during play. See [Scene and Components](scene.md).
- **Sound:** the settings drive the master, music and effects volumes of the mixer buses (`volume_*`), see
  [Sound](sound.md).
- **Input:** an input actions file maps keyboard, mouse and gamepad together, see [Events and Input](event_input.md).
- **Progress:** the best scores, the levels done and the settings are kept between sessions.
- **Generators:** the first art, sounds, layouts and menus come from `tools/samples/<game>/`; running a generator again
  overwrites the edits made in Owl Nest.

## Where next

[Getting Started](getting_started.md) opens the Engine Demo and the samples, [Scripting Games in Lua](scripting.md)
explains the scripts they use, and [Making Games with Owl](guide_game_maker.md) lists the other pages.
