# The Owl Nest Window {#page-editor}

[TOC]

Owl Nest opens on one window: the ribbon at the top, the open documents in the centre, the scene panels on the right,
the asset and log panels at the bottom and a status bar. Every panel docks, tabs and floats; the layout is saved
between runs.

![Owl Nest with the Iso Town scene of the Engine Demo: ribbon, viewport, Scene Hierarchy, Properties and Content Browser](../images/nest/main_window.png)

## Layout

| Area          | Panels                                                                                               |
|---------------|------------------------------------------------------------------------------------------------------|
| Centre        | The documents as tabs (scenes, scripts, graphs, tilemaps...) and the Help; a scene tab is a viewport |
| Right, top    | **Scene Hierarchy**: the entities of the active scene                                                |
| Right, bottom | **Properties**, with **Tile Palette**, **Voxel Palette** and **Scene Settings** as tabs              |
| Bottom        | **Content Browser**, **Log**, **Console**, **Script Debug** and **Stats**                            |
| Status bar    | Mode, active document, autosave, backend, warnings, previews, notifications, MCP                     |

- The Hierarchy, Properties and viewport always show the **active** document: the tab in front.
- Drag a tab by its title to dock it elsewhere or tear it out as a floating window. Under Wayland a torn-out window
  stays inside the main one; with `OWL_WINDOW_PLATFORM=x11` it becomes a desktop window of its own.
- **View > Layout > Hierarchy Left** (`Alt+2`) moves the Scene Hierarchy to a column left of the documents, `Alt+1`
  puts the default layout back; **Reset Layout** returns every panel to its place in the current layout.
- The palettes show only with their document: the Tile Palette with an open tilemap, the Voxel Palette with a scene
  holding a voxel world, in Edit.

## The ribbon

The ribbon is a row of tabs, each a strip of button groups; every button has a tooltip with its shortcut and is
greyed when it does not apply.

| Tab          | Groups and buttons                                                                                                                                                                             |
|--------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **File**     | Opens the File screen (below)                                                                                                                                                                  |
| **Home**     | Edit (Undo, Redo, Duplicate), Create (**New** scene, HUD, animation, tilemap, tileset), Project (Settings, Input Actions, Localisation, Pack Game)                                             |
| **View**     | Layout (Hierarchy Left, Reset Layout), Camera (Reset View, Axis Views, Frame Scene, Camera Viewpoint), Panels (Frame Stats, Help), Appearance (Owl Dark, Owl Light), Settings (Editor, Engine) |
| Document tab | The last tab, in the accent colour: the commands of the active document (below)                                                                                                                |

The document tab follows the active document: **Scene** (New, Open, Save, Save As, Import, Close; Cameras, HUD
Editor, Settings, Pack Scene), **Text** (Save, Close, Preview), **Graph**, **Animation** (Play, Pause, Stop,
Previous, Next, Save, Save As, Close), **Tilemap**, **Tileset**, **Font**, **Image**, **Mesh**, **Material** and
**Input Actions**.

The right end of the tab row holds the search box of the [command palette](editor_console.md#command-palette)
(`Shift` twice), Save, Undo, Redo and the collapse arrow: `Ctrl+F1`, or a click on the selected tab, folds the ribbon
to its tab row.

## The File screen

The **File** button opens the File screen over the editor: the project commands on the left, the open project, the
start cards and the recent projects on the right. It opens by itself when no project is open; `Escape` or **Back to
the editor** closes it.

![The File screen with the Engine Demo open, the sample games and the recent projects](../images/nest/file_screen.png)

| Entry                         | Effect                                                                                         |
|-------------------------------|------------------------------------------------------------------------------------------------|
| New project                   | Name, folder and template of a new project ([templates](editor_projects.md#project-templates)) |
| Open project                  | A folder holding an `owl_project.yml`, or the file itself                                      |
| Save project, Save project as | Write the project settings                                                                     |
| Package the game              | Export the project as a game folder ([export](editor_projects.md#exporting-a-game))            |
| Editor settings               | [Settings](editor_settings.md) of the editor                                                   |
| Project settings              | [Project Settings](editor_projects.md#project-settings)                                        |
| Engine parameters             | Window, backends and debug options of the engine                                               |
| Help                          | This help (`F1`)                                                                               |
| Close project, Exit           | Close the project (its documents and assets), quit                                             |

The cards: **New project**, **Open project**, **Getting started** (the [tour](getting_started.md)) and **Engine
Demo** when the demo sits beside the editor; below, the **Sample games** and the **Recent projects** (up to ten; a
click opens one, the cross forgets it).

## The status bar

It shows:

- the mode: `EDIT`, `PLAYING` or `PAUSED`, in the play colour while the scene runs;
- the active document and its entity count, and the time since the last autosave;
- the graphics backend and device;
- the warnings and errors logged since you last looked: a click opens the Log;
- the thumbnails the Content Browser still has to render;
- a bell with the last 30 notifications (the toasts of the editor), the count of the unread ones;
- the MCP indicator: `MCP off`, `MCP listening`, or `MCP client active` for ten seconds after a request; a click opens
  its settings ([MCP server](mcp.md)).

## Window title and unsaved changes

The title names the project and the active document, with `*` while the document differs from its saved state:
undoing back to the save point clears it. A tab with unsaved changes shows a dot; closing it asks to save, discard or
cancel. Every modified document is also copied aside every minute ([autosave](editor_projects.md#autosave-and-crash-recovery)).

## Text and fonts

The interface uses Inter and the code documents JetBrains Mono. **Settings > General** scales the text at once (75 to
150 %, `Ctrl+=`, `Ctrl+-`, `Ctrl+0`) and sets the UI and code font sizes, applied at the next start. The size follows
the scale of the monitor.
