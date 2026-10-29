# Particles and effects {#page-particles}

[TOC]

Particle emitters, ribbon trails, emissive effects and lens flares, in 2D and in 3D.

## Set up the layer

Particles and trails are drawn by a `RendererParticles` layer of the render stack (Project Settings, or
`project.add_render_layer`). Put it after the world layers it decorates and before the HUD layers: it draws with the
scene camera, perspective or orthographic, depth tested against what the layers before it drew.

```yaml
RendererStack:
  - Type: RendererMesh
    Name: mesh_world
  - Type: RendererParticles
    Name: particles
  - Type: Renderer2D
    Name: ui
```

An emitter or a trail is drawn by the layer its `RendererTag` names (untagged: the first layer of the stack). Owl Nest
tags a new emitter to the first particle layer.

## ParticleEmitter

The particles are simulated on the GPU by a compute shader, on OpenGL as on Vulkan; on the Null backend the same
simulation runs on the CPU. An emitter keeps at most `maxParticles` particles: past it the oldest one is replaced.

| Group    | Fields                                                                                                                    |
|----------|---------------------------------------------------------------------------------------------------------------------------|
| Emission | `maxParticles` (1 to 65536), `rate` per second, `burst` at each start, `duration` (0: forever), `loop`, `seed`            |
| Life     | `lifetimeMin`, `lifetimeMax` in seconds                                                                                   |
| Shape    | `shape` (`Point`, `Sphere`, `Box`, `Cone` along +Y, `Circle` in the XY plane for 2D), `shapeSize`, `coneAngle`            |
| Motion   | `speedMin`, `speedMax`, `velocity` (entity space), `gravity` (world), `drag`, `spinMin`, `spinMax`, `space`               |
| Look     | `sizeMin`, `sizeMax`, `sizeOverLife` (curve), `color`, `colorOverLife` (gradient), `emissive` (nits), `blend`, `texture`  |
| Trails   | `trailLength` (0 to 16 points), `trailInterval` in seconds, `trailWidth` relative to the particle                         |
| Play     | `playOnStart`: the emission starts with the scene in Play; off waits for a script                                         |

- `space: World` leaves the particles where they were spawned when the entity moves; `Local` carries them along.
- `blend: Additive` adds the particles to the scene in any order (fire, sparks, magic); `Alpha` blends them over it,
  sorted far to near on the GPU (smoke, dust), at most 1024 particles. Emitters are not sorted between them.
- Without a texture a particle is a soft disc.
- Particles and trails follow a shift of the floating origin.

Presets: `Fire`, `Smoke`, `Sparks`, `Magic`, `Snow` (`renderer::makeParticlePreset`, Owl Nest **Create > Effects**).

## Trail

A ribbon behind the entity, facing the camera: a point is recorded each time the entity moved `minDistance`, and lives
`lifetime` seconds (`maxPoints` at most, up to 128). `width` with `widthOverLength`, and `color` with `colorOverLength`
from the entity (0) to the tail (1); the defaults taper and fade it. `emissive` and `blend` as for the particles. A
trail on a child entity follows its parent.

## Emissive effects

`emissive` is a luminance in nits, pre-exposed like the lights by the exposure of the camera (EV100): the particles and
trails then go above 1 in the HDR scene colour and the bloom of the camera makes them glow. Expect a few hundred nits
in a candle-lit room (EV100 4), tens of thousands under the sun (EV100 15).

## Lens flares

A camera with **Lens flares** (post process) shows the flares of the lights whose **Lens Flare** strength (`flare`) is
above 0, the sun included: a glow with streaks at the light, a halo around the centre of the image and ghosts along the
line from the light through the centre. A light flares with the pre-exposed illuminance it gives at the camera, times
its strength, the brightest 8 at once. Each light is tested against the depth of the meshes and voxel worlds: hidden it
does not flare, half hidden it flares half.

| Camera field         | Default | Meaning                                                                 |
|----------------------|---------|-------------------------------------------------------------------------|
| `lensFlare`          | `false` | Show the lens flares                                                    |
| `lensFlareIntensity` | `1`     | Strength of every flare                                                 |
| `lensFlareGhosts`    | `4`     | Ghosts per light, 0 to 8                                                |
| `lensFlareSpacing`   | `0.45`  | Distance between two ghosts, a fraction of the line centre to light     |
| `lensFlareHalo`      | `0.6`   | Radius of the halo, a fraction of the image height                      |
| `lensFlareSize`      | `0.12`  | Size of the glow at a light, a fraction of the image height             |

## In Owl Nest

- **Create > Effects** adds an emitter from a preset (`entity.create_particle_emitter`).
- In Edit mode every emitter plays in the viewport; the inspector's **Preview** row pauses, restarts, stops or bursts
  it (`particles.preview`, also in Play), without changing the scene.
- The inspector edits every field, the colour over life in a gradient editor (double-click to add a stop, right-click
  to remove it) and the size over life in a curve editor; every edit is undoable.
- Particles are picked in the viewport: a click selects their emitter.

## In Lua

```lua
entity.particles_play(id)        -- start or resume
entity.particles_pause(id)       -- freeze
entity.particles_stop(id)        -- stop emitting, the living particles finish
entity.particles_restart(id)     -- clear and start again
entity.particles_emit(id, 30)    -- a burst now
entity.particles_is_emitting(id)
entity.set_trail_enabled(id, false)
```

Reference: [Lua API, entities](lua-api-entities.md).
