# Planets {#page-planets}

[TOC]

A **planet** is a sphere of terrain with an atmosphere, drawn from the ground to orbit by a `RendererPlanet` layer of the
render stack. Design notes and what is planned: [Large worlds](design/large-worlds.md).

## Set up the layer {#planets-set-up-the-layer}

Put a `RendererPlanet` layer in the project stack (Project Settings, or `project.add_render_layer`), after the world
layers it composes with and before the HUD layers:

```yaml
RendererStack:
  - Type: RendererMesh
    Name: mesh_world
  - Type: RendererPlanet
    Name: planet_world
  - Type: Renderer2D
    Name: ui
```

The layer reads `SunDirection`, `Ambient`, `SunIlluminance` and `AmbientLuminance` like `RendererMesh`. The terrain
chunks are drawn through `Renderer3D` as meshes lit by the `Light` components of the scene; the atmospheres are blended
over them. The **Planet** entry of the Effects menu of the Scene Hierarchy (`entity.create_planet`) appends such a layer
to the project stack when it has none.

## Planet component {#planets-planet-component}

YAML key `Planet`. The centre of the planet is the world position of the entity: its `WorldPosition` for a root entity
([World positions](scene.md#world-positions)). The settings are written flat in the component.

| Group      | Fields                                                                                                                                                            |
|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Heights    | `graph` (a `.owlpcg` with an `output.height` node; empty: the detail noise alone), `seed`, `heightResolution` (8 to 2048 cells)                                   |
| Shape      | `radius` of the sea surface in metres, `heightScale` in metres for a height of 1, `seaLevel` (0 to 1), `detailAmplitude`, `detailFrequency`                       |
| Detail     | `maxLevel` (deepest quadtree level), `lodFactor` (a chunk splits while the eye is nearer than this many times its size), `chunkResolution` (quads per chunk side) |
| Colours    | `seaColor`, `lowColor`, `highColor`, `peakColor` (sRGB), blended along the height                                                                                 |
| Atmosphere | `atmosphere`, `atmosphereHeight` in metres, `rayleighDepth` (per channel), `mieDepth`, `scatteringIntensity`                                                      |
| Far view   | `scaledSpaceDistance` in metres, 0 never                                                                                                                          |

Heights below `seaLevel` are flattened to it and drawn as sea.

## Terrain {#planets-terrain}

The sphere is a cube projected onto a sphere: six faces, each the root of a quadtree. A chunk splits while the eye is
nearer than `lodFactor` times its size, down to `maxLevel`. Chunks are built on the workers of the task scheduler and
uploaded by the main thread; until a chunk is ready its nearest built ancestor stands in. Each chunk carries skirts that
hide the cracks between levels, and chunks hidden behind the horizon are not drawn.

The heights come from a height map sampled on the sphere by a triplanar blend, plus a detail noise (`detailAmplitude`,
`detailFrequency`) that adds relief finer than the map. Without a `graph` the planet is made of the noise alone.

## Heights from a PCG graph {#planets-heights-from-a-pcg-graph}

The `Write Heights` node (`output.height`) of a [PCG graph](pcg_graphs.md) writes the height map: its field, clamped to
0..1, evaluated on a `heightResolution` square from the `seed` of the component. A graph without that node, or one
that fails, gives no height map (the detail noise alone). The graph is read through the asset search paths and the surface
is made again when the graph, the seed, the resolution or a shape setting changes. The export packs the graph with the
scene.

## Atmosphere {#planets-atmosphere}

The atmosphere is a shell of `atmosphereHeight` metres above the sea surface, shaded by single scattering
(`engine_assets/shaders/planet/planet_atmosphere.slang`): a Rayleigh term, whose optical depth per channel colours the
sky (Earth: 0.046, 0.108, 0.265), and a Mie term for the haze. The shell draws its front faces seen from outside and
its back faces seen from inside. `scatteringIntensity` scales the scattered sunlight; above 1 it stands in for the
multiple scattering.

## Scaled space {#planets-scaled-space}

Beyond `scaledSpaceDistance` from its centre (at least twice the radius when set) the planet is drawn shrunk toward the
camera, so that it keeps its angular size and stays within the depth range, with the level-0 chunks only. Nearer, it is
drawn at full scale and the detail takes over as the eye approaches.

## Precision {#planets-precision}

Model matrices are in the float space of the scene: keep a floating origin ([World positions](scene.md#world-positions))
when the planet is large or far from zero, so that the camera and the chunks stay near the origin.

## In Owl Nest {#planets-in-owl-nest}

- **Scene Hierarchy > Effects > Planet** creates the entity (`entity.create_planet`: `name`, `radius`, `graph`,
  `translation`, `parent`).
- The Planet inspector groups **Shape**, **Detail and Distance**, **Colours and Atmosphere**, then the **Height Graph**
  (a dropped `.owlpcg`), the **Seed** and the **Height Resolution**; every edit is an undo step.
- A planet is picked in the viewport like a mesh.

## Lua {#planets-lua}

`scene.get_planet_altitude(planet_id, entity_id)` gives the height of an entity above the ground (the sea surface over
the sea) of a planet, in metres ([Lua API](lua-api-scene.md)). The Engine Demo far orbit shows it in its HUD.
