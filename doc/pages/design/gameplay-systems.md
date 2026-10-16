# Gameplay systems {#page-design-gameplay-systems}

[TOC]

Design page for the gameplay systems of the v0.7.0 release (its deterministic simulation is in
[Simulation and networking](simulation-networking.md)), summarised in the [Roadmap](../roadmap.md). Gameplay systems ship as optional
modules on the v0.3.0 phased systems, not inside `Scene` (A-02). 3D physics is described in
[Physics API](physics-api.md).

## AI

- Pathfinding
    - Navigation mesh generation from scene geometry
    - A* pathfinding on NavMesh
    - Dynamic obstacle avoidance
    - Tile-graph pathfinding moved to v0.4.0 ([2D complete](2d-complete.md))
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
