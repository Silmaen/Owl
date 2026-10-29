# Console, Log and Commands {#page-editor-console}

[TOC]

Every change Owl Nest makes to a scene is a named **command**: a gizmo drag, an inspector edit, a hierarchy drop as
well as a line typed in the Console, a macro or a request of an [MCP](mcp.md) client. They all go to the same undo
history and to the session journal, which the Console shows.

## Log

Engine and game messages, newest at the bottom. Chips filter by level (Trace, Debug, Info, Warning, Error, Critical,
with their counts) and by logger (Engine, Game); the search field filters by text; **Clear** empties it. The tab
title counts the warnings and errors; the status bar counts the new ones and opens the Log on a click.

A failed scene, script or pack names the file, the entity (name, id, line) and ends with a `Fix:` hint:

```
SceneSerializer: Cannot load scene 'scenes/level.owl': entity #1 (id 42) 'Broken' at line 5 is malformed
(YAML: Bad conversion at line 7.). Fix: fix or remove the reported entity in the file, then
reload the scene.
LuaEngine: Error in on_update of script 'scripts/hero.lua' on entity 'Hero' (77): [string "scripts/hero.lua"]:2:
boom. Fix: fix the script at the first line of the traceback; the callback runs again on its next call.
```

## Console {#editor-console}

