# Material graphs {#page-design-material-graphs}

[TOC]

Design page for the material graphs of v0.7.0, summarised in the [Roadmap](../roadmap.md). The parameter materials
they extend are in [3D core](3d-core.md#materials).

## Goal

A material graph is Slang written as a graph: small nodes for the maths, large pre-coded nodes for the usual effects,
custom nodes for the Slang a node library lacks. The graph computes the surface of a material; the lights, the shadows
and the shading models stay those of the PBR materials.

## Decisions

| Decision    | Choice                                                                                                       |
|-------------|--------------------------------------------------------------------------------------------------------------|
| Asset       | A `Graph` kind of `.owlmat` beside the parameter kind, its graph in the same file                            |
| Editor      | A `NodeCanvas` document in Owl Nest, like the visual scripts and the PCG graphs                              |
| Output      | One surface node: base colour, alpha, metallic, roughness, normal, occlusion, emissive                       |
| Compiler    | The graph becomes a Slang surface function, compiled into the shader of the renderer                         |
| Small nodes | Maths, texture samples, UV, time, world position and normal, vertex colour                                   |
| Large nodes | Pre-coded Slang functions: noise, triplanar mapping, parallax, detail maps, height blend                     |
| Custom node | Its author gives the title, the typed pins and the Slang body                                                |
| Functions   | Material functions: reusable sub-graphs, on the mechanism of the visual-script functions                     |
| Parameters  | Values exposed by the graph; a `.owlmat` instance (`Parent`) overrides them without a recompile              |
| Shaders     | Compiled by Owl Nest on edit, baked by the export like the engine shaders                                    |
| Renderers   | The mesh renderer (static and skinned) first; voxel, planet and particles once they read the same surface    |

## Open questions

- Preview: one lit mesh for the whole graph, or a preview on every node
- The parameter kind as a pre-coded graph (one shading path), or kept beside the graph kind
- The clearcoat, sheen and anisotropy layers as outputs of the surface node
- A vertex stage (world position offset) after the surface

## Status

| Part                                               | Status  |
|----------------------------------------------------|---------|
| `Graph` kind of `.owlmat`, surface node, compiler  | Planned |
| Small, large and custom Slang nodes                | Planned |
| Material functions and parameters                  | Planned |
| Owl Nest editor and preview                        | Planned |
| Engine Demo scene and scenario                     | Planned |
