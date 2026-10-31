# Cutscene sequences {#page-sequences}

[TOC]

A sequence is a short film made of the game's own camera: shots that move the camera along a path and event keys that
call Lua functions at given times. It is a `.owlseq` asset, edited on a timeline in Owl Nest and played by a
`SequencePlayer` component or from Lua. Plan: [Large worlds](design/large-worlds.md#large-worlds-sequences).

## Shots, cuts and events {#sequences-model}

A sequence has a **duration** in seconds and two kinds of tracks.

- A **camera track** is a shot. Its keys hold a time, a position, a rotation, a field of view, an orthographic size and
  an easing. The shot is live from its first key to its last.
- An **event track** holds keys with a time and the name of a Lua function.

| Easing   | Between a key and the next                                                              |
|----------|-----------------------------------------------------------------------------------------|
| `Smooth` | Catmull-Rom curve through the neighbour keys, flat at the first and last key of a track |
| `Linear` | Straight interpolation                                                                  |
| `Step`   | The key is held until the next one: a cut                                               |

Rotations interpolate the short way. At a given time:

- the **last unmuted camera track** spanning the time drives the camera;
- in a gap between shots, the shot that ended last holds its last key;
- before any shot, the earliest one holds its first key.

A muted track is ignored. An event key calls the function of that name in the script of the player entity, else in the
scene script; a seek calls no event.

## The SequencePlayer component {#sequences-player}

The `SequencePlayer` ([fields](scene.md#sequence-player-component)) holds the sequence, the entity whose camera it
drives (itself by default), whether it starts with Play, loops, its speed and an `onFinished` Lua function. The keys
are in the space of the player entity (of its parent when it drives its own camera), so moving the entity moves the
whole cutscene. While it plays, the driven camera is the primary one; the cameras it replaced are primary again when
the sequence stops or ends, then `onFinished` is called. The export packs the `.owlseq` of every player.

## Editing in Owl Nest {#sequences-editor}

Create one with **New Sequence** in the Content Browser folder menu. The **Open** button of a `SequencePlayer` in the
Inspector, or a double-click on the file, opens the **Sequencer**: a tab docked with the bottom panels, under the scene.
It previews on a scene that plays the sequence: the selected player when it was opened from the Inspector, else the
first open scene with a `SequencePlayer` of this file. The scene stays in the viewport and in the Hierarchy; the
Properties panel shows the sequence while the Sequencer is the active tab, and a click in the viewport gives it back
to the scene.

- **Toolbar**: Play / Pause (`Space`), back to start, the playhead time, the **Duration**, **Add a camera track**,
  **Add an event track**, **Add key** (`K`), **Remove keys** (`Delete`), **Snap** to 0.1 s, the zoom in pixels per
  second, **Look through** and the player and scene of the preview.
- **Timeline**: a ruler, one row per camera track (diamond keys, the shot bar brighter while it is live), then the
  event tracks (round keys labelled with their function). Click selects a key (`Ctrl` adds), drag moves the selected
  keys, a click on an empty lane or on the ruler moves the playhead, a double-click adds a key. The context menu of a
  row has **Add key at playhead**, **Mute** / **Unmute**, **Move up** / **Move down** and **Remove track**. A new
  camera track starts where the previous shot ends.
- **Properties panel**: the **Duration**, the **Preview** and the playback settings of its player; the name and
  **Muted** state of the selected track; for a camera key the **Time**, **Position**, **Rotation** (degrees), **Field
  of view** (degrees), **Size** and **Easing**; for an event key the **Time** and **Function**.
- The ribbon tab **Sequence** has **Save**, **Close**, **Camera Track**, **Event Track**, **Add Key** and **Play**.
- Every gesture is a `sequence.*` command ([Console](editor_console.md)): `create`, `set_duration`, `add_track`,
  `remove_track`, `set_track`, `move_track`, `add_key`, `set_key`, `remove_keys`, `move_keys`. Each is one undo step of
  the Sequencer, which has its own history and autosave. **Save** (`Ctrl+S`) writes the file and the next plays read it
  again.

In the viewport of the preview scene, the player draws the path of every camera track, a frustum at each key and a
larger white one at the playhead, unsaved edits included ([Sequences in the viewport](editor_viewport.md#viewport-sequences)).
**Look through** shows the scene from the sequence camera at the playhead, following the scrub and the playback; turned
off, the editor view stays at that pose. Move the view from there, then **Key from view** writes its pose at the
playhead of the selected camera track: the key already at that time changes, else a key is added.

## Playing from Lua {#sequences-lua}

The scripts start, stop and scrub the player of an entity. Reference: [Sequences](lua-api-sequence.md).

```lua
local intro_id = 0

function on_create()
    intro_id = scene.find_entity("TavernIntro")
end

function on_update(dt)
    if intro_id ~= 0 and sequence.is_playing(intro_id) and input.is_action_just_pressed("jump") then
        sequence.stop(intro_id)         -- skip the cutscene
    end
end

function on_intro_hearth()              -- an event key of the sequence
    ui.set_text(ui.find("ObjectiveText"), "A fire still burns in the hearth.")
end
```

The Engine Demo tavern is a complete example: `engine_demo/sequences/tavern_intro.owlseq`, an entity `TavernIntro`
with its camera `IntroCamera`, and `engine_demo/scripts/mesh/tavern.lua`.

## The sequence file {#sequences-file}

A YAML file, format 1, under a `Sequence` root key. Angles are in radians; the keys of a track are sorted by time when read.

```yaml
Sequence: ""
FormatVersion: 1
duration: 7
cameraTracks:
  - name: Room
    muted: false
    keys:
      - {time: 0, position: [3.7, 2.5, 3.7], rotation: [-0.345, 0.651, 0], fov: 0.68, size: 8, easing: Smooth}
      - {time: 3.5, position: [1.4, 1.9, 2.4], rotation: [-0.241, 0.514, 0], fov: 0.68, size: 8, easing: Smooth}
eventTracks:
  - name: Story
    muted: false
    keys:
      - {time: 0, function: on_intro_room}
```
