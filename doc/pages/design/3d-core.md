# 3D core {#page-design-3d-core}

[TOC]

Design page for the v0.5.0 release, summarised in the [Roadmap](../roadmap.md). It builds on the repaired
[Owl RHI](owl-rhi.md) of v0.3.0.

## Delivery plan

One pull request per row, in this order; 2D lighting, HUD minimap, GPU raycast sprites, spatial partitioning,
multithreaded preparation and the Mini Transport Tycoon follow.

| #  | Pull request                                                                                          | Needs |
|----|-------------------------------------------------------------------------------------------------------|-------|
| 1  | `MeshRenderer` component and mesh asset as a stack layer, full editor support, 3D gizmos and snapping | —     |
| 2  | Viewport orbit / fly camera, "look through scene camera" mode, mesh import preview                    | 1     |
| 3  | Render graph hosting the existing layers as passes, no visual change (one pull request with 4)        | —     |
| 4  | HDR scene colour, linear colour space, per-camera tone mapping (one pull request with 3)              | 3     |
| 5  | Directional, point and spot light components, forward light SSBO (one pull request with 6)            | 1     |
| 6  | Directional shadow map (one pull request with 5)                                                      | 3, 5  |
| 7  | PBR material asset, material editor and library                                                       | 4, 5  |
| 8  | Post-process stack per camera: bloom, vignette, LUT                                                   | 4     |
| 9  | Instancing and GPU frustum / occlusion culling fed by the scene graph                                 | 1, 3  |
| 10 | Engine Demo 3D scene mixing a 3D level, a 2D HUD and a raycast or voxel area                          | 5–8   |
| 11 | Camera-relative model matrices, reversed-Z depth with an infinite far plane                           | 1, 3  |

## Colour space

Lighting, blending and filtering happen on linear values; encoding to sRGB happens once, at the end of the chain.

| Decision           | Choice                                                                                                     |
|--------------------|------------------------------------------------------------------------------------------------------------|
| Scope              | Every world layer (2D, tilemap, isometric, raycast, voxel, 3D) renders linear; no per-project gamma mode   |
| Colour textures    | Stay UNORM, decoded in the shaders (Owl Nest shows them through ImGui, which `*_SRGB` darkens)             |
| Data textures      | Stay UNORM; the shaders know which samplers are data (the MSDF coverage is not decoded)                    |
| Authored colours   | Stay sRGB in the inspector and in YAML, converted to linear when uploaded                                  |
| Scene colour       | RGBA16F, declared by the render graph: two attachment layouts, linear for the world, display for the end   |
| Encoding           | Tone map then sRGB encode in a shader pass, into the existing UNORM target; swapchain stays UNORM          |
| HUD and Owl Nest   | Drawn after the encode, in gamma space, unchanged                                                          |
| Tone mapping       | Per camera: `None` (clamp, default, exact 2D colours), Khronos PBR Neutral, ACES                           |
| Visible change     | Alpha blending of the 2D layers becomes linear; image-test references regenerated after a visual review    |
| Delivered by       | Pull requests 3 and 4 together                                                                             |

## Lighting

Lights reach the 3D layers only (`MeshRenderer` meshes and voxel worlds); 2D lighting is a separate item.

| Decision        | Choice                                                                                                  |
|-----------------|---------------------------------------------------------------------------------------------------------|
| Component       | One `Light` component with a type (`Directional`, `Point`, `Spot`), not one component per kind           |
| Orientation     | Shines along -Z of the world transform; a point light ignores rotation, a directional one the position  |
| Fallback        | A scene without a `Light` keeps the sun and ambient of the layer config; ambient always comes from it   |
| Diffuse         | Lambert; colour authored in sRGB, converted to linear on upload                                         |
| Falloff         | Point and spot reach zero at `range` through the window `(1 - (d/range)^4)^2`, no inverse square        |
| Spot cone       | Smooth fade between the inner and outer half angles                                                     |
| Forward lights  | Up to 64 lights in a storage buffer (`renderer::GpuLight`), no clustering yet                           |
| Shadows         | The first visible directional light with `castShadows`; point and spot lights cast none                 |
| Cascades        | 1 to 4 (`cascadeCount`), splits blending uniform and logarithmic (0.75) over `[near, shadowDistance]`  |
| Cascade fit     | Orthographic light camera around a sphere bounding the slice, centre snapped to the texel grid         |
| Shadow map      | `RedFloat` target of `shadowResolution`^2 per cascade, one frame-graph pass each, opaque casters only   |
| Filtering       | 3x3 PCF with a normal-offset bias in texels (`shadowBias`)                                              |
| Delivered by    | Pull requests 5 and 6 together                                                                          |