The **Console** (Ctrl+&#96;) is a command line over the editor commands and the journal of the session.

![The Console: commands sent over MCP in the journal, then `help entity.rename` typed in the command line](../images/nest/console.png)

- **Command line**: one [command](#command-line) per input, run on the active scene with `Enter`. The result shows
  as `$n  description  (data)`, an error with its column. `help` lists the commands, `help <command>` its arguments;
  `Tab` completes the command, then the argument names, then their choices; `↑` / `↓` browse the last 100 lines.
- **Journal**: one line per command run, whatever its source (gesture, console, palette, MCP, macro, scenario), with
  its number `$n`, its source and its result. The source chips filter it; **Clear** empties the view.

### Lua mode {#editor-lua}

The code button of the Console switches the command line to **Lua**.

- The globals are kept from one line to the next; an expression prints its value.
- A line that leaves a block open continues on the next line; **Esc** cancels the input.
- `help(editor)` lists the functions of the module, `help(editor.entity)` those of one group.
- A line in command syntax still runs as a command.
- The module `editor` has one function per command of the registry: `editor.entity.create{name = "Tree"}` returns a
  table with `entity`, `description` and the fields of the result. A refused or failing command, and a command refused
  during Play, raises a Lua error. The journal records them with the source `script`.
- The Lua state is separate from the game: it has the sandbox rules of the game scripts, no engine binding (`transform`,
  `physics`...), and never runs inside Play.

### Editor scripts

An editor script is a `.lua` file under an `editor/` folder of an asset folder of the project, for instance
`engine_demo/editor/plant_row.lua`. Run it from the **Editor Scripts** menu of the Console, or from the context menu of
the file in the Content Browser (**Run as Editor Script**). A whole script is one undo step; an error undoes what it did.

A `.owlmacro` file has **Export to Lua** in the Content Browser: it writes the equivalent `.lua` file.

### Macros

Select lines of the journal and **Save as Macro**: a `.owlmacro` file replays them, the entities they created written
as `$n` references. A double-click on the file in the Content Browser (which also offers **Edit as Text**) or
`macro.run path=...` replays it as one undo step; the first failing line stops it, undoes it and reports its line.

## Command palette

`Shift` twice (outside a text field), or a click in the search box of the ribbon, opens a search over every editor
action, every command, the entities of the active scene and the asset files. Letters in order match (`svdoc` finds
*Save Document*), word starts first; arrows move, `Enter` runs, `Escape` closes. An entity is selected, an asset
opened.

A command without arguments runs at once; otherwise a form takes its place: text and number fields, boxes, vectors,
a colour picker, a list for a choice or an entity (the selection by default). **Run** (`Ctrl+Enter`) stays disabled,
with the reason, until the form is valid; the result shows in a toast. Commands that edit the scene are greyed during
Play.

## Commands

A refused command changes nothing and logs why (a bad argument, an edit during Play). The main ones:

| Command                           | Arguments                                                                                               |
|-----------------------------------|---------------------------------------------------------------------------------------------------------|
| `entity.create`                   | `name` (`Empty Entity`), `parent` (root when absent)                                                    |
| `entity.delete`                   | `entity`, `children` (delete the subtree, else the children move up)                                    |
| `entity.duplicate`                | `entity`, `children` (duplicate the subtree)                                                            |
| `entity.reparent`                 | `entity`, `parent` (root when absent); the world position is kept                                       |
| `entity.rename`                   | `entity`, `name`                                                                                        |
| `entity.set_transform`            | `entity`, `translation` / `rotation` (radians) / `scale`                                                |
| `entity.create_light`             | `type` (`Directional`, `Point`, `Spot`, `Rectangle`, `Disc`, `Sphere`), `name`, `translation`, `parent` |
| `entity.create_light2d`           | `type` (`Ambient`, `Point`, `Spot`), `name`, `translation`, `parent`                                    |
| `entity.create_reflection_probe`  | `name`, `translation`, `parent`                                                                         |
| `component.add`                   | `entity`, `component` (display name `Circle Renderer` or file key)                                      |
| `component.remove`                | `entity`, `component`                                                                                   |
| `component.set`                   | `entity`, `component`, `values` (a YAML / JSON map merged into the component), `replace`                |
| `prefab.instantiate`              | `path` (prefab file), `asset_path`                                                                      |
| `mesh.create`                     | `path` (mesh file), `asset_path`, `name`, `translation` (world), `parent`, `layer`                      |
| `material.create`                 | `path` (new `.owlmat` file), `parent` (the parent material, a root material when absent)                |
| `sound.create`                    | `path` (new `.owlsound` file), `file` (the audio file of a first take, none when absent)                |
| `material.assign`                 | `entity`, `slot` (from 0, -1 for every slot), `material` (empty for the file's own)                     |
| `scene.save`                      | `path` (the scene file when absent)                                                                     |
| `scene.capture_reflection_probes` | none: every probe is captured again (not an undo entry)                                                 |
| `asset.set_import`                | `path` (texture or folder), `nearest`, `compression` (`None`, `Uastc`, `Etc1s`), `mips`; undoable       |
| `asset.rename`                    | `path`, `name`; the sidecar follows, references follow the GUID                                         |
| `asset.move`                      | `path`, `folder`; the sidecar follows, references follow the GUID                                       |
| `asset.delete`                    | `path` (file or folder); not an undo step                                                               |
| `asset.cook`                      | `path` (file or folder), cooked into the project cache                                                  |
| `asset.clear_cache`               | none                                                                                                    |
| `asset.upgrade_references`        | none; rewrites the references of every project document by GUID                                         |
| `edit.undo`, `edit.redo`          | none                                                                                                    |
| `macro.run`                       | `path` (`.owlmacro`)                                                                                    |
| `hud.add_widget`                  | `kind` (`panel`, `label`...), `name`, `parent`, `size`, `anchor`, `offset`, `size_unit`, `offset_unit`  |
| `hud.set_rect`                    | `entity`, `anchor_min`, `anchor_max`, `pivot`, `size`, `offset`, `size_unit`, `offset_unit`             |
| `hud.move`, `hud.delete`          | `entities` (comma separated), `delta` for a move                                                        |
| `hud.extract_canvas`              | `entity` (root canvas), `path` (`.owlhud`)                                                              |

`help` in the Console lists every command with its arguments, defaults included.

### Command line

One command per line, arguments as `name=value`, `#` starting a comment:

```
entity.create name="Tree" parent=$1          # parent: the entity of command 1
entity.set_transform entity=$_ translation=(0, 2, 0) scale=(1, 1, 1)
component.add entity="Tree" component="Circle Renderer"
component.set entity=$1 component=Transform values="{translation: [4, 3, 0]}"
```

| Type    | Written as                                                                                |
|---------|-------------------------------------------------------------------------------------------|
| bool    | `true`, `false`                                                                           |
| integer | `-12`                                                                                     |
| number  | `0.25`, `1e-3`                                                                            |
| text    | `"quoted"` (escapes `\"`, `\\`, `\n`, `\t`) or a bare word                                |
| entity  | a UUID, a name (`"Tree"`), `$n` (the entity of command `n`) or `$_` (of the last command) |
| vectors | `(1, 2)`, `(1, 2, 3)`, `(1, 2, 3, 4)`                                                     |
| colour  | `#rrggbb`, `#rrggbbaa` or `(r, g, b[, a])`                                                |
| choice  | the choice, bare or quoted                                                                |

`$n` counts the session journal in the Console and the macro's own commands in a macro, so a recording needs no
fixed id.

## Undo and redo

`Ctrl+Z` and `Ctrl+Y` (or the arrows of the ribbon) step through the history of the active document; the buttons
name the next step ("Undo Delete 'Player'"). Undo puts an entity back in place: same id, children, position in the
hierarchy and selection. Each document has its own history, kept until it closes.

## Stats

The **Stats** tab of the bottom strip (switched on in **Settings > General**) shows the displayed frame rate, the
frame cost (CPU and GPU time, the waits for the display excluded) and the frame rate it allows, the memory in use
and its peak, the allocations of the frame, the entity under the mouse, the draw calls and quads of the 2D renderer,
the viewport size and, on Vulkan, the submits, queue drains, fence waits and pacing wait of the last frame with their
maximum over two seconds.

Under vertical sync the displayed rate is the refresh rate whatever a frame costs; the frame cost measures the frame
itself, averaged over half a second. Capped by the display, the CPU and the GPU lower their clocks: a frame then costs
several times more than at full load (the Engine Demo hub: 0.79 ms CPU and 0.55 ms GPU at 60 Hz, 0.20 and 0.07 ms
uncapped), so the possible rate is no capacity. **Measure at Full Load** lifts vsync, the frame rate limit and the
idle wait for 3 s (1 s for the clocks to rise, 2 s measured) and shows the real frame rate with its CPU and GPU
cost; `OwlRunner --frame-bench` remains the reference to compare builds.
