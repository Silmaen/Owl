# Material graphs {#page-design-material-graphs}

[TOC]

Design page for the material graphs of v0.7.0, summarised in the [Roadmap](../roadmap.md). The parameter materials
they extend are in [3D core](3d-core.md#materials).

## Goal

A material graph is Slang written as a graph: small nodes for the maths, large pre-coded nodes for the usual effects,
custom nodes for the Slang a node library lacks. The graph computes the surface of a material; the lights, the shadows
and the shading models stay those of the PBR materials.

## Decisions

| Decision       | Choice                                                                                                                                                            |
|----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Asset          | A `graph` section in a `.owlmat` (`MaterialAsset::graph`); a child material takes the graph of its parent                                                         |
| Editor         | A `NodeCanvas` in the material document of Owl Nest, like the visual scripts and the PCG graphs                                                                   |
| Output         | One surface node: base colour, alpha, normal, metallic, roughness, occlusion, emissive, the clearcoat, sheen and anisotropy layers, and the world position offset |
| Compiler       | `compileMaterialGraph` writes the material block of `common/mesh_surface.slangh`: `evaluateSurface` and `evaluateVertexOffset`                                    |
| Shaders        | A variant of `mesh3d` / `skinned3d` per program (`getMaterialShaderName`), baked by the export, else built on its first draw                                      |
| Draws          | `DrawData::setShaderVariant`: the mesh keeps its buffers, Vulkan builds one pipeline per variant and layout                                                       |
| Small nodes    | Maths, vectors and uv, texture sample, inputs (uv, world position and normal, view, time, vertex occlusion)                                                       |
| Large nodes    | Pre-coded Slang of `common/material_nodes.slangh`: noise, triplanar, parallax, height and detail blends, normal blending                                          |
| Custom node    | Its author gives the title, the typed pins and the Slang body                                                                                                     |
| Functions      | Material functions: sub-graphs with a `function.input` and a `function.output` node                                                                               |
| Parameters     | 8 numbers in `MaterialData.graph`, 6 textures in the map slots; a child overrides them in `graphValues`                                                           |
| Parameter kind | The pre-coded graph `getParameterMaterialGraph()` (`surface.parameters` into the surface): one shading path                                                       |
| Preview        | A lit mesh for the whole graph and a thumbnail per node (`MaterialPreviewTarget`, unlit)                                                                          |
| Renderers      | The mesh renderer (static, skinned, instanced); voxel, planet and particles once they read the same surface                                                       |

## Limits

- Triplanar, parallax and the parameter surface sample with derivatives: fragment stage only, refused in the world
  position offset; the texture sample reads mip 0 there.
- Texture parameters take the map slots from the base colour on; the maps of the material keep the slots that follow.
- The `Time` input wraps every hour.

## Status

| Part                                                 | Status |
|------------------------------------------------------|--------|
| `graph` section of `.owlmat`, surface node, compiler | Done   |
| Small, large and custom Slang nodes                  | Done   |
| Material functions and parameters                    | Done   |
| Owl Nest editor and preview                          | Done   |
| Graph shaders baked by the export                    | Done   |
| Engine Demo scene                                    | Done   |
