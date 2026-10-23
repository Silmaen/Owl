/**
 * @file MeshCullingPass.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Frustum.h"
#include "math/matrices.h"
#include "math/matrixCreation.h"
#include "math/vectors.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/Texture.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer::utils {

/**
 * @brief
 *  Compute pass culling the instances of the mesh layer: for each group (one mesh with one material) it fills the
 *  instance count of an indirect draw command and the list of its visible instances.
 *
 * With an occlusion pyramid (`buildPyramid`, from a depth of the occluders drawn with the same camera), an instance
 * whose box lies behind the occluders is dropped too. Without GPU compute (Null backend) the frustum culling runs on
 * the CPU, so the counts and lists stay exact and the layer draws them with `drawMeshInstanced`; there is no pyramid.
 */
class MeshCullingPass final {
public:
	/**
	 * @brief
	 *  One instance to cull (std430, 48 bytes).
	 */
	struct Item {
		/// xyz: world min corner; w unused.
		math::vec4 boxMin{0.f, 0.f, 0.f, 0.f};
		/// xyz: world max corner; w unused.
		math::vec4 boxMax{0.f, 0.f, 0.f, 0.f};
		/// Group of the instance.
		uint32_t group = 0;
		/// Padding.
		std::array<uint32_t, 3> pad{};
	};

	/**
	 * @brief
	 *  A group of instances drawn by one command.
	 */
	struct Group {
		/// First entry of the group in the visible list.
		uint32_t visibleBase = 0;
		/// Padding.
		std::array<uint32_t, 3> pad{};
	};

	/**
	 * @brief
	 *  A `DrawIndexedIndirectCommand` (20 bytes).
	 */
	struct DrawCommand {
		/// Indices per instance.
		uint32_t indexCount = 0;
		/// Instances drawn, written by the pass.
		uint32_t instanceCount = 0;
		/// First index.
		uint32_t firstIndex = 0;
		/// Added to every index.
		int32_t baseVertex = 0;
		/// First instance id.
		uint32_t baseInstance = 0;
	};

	/**
	 * @brief
	 *  Default constructor.
	 */
	MeshCullingPass() = default;
	/**
	 * @brief
	 *  Not copyable.
	 */
	MeshCullingPass(const MeshCullingPass&) = delete;
	/**
	 * @brief
	 *  Not movable.
	 */
	MeshCullingPass(MeshCullingPass&&) = delete;
	/**
	 * @brief
	 *  Not copyable.
	 * @return This pass.
	 */
	auto operator=(const MeshCullingPass&) -> MeshCullingPass& = delete;
	/**
	 * @brief
	 *  Not movable.
	 * @return This pass.
	 */
	auto operator=(MeshCullingPass&&) -> MeshCullingPass& = delete;
	/**
	 * @brief
	 *  Destructor.
	 */
	~MeshCullingPass() = default;

	/**
	 * @brief
	 *  Create the compute shader (with a GPU backend) and the fixed buffers.
	 */
	OWL_API void init();

	/**
	 * @brief
	 *  Release the shader and the buffers.
	 */
	OWL_API void shutdown();

	/**
	 * @brief
	 *  Cull the instances of a pass.
	 * @param[in] iItems One item per instance, in instance order.
	 * @param[in] iGroups The groups; the visible entries of group g start at `iGroups[g].visibleBase`.
	 * @param[in] iCommands One command per group, its instance count ignored.
	 * @param[in] iFrustum The frustum of the pass.
	 * @param[in] iOcclusion True to test the instances against the pyramid too (it must exist, `hasPyramid`).
	 */
	OWL_API void cull(std::span<const Item> iItems, std::span<const Group> iGroups,
					  std::span<const DrawCommand> iCommands, const math::Frustum& iFrustum, bool iOcclusion = false);

	/**
	 * @brief
	 *  Build the occlusion pyramid from the depth of the occluders (`RedFloat`, clip z / w, reversed-Z: 0 where empty).
	 * @param[in] iDepth The depth texture, finished being drawn.
	 * @param[in] iSize Size of the texture, in texels.
	 * @param[in] iViewProjection View-projection the occluders were drawn with.
	 */
	OWL_API void buildPyramid(const shared<gpu::Texture2D>& iDepth, const math::vec2ui& iSize,
							  const math::mat4& iViewProjection);

