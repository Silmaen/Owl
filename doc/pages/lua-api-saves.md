# Lua API: Game state, saves and settings {#page-lua-api-saves}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `gamestate`

Values kept across levels and stored in saves.

| Function                                     | Returns      | Description                                                                         |
|----------------------------------------------|--------------|-------------------------------------------------------------------------------------|
| `gamestate.set(key: string, value: any)`     |              | Store a value (integer, number, string or boolean) kept across levels and in saves. |
| `gamestate.get(key: string, [default: any])` | `value: any` | Stored value, or `default` (nil when absent) when the key is missing.               |
| `gamestate.remove(key: string)`              |              | Remove a key.                                                                       |
| `gamestate.clear()`                          |              | Remove every key.                                                                   |

## `save`

Save slots (applied after the frame).

| Function                          | Returns           | Description                                                                        |
|-----------------------------------|-------------------|------------------------------------------------------------------------------------|
| `save.save_game(slot: integer)`   |                   | Save the level and the game state to a slot after this frame.                      |
| `save.load_game(slot: integer)`   |                   | Load a slot after this frame; the level keeps running when the slot does not load. |
| `save.has_save(slot: integer)`    | `exists: boolean` | Whether a slot holds a save.                                                       |
| `save.delete_save(slot: integer)` |                   | Delete the save of a slot.                                                         |
| `save.list_saves()`               | `saves: table`    | Every save, as a list of `{slot, timestamp, scene}`.                               |

## `settings`

Game settings: defaults from `game_settings.yml`, user overrides.

| Function                                    | Returns       | Description                                                                        |
|---------------------------------------------|---------------|------------------------------------------------------------------------------------|
| `settings.get(key: string, [default: any])` | `value: any`  | Setting value: user override, else game default, else `default` (nil when absent). |
| `settings.set(key: string, value: any)`     |               | Set a user override (integer, number, string or boolean).                          |
| `settings.save()`                           | `ok: boolean` | Write the user overrides to `settings.yml`.                                        |
| `settings.load()`                           |               | Reload the user overrides from `settings.yml`.                                     |
| `settings.reset(key: string)`               |               | Remove a user override (back to the game default).                                 |
| `settings.reset_all()`                      |               | Remove every user override.                                                        |
| `settings.apply()`                          |               | Apply the built-in keys to the window and the sound.                               |
