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

- Framebuffers use `VK_FORMAT_R8G8B8A8_UNORM` (not SRGB)
- **No sRGB conversion in shaders** — colours are linear throughout
- No `pow(color, 2.2)` or `pow(color, 1/2.2)` gamma correction

## Compilation Pipeline

- Source: `engine_assets/shaders/<renderer>/slang/<name>.slang`
- Compiled at build time by `OwlShaderBake` (`source/tools/`) into `bin/assets/shaders/<renderer>/spirv/<api>/`;
  at runtime `loadOrCompileSpirv()` reads that output, then the `.spv` cache, and calls `compileSlangToSpirv()` only
  when neither matches the cache key (hash-based invalidation): never put Slang back on the startup path
- Reflection via spirv-cross extracts uniform buffers and sampled images
- Measured in Release (`bench/`, Slang 2026.19): ~165 ms cold session, 21–35 ms per shader, 270 ms for all 12
  (2026.1 was ~95 ms, 16–20 ms, 176 ms: the slowdown is inside Slang's IR linking, not Owl).
  Share the session across tests with `SetUpTestSuite`. Headless compilation tests need no GPU.
- Slang warning 41012 (capability auto-upgrade) is filtered from the logs on purpose.
- Slang `float4x4(v0, v1, v2, v3)` takes **rows** (GLSL `mat4(...)` takes columns): `transpose()` when porting.
