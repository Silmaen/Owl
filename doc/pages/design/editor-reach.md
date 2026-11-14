# Editor reach {#page-design-editor-reach}

[TOC]

Everything a game of the Engine Demo or of the [sample games](sample-games.md) uses can be found, read and tuned in
Owl Nest, without opening a Lua or YAML file to discover it. This extends the *Editor coverage* rule of
[Ongoing quality](ongoing-quality.md) from the authored objects to the scripts and the settings that drive them.

## Goal

- A designer opening a scene sees every script it runs, every Lua function the engine calls, and every value a
  script reads from the scene, each one clickable to its source
- Generic behaviours (a flickering light, a moving platform, a patrol) are engine components edited in the
  inspector, not code copied from one script to the next
- A value a designer tunes (a jump height, a spawn count, a colour) is a field of the inspector, undoable, saved in
  the scene, never a `local X = 7.5` at the top of a script
- A gap is caught by the CI, not by the maintainer playing a demo

## Rules

| Rule               | Choice                                                                                                                                                                     |
|--------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Scripts            | Every script a scene runs is listed: the scene script in Scene Settings, entity scripts on their entity, with an **Open** button                                           |
| Lua functions      | Every function the engine may call is shown where it is bound: lifecycle hooks on the script, HUD handlers on their widget, trigger and timer callbacks on their component |
| Hook state         | Each known hook of a script shows *defined* or *not defined*; a defined hook opens the code editor at the line of its `function`                                           |
| Scene script       | A scene owns one optional script of its own (`SceneScript`), with the hooks of an entity script but no entity; no "global" script on the camera                            |
| Tuned values       | A value a designer changes is a script property (`properties = { ... }`) or a component field, drawn by the inspector, never a top-level local                             |
| Generic behaviours | A behaviour found in two scripts, or that any game would want, becomes an engine component with a system `owl.*`, its script removed                                       |
| Layer settings     | Every key a render-stack layer reads in `applyConfig` has a field in the layer overrides of Scene Settings                                                                 |
| Files              | Every file type the engine reads opens in Owl Nest: a document of its own, the code editor for text, the system viewer otherwise                                           |
| Check              | `EditorCoverage` (gui_tests) fails when a component field has no inspector widget or a layer key has no override field                                                     |

## Scripts in the editor

```mermaid
flowchart LR
    S["Scene"] --> SS["SceneScript<br/>(Scene Settings)"]
    S --> E["Entity"]
    E --> LS["LuaScript<br/>(inspector)"]
    E --> H["HUD widget / trigger / timer<br/>(inspector)"]
    SS --> HK["Hooks: on_create, on_update, on_tick..."]
    LS --> HK
    H --> FN["Named handler function"]
    HK --> CE["Code editor at the line"]
    FN --> CE
    LS --> P["properties (inspector fields)"]
    SS --> P
```

- **Hook list.** The inspector of a `LuaScript` (and the scene script section) lists the hooks the engine calls:
  `on_create`, `on_update`, `on_tick`, `on_destroy`, `on_collision`, `on_trigger_enter`, `on_trigger_exit`,
  `on_triggered`, `on_interact`, `on_timer`. A defined hook shows its line and an **Open** button; an undefined one is
  dimmed. The lines come from the loaded chunk (`debug.getinfo(fn, "S").linedefined`), refreshed with the file.
- **Bound handlers.** A component that names a Lua function (a HUD button, a trigger, a timer) shows the script that
  defines it, a warning when no script of the scene defines it, and the same **Open** button.
- **Scene script.** `SceneScript` is saved in the scene file and edited in Scene Settings; it runs before the entity
  scripts in the `Scripts` phase. Lua: `scene.*` from it as from an entity script, `self` absent.
- **Properties.** The `properties` table of a script takes numbers, booleans, strings, `color` (sRGB, a colour picker),
  `vec2` and `vec3`, optional `min`, `max` and `step`, an entity reference (UUID picker) and a list of numbers or
  strings; the inspector shows the default from the script and a reset button per field.

## Behaviour components

| Component      | Fields                                                                                                                                                                             |
|----------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `LightFlicker` | Style (`Fire`, `Candle`, `Pulse`), amplitude, speed, seed, colour shift, position jitter, dips (rate, depth); on a sprite or mesh, a followed light whose flicker drives its scale |
| `Oscillator`   | Axis or offset, amplitude, period, phase, easing; kinematic bodies moved through physics                                                                                           |
| `Patrol`       | Waypoints (edited in the viewport), wait time, speed, loop or ping-pong, random wander radius                                                                                      |
| `Drift`        | Velocity, range, wrap, follow-camera factor                                                                                                                                        |

Each has an inspector, viewport gizmos where it has a shape (patrol waypoints, oscillator range), undo through the
command registry, Lua accessors and YAML, as every authored object.
