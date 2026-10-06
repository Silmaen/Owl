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

- **OpenGL bindings are global state.** `Renderer2D`, `RendererTilemap` and `Renderer3D` share UBO binding
  0: every renderer must call its UBO `bind()` before its draws. A no-op `bind()` = silent collision.
- **Vulkan: open the batch before recording binds.** `RenderAPI::drawData` lazily calls `beginBatch()`,
  which resets the command buffer and would wipe binds already recorded. `beginBatch()` is idempotent;
  never reset the in-flight fence twice without a submit in between.
- **Vulkan descriptor sets are per draw.** `internal::DescriptorRing` hands a distinct set to every draw,
  recycled by **submit fence**, not frame index (each framebuffer batch is its own submit). Never "fix"
  descriptor errors with `UPDATE_AFTER_BIND`: with a shared set every draw samples the last write.
- **Shared model UBO.** `Renderer3D::drawMesh` writes the model matrix into one shared UBO, so several
  `drawMesh` calls with different models in one frame are last-write-wins on Vulkan. Use `drawMeshes`
  with a shared model (voxel bakes the chunk origin into vertices). Real fix (push constant, SSBO or
  dynamic UBO) is due with static meshes — verify the current state before relying on either behaviour.
- **One attachment layout for every framebuffer** (`Surface`, `RedInteger`, `Depth24Stencil8`, swapchain
  included) so all pipelines stay render-pass compatible.
- **Pipelines are deduplicated** in `VulkanHandler::pushPipeline` (key = shader name, sidedness, set
  layout, render pass, vertex input; refcounted). Never build a pipeline per mesh.
- **No GPU resource creation inside a render pass** (`endSingleTimeCommands` waits on the queue). Build
  meshes and textures beforehand (`Scene::prepareVoxelRenderData()`, run by `renderWithStack` before any layer).
- **Process-static GPU holders** (e.g. `gui::IconBank`) must be cleared by their owner before device
  teardown (`EditorLayer::onDetach()`), or they leak at `vkDestroyDevice`.
- **One 2D camera per frame.** Renderer2D's view-projection UBO is shared too: mixing a perspective and an
  ortho `Renderer2D` scene in one frame applies the last camera to every 2D draw. World-space HUD goes
  through the perspective camera.
- **Blend is always on** (both backends); transparency only toggles depth-write (dynamic state on Vulkan).
  Transparent geometry is sorted back-to-front in its own pass.
- **Vulkan flips projection Y** (`proj(1,1) *= -1`): any screen→NDC ray or picking math is backend
  dependent (`isOpenGl ? -y : y`). Iso / gizmo projections must follow the same convention.
- **Tiling an atlas cell with `frac(uv)`** needs all three: half-texel inset of the cell, self-tileable
  textures, and `SampleGrad` with the continuous (non-fract) UV derivatives. `Texture2D::setFilterMode`
  is not implemented on Vulkan (linear + anisotropic sampler).

- **Vulkan descriptors are written at draw time** (`RendererDescriptors::getDescriptorSet`, from
  `bindPipeline`), after the draw's SSBO binds; every declared binding is written, unbound ones with a default
  resource. Never write a set at `endTextureLoad`: the storage buffers are bound after it.
- **Colour samplers never enable depth comparison**: NVIDIA ignores it, lavapipe returns the compare result.
- **OpenGL texture units are global too**: a pass that binds its own textures (tilemap) clobbers units 0..n; a
  renderer drawing after it rebinds its slots.
- **Vertices given in NDC** (background quad) need the Vulkan Y flip in the shader, like the projections.
- **OpenGL shader path**: SPIR-V when the driver has GL 4.6 or `GL_ARB_gl_spirv` (built-ins remapped by
  `remapBuiltinsForOpenGl`), GLSL 4.50 from spirv-cross otherwise (llvmpipe). `OWL_OPENGL_SHADERS=glsl|spirv`
  forces it; check both when touching a shader.

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
