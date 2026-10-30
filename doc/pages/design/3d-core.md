# 3D core {#page-design-3d-core}

[TOC]

Design page for the v0.5.0 release, summarised in the [Roadmap](../roadmap.md). It builds on the repaired
[Owl RHI](owl-rhi.md) of v0.3.0.

## Delivery plan

One pull request per row, in this order; 2D lighting and multithreaded preparation follow.

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
| Directional angular size | A sun of real apparent size (about 0.5 degree)              | `angularRadius` on the directional `Light`: PCSS penumbra, wider highlight, sky disc                        | v0.6.0                    |
| Area light               | A rectangle, disc or sphere that lights the other surfaces  | LTC (linearly transformed cosines) for rectangles and discs, representative point for spheres; PCSS shadows | v0.6.0                    |
| Point and spot shadows   | Shadows of the local lights                                 | Cube shadow maps (point), perspective maps (spot), budgeted per frame                                       | v0.6.0                    |
| Bounced light            | A lit or glowing surface tinting its neighbours             | Probes or voxel global illumination                                                                         | Stretch goal              |

Particles and trails glow the same way, with a luminance in nits pre-exposed like the lights, and the bright lights and
the sun flare in the lens, hidden by the geometry in front of them: [Effects](gpu-effects.md).

A star seen from its planet is a directional light with an angular radius. Seen from space, beside the planet, it
is an emissive body carrying a point light without falloff; its scale belongs to
[Large worlds](large-worlds.md#lights-at-astronomical-scale).

## Procedural sky and day-night cycle {#procedural-sky-and-day-night-cycle}

`ProceduralSky` computes the image an `Environment` would load (Preetham clear sky, dusk glow, stars, sun and moon
discs) and feeds the same prefiltering as an HDRI, built on a worker and again when the sun moves, so the sky lights
and reflects like an image ([Renderer](../renderer.md#renderer-ibl)). `DayNightCycle` advances the time of day, turns a
directional `Light` (shadows included) along the sun then the moon path, writes the sun direction into the sky and adds
a night exposure to the cameras in Play. Components: [Scene](../scene.md#proceduralsky-component); benchmark `sky/*`
in `test/bench/cases/SkyBench.cpp`. The voxel level uses both.

## Reflection probes {#reflection-probes}

Without a probe, a metal reflects the environment of the scene, a constant sky when there is no image: indoors,
brass shows a grey sky instead of the fire and the room. Local probes give each place its own reflection.

| Point     | Decision                                                                                                                      |
|-----------|-------------------------------------------------------------------------------------------------------------------------------|
| Capture   | A `ReflectionProbe` component renders the sky and the 3D layers into a cube on the GPU when it first shows, moves or is baked |
| Filtering | The mip chain of the capture, then GGX levels by filtered importance sampling; the diffuse stays the environment's            |
| Influence | A box (or sphere) volume with a blend distance; a surface blends the two probes of largest influence, else the sky            |
| Parallax  | Box or sphere projection of the reflected ray, so reflections line up with the walls of the room                              |
| Runtime   | Optional real-time capture, the realtime probes in turn, one per frame                                                        |
| Editor    | Volume and capture-point markers, inspector with a Bake button, `scene.capture_reflection_probes`                             |
| Budget    | The point shadow budget: 4 probes on OpenGL, 16 on Vulkan, the nearest to the camera                                          |
| Target    | v0.6.0                                                                                                                        |

## Instancing and culling {#instancing-and-culling}

Opaque meshes are drawn as GPU-culled instanced batches; a loose octree spares the CPU the entities outside the view.

| Decision        | Choice                                                                                                                                              |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| Grouping        | `RendererMesh` queues opaque meshes and, at `endScene`, groups them by mesh, material and texture                                                   |
| Instance buffer | `renderer::MeshInstance` (model, tint, entity id) in a storage buffer, read by `mesh3d.slang` when `gScene.ids.w >= 0`                              |
| Frustum culling | `mesh_culling.slang` (`renderer::utils::MeshCullingPass`) tests each instance box and writes the visible list                                       |
| Indirect draws  | One `DrawIndexedIndirectCommand` per group, its instance count set by the compute pass, drawn by `Renderer3D::drawMeshIndirect`                     |
| RHI             | `RenderCommand::drawIndexedIndirectAt` issues one command (`vkCmdDrawIndexedIndirect`, `glDrawElementsIndirect`), no count buffer                   |
| Occluder source | With `OcclusionCulling`, the mesh and voxel layers drawn in shadow mode with the render camera into a depth target                                  |
| Hi-Z pyramid    | Max-depth pyramid built by compute shaders (`hiz_seed.slang`, `hiz_reduce.slang`) in a storage buffer                                               |
| Occlusion test  | An instance is dropped when its projected box lies behind the farthest occluder depth of the texels it covers                                       |
| Conservative    | The pyramid level is picked so the box spans at most 2x2 texels; boxes crossing the camera plane are kept                                           |
| Blended meshes  | Keep the per-draw path, sorted back-to-front                                                                                                        |
| Null backend    | Culls on the CPU and draws with `drawMeshInstanced`                                                                                                 |
| Spatial index   | `data::geometry::LooseOctree` of boxes keyed by entity index, one for meshes and one for sprites, refiled when the world matrix or the mesh changes |
| Octree cells    | Loose cells, minimum half size 8 so small boxes share a cell; the root grows toward far boxes; empty cells are released                             |
| Octree use      | The mesh pass and the 2D sprite loop skip entities outside the frustum, submission order kept                                                       |
| Delivered by    | Pull request 9                                                                                                                                      |

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
- Compute-driven frustum / occlusion culling feeding indirect draws ([Instancing and culling](#instancing-and-culling))
- GPU raycast sprites: one instanced draw sorted by `BitonicSortPass` ([Raycaster method](../raycaster_method.md))
- Precision at any distance ([Large worlds](large-worlds.md))
    - Model matrices built relative to the camera on the CPU, so vertices near the camera keep their precision
    - Reversed-Z with a floating-point depth buffer and an infinite far plane, on Vulkan and OpenGL
- Spatial partitioning: a loose octree per scene, shared by the 2D and 3D layers ([Instancing and culling](#instancing-and-culling))
- Multithreaded render preparation: world transforms and spatial boxes of large scenes on the task scheduler, GPU
  submission on the main thread ([Renderer](../renderer.md))

## Scene editing

- 3D scene editing in Owl Nest
    - 3D viewport with orbit/fly camera
    - 3D gizmos (translate, rotate, scale in 3 axes)
    - Grid snapping, vertex snapping
    - Mesh import preview

## Engine Demo

- A 3D demo scene mixing a 3D level with a 2D HUD and a raycast or voxel side area
