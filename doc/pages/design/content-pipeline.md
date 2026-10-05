# Content pipeline {#page-design-content-pipeline}

[TOC]

Design page for the v0.7.0 content work (assets, animation, effects), summarised in the [Roadmap](../roadmap.md).
Visual scripting and procedural graphs have their own pages: [Visual scripting](visual-scripting.md),
[PCG graphs](pcg-graphs.md).

## Offline asset pipeline (cooking)

- Offline asset preprocessing: texture compression, atlas generation, mesh optimization, LOD generation
- Cooked assets for faster runtime loading, content-addressed cache
- Incremental cooking (only changed assets)
- Asset GUIDs instead of paths (K-12)

## Binary scene format

- Replace YAML with a binary format (MessagePack / flatbuffers / custom) for `.owl` scenes, `.owltilemap`,
  `.owltileset` at runtime. yaml-cpp allocates per node — a 1000-entity scene parses ~10× slower than the same data in
  binary. YAML stays the editor / source format and the cook converts it.
- Relies on the scene format version shipped in v0.3.0 (PR-25), without which no migration is possible (C-10)

## Parallel pack `readEntry`

Wrap `PackReader::readEntry` so multiple entries can be zstd-decompressed concurrently (file seek serialized under a
mutex, decompression off-mutex). Useful for packed games loading dozens of textures in parallel at startup. Needs
PackReader thread-safety audit + a worker-friendly API.

## Hot reload

- Assets, Slang shaders and Lua scripts hot reload ship in v0.3.0 (see [Foundations](foundations.md))
- ![To evaluate][evaluate] Hot reload of a C++ game module (game code built as a shared library, swapped while the
  editor runs, state kept through serialization)

## Animation

- Skeletal animation
    - Bone hierarchy and skinning
    - Animation clips with blending and transitions
    - Animation state machine component
- Mesh LOD support (multiple detail levels per mesh, produced by the cook)

## Effects

- Particle system
    - GPU-accelerated particle emitters
    - Configurable: lifetime, velocity, colour over time, size, gravity
    - Emitter shapes: point, sphere, cone, box
    - Editor: visual particle preview and property curves
- Advanced post-processing on the v0.6.0 stack: chromatic aberration, film grain, motion blur, depth of field
- Weather and environment effects (rain, snow, fog, day-night cycle) are a sample-project demo built on particles and
  post-processing, not an engine system

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
