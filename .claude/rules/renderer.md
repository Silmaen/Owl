---
paths:
  - "source/owl/*/renderer/**"
  - "test/renderer_tests/**"
  - "engine_assets/shaders/**"
---

# Renderer & GPU backends

User-facing references: `doc/pages/renderer.md`, `doc/pages/voxel.md`, `doc/pages/raycaster_method.md`.

## Before touching a working render path

A render path that works on `main` is ground truth, even if the roadmap or a comment says otherwise. Before
changing a shipped shader, backend path or descriptor binding, state the change and why it is needed, and
get confirmation. A passing headless (Null backend) test is evidence the path works, not a lead.

## Backend invariants (each one cost a debugging session)

- **Owl RHI: a draw inherits nothing.** Fixed-function state is the `PipelineState` given to `DrawData::init`
  (topology, culling, blending, depth); there is no global toggle. Bindings belong to the renderer block: create
  UBOs, bind textures (`RenderCommand::bindTextures`) and SSBOs, and draw under the renderer's `ScopedActive`. On
  OpenGL (global binding points, UBO 0 and units 0..n shared) `opengl::BindingTable` re-applies the block's UBOs and
  textures when another block drew in between; a bind made outside any block is not tracked.
- **Vulkan: one command buffer per frame, two frames in flight.** A batch is a render pass in it, not a
  submission; the frame is submitted once at `endFrame`. Never wait on a queue or the device in a frame:
  a CPU read-back goes through `VulkanHandler::flushFrame` / `submitNow` (counted as `fence_wait`).
- **Vulkan: nothing the GPU may still read is written or destroyed in place.** CPU data reaches the GPU
  through the frame ring (UBO, streamed SSBO, staging) or a copy recorded in the frame; destruction goes
  through `VulkanHandler::deferRelease` (`releaseBuffer`, `TextureData::freeTexture`, framebuffers).
- **Vulkan descriptor sets are per draw.** `internal::DescriptorRing` hands a distinct set to every draw
  from pools owned by the frame slot, reset when the slot comes back. Never "fix" descriptor errors with
  `UPDATE_AFTER_BIND`: with a shared set every draw samples the last write.
- **Uniforms are versioned per draw.** A UBO set between two draws gives each its own copy (frame ring), so
  several `drawMesh` models or 2D cameras in one frame are correct on Vulkan as on OpenGL.
- **One attachment layout for every framebuffer** (`Surface`, `RedInteger`, `Depth24Stencil8`, swapchain
  included) so all pipelines stay render-pass compatible.
- **Pipelines are deduplicated** in `VulkanHandler::pushPipeline` (key = shader name, topology, culling,
  blending, set layout, render pass, vertex input; refcounted; depth is dynamic state set at each bind). Never build
  a pipeline per mesh.
- **Uploads close the render pass.** A buffer or texture upload in a frame is recorded outside any pass
  (the next draw reopens one that loads the attachments): build meshes and textures before the layers
  draw (`Scene::prepareVoxelRenderData()`, run by `renderWithStack` before any layer) to keep passes whole.
- **Process-static GPU holders** (e.g. `gui::IconBank`) must be cleared by their owner before device
  teardown (`EditorLayer::onDetach()`), or they leak at `vkDestroyDevice`.
- **Transparency** is alpha blending with depth test but no depth write (`Renderer3D::transparentMeshState`), the
  geometry sorted back-to-front in its own pass. Front faces are counter-clockwise on both backends (Vulkan pipelines
  use `COUNTER_CLOCKWISE` with the flipped projection); no renderer culls yet.
- **Vulkan flips projection Y** (`proj(1,1) *= -1`): any screen→NDC ray or picking math is backend
  dependent (`isOpenGl ? -y : y`). Iso / gizmo projections must follow the same convention.
- **Tiling an atlas cell with `frac(uv)`** needs all three: half-texel inset of the cell, self-tileable
  textures, and `SampleGrad` with the continuous (non-fract) UV derivatives. `Texture2D::setFilterMode`
  works on both backends (the old sampler is released once the frames using it are done).

- **Vulkan descriptors are written at draw time** (`RendererDescriptors::getDescriptorSet`, from
  `bindPipeline`), after the draw's SSBO binds; every declared binding is written, unbound ones with a default
  resource. Never write a set when the textures are bound: the storage buffers are bound after them.
- **Colour samplers never enable depth comparison**: NVIDIA ignores it, lavapipe returns the compare result.
- **OpenGL `glClear` honours the depth mask**: `RenderAPI::clear` turns it back on and forgets the applied state.
- **Vertices given in NDC** (background quad) need the Vulkan Y flip in the shader, like the projections.
- **OpenGL shader path**: SPIR-V when the driver has GL 4.6 or `GL_ARB_gl_spirv` (built-ins remapped by
  `remapBuiltinsForOpenGl`), GLSL 4.50 from spirv-cross otherwise (llvmpipe). `OWL_OPENGL_SHADERS=glsl|spirv`
  forces it; check both when touching a shader.
- **One element type per storage buffer in a shader.** On the OpenGL SPIR-V path NVIDIA binds every SSBO sharing a
  block type to one buffer (two `StructuredBuffer<float4x4>` read the same data); wrap the element in a struct of its
  own. `SlangCompute.shippedShadersGiveEachStorageBufferItsOwnBlockType` checks it; llvmpipe (GLSL path) never shows it.

## Vulkan validation

Enabled at runtime, not by CMake: Owl Nest *Parameters → Use Debugging* (`useDebugging`), `OwlRunner
--frame-bench … --validation`. Layers come from the system or `-DOWL_ENABLE_VULKAN_LAYERS=ON`. Use a **release**
build (debug builds flood stdout with gcov `profiling:` lines). `VulkanCore::setObjectName` tags resources so leak
reports name them. **Zero validation message** on the sample scenes is the bar on NVIDIA, Intel and lavapipe
(`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`, no GPU needed). The image tests (`ctest -L render`) fail on
any `VUID`. Never mark a GPU issue fixed without a validation-layer run.

## Frame cost

Hot paths: `Renderer2D::flush`, tilemap/raycast/voxel passes, `Scene::onUpdateRuntime`. No per-frame
allocation, no per-draw `setData` of a UBO when a batched call exists, no log in a draw loop (use
`OWL_CORE_FRAME_TRACE`). Measure with `OWL_PROFILE_SCOPE` before and after any change.
