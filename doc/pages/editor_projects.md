# Projects and Export {#page-editor-projects}

[TOC]

A project is a folder holding an `owl_project.yml` and its assets: scenes, scripts, textures, sounds, HUDs. Owl Nest
opens one project at a time and exports it as a game folder players can run without the editor.

## Opening and closing

- **New project** (File screen) asks for a [template](#project-templates), a name and a location in the
  [New Project dialog](#new-project-dialog), creates `<location>/<name>` and opens its first scene.
- **Open project** takes the folder or its `owl_project.yml`; the recent projects of the File screen reopen with a
  click. `OwlNest --project <folder or owl_project.yml>` opens one from the command line.
- Opening a project closes the current one first: Play stops, the session is recorded, unsaved changes go to the
  [recovery](#autosave-and-crash-recovery) and every document closes. Two projects using the same asset names never
  see each other's files.
- **Session restore**: reopening a project restores its open tabs, the active tab and the selected entity instead of
  its first scene (a missing file is skipped with a warning). The dock layout is shared by every project.

## Project templates

| Template      | Tags             | Renderer stack                            | Content                                                                              |
|---------------|------------------|-------------------------------------------|--------------------------------------------------------------------------------------|
| Empty 2D      | 2D               | `world` (2D), `ui`                        | Camera, ground, player sprite moved by Lua (WASD), HUD title                         |
| 2D platformer | 2D               | `world` (2D), `ui`                        | Box2D ground and platforms, Lua player (A/D, space), camera on it                    |
| 3D            | 3D               | `mesh_world`, `ui`                        | Ground and crate meshes, sun, procedural sky, `FirstPersonController`                |
| Isometric     | Iso, 2D          | `world`, `iso_world`, `ui`                | Tilemap island, tree and lamp sprites, Lua flag (WASD), camera on it                 |
| Raycast       | Raycast          | `raycast_world`, `ui`                     | Tilemap room, Lua first-person player (W/S, A/D), HUD hint                           |
| Voxel         | Voxel, 3D        | `voxel_world`, `ui`                       | Procedural streamed terrain, sky, sun, voxel player (WASD, space)                    |
| Mixed styles  | Mixed, 3D, 2D... | all of the above, `billboards`, `minimap` | 3D yard, 2D billboards, 2D minimap in one frame; keys 1-3: `room.owl`, `terrain.owl` |

Every template opens on a playable scene moved with the built-in `move` action, ships an `input_actions.yml` with the
built-in [input actions](editor_documents.md#editor-action-map) and has a scenario (`test/unit/scenarios/template_*`).
The templates are the folders of `engine_assets/project_templates/`: a folder there with a `template.yml` and an
`owl_project.yml` is a template too. `template.yml` holds the `name`, the one-line `description`, the `order` in the
dialog, the style `tags` and the `contents` lines shown in the details; `preview.png` beside it (480 x 270, captured
with `OwlRunner --frame-bench <scene> --project <template> --capture preview.png --size 480x270`) is its image. Neither
file is copied into the new project. `tools/demo/generate_templates.py` writes the meshes and the isometric atlas,
tileset and map of the templates.

### New Project dialog

**New project** (File screen) opens a dialog in three parts:

- **Templates**: a card per template with its preview, name, description and tags; the chips above filter the cards by
  tag (**All**, 2D, 3D, Iso, Raycast, Voxel, Mixed). Click a card to select it, double-click to create; the arrow keys
  walk the cards.
- **Details**: the larger preview, the tags, the description and the **Contents** of the selected template.
- **Project**: the **Name** (also the folder name) and the **Location** (typed, or picked with the folder button). The
  dialog checks them as you type: a name holding `/ \ : * ? " < > |` or a leading or trailing space or dot, a location
  that is not a folder and a project folder that already holds files are refused with the reason, and **Create** stays
  disabled; otherwise it shows the folder it creates. A free name (`MyGame`, `MyGame2`...) is proposed.

**Create** (or Enter) copies the template into `<location>/<name>`, renames the project and opens its first scene;
**Cancel** or Escape closes the dialog. Without any template installed, the project starts empty.

## Project Settings

**Home > Project > Settings** edits the project in five sections; **OK** writes `owl_project.yml`, **Cancel** drops
the changes (they are not undo steps).

| Section        | Fields                                                                                             |
|----------------|----------------------------------------------------------------------------------------------------|
| General        | Name, first scene, default HUD (for the scenes that name none), version, author, description, icon |
| Window         | Width (320 to 7680), height (240 to 4320), fullscreen, resizable, ImGui overlay                    |
| Input          | Gamepad mappings file (SDL_GameControllerDB format), **Edit Input Actions**                        |
| Simulation     | Tick rate (1 to 1000 per second, 60 by default), max ticks per frame (1 to 64, 8 by default)       |
| Renderer Stack | The layers that draw every scene, in order, with their configuration                               |

An empty version follows the engine version and an empty icon gives the Owl logo. **ImGui overlay** (off by default)
makes the exported game create an ImGui layer and pack its fonts, for a game whose C++ code draws with ImGui; a game
drawing its interface with HUDs leaves it off. The tick rate applies from the next Play
([fixed simulation tick](scene.md#fixed-tick)); the window and simulation settings are what the exported game uses.
`game_settings.yml`, beside `owl_project.yml`, holds the default game settings the player can change through the Lua
`settings` API.

## Autosave and crash recovery

Every modified document of the project (scene, script, tilemap, tileset, animation, node graph) is copied aside every
60 seconds (**Settings > General > Autosave Every**, 0 turns it off) and when the editor quits or the project
closes. The copies sit in `OwlNest_recovery/` beside the editor settings and never replace your files.

When a project opens with copies newer than their files (after a crash, or a quit without saving), a prompt offers
them: **Recover** reopens each document with its changes, marked modified; **Discard** deletes the copies. Saving a
document drops its copy.

## Exporting a game

**Home > Project > Pack Game** (or **Package the game** in the File screen) first lists the missing references
(textures, sounds, scripts, scenes): cancel to fix them, or pack anyway. It then writes `<output>/<GameName>/` behind
a progress dialog (spaces and reserved characters of the name become `_`):

| File                       | Content                                                                                                                                                  |
|----------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| `<GameName>.owlpack`       | The assets the scenes reference, the shaders of the renderers the game uses (project shaders compiled), the fonts and their atlases, `game_settings.yml` |
| `<GameName>` / `.exe`      | The game executable (a copy of `OwlRunner`)                                                                                                              |
| `*.so` / `*.dll`           | The shared libraries of the engine                                                                                                                       |
| `runner.yml`               | First scene, pack, window, renderer stack, renderer families, ImGui, icon                                                                                |
| `game_info.yml`            | Name, version, author, description, engine version, date, platform                                                                                       |
| `launch.sh` (Linux)        | Starts the game with its libraries                                                                                                                       |
| `<GameName>.zip` (Windows) | The game folder as an archive, next to it                                                                                                                |
| icon                       | The project icon, or the Owl logo                                                                                                                        |

The game initialises the renderer families of its renderer stack only (`Renderers` in `runner.yml`): raycast,
isometric, and `Renderer3D` for a mesh or voxel layer or a scene with an `Environment`. Owl Nest itself keeps every
renderer. The end of the export lists those families, the number of engine shader files left out and the number of
project shaders compiled. The game reads everything from its pack in memory and writes nothing next to itself.

**Scene > Scene > Pack Scene** packs the active scene alone into an `.owlpack`.

The same export runs without a window, as the CI does; `--headless` plays on the Null backends and
`--smoke-test [frames]` plays every scene of the pack, exiting with 1 on any logged error:

```bash
OwlNest --export <project dir or owl_project.yml> <output dir>
<output dir>/<GameName>/launch.sh --headless --smoke-test 60
```

Cooked assets are cached in `<project>/.owl/cache/cooked/`; `--cook-cache <dir>` puts the cache elsewhere (a CI
workspace shared between builds). `OwlNest --upgrade-assets <project dir>` creates the missing `.owlmeta` sidecars and
rewrites the asset references of the project documents by GUID, as the `asset.upgrade_references` command does.

```bash
OwlNest --export <project> <output dir> --cook-cache <cache dir>
OwlNest --upgrade-assets <project dir>
```

On Windows the game executable is not signed: SmartScreen may warn the players at the first start (**More info**,
then **Run anyway**).
