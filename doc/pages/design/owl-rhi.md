# Owl RHI {#page-design-owl-rhi}

[TOC]

Design page for the engine's render hardware interface, summarised in the [Roadmap](../roadmap.md).

## Decision

Owl keeps its own render layer, now **named and presented as the Owl RHI** (today
`source/owl/private/renderer/gpu/{vulkan,opengl,null}`). Third-party RHIs (SDL GPU, NVRHI, Diligent, bgfx, wgpu) were
compared in the audit (`doc/audit/30-etat-de-l-art.md`, fiche 2); the maintainer keeps the in-house layer for control
and because it already carries the renderer stack.

| Backend | Role                                                         | From    |
|---------|--------------------------------------------------------------|---------|
| Vulkan  | Reference backend: every feature lands here first            | v0.3.0  |
| OpenGL  | Compatibility / fallback backend, frozen in features, tested | v0.3.0  |
| Null    | Headless tests and benchmarks                                | today   |
| WebGPU  | Browser target, so a game is playable on the Web             | v0.10.0 |

## v0.3.0 — repair and name the RHI

- Real frames in flight: no `vkQueueWaitIdle` on the hot path (≥ 10 queue drains per frame today, B-01)
- Image transitions recorded inside the frame, correct `loadOp` between batches (B-02), swapchain image written only
  after its acquire semaphore (B-19), versioned UBO / SSBO (B-04)
- Per-frame uniform ring, so several `drawMesh` and several cameras per frame are correct (B-03), VMA
  sub-allocation (B-11, B-23)
- OpenGL frozen in features but fixed and tested: the "4.5" claim made true or the docs say 4.6 (B-16), culling and
  mipmaps as documented (B-07, B-18)
- Image-comparison render tests on lavapipe (Vulkan) and llvmpipe (OpenGL), validation clean on NVIDIA, Intel and
  lavapipe
- The backend interface is cleaned up and documented so a further backend can be added without touching the renderers

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
