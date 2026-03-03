# Roadmap {#roadmap}

[TOC]

This page tracks planned and completed features across Owl releases.

## v0.1.0 -- Future

Unordered ideas and long-term goals.

- Graphics
    - <span class="badge-planned">Planned</span> HUD display
    - <span class="badge-planned">Planned</span> Animated textures
    - <span class="badge-planned">Planned</span> Advanced materials
- Gameplay
    - <span class="badge-planned">Planned</span> Inventory system
    - <span class="badge-planned">Planned</span> Collectible objects
        - Part of inventory
    - <span class="badge-planned">Planned</span> Key-locked switches
        - Part of inventory
    - <span class="badge-planned">Planned</span> Enemies
- Game Designer (Owl Map)
    - <span class="badge-planned">Planned</span> Menu edition
    - <span class="badge-planned">Planned</span> Node editing

## v0.0.3 -- Expected 2026-07-01

- Sound
    - <span class="badge-planned">Planned</span> Sound effects
    - <span class="badge-planned">Planned</span> Music
    - <span class="badge-planned">Planned</span> Sound management
        - Play, stop, loop, volume, etc.
    - <span class="badge-planned">Planned</span> Sound spatialization
        - 3D positional audio
    - <span class="badge-planned">Planned</span> Moving sound
- Graphics
    - <span class="badge-planned">Planned</span> Animated sprites
    - <span class="badge-planned">Planned</span> Basic 3D rendering
        - <span class="badge-planned">Planned</span> Simple lighting
- Objects
    - <span class="badge-done">Done</span> Mesh object support
    - <span class="badge-done">Done</span> Mesh loading (OBJ, glTF, GLB, FBX)
        - Via tinygltf, tinyobjloader, ufbx
    - <span class="badge-planned">Planned</span> Basic mesh manipulation
        - Scale, rotate, translate
    - <span class="badge-planned">Planned</span> Mesh collision
    - <span class="badge-planned">Planned</span> Mesh materials
        - Basic colors and textures
- Miscellaneous
    - <span class="badge-planned">Planned</span> Different scene types
        - Games and menus
    - <span class="badge-planned">Planned</span> Configurable keymap
- Game Designer (Owl Map)
    - <span class="badge-planned">Planned</span> Export game for runner
        - Standalone game runner package
    - <span class="badge-planned">Planned</span> Asset packing
        - With unpack support in runner
    - <span class="badge-planned">Planned</span> View of level links
    - <span class="badge-planned">Planned</span> Entity management shortcuts
        - Add/remove with delete confirmation
    - <span class="badge-planned">Planned</span> Separate editor/game display
        - Entities rendered differently

## v0.0.2 -- 2026-03-03

- Developers
    - <span class="badge-done">Done</span> Public engine headers 3rd-party independent
    - <span class="badge-done">Done</span> Remove fmt public dependency
    - <span class="badge-done">Done</span> Reduce needed public binaries
        - More static linking of 3rd-party libs
- Graphics
    - <span class="badge-done">Done</span> Backgrounds / skyboxes
    - <span class="badge-done">Done</span> Migrate shaders to Slang
- Miscellaneous
    - <span class="badge-done">Done</span> Pause / unpause game
    - <span class="badge-done">Done</span> Frame-by-frame stepping
        - When paused
    - <span class="badge-done">Done</span> General settings management
- Gameplay
    - <span class="badge-done">Done</span> Jump between scenes
- Game Designer (Owl Map)
    - <span class="badge-done">Done</span> Global game settings
    - <span class="badge-done">Done</span> Log frame
    - <span class="badge-done">Done</span> In-game visibility
    - <span class="badge-done">Done</span> In-editor visibility

## v0.0.1 -- 2025-02-06

First basic release: minimal viable engine with the ability to run simple games
defined in scenes.

## Badge Legend

- <span class="badge-done">Done</span> Completed features
- <span class="badge-progress">In Progress</span> Features currently being implemented
- <span class="badge-planned">Planned</span> Features that are planned but not yet being worked on
