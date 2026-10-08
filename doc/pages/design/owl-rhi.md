# Owl RHI {#page-design-owl-rhi}

[TOC]

Design page for the engine's render hardware interface, summarised in the [Roadmap](../roadmap.md).

## Decision

Owl keeps its own render layer, **named and presented as the Owl RHI**: the interfaces of `owl::renderer::gpu` and their
backends in `source/owl/private/renderer/gpu/{vulkan,opengl,null}` (user documentation: the *Owl RHI* section of
[Renderer](../renderer.md)). Third-party RHIs (SDL GPU, NVRHI, Diligent, bgfx, wgpu) were
compared in the audit (`doc/audit/30-etat-de-l-art.md`, fiche 2); the maintainer keeps the in-house layer for control
and because it already carries the renderer stack.

| Backend | Role                                                         | From    |
|---------|--------------------------------------------------------------|---------|
| Vulkan  | Reference backend: every feature lands here first            | v0.3.0  |
| OpenGL  | Compatibility / fallback backend, frozen in features, tested | v0.3.0  |
| Null    | Headless tests and benchmarks                                | today   |
| WebGPU  | Browser target, so a game is playable on the Web             | v0.10.0 |

## v0.3.0 — repair and name the RHI

- Done (PR-28): real frames in flight, no `vkQueueWaitIdle` on the hot path (≥ 10 queue drains per frame before, B-01)
- Done (PR-28): image transitions recorded inside the frame, correct `loadOp` between batches (B-02), swapchain
  image written only after its acquire semaphore (B-19), versioned UBO / SSBO (B-04)
- Done (PR-29): per-frame uniform ring, so several `drawMesh` and several cameras per frame are correct (B-03), VMA
  sub-allocation (B-11), per-frame descriptor pools (B-23)
- Done: pipeline objects and explicit bindings instead of global state and call-order conventions (B-07), see
  [below](#owl-rhi-pipeline-bindings)
- OpenGL frozen in features but fixed and tested: the "4.5" claim made true (B-16, done in PR-18, see below), culling
  available on both backends with one winding convention (B-07), mipmaps as documented (B-18)
- Done (PR-18): image-comparison render tests on lavapipe (Vulkan) and llvmpipe (OpenGL), validation clean on NVIDIA,
  Intel and lavapipe for the sample scenes

### OpenGL as the fallback

OpenGL 4.5 core is the minimum (DSA, SSBOs, compute). Shaders are still written once in Slang and compiled to SPIR-V;
the backend then picks how the driver receives them:

| Driver                                      | Path                                                                       |
|---------------------------------------------|----------------------------------------------------------------------------|
| GL 4.6 or `GL_ARB_gl_spirv` (NVIDIA, Mesa)  | SPIR-V through `glShaderBinary` + `glSpecializeShader` (default)           |
| GL 4.5 without it (llvmpipe, older drivers) | GLSL 4.50 translated from the same SPIR-V by spirv-cross, `glShaderSource` |

Both paths share the SPIR-V cache and its reflection. `OWL_OPENGL_SHADERS=glsl` (or `spirv`) forces one. Only
`glMultiDrawElementsIndirectCount` (indirect draws, unused in production) still needs GL 4.6 and is skipped below it.
The NVIDIA SPIR-V path binds every storage buffer of a shader that shares a block type with another to one buffer, so
each storage buffer gets an element type of its own (checked by a test over every shipped shader); llvmpipe, on the GLSL
path, cannot show it, so the image tests do not cover it.

### Pipeline objects and explicit bindings {#owl-rhi-pipeline-bindings}

B-07 listed the global state and the conventions of the old layer. What replaces each:

| Before                                                                          | Owl RHI                                                                   |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------|
| `setDepthTest` / `setDepthMask` toggles, left on by the raycast layer           | `PipelineState` given to `DrawData::init`, applied by every draw          |
| Line topology chosen by the shader name `"line"`, `drawLine*` entry points      | `PipelineState::topology`, one `drawData` / `drawDataInstanced`           |
| Blending hard-coded, culling never set (`iDoubleSided = true`)                  | `PipelineState::blendMode` and `cullMode` in the Vulkan pipeline key      |
| `beginTextureLoad` / `bind` in order / `endTextureLoad`, Vulkan ignoring slots  | `RenderCommand::bindTextures(span)` per block; `Texture2D::bind(slot)`    |
| OpenGL: each renderer re-binds its UBO and textures before drawing (binding 0)  | `opengl::BindingTable` per block, re-applied when another block drew      |

The renderer block (`RendererDescriptors` + `ScopedActive`) is the bind group: Vulkan writes it into a descriptor set
per draw, OpenGL records its uniform buffers and textures. Storage buffers are still bound right before each draw, on
both backends. Culling follows one convention (front faces counter-clockwise, Vulkan pipelines use
`VK_FRONT_FACE_COUNTER_CLOCKWISE` with the flipped projection); no renderer enables it yet. A check with back-face culling
on the voxel opaque meshes gave the same `stylemix` image on lavapipe and llvmpipe: turning it on is a measured
optimisation left to the maintainer.

Left for later: the shader reflection (computed by spirv-cross) still does not build the layouts, which `declare` lists
by hand; `beginBatch` / `endBatch` / `nextSubpass` stay public for the editor's ImGui pass.

- Done: the backend interface is documented (*Owl RHI* in [Renderer](../renderer.md), with the list of classes
  a backend implements) so a further backend can be added without touching the renderers

## v0.6.0 — 3D on the RHI

The render graph, PBR, shadows and GPU-driven culling build on the repaired Vulkan backend; OpenGL receives only what
is needed to keep the 2D, raycast and voxel paths working. See [3D core](3d-core.md).

## v0.10.0 — new backends

- ![Planned][planned] WebGPU backend (Dawn or wgpu through `webgpu.h`), the third platform for 1.0 (see
  [Modding and platforms](modding-platforms.md))
- ![To evaluate][evaluate] Metal (macOS) and D3D12 — after 1.0, the maintainer has no Mac

## SDL GPU

The SDL3 evaluation of v0.3.0 (see [Windowing and input](windowing-input.md)) may also consider SDL GPU as an Owl RHI
backend (Vulkan, D3D12, Metal). It is an option, not a commitment.

[planned]: https://img.shields.io/badge/-Planned-1f6feb?style=flat-square

[evaluate]: https://img.shields.io/badge/-To_evaluate-8250df?style=flat-square
