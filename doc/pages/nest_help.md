# Owl Nest Help {#page-nest-help}

[TOC]

Owl Nest is the editor of Owl Engine: you build a game in it, scene by scene, and play it in place. Scenes, tilemaps,
HUDs, materials, animations and Lua scripts open as tabs; every change can be undone; **Play** runs the scene you are
looking at, and **Pack Game** turns the project into a game folder you can hand to players.

## Start here

1. **Open something.** Without a project the **File** screen shows: **New project** (from a template), **Open
   project**, the **Engine Demo** and the sample games. Open the Engine Demo first: every engine feature is in it.
2. **Look around a scene.** Double-click a scene (`.owl`) in the Content Browser at the bottom. Right drag in the
   viewport looks around, the middle button pans, the wheel zooms; a click selects an entity.
3. **Change it.** Move the selection with the gizmo (`W` move, `E` rotate, `R` scale), edit its fields in
   **Properties** on the right, add an entity from the **Scene Hierarchy**. `Ctrl+Z` undoes any step.
4. **Play it.** `F5` (or the Play button over the viewport) plays a copy of the scene; `F7` stops and gives the scene
   back untouched.
5. **Ship it.** **Home > Project > Pack Game** exports the project, see [Projects and export](editor_projects.md).

A longer walk through the same steps: [Getting started](getting_started.md).

## Where things are

![Owl Nest with the Iso Town scene of the Engine Demo](../images/nest/main_window.png)

| Area                | What it holds                                                                 | Page                                       |
|---------------------|-------------------------------------------------------------------------------|--------------------------------------------|
| Ribbon (top)        | **File** screen, then the Home, View and active-document tabs                 | [The Owl Nest window](editor.md)           |
| Documents (centre)  | One tab per open file: scene viewport, code, tilemap, HUD, material, help...  | [Documents](editor_documents.md)           |
| Viewport overlays   | Gizmo mode and snap, Play controls, view and camera menus, view-cube          | [Viewport and gizmos](editor_viewport.md)  |
| Right column        | Scene Hierarchy, then Properties with the palettes and Scene Settings as tabs | [Hierarchy and inspector](editor_scene.md) |
| Bottom strip        | Content Browser, Log, Console, Stats                                          | [Content Browser](editor_assets.md)        |
| Status bar (bottom) | Mode, active document, autosave, backend, warnings, notifications, MCP        | [The Owl Nest window](editor.md)           |

## Find a page

| You want to                                      | Read                                           |
|--------------------------------------------------|------------------------------------------------|
| Paint a tile map or set up a tileset             | [Tilemaps and tilesets](editor_tilemaps.md)    |
| Lay out a HUD, menus, buttons                    | [HUD editor](editor_hud.md)                    |
| Understand what Play runs and what Stop restores | [Play mode](editor_play.md)                    |
| Type a command, record a macro, read the log     | [Console, log and commands](editor_console.md) |
| Create, open, save or export a project           | [Projects and export](editor_projects.md)      |
| Change a shortcut, the theme, the camera speed   | [Settings and shortcuts](editor_settings.md)   |
| Drive the editor from an AI agent                | [MCP server](mcp.md)                           |
| Know every component and its fields              | [Scene and components](scene.md)               |
| Write gameplay in Lua                            | [Scripting games in Lua](scripting.md)         |

## Using this help

- `F1` opens the help; over a component of the inspector it opens the page of that component.
- The sidebar groups the pages by theme; the filter at its top keeps the titles that contain what you type.
- Links between pages stay in the panel, with **<** and **>** to go back and forward; web links open in your browser.
- The **Help** window is a tab among the documents: drag it out to read beside the viewport.
