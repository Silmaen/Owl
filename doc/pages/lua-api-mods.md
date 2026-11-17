# Lua API: Mods {#page-lua-api-mods}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `mods`

The mods of the game: list them, enable, disable and reorder them, see their conflicts and apply the choices of the player.

| Function                                         | Returns            | Description                                                                                                                                                                                            |
|--------------------------------------------------|--------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `mods.is_managed()`                              | `managed: boolean` | Whether the player chooses the mods of this game: true in a game runner with a mods folder, false in Owl Nest (the plugins of the project are set in its Extensions panel).                            |
| `mods.count()`                                   | `count: integer`   | Number of mods in the mods folder, enabled or not.                                                                                                                                                     |
| `mods.get(index: integer)`                       | `mod: table`       | The mod at a place of the list (1 loads first): a table with `id`, `name`, `version`, `author`, `description`, `enabled`, `mounted` and `issue` (why an enabled mod is not mounted); nil past the end. |
| `mods.set_enabled(id: string, enabled: boolean)` | `known: boolean`   | Enable or disable a mod, applied by `mods.apply`; false for an unknown id.                                                                                                                             |
| `mods.move(id: string, offset: integer)`         | `moved: boolean`   | Move a mod up (negative offset, loads earlier) or down (loads later, wins the assets it shares), applied by `mods.apply`; false when it does not move.                                                 |
| `mods.is_dirty()`                                | `dirty: boolean`   | Whether a choice changed since the mods were last applied.                                                                                                                                             |
| `mods.apply([level: string])`                    | `applied: boolean` | After this frame, mount the mods chosen, save the choices and load a level (this one when none is given); false when the player does not choose the mods of this game.                                 |
| `mods.open_screen(return_level: string)`         |                    | After this frame, load the mod screen of the game (its `ModScreen`, else the one of the engine), which goes back to the level given when the player leaves it.                                         |
| `mods.return_level()`                            | `level: string`    | Level the mod screen goes back to (empty when it was not opened by `mods.open_screen`).                                                                                                                |
| `mods.conflicts()`                               | `conflicts: table` | The assets several mounted mods hold: a list of tables with `path`, `winner` (the id of the mod serving it) and `providers` (every id holding it, in load order).                                      |
