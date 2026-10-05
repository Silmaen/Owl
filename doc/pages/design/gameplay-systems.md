# Gameplay systems {#page-design-gameplay-systems}

[TOC]

Design page for the v0.8.0 release, summarised in the [Roadmap](../roadmap.md). Gameplay systems ship as optional
modules on the v0.3.0 phased systems, not inside `Scene` (A-02). 3D physics is described in
[Physics API](physics-api.md).

## AI

- Pathfinding
    - Navigation mesh generation from scene geometry
    - A* pathfinding on NavMesh
    - Dynamic obstacle avoidance
- Behaviour trees
    - Visual behaviour tree editor in Owl Nest (node graph)
    - Standard nodes: sequence, selector, parallel, decorator, condition, action
    - Lua-scriptable leaf nodes for custom actions/conditions
    - Savable `.owlbt` behaviour tree assets
- Steering behaviours
    - Seek, flee, arrive, wander, pursue, evade
    - Flocking (separation, alignment, cohesion)
    - Composable via behaviour tree or Lua

## Audio

- Audio mixer
    - Bus system: Master → Music / SFX / Ambient / Voice
    - Per-bus volume, mute, effects
    - Crossfade between music tracks
- Audio effects
    - Reverb zones (e.g. cave vs outdoor)
    - Low-pass/high-pass filters (e.g. underwater, behind walls)

## Narrative

- Dialogue system
    - Dialogue tree asset (`.owldlg`) with branching conversations
    - Visual dialogue editor in Owl Nest (node graph)
    - Lua hooks for conditions and consequences
    - Subtitle rendering via in-game UI
