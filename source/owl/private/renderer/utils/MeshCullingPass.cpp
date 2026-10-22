/**
 * @file MeshCullingPass.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "MeshCullingPass.h"

#include "renderer/gpu/RenderCommand.h"

#include <algorithm>

namespace owl::renderer::utils {

namespace {
constexpr const char* g_renderer = "mesh_culling";
constexpr const char* g_shaderName = "mesh_culling";
constexpr uint32_t g_workgroupSize = 64;
constexpr uint32_t g_bindingItems = 0;
constexpr uint32_t g_bindingGroups = 1;
constexpr uint32_t g_bindingParams = 2;
constexpr uint32_t g_bindingCommands = 3;
constexpr uint32_t g_bindingVisible = 4;
constexpr uint32_t g_bindingPyramid = 5;
constexpr const char* g_hizRenderer = "hiz_build";
constexpr uint32_t g_hizBindingDepth = 0;
constexpr uint32_t g_hizBindingParams = 1;
constexpr uint32_t g_hizBindingPyramid = 2;
constexpr uint32_t g_hizTile = 8;
constexpr uint32_t g_maxLevels = 16;

struct Params {
	std::array<math::vec4, 6> planes{};
	std::array<math::vec4, 4> viewProjectionRows{};
	uint32_t itemCount = 0;
	uint32_t hizLevels = 0;
	uint32_t hizWidth = 0;
	uint32_t hizHeight = 0;
	std::array<uint32_t, 16> levelOffsets{};
};

struct HizParams {
	uint32_t srcWidth = 0;
	uint32_t srcHeight = 0;
	uint32_t srcOffset = 0;
	uint32_t dstOffset = 0;
	uint32_t dstWidth = 0;
	uint32_t dstHeight = 0;
	std::array<uint32_t, 2> pad{};
};

static_assert(sizeof(MeshCullingPass::Item) == 48, "Item must match CullItem in mesh_culling.slang.");
static_assert(sizeof(MeshCullingPass::Group) == 16, "Group must match CullGroup in mesh_culling.slang.");
static_assert(sizeof(MeshCullingPass::DrawCommand) == 20, "DrawCommand must match DrawIndexedIndirectCommand.");
static_assert(sizeof(Params) == 240, "Params must match CullParams in mesh_culling.slang.");
static_assert(sizeof(HizParams) == 32, "HizParams must match HizParams in the hiz_build shaders.");

auto nextLevel(const math::vec2ui& iSize) -> math::vec2ui {
	return math::vec2ui{std::max(1u, (iSize.x() + 1) / 2), std::max(1u, (iSize.y() + 1) / 2)};
}
}// namespace

void MeshCullingPass::init() {
	if (m_ready)
		return;
	m_ready = true;
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Null)
		return;
	m_shader = gpu::ComputeShader::create(g_shaderName, g_renderer);
	m_params = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(Params)), g_bindingParams, g_renderer);
	// A one-texel pyramid keeps the pyramid binding valid while no occlusion runs.
	m_pyramid = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(float)), g_bindingPyramid, g_renderer);
	if (m_shader == nullptr || m_params == nullptr || m_pyramid == nullptr) {
		OWL_CORE_WARN("MeshCullingPass: Compute shader unavailable, the mesh instances are culled on the CPU.")
		m_shader.reset();
		m_params.reset();
		m_pyramid.reset();
		return;
	}
	m_pyramidCapacity = 1;
	m_seedShader = gpu::ComputeShader::create("hiz_seed", g_hizRenderer);
	m_reduceShader = gpu::ComputeShader::create("hiz_reduce", g_hizRenderer);
	m_pyramidParams =
			gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(HizParams)), g_hizBindingParams, g_hizRenderer);
	if (m_seedShader == nullptr || m_reduceShader == nullptr || m_pyramidParams == nullptr) {
		OWL_CORE_WARN(
				"MeshCullingPass: Occlusion pyramid shaders unavailable, the mesh instances are culled by frustum.")
		m_seedShader.reset();
		m_reduceShader.reset();
		m_pyramidParams.reset();
	}
}

void MeshCullingPass::shutdown() {
	m_shader.reset();
	m_seedShader.reset();
	m_reduceShader.reset();
	m_pyramid.reset();
	m_pyramidParams.reset();
	m_pyramidCapacity = 0;
	m_pyramidLevels = 0;
	m_items.reset();
	m_groups.reset();
	m_params.reset();
	m_commands.reset();
	m_visible.reset();
	m_itemCapacity = 0;
	m_groupCapacity = 0;
	m_ready = false;
}

auto MeshCullingPass::reserve(const uint32_t iItemCount, const uint32_t iGroupCount) -> bool {
	if (iItemCount > m_itemCapacity || !m_visible) {
		const uint32_t capacity = std::max({iItemCount, m_itemCapacity * 2, g_workgroupSize});
		m_visible = gpu::StorageBuffer::create(capacity * static_cast<uint32_t>(sizeof(uint32_t)), g_bindingVisible,
											   g_renderer);
		if (isGpu())
			m_items = gpu::StorageBuffer::create(capacity * static_cast<uint32_t>(sizeof(Item)), g_bindingItems,
												 g_renderer);
		m_itemCapacity = capacity;
	}
	if (iGroupCount > m_groupCapacity || !m_commands) {
		const uint32_t capacity = std::max({iGroupCount, m_groupCapacity * 2, 16u});
		m_commands = gpu::StorageBuffer::create(capacity * static_cast<uint32_t>(sizeof(DrawCommand)),
												g_bindingCommands, g_renderer);
		if (isGpu())
			m_groups = gpu::StorageBuffer::create(capacity * static_cast<uint32_t>(sizeof(Group)), g_bindingGroups,
												  g_renderer);
		m_groupCapacity = capacity;
	}
	if (!m_visible || !m_commands || (isGpu() && (!m_items || !m_groups))) {
		OWL_CORE_ERROR("MeshCullingPass: Failed to grow the buffers to {} instances and {} groups.", iItemCount,
					   iGroupCount)
		m_itemCapacity = 0;
		m_groupCapacity = 0;
		return false;
	}
	return true;
}

void MeshCullingPass::cull(const std::span<const Item> iItems, const std::span<const Group> iGroups,
						   const std::span<const DrawCommand> iCommands, const math::Frustum& iFrustum,
						   const bool iOcclusion) {
	OWL_PROFILE_FUNCTION()

	if (!m_ready)
		init();
	const auto itemCount = static_cast<uint32_t>(iItems.size());
	const auto groupCount = static_cast<uint32_t>(iGroups.size());
	m_cpuCounts.clear();
	if (itemCount == 0 || groupCount == 0 || iCommands.size() != iGroups.size() || !reserve(itemCount, groupCount))
		return;
	m_commandHost.assign(iCommands.begin(), iCommands.end());
	for (auto& command: m_commandHost) command.instanceCount = 0;
	m_visibleHost.assign(itemCount, 0);
	if (!isGpu()) {
		m_cpuCounts.assign(groupCount, 0);
		for (uint32_t index = 0; index < itemCount; ++index) {
			const auto& item = iItems[index];
			const math::box3f box{math::vec3{item.boxMin.x(), item.boxMin.y(), item.boxMin.z()},
								  math::vec3{item.boxMax.x(), item.boxMax.y(), item.boxMax.z()}};
			if (!iFrustum.intersects(box))
				continue;
			const uint32_t slot = m_cpuCounts[item.group]++;
			m_visibleHost[iGroups[item.group].visibleBase + slot] = index;
		}
		for (uint32_t group = 0; group < groupCount; ++group) m_commandHost[group].instanceCount = m_cpuCounts[group];
		m_visible->setData(m_visibleHost.data(), itemCount * static_cast<uint32_t>(sizeof(uint32_t)), 0);
		m_commands->setData(m_commandHost.data(), groupCount * static_cast<uint32_t>(sizeof(DrawCommand)), 0);
		return;
	}
	// Every output is streamed for this pass: a later pass of the frame gets buffers of its own.
	m_visible->setData(m_visibleHost.data(), itemCount * static_cast<uint32_t>(sizeof(uint32_t)), 0);
	m_commands->setData(m_commandHost.data(), groupCount * static_cast<uint32_t>(sizeof(DrawCommand)), 0);
	m_items->setData(iItems.data(), itemCount * static_cast<uint32_t>(sizeof(Item)), 0);
	m_groups->setData(iGroups.data(), groupCount * static_cast<uint32_t>(sizeof(Group)), 0);
	Params params{.planes = iFrustum.getPlanes(), .itemCount = itemCount};
	if (iOcclusion && m_pyramidLevels > 0) {
		for (size_t row = 0; row < 4; ++row) params.viewProjectionRows[row] = m_pyramidViewProjection.row(row);
		params.hizLevels = m_pyramidLevels;
		params.hizWidth = m_pyramidSize.x();
		params.hizHeight = m_pyramidSize.y();
		params.levelOffsets = m_levelOffsets;
	}
	m_params->setData(&params, static_cast<uint32_t>(sizeof(Params)), 0);
	m_shader->bindStorageBuffer(g_bindingItems, m_items);
	m_shader->bindStorageBuffer(g_bindingGroups, m_groups);
	m_shader->bindStorageBuffer(g_bindingParams, m_params);
	m_shader->bindStorageBuffer(g_bindingCommands, m_commands);
	m_shader->bindStorageBuffer(g_bindingVisible, m_visible);
	m_shader->bindStorageBuffer(g_bindingPyramid, m_pyramid);
	m_shader->dispatch((itemCount + g_workgroupSize - 1) / g_workgroupSize, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

void MeshCullingPass::buildPyramid(const shared<gpu::Texture2D>& iDepth, const math::vec2ui& iSize,
								   const math::mat4& iViewProjection) {
	OWL_PROFILE_FUNCTION()

	m_pyramidLevels = 0;
	if (!m_ready)
		init();
	if (!m_seedShader || !m_reduceShader || !iDepth || iSize.x() == 0 || iSize.y() == 0)
		return;
	uint32_t texels = 0;
	uint32_t levels = 0;
	for (math::vec2ui size = iSize; levels < g_maxLevels; size = nextLevel(size)) {
		m_levelOffsets[levels++] = texels;
		texels += size.x() * size.y();
		if (size.x() == 1 && size.y() == 1)
			break;
	}
	if (texels > m_pyramidCapacity) {
		m_pyramid =
				gpu::StorageBuffer::create(texels * static_cast<uint32_t>(sizeof(float)), g_bindingPyramid, g_renderer);
		m_pyramidCapacity = m_pyramid ? texels : 0;
		if (!m_pyramid) {
			OWL_CORE_ERROR("MeshCullingPass: Failed to create an occlusion pyramid of {} texels.", texels)
			return;
		}
	}
	const auto dispatch = [this](const shared<gpu::ComputeShader>& iShader, const HizParams& iParams) -> void {
		m_pyramidParams->setData(&iParams, static_cast<uint32_t>(sizeof(HizParams)), 0);
		iShader->bindStorageBuffer(g_hizBindingParams, m_pyramidParams);
		iShader->bindStorageBuffer(g_hizBindingPyramid, m_pyramid);
		iShader->dispatch((iParams.dstWidth + g_hizTile - 1) / g_hizTile,
						  (iParams.dstHeight + g_hizTile - 1) / g_hizTile, 1);
		gpu::RenderCommand::storageBufferMemoryBarrier();
	};
	m_seedShader->bindTexture(g_hizBindingDepth, iDepth);
	dispatch(m_seedShader, {.srcWidth = iSize.x(),
							.srcHeight = iSize.y(),
							.srcOffset = 0,
							.dstOffset = 0,
							.dstWidth = iSize.x(),
							.dstHeight = iSize.y()});
	math::vec2ui size = iSize;
	for (uint32_t level = 1; level < levels; ++level) {
		const math::vec2ui next = nextLevel(size);
		dispatch(m_reduceShader, {.srcWidth = size.x(),
								  .srcHeight = size.y(),
								  .srcOffset = m_levelOffsets[level - 1],
								  .dstOffset = m_levelOffsets[level],
								  .dstWidth = next.x(),
								  .dstHeight = next.y()});
		size = next;
	}
	m_pyramidLevels = levels;
	m_pyramidSize = iSize;
	m_pyramidViewProjection = iViewProjection;
}

}// namespace owl::renderer::utils
