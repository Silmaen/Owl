/**
 * @file FrustumCullingPass.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrices.h"
#include "math/vectors.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::utils {

/**
 * @brief
 *  GPU frustum-culling compute pre-pass (#34). One thread per entity AABB.
 *  Visible entities atomically append a `VkDrawIndexedIndirectCommand` /
 *  `glMultiDrawElementsIndirect` entry to a draw-command SSBO. The graphics
 *  pass then issues a single
 *  `RenderCommand::drawIndexedIndirect(getCommandBuffer(), getCommandCountBuffer())`
 *  call regardless of how many entities pass the test.
 *
 *  Inputs (uploaded by `update(...)`):
 *    - AABBs of each entity (xyz min + xyz max, both `vec3`).
 *    - 6 frustum planes (extracted from a view-projection matrix via
 *      `extractFrustumPlanes`).
 *    - A single template `DrawCommand` (per-instance index count + base
 *      offsets) — used by the shader to fill the per-entity command.
 *
 *  Outputs:
 *    - `getCommandBuffer()` — packed array of `DrawCommand`.
 *    - `getCommandCountBuffer()` — single-uint SSBO carrying the visible
 *      entity count (read by `drawIndexedIndirect` to limit the draw).
 *
 *  Caller responsibilities:
 *    - Pad AABBs to a multiple of 64 with `minPos.w = 0` sentinels (the
 *      utility handles padding internally).
 *    - Reset the counter SSBO to 0 before each frame (the utility does
 *      this as part of `dispatch()`).
 */
class FrustumCullingPass final {
public:
	/**
	 * @brief
	 *  Axis-aligned bounding box. `w` of `minPos` is the validity flag —
	 *  `1.0f` for a real entity, `0.0f` for padding. The utility sets
	 *  this internally; callers populate `minPos.xyz` / `maxPos.xyz`.
	 */
	struct Aabb {
		/// xyz = min corner; w = validity flag.
		math::vec4 minPos{0.0f, 0.0f, 0.0f, 1.0f};
		/// xyz = max corner; w = unused.
		math::vec4 maxPos{0.0f, 0.0f, 0.0f, 0.0f};
	};

	/**
	 * @brief
	 *  Indirect draw command layout. Matches
	 *  `VkDrawIndexedIndirectCommand` and OpenGL
	 *  `DrawElementsIndirectCommand` byte-for-byte.
	 */
	struct DrawCommand {
		/// Indices per instance.
		uint32_t indexCount = 0;
		/// Number of instances of this command.
		uint32_t instanceCount = 0;
		/// First index inside the bound index buffer.
		uint32_t firstIndex = 0;
		/// Vertex offset added to every fetched index.
		uint32_t baseVertex = 0;
		/// First instance ID exposed to the vertex shader.
		uint32_t baseInstance = 0;
	};

	FrustumCullingPass() = default;

	FrustumCullingPass(const FrustumCullingPass&) = delete;

	FrustumCullingPass(FrustumCullingPass&&) = delete;

	auto operator=(const FrustumCullingPass&) -> FrustumCullingPass& = delete;

	auto operator=(FrustumCullingPass&&) -> FrustumCullingPass& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~FrustumCullingPass() = default;

	/**
	 * @brief
	 *  Compile the compute shader. Buffers are allocated lazily on the
	 *  first `dispatch()` call so they match the high-water entity count.
	 *  Idempotent.
	 */
	OWL_API void init();

	/**
	 * @brief
	 *  Release the compute shader and all SSBOs.
	 */
	OWL_API void shutdown();

