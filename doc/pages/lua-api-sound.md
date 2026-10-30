# Lua API: Sound {#page-lua-api-sound}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `sound`

Sound playback by handle, the mixer buses and the music.

| Function                                                       | Returns           | Description                                                                                                            |
|----------------------------------------------------------------|-------------------|------------------------------------------------------------------------------------------------------------------------|
| `sound.play(asset_path: string)`                               | `handle: integer` | Play a sound asset (loaded on first use); an invalid handle when sound is off or missing.                              |
| `sound.stop(handle: integer)`                                  |                   | Stop a playing sound.                                                                                                  |
| `sound.pause(handle: integer)`                                 |                   | Pause a playing sound.                                                                                                 |
| `sound.resume(handle: integer)`                                |                   | Resume a paused sound.                                                                                                 |
| `sound.set_volume(handle: integer, volume: number)`            |                   | Set the volume of a sound (0.0 to 2.0).                                                                                |
| `sound.set_bus_volume(bus: string, volume: number)`            |                   | Set the volume of a mixer bus (`Master`, `Music`, `SFX`, `Ambient`, `Voice`; 0.0 to 2.0).                              |
| `sound.get_bus_volume(bus: string)`                            | `volume: number`  | Volume of a mixer bus.                                                                                                 |
| `sound.set_bus_muted(bus: string, muted: boolean)`             |                   | Mute or unmute a mixer bus.                                                                                            |
| `sound.is_bus_muted(bus: string)`                              | `muted: boolean`  | Whether a mixer bus is muted.                                                                                          |
| `sound.play_music(asset_path: string, [fade: number])`         | `handle: integer` | Play a looping track on the Music bus, crossfading from the track playing (1 s by default).                            |
| `sound.stop_music([fade: number])`                             |                   | Fade out and stop the music track (1 s by default).                                                                    |
| `sound.crossfade(from: integer, to: integer, seconds: number)` |                   | Crossfade two sounds: the first fades out and stops, the second fades in from silence.                                 |
| `sound.set_music_state(state: string)`                         | `known: boolean`  | Switch the adaptive musics of the scene to a state (fading the layers, playing its stinger); false when none knows it. |
| `sound.get_music_state()`                                      | `state: string`   | State of the adaptive music of the scene, empty when none plays.                                                       |
