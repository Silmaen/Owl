# Lua API: Cutscene sequences {#page-lua-api-sequence}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `sequence`

Cutscenes of the `.owlseq` sequences: play, stop and seek a `SequencePlayer`.

| Function                                               | Returns            | Description                                                                                                                                                         |
|--------------------------------------------------------|--------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `sequence.play(entity_id: entity, [from: number])`     | `done: boolean`    | Play the `.owlseq` of the entity's `SequencePlayer` from a time (0 by default), its camera made primary; false when it has none or the sequence cannot be read.     |
| `sequence.stop(entity_id: entity)`                     | `done: boolean`    | Stop the sequence of the entity's `SequencePlayer`: the cameras it replaced are primary again and its `onFinished` function is called; false when it does not play. |
| `sequence.seek(entity_id: entity, time: number)`       | `done: boolean`    | Move the playing sequence of the entity to a time, in seconds, without calling the events in between; false when it does not play.                                  |
| `sequence.is_playing(entity_id: entity)`               | `playing: boolean` | Whether the entity's `SequencePlayer` plays; nothing when it has none.                                                                                              |
| `sequence.get_time(entity_id: entity)`                 | `time: number`     | Playback time of the entity's `SequencePlayer`, in seconds; nothing when it has none.                                                                               |
| `sequence.get_duration(entity_id: entity)`             | `duration: number` | Length of the sequence of the entity's `SequencePlayer`, in seconds; nothing when it has none or it cannot be read.                                                 |
| `sequence.set_speed(entity_id: entity, speed: number)` | `done: boolean`    | Set the playback speed of the entity's `SequencePlayer` (1: real time, 0: paused); false when it has none.                                                          |
