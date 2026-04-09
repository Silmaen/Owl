---
name: Roadmap overview
description: Long-term release plan from v0.1.0 to v0.5.0, documented in doc/pages/roadmap.md
type: project
---

## Owl Release Roadmap

Full roadmap lives in `doc/pages/roadmap.md`. Summary:

- **v0.1.0** (Expected 2026-06-01) — Lua scripting, in-game UI, save system, runner polish, editor improvements (prefabs, undo/redo)
- **v0.2.0** (Expected 2026-08-01) — Raycasting renderer, voxel engine, 2D lighting, tilemap system, renderer architecture
- **v0.3.0** (Expected 2026-10-01) — Full 3D rendering (PBR, lighting, shadows), skeletal animation, particles, post-processing, weather, 3D physics
- **v0.4.0** (Expected 2026-12-01) — Networking & multiplayer, AI (pathfinding, behavior trees), advanced physics (queries, joints, callbacks), audio mixer, dialogue system
- **v0.5.0** (Expected 2027-02-01) — Modding, level streaming, asset pipeline/cooking, platform expansion (Web/WASM, Android, gamepad)

Profiling tools and rendering optimizations are cross-cutting concerns worked on continuously across all releases.

**Why:** v0.0.3 released 2026-04-09. Currently developing v0.1.0.

**How to apply:** All feature work should target a specific release. 3D rendering is v0.3.0, not earlier.