	/**
	 * @brief
	 *  Forget the pyramid: the next culls test the frustum only.
	 */
	void clearPyramid() { m_pyramidLevels = 0; }

	/**
	 * @brief
	 *  Check whether a pyramid is ready for the view-projection of a pass.
	 * @param[in] iViewProjection View-projection of the pass.
	 * @return True when `buildPyramid` ran for that view-projection since the last `clearPyramid`.
	 */
	[[nodiscard]] auto hasPyramid(const math::mat4& iViewProjection) const -> bool {
		return m_pyramidLevels > 0 && m_pyramidViewProjection == iViewProjection;
	}

	/**
	 * @brief
	 *  Get the number of levels of the pyramid.
	 * @return The count, 0 without pyramid.
	 */
	[[nodiscard]] auto getPyramidLevelCount() const -> uint32_t { return m_pyramidLevels; }

	/**
	 * @brief
	 *  Check whether the culling runs on the GPU (the counts are then only known there).
	 * @return True with a compute shader.
	 */
	[[nodiscard]] auto isGpu() const -> bool { return m_shader != nullptr; }

	/**
	 * @brief
	 *  Get the commands of the last `cull`, instance counts filled.
	 * @return The command buffer.
	 */
	[[nodiscard]] auto getCommandBuffer() const -> const shared<gpu::StorageBuffer>& { return m_commands; }

	/**
	 * @brief
	 *  Get the visible list of the last `cull`.
	 * @return The visible buffer.
	 */
	[[nodiscard]] auto getVisibleBuffer() const -> const shared<gpu::StorageBuffer>& { return m_visible; }

	/**
	 * @brief
	 *  Get the instance count of each group after a CPU `cull` (empty on the GPU).
	 * @return The counts, one per group.
	 */
	[[nodiscard]] auto getCpuCounts() const -> std::span<const uint32_t> { return m_cpuCounts; }

private:
	/**
	 * @brief
	 *  Grow the buffers sized by the item and group counts.
	 * @param[in] iItemCount Items of the pass.
	 * @param[in] iGroupCount Groups of the pass.
	 * @return False when a buffer could not be created.
	 */
	auto reserve(uint32_t iItemCount, uint32_t iGroupCount) -> bool;

	/// Compute shader, null without GPU compute.
	shared<gpu::ComputeShader> m_shader;
	/// Compute shader writing the first pyramid level.
	shared<gpu::ComputeShader> m_seedShader;
	/// Compute shader reducing a pyramid level into the next.
	shared<gpu::ComputeShader> m_reduceShader;
	/// Occlusion pyramid, every level.
	shared<gpu::StorageBuffer> m_pyramid;
	/// Sizes and offsets of a pyramid dispatch, streamed per dispatch.
	shared<gpu::StorageBuffer> m_pyramidParams;
	/// Texels the pyramid buffer holds.
	uint32_t m_pyramidCapacity = 0;
	/// Levels of the current pyramid, 0 without one.
	uint32_t m_pyramidLevels = 0;
	/// Size of the first level.
	math::vec2ui m_pyramidSize{0, 0};
	/// First texel of each level.
	std::array<uint32_t, 16> m_levelOffsets{};
	/// View-projection the pyramid was built for.
	math::mat4 m_pyramidViewProjection = math::identity<float, 4>();
	/// Items (input).
	shared<gpu::StorageBuffer> m_items;
	/// Groups (input).
	shared<gpu::StorageBuffer> m_groups;
	/// Frustum and item count (input).
	shared<gpu::StorageBuffer> m_params;
	/// Indirect commands (output: instance counts).
	shared<gpu::StorageBuffer> m_commands;
	/// Visible instance indices (output).
	shared<gpu::StorageBuffer> m_visible;
	/// Item capacity of the buffers.
	uint32_t m_itemCapacity = 0;
	/// Group capacity of the buffers.
	uint32_t m_groupCapacity = 0;
	/// Instance counts of a CPU cull.
	std::vector<uint32_t> m_cpuCounts;
	/// Visible list of a CPU cull, or zeros streamed before a GPU cull.
	std::vector<uint32_t> m_visibleHost;
	/// Commands uploaded, instance counts zeroed.
	std::vector<DrawCommand> m_commandHost;
	/// True once `init()` ran.
	bool m_ready = false;
};

}// namespace owl::renderer::utils