	/**
	 * @brief
	 *  Run the cull. Uploads AABBs (padded to a multiple of 64), uploads
	 *  the 6 frustum planes + template command, resets the counter to 0,
	 *  dispatches one thread per padded slot, and emits a storage
	 *  barrier so the next draw can read the output SSBOs.
	 * @param[in] iAabbs Per-entity AABBs. The utility writes the `w`
	 *  validity flag internally; xyz / xyz are taken from the input.
	 * @param[in] iFrustumPlanes The 6 frustum planes
	 *  `(nx, ny, nz, d)` such that a point is inside the frustum when
	 *  `dot(plane.xyz, point) + plane.w >= 0` for every plane.
	 * @param[in] iTemplateCommand Per-entity command template — the
	 *  shader copies the index-count + offsets into every visible
	 *  entity's command and sets `instanceCount = 1`, `baseInstance =
	 *  entity index`.
	 */
	OWL_API void dispatch(std::span<const Aabb> iAabbs, const std::array<math::vec4, 6>& iFrustumPlanes,
						  const DrawCommand& iTemplateCommand);

	/**
	 * @brief
	 *  Packed draw-command SSBO. The first `getMaxCommandCount()` slots
	 *  are valid storage; the first `<count from getCommandCountBuffer()>`
	 *  of them contain real commands.
	 * @return Draw-command SSBO, or nullptr before the first dispatch.
	 */
	[[nodiscard]] auto getCommandBuffer() const -> shared<gpu::StorageBuffer> { return m_commandsBuffer; }

	/**
	 * @brief
	 *  Single-uint SSBO carrying the visible-entity count. Read by
	 *  `drawIndexedIndirect` (the `drawCount` parameter).
	 * @return Counter SSBO, or nullptr before the first dispatch.
	 */
	[[nodiscard]] auto getCommandCountBuffer() const -> shared<gpu::StorageBuffer> { return m_counterBuffer; }

	/**
	 * @brief
	 *  Maximum number of draw commands the output buffer can hold (= the
	 *  padded entity capacity).
	 * @return Capacity in commands.
	 */
	[[nodiscard]] auto getMaxCommandCount() const -> uint32_t { return m_paddedCapacity; }

	/**
	 * @brief
	 *  Extract the 6 view-projection frustum planes from a column-major
	 *  view-projection matrix (the same layout uploaded to vertex
	 *  shaders). Plane equations are in the form
	 *  `dot(plane.xyz, point) + plane.w = 0`, oriented so the inside of
	 *  the frustum is the positive half-space.
	 * @param[in] iViewProj The view-projection matrix.
	 * @return The 6 frustum planes (left, right, bottom, top, near, far).
	 */
	OWL_API [[nodiscard]] static auto extractFrustumPlanes(const math::mat4& iViewProj) -> std::array<math::vec4, 6>;

	/**
	 * @brief
	 *  Conservative CPU test of an axis-aligned box against a frustum's 6 planes.
	 *
	 * Uses the positive-vertex (n/p-vertex) optimisation: the box is culled only
	 * when it lies fully behind one plane, so it never rejects a visible box (it
	 * may keep a box straddling a frustum edge, which is acceptable for draw
	 * culling). Box coordinates must be in the same space as the matrix passed to
	 * `extractFrustumPlanes`.
	 * @param[in] iFrustumPlanes The 6 planes from `extractFrustumPlanes`.
	 * @param[in] iMin The box min corner.
	 * @param[in] iMax The box max corner.
	 * @return True when the box is at least partially inside the frustum.
	 */
	OWL_API [[nodiscard]] static auto isAabbVisible(const std::array<math::vec4, 6>& iFrustumPlanes,
													const math::vec3& iMin, const math::vec3& iMax) -> bool;

private:
	/// Workgroup size, matches the Slang shader.
	static constexpr uint32_t kWorkgroupSize = 64;

	/// Compute shader instance.
	shared<gpu::ComputeShader> m_shader;
	/// AABB SSBO (input).
	shared<gpu::StorageBuffer> m_aabbsBuffer;
	/// Frustum + template-command SSBO (input).
	shared<gpu::StorageBuffer> m_frustumBuffer;
	/// Visible-entity counter SSBO (output, single uint).
	shared<gpu::StorageBuffer> m_counterBuffer;
	/// Packed draw-command SSBO (output).
	shared<gpu::StorageBuffer> m_commandsBuffer;
	/// Padded capacity in entries.
	uint32_t m_paddedCapacity = 0;
	/// True once `init()` succeeded.
	bool m_ready = false;
};

}// namespace owl::renderer::utils
