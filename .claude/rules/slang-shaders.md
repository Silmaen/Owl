---
paths:
  - "engine_assets/shaders/**/*.slang"
  - "source/owl/private/renderer/utils/shaderFileUtils.*"
---

# Slang Shader Conventions

## File Structure

Each `.slang` file contains both vertex and fragment shaders with annotated entry points:
```slang
[shader("vertex")]
VertexOutput vertexMain(VertexInput input) { ... }

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target { ... }
```

## Matrix Layout

Slang defaults to **row-major**. C++ (`owl::math`) sends **column-major** matrices. Always declare:
```slang
column_major float4x4 u_ViewProjection;
```
If constructing a matrix from row vectors in Slang, use `transpose()`.

## Backend Differences

Use preprocessor defines set by the compilation pipeline:
```slang
#ifdef BACKEND_VULKAN
    // Vulkan-specific: binding starts at 1 for textures
    [[vk::binding(1)]] Texture2D u_Textures[32];
#else
    // OpenGL: binding starts at 0 for textures
    Texture2D u_Textures[32];
#endif
```

## Binding Conventions

- `[[vk::binding(N)]]` for explicit Vulkan descriptor bindings
- Uniform buffers use `ConstantBuffer<T>` or `cbuffer` blocks
- Texture arrays use `Texture2D[]` with `SamplerState`

## Texture Array Indexing

Always use `NonUniformResourceIndex()` for dynamic texture array access (required for both Vulkan and OpenGL correctness):
```slang
float4 texColor = u_Textures[NonUniformResourceIndex(texIndex)].Sample(u_Sampler, uv);
```

## Colour Space

- Framebuffers and textures are UNORM (never `*_SRGB`): Owl Nest shows the same textures through ImGui
- The scene colour is `Rgba16F` and linear; a world shader reads the `linearOutput` flag (uniform block or vertex
  attribute) and, when set, decodes its sRGB colour (2D-like) or texel and tint before lighting (mesh3d, voxel)
- With the flag at 0 (HUD, thumbnails, previews, ImGui) the maths stays as it was
- sRGB encoding and tone mapping happen only in `color_encode/color_encode.slang`
- No `pow(color, 2.2)` or `pow(color, 1/2.2)` elsewhere

## Compilation Pipeline

- Source: `engine_assets/shaders/<renderer>/<name>.slang`
- Compiled at build time by `OwlShaderBake` (`source/tools/`) into `bin/assets/shaders/<renderer>/spirv/<api>/`;
  at runtime `loadOrCompileSpirv()` reads the `.spv` cache, then that output, and calls `compileSlangToSpirv()` only
  when neither matches the cache key (hash-based invalidation): never put Slang back on the startup path
- Stored SPIR-V goes through `checkSpirv()` before any driver: invalid SPIR-V is undefined behaviour (Mesa ANV crashed
  with SIGFPE on the next compute pipeline). A refusal by the driver goes through `recompileSpirv()` once (Vulkan
  `Shader::recoverRejectedSpirv`, `DrawData::buildPipeline`, compute shaders; OpenGL `Shader::buildProgram`)
- Reflection via spirv-cross extracts uniform buffers and sampled images
- Measured in Release (`test/bench/`, Slang 2026.19): ~165 ms cold session, 21–35 ms per shader, 270 ms for all 12;
  the time is spent inside Slang's IR linking, not in Owl.
  Share the session across tests with `SetUpTestSuite`. Headless compilation tests need no GPU.
- Slang warning 41012 (capability auto-upgrade) is filtered from the logs on purpose.
- Slang `float4x4(v0, v1, v2, v3)` takes **rows** (GLSL `mat4(...)` takes columns): `transpose()` when porting.