## Graphics

- 3D render pipeline
    - Forward rendering with depth buffer
    - Camera: perspective projection, free-look, orbit controls
- Render graph (K-21)
    - Passes declare their resources; barriers, transient attachments and pass ordering are derived
    - Hosts the existing renderer-stack layers as passes, so 3D mixes with 2D, raycast, voxel and isometric layers
- Lighting system
    - Directional light (sun), point lights, spotlights
    - Shadow mapping (at least for directional light)
    - Ambient light; a basic global-illumination approximation is a stretch goal
- Material system
    - PBR materials (albedo, normal, metallic, roughness, AO)
    - Material editor in Owl Nest inspector
    - Material library / reusable material assets
- Mesh rendering
    - Static mesh component with transform
    - Instanced rendering for repeated meshes
    - LOD support moves to the v0.6.0 [Content pipeline](content-pipeline.md) (LODs produced by the cook)
- Essential post-processing pipeline
    - Configurable post-process stack per camera
    - Effects: tone mapping, bloom, vignette, colour grading (LUT)
    - Chromatic aberration, film grain, motion blur and depth of field move to v0.6.0
- Compute-driven frustum / occlusion culling pre-pass feeding indirect draws
    - The `renderer::utils::FrustumCullingPass` utility + `frustum_culling.slang` shader ship, alongside the
      `RenderCommand::drawIndexedIndirect` API (Vulkan `vkCmdDrawIndexedIndirectCount`, OpenGL
      `glMultiDrawElementsIndirectCount`, Null no-op). `extractFrustumPlanes(viewProj)` Gribb-Hartmann helper for the
      CPU side. Headless tests on the Null backend pass.
    - With the SSBO-indexed instanced pipeline landed in v0.2.0, adoption is a drop-in: populate the AABB SSBO from
      the scene graph and replace the per-entity draw loop with one `drawIndexedIndirect`.
    - Comes after the v0.3.0 image tests: these passes never ran on a real GPU (B-20, F-01).
- GPU raycast sprite stripes + `BitonicSortPass` adoption (#32, deferred from v0.2.0)
    - The `BitonicSortPass` utility + `bitonic_sort.slang` shipped and are headless-tested in v0.2.0, but adoption was
      held back because `RendererRaycast::drawSprites` still emits per-column via `Renderer2D::drawQuad` (only the
      wall stripes moved to `raycast_stripe.slang` in v0.2.0 Phase 3).
    - Remaining work: move sprite stripe emission to a GPU instanced shader fed by a GPU-side `zBuffer[]` occlusion
      read (drop the per-frame readback the wall path currently does), then dispatch `BitonicSortPass` on the sprite
      depths so the back-to-front order stays on the GPU and feeds the stripe shader directly — zero CPU readback.
      Same applies to the door / pushwall stripe consumers.
- Precision at any distance ([Large worlds](large-worlds.md))
    - Model matrices built relative to the camera on the CPU, so vertices near the camera keep their precision
    - Reversed-Z with a floating-point depth buffer and an infinite far plane, on Vulkan and OpenGL
- Spatial partitioning (quadtree / octree) for culling, shared by the 2D and 3D layers (former "Rendering
  optimizations" ongoing item)
- Multithreaded render preparation (command recording per pass on the task system)

## Scene editing

- 3D scene editing in Owl Nest
    - 3D viewport with orbit/fly camera
    - 3D gizmos (translate, rotate, scale in 3 axes)
    - Grid snapping, vertex snapping
    - Mesh import preview

## Engine Demo

- A 3D demo scene mixing a 3D level with a 2D HUD and a raycast or voxel side area
