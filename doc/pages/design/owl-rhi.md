# Owl RHI {#page-design-owl-rhi}

[TOC]

Design of the engine's render hardware interface.

## Decision

Owl Engine keeps its own render layer, **named and presented as the Owl RHI**: the interfaces of `owl::renderer::gpu`
and their backends in `source/owl/private/renderer/gpu/{vulkan,opengl,null}` (user documentation: the *Owl RHI*
section of [Renderer](../renderer.md), with the list of classes a backend implements, so a further backend can be added
without touching the renderers). The in-house layer gives control and carries the renderer stack.

| Backend | Role                                                      |
|---------|-----------------------------------------------------------|
| Vulkan  | Reference backend: every feature lands here first         |
| OpenGL  | Compatibility / fallback backend, tested                  |
| Null    | Headless tests and benchmarks                             |
| WebGPU  | Planned: browser target, so a game is playable on the Web |

- Real frames in flight, no `vkQueueWaitIdle` on the hot path; image transitions recorded inside the frame, the right
  `loadOp` between batches; a swapchain image written only after its acquire semaphore
- Per-frame uniform ring, so several `drawMesh` and several cameras per frame are correct; VMA sub-allocation;
  per-frame descriptor pools
- Image-comparison render tests on lavapipe (Vulkan) and llvmpipe (OpenGL)
- Planned: a WebGPU backend (Dawn or wgpu through `webgpu.h`), the Web target
  ([Modding and platforms](modding-platforms.md)); Metal and D3D12 are to be evaluated after 1.0

## OpenGL as the fallback

OpenGL 4.5 core is the minimum (DSA, SSBOs, compute). Shaders are written once in Slang and compiled to SPIR-V; the
backend then picks how the driver receives them:

| Driver                                      | Path                                                                       |
|---------------------------------------------|----------------------------------------------------------------------------|
| GL 4.6 or `GL_ARB_gl_spirv` (NVIDIA, Mesa)  | SPIR-V through `glShaderBinary` + `glSpecializeShader` (default)           |
| GL 4.5 without it (llvmpipe, older drivers) | GLSL 4.50 translated from the same SPIR-V by spirv-cross, `glShaderSource` |

Both paths share the SPIR-V cache and its reflection. `OWL_OPENGL_SHADERS=glsl` (or `spirv`) forces one. Only
`glMultiDrawElementsIndirectCount` (indirect draws) needs GL 4.6 and is skipped below it.
The NVIDIA SPIR-V path binds every storage buffer of a shader that shares a block type with another to one buffer, so
each storage buffer gets an element type of its own (checked by a test over every shipped shader); llvmpipe, on the GLSL
path, cannot show it, so the image tests do not cover it.

## Pipeline objects and explicit bindings {#owl-rhi-pipeline-bindings}

No global state nor call-order convention:

| Concern      | Owl RHI                                                                |
|--------------|------------------------------------------------------------------------|
| Depth state  | `PipelineState` given to `DrawData::init`, applied by every draw       |
| Topology     | `PipelineState::topology`, one `drawData` / `drawDataInstanced`        |
| Blend, cull  | `PipelineState::blendMode` and `cullMode` in the Vulkan pipeline key   |
| Textures     | `RenderCommand::bindTextures(span)` per block; `Texture2D::bind(slot)` |
| OpenGL state | `opengl::BindingTable` per block, re-applied when another block drew   |

The renderer block (`RendererDescriptors` + `ScopedActive`) is the bind group: Vulkan writes it into a descriptor set
per draw, OpenGL records its uniform buffers and textures. Storage buffers are bound right before each draw, on
both backends. Culling follows one convention (front faces counter-clockwise, Vulkan pipelines use
`VK_FRONT_FACE_COUNTER_CLOCKWISE` with the flipped projection); the planet layer culls, the other renderers draw both
faces.

The shader reflection (computed by spirv-cross) does not build the layouts, which `RendererDescriptors::declare` lists
by hand; `beginBatch` / `endBatch` / `nextSubpass` stay public for the editor's ImGui pass.
