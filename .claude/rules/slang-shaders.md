---
paths:
  - "engine_assets/shaders/**/*.slang"
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

Slang defaults to **row-major**. C++ (GLM/zeus) sends **column-major** matrices. Always declare:
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

## Color Space

- Framebuffers use `VK_FORMAT_R8G8B8A8_UNORM` (not SRGB)
- **No sRGB conversion in shaders** — colors are linear throughout
- No `pow(color, 2.2)` or `pow(color, 1/2.2)` gamma correction

## Compilation Pipeline

- Source: `engine_assets/shaders/<renderer>/slang/<name>.slang`
- Compiled at runtime by `compileSlangToSpirv()` in `shaderFileUtils.cpp`
- SPIR-V output cached as `.spv` with hash-based invalidation
- Reflection via spirv-cross extracts uniform buffers and sampled images
