# Roadmap {#roadmap}

[TOC]

This page tracks planned and completed features across Owl releases.

## v0.1.0 -- Future

Unordered ideas and long-term goals.

- Graphics
    - <span class="badge-planned">Planned</span> HUD display
    - <span class="badge-planned">Planned</span> Animated textures
    - <span class="badge-planned">Planned</span> Advanced materials
    - <span class="badge-planned">Planned</span> Basic 3D rendering
        - <span class="badge-planned">Planned</span> Simple lighting
- Gameplay
    - <span class="badge-planned">Planned</span> Inventory system
    - <span class="badge-planned">Planned</span> Collectible objects
        - Part of inventory
    - <span class="badge-planned">Planned</span> Key-locked switches
        - Part of inventory
    - <span class="badge-planned">Planned</span> Enemies
- Game Designer (Owl Nest)
    - <span class="badge-planned">Planned</span> Menu edition
    - <span class="badge-planned">Planned</span> Multiple open scene support editing
- Scene
    - <span class="badge-planned">Planned</span> Scene scripting
        - Lua or similar embedded language
    - <span class="badge-planned">Planned</span> More Scene events and triggers
        - E.g. on enter, on interact, on timer, etc.
        - Trigger actions: play sound, change scene, modify objects, etc.
        - Event parameters: e.g. trigger volume, interaction range, timer duration, etc.
        - Editor support for placing triggers and defining events
    - <span class="badge-planned">Planned</span> Allow common properties across different scene (player spawn point,
      lighting, background, etc.)
- Objects
    - <span class="badge-planned">Planned</span> Basic mesh manipulation
        - Scale, rotate, translate
    - <span class="badge-planned">Planned</span> Mesh collision
    - <span class="badge-planned">Planned</span> Mesh materials
        - Basic colors and textures
- Miscellaneous
    - <span class="badge-planned">Planned</span> Different scene types
        - Games and menus

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
- Objects
    - <span class="badge-done">Done</span> Mesh object support
    - <span class="badge-done">Done</span> Mesh loading (OBJ, glTF, GLB, FBX)
        - Via tinygltf, tinyobjloader, ufbx
- Miscellaneous
    - <span class="badge-done">Done</span> Configurable keymap
- Scene
    - <span class="badge-done">Done</span> Scene hierarchy (parent-child entities)
        - Transform and visibility inheritance
        - Reparenting preserves world position
        - Delete orphans children to grandparent, or cascade delete
        - Duplicate entity or entire subtree
    - <span class="badge-done">Done</span> Asset packing (`.owlpack` binary format)
        - zstd compression, XOR-obfuscated TOC
        - Pack-aware loading in runner
    - <span class="badge-done">Done</span> Task system (Taskflow backend)
        - Async work scheduling, parallel utilities
- Game Designer (Owl Nest)
    - <span class="badge-done">Done</span> Project system
        - Create, open, save, and close projects (`owl_project.yml`)
        - Dynamic asset directory management for project folders
        - Project name displayed in window title
    - <span class="badge-done">Done</span> Project settings panel
        - Edit project name and first scene (dropdown of available scenes)
    - <span class="badge-done">Done</span> Scene import into project
    - <span class="badge-done">Done</span> Export game for runner
        - Standalone game runner package
    - <span class="badge-done">Done</span> Scene hierarchy panel
        - Tree display with drag-drop reparenting
        - Context menu: create root/child entity, duplicate/subtree, unparent, delete/cascade
        - Visibility toggle buttons per entity (editor + game)
    - <span class="badge-done">Done</span> Icon system with runtime SVG rendering
        - SVG sources organized by usage (toolbar, browser, components, actions, etc.)
        - Runtime rasterization via lunasvg with dynamic theme color substitution
        - White → theme text color, fuchsia → theme accent color
        - Atlas with mipmaps, rebuild on theme change
    - <span class="badge-done">Done</span> View of level links
    - <span class="badge-done">Done</span> Separate editor/game display
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
