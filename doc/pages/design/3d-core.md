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
| 7  | PBR material asset, material editor and library (one pull request with 8)                             | 4, 5  |
| 8  | Post-process stack per camera: bloom, vignette, LUT (one pull request with 7)                         | 4     |
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
| Delivered by | Pull requests 3 and 4 together |

## Lighting

Lights reach the 3D layers only (`MeshRenderer` meshes and voxel worlds); 2D lighting is a separate item.

| Decision       | Choice                                                                                                            |
|----------------|-------------------------------------------------------------------------------------------------------------------|
| Component      | One `Light` component with a type (`Directional`, `Point`, `Spot`), not one component per kind                    |
| Orientation    | Shines along -Z of the world transform; a point light ignores rotation, a directional one the position            |
| Fallback       | The layer sun lights only a scene without any `Light`; the ambient comes from the `Environment`                   |
| Shading        | The `Standard` model of the [Materials](#materials); colour authored in sRGB, converted to linear on upload       |
| Units          | Lux for a directional light, lumens for a point (lm / 4π cd) or spot (lm / π cd) light, nits for emissive and sky |
| Falloff        | Inverse square (1 cm floor), cut off smoothly at `range` through the window `(1 - (d/range)^4)^2`                 |
| Spot cone      | Smooth fade between the inner and outer half angles                                                               |
| Forward lights | Up to 64 lights in a storage buffer (`renderer::GpuLight`), no clustering yet                                     |
| Shadows        | The first visible directional light with `castShadows`; point and spot lights cast none                           |
| Cascades       | 1 to 4 (`cascadeCount`), splits blending uniform and logarithmic (0.75) over `[near, shadowDistance]`             |
| Cascade fit    | Orthographic light camera around a sphere bounding the slice, centre snapped to the texel grid                    |
| Shadow map     | `RedFloat` target of `shadowResolution`^2 per cascade, one frame-graph pass each, opaque casters only             |
| Filtering      | 3x3 PCF with a normal-offset bias in texels (`shadowBias`)                                                        |
| Delivered by   | Pull requests 5 and 6 together                                                                                    |

## Materials {#materials}

Pull requests 7 and 8 together: PBR materials, image-based lighting and the per-camera post-process stack.

| Decision        | Choice                                                                                                                                                                 |
|-----------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Asset           | `.owlmat`, versioned YAML: shading model, alpha mode, factors, texture paths; a `Parent` makes it an instance overriding only the fields it lists                      |
| Shading models  | Per material: `Standard`, `Unlit` (base colour plus emissive, no light), `Toon`; Lambert is gone, voxel worlds included                                                |
| Standard        | Filament model: Burley diffuse, GGX distribution, height-correlated Smith visibility, Schlick Fresnel, multi-scattering energy compensation                            |
| Layers          | Clearcoat (GGX, Kelemen visibility), sheen (Charlie distribution, Neubelt visibility), anisotropy (anisotropic GGX); off at zero                                       |
| Toon            | Banded diffuse from the same lights and shadows, stepped highlight, a rim and the spherical-harmonics ambient (no reflections); `toonBands` and `toonRim` per material |
| Tangent frame   | Built per pixel from the position and UV derivatives: no tangent in the vertex layout; voxel worlds build theirs from the normal                                       |
| Data textures   | Normal, metallic-roughness, occlusion are read raw; base colour and emissive are decoded from sRGB                                                                     |
| Mesh slots      | One material slot per submesh (glTF primitive, OBJ material group); an empty slot takes the file's own material                                                        |
| Overrides       | A `MeshRenderer` slot overrides chosen fields of its material per entity; prefabs keep them as overrides                                                               |
| Synchronisation | `MaterialLibrary` (engine context) shares one material per file; editing or reloading a `.owlmat` updates its instances and every entity using them                    |
| Voxel worlds    | The default `Standard` material (no metal, roughness 0.5), no layers, the face occlusion as ambient occlusion                                                          |
| Future          | A `Graph` kind of `.owlmat`, edited on `NodeCanvas` and compiled to Slang, beside the parameter kind                                                                   |

| Decision        | Choice                                                                                                                |
|-----------------|-----------------------------------------------------------------------------------------------------------------------|
| Environment     | `Environment` component: an equirectangular `.hdr` (Poly Haven), luminance in nits, rotation, shown as the sky or not |
| Representation  | Kept equirectangular: mips prefiltered by roughness (GGX importance sampling) on the task system at load              |
| Diffuse ambient | Order-2 spherical harmonics (9 coefficients) projected from the image                                                 |
| Split sum       | DFG table (scale, bias, sheen) computed once at start-up, with the multi-scattering term                              |
| Without one     | The `Environment` `ambient` colour, else the layer ambient, as a constant sky                                         |

| Decision       | Choice                                                                                                                                                                                                            |
|----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Settings       | `PostProcessSettings` on `renderer::Camera`, saved with the Camera component, beside the tone mapping                                                                                                             |
| Exposure       | EV100 (default 15), exposure `1 / (1.2 · 2^EV100)` applied to the physical light values before shading; compensation in EV; automatic exposure from the GPU log-average luminance, adapted in EV/s within a range |
| Bloom          | Threshold with a soft knee, 13-tap downsample and 9-tap tent upsample over up to 6 levels (halving while a side stays at least 8 pixels), intensity and scatter                                                   |
| Vignette       | Intensity, smoothness, colour                                                                                                                                                                                     |
| Colour grading | `.cube` 3D LUT, stored as a 2D strip, looked up on the sRGB-encoded colour and mixed in by a contribution weight                                                                                                  |
| Frame graph    | Bloom levels are transient linear targets of decreasing size, the first at half the frame; the encode pass applies exposure, bloom, tone mapping, vignette, the sRGB encoding, then the LUT on the encoded colour |

## Emissive and area lights {#emissive-and-area-lights}

A glowing surface is one of three things, in increasing cost.

| Kind                     | What it does                                                | How                                                                                                         | When                      |
|--------------------------|-------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------|---------------------------|
| Emissive surface         | Shines above 1 in the HDR scene colour, lights nothing else | Emissive colour and luminance in nits in the PBR material; the halo is the bloom of the post stack          | v0.5.0, pull requests 7–8 |
| Directional angular size | A sun of real apparent size (about 0.5 degree)              | `angularRadius` on the directional `Light`: PCSS penumbra, wider highlight, sky disc                        | To evaluate, v0.5.0       |
| Area light               | A rectangle, disc or sphere that lights the other surfaces  | LTC (linearly transformed cosines) for rectangles and discs, representative point for spheres; PCSS shadows | To evaluate, v0.6.0       |
| Point and spot shadows   | Shadows of the local lights                                 | Cube shadow maps (point), perspective maps (spot), budgeted per frame                                       | Planned, v0.6.0           |
| Bounced light            | A lit or glowing surface tinting its neighbours             | Probes or voxel global illumination                                                                         | Stretch goal              |

A star seen from its planet is a directional light with an angular radius. Seen from space, beside the planet, it
is an emissive body carrying a point light without falloff; its scale belongs to
[Large worlds](large-worlds.md#lights-at-astronomical-scale).

## Reflection probes {#reflection-probes}

Without a probe, a metal reflects the environment of the scene, a constant sky when there is no image: indoors,
brass shows a grey sky instead of the fire and the room. Local probes give each place its own reflection.

| Point     | Decision                                                                                                           |
|-----------|--------------------------------------------------------------------------------------------------------------------|
| Capture   | A `ReflectionProbe` component renders its surroundings into a cube map, baked in the editor and stored as an asset |
| Filtering | The same GGX prefiltered levels and spherical harmonics as the `Environment`, so the shading code is shared        |
| Influence | A box (or sphere) volume with a blend distance; a surface blends the two probes nearest to it, else the sky        |
| Parallax  | Box projection of the reflected ray, so reflections line up with the walls of the room                             |
| Runtime   | Optional real-time update of one probe per frame for moving lights (the fire), budgeted                            |
| Editor    | Volume gizmo, bake button and preview sphere in Owl Nest                                                           |
| Target    | Planned, v0.6.0, with the point and spot shadows (both make interiors read)                                        |

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
