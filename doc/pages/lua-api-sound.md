# Lua API: Sound {#page-lua-api-sound}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `sound`

Sound playback, by handle.

| Function                                            | Returns           | Description                                                                               |
|-----------------------------------------------------|-------------------|-------------------------------------------------------------------------------------------|
| `sound.play(asset_path: string)`                    | `handle: integer` | Play a sound asset (loaded on first use); an invalid handle when sound is off or missing. |
| `sound.stop(handle: integer)`                       |                   | Stop a playing sound.                                                                     |
| `sound.pause(handle: integer)`                      |                   | Pause a playing sound.                                                                    |
| `sound.resume(handle: integer)`                     |                   | Resume a paused sound.                                                                    |
| `sound.set_volume(handle: integer, volume: number)` |                   | Set the volume of a sound (0.0 to 2.0).                                                   |
