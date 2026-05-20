/**
 * @file FrustumCullingPass.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "FrustumCullingPass.h"

#include "renderer/gpu/RenderCommand.h"

#include <cmath>

namespace owl::renderer::utils {

namespace {
constexpr const char* kRenderer = "frustum_culling";
constexpr const char* kShaderName = "frustum_culling";
constexpr uint32_t kBindingAabbs = 0;
constexpr uint32_t kBindingFrustum = 1;
constexpr uint32_t kBindingCounter = 2;
constexpr uint32_t kBindingCommands = 3;

// Layout of the frustum SSBO: 6 plane equations + a template draw command.
struct FrustumPayload {
	math::vec4 planes[6];
	uint32_t indexCount = 0;
	uint32_t firstIndex = 0;
	uint32_t baseVertex = 0;
	uint32_t _pad = 0;
};

auto normalisePlane(const math::vec4& iPlane) -> math::vec4 {
	const float lenSq = iPlane.x() * iPlane.x() + iPlane.y() * iPlane.y() + iPlane.z() * iPlane.z();
	if (lenSq <= 0.0f)
		return iPlane;
	const float inv = 1.0f / std::sqrt(lenSq);
	return math::vec4{iPlane.x() * inv, iPlane.y() * inv, iPlane.z() * inv, iPlane.w() * inv};
}
}// namespace

void FrustumCullingPass::init() {
	if (m_ready)
		return;
	m_shader = gpu::ComputeShader::create(kShaderName, kRenderer);
	if (m_shader == nullptr) {
		OWL_CORE_ERROR("FrustumCullingPass: failed to create compute shader.")
		return;
	}
	// Frustum + counter are fixed-size, allocate now.
	m_frustumBuffer =
			gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(FrustumPayload)), kBindingFrustum, kRenderer);
	m_counterBuffer = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(uint32_t)), kBindingCounter, kRenderer);
	if (m_frustumBuffer == nullptr || m_counterBuffer == nullptr) {
		OWL_CORE_ERROR("FrustumCullingPass: failed to create frustum/counter SSBO.")
		m_shader.reset();
		m_frustumBuffer.reset();
		m_counterBuffer.reset();
		return;
	}
	m_ready = true;
}

void FrustumCullingPass::shutdown() {
	m_shader.reset();
	m_aabbsBuffer.reset();
	m_frustumBuffer.reset();
	m_counterBuffer.reset();
	m_commandsBuffer.reset();
	m_paddedCapacity = 0;
	m_ready = false;
}

void FrustumCullingPass::dispatch(std::span<const Aabb> iAabbs, const std::array<math::vec4, 6>& iFrustumPlanes,
								  const DrawCommand& iTemplateCommand) {
	if (!m_ready)
		return;
	const auto entryCount = static_cast<uint32_t>(iAabbs.size());
	if (entryCount == 0)
		return;
	const uint32_t paddedCount = ((entryCount + kWorkgroupSize - 1) / kWorkgroupSize) * kWorkgroupSize;
	if (paddedCount > m_paddedCapacity) {
		const uint32_t aabbBytes = paddedCount * static_cast<uint32_t>(sizeof(Aabb));
		const uint32_t cmdBytes = paddedCount * static_cast<uint32_t>(sizeof(DrawCommand));
		m_aabbsBuffer = gpu::StorageBuffer::create(aabbBytes, kBindingAabbs, kRenderer);
		m_commandsBuffer = gpu::StorageBuffer::create(cmdBytes, kBindingCommands, kRenderer);
		if (m_aabbsBuffer == nullptr || m_commandsBuffer == nullptr) {
			OWL_CORE_ERROR("FrustumCullingPass: failed to grow buffers for {} entries.", paddedCount)
			return;
		}
		m_paddedCapacity = paddedCount;
	}

	thread_local std::vector<Aabb> aabbHost;
	aabbHost.resize(paddedCount);
	for (uint32_t i = 0; i < entryCount; ++i) {
		aabbHost[i] = iAabbs[i];
		aabbHost[i].minPos = math::vec4{iAabbs[i].minPos.x(), iAabbs[i].minPos.y(), iAabbs[i].minPos.z(), 1.0f};
	}
	for (uint32_t i = entryCount; i < paddedCount; ++i) {
		aabbHost[i].minPos = math::vec4{0.0f, 0.0f, 0.0f, 0.0f};// validity = 0
		aabbHost[i].maxPos = math::vec4{0.0f, 0.0f, 0.0f, 0.0f};
	}
	m_aabbsBuffer->setData(aabbHost.data(), paddedCount * static_cast<uint32_t>(sizeof(Aabb)), 0);

	FrustumPayload payload;
	for (int p = 0; p < 6; ++p) payload.planes[p] = iFrustumPlanes[static_cast<size_t>(p)];
	payload.indexCount = iTemplateCommand.indexCount;
	payload.firstIndex = iTemplateCommand.firstIndex;
	payload.baseVertex = iTemplateCommand.baseVertex;
	m_frustumBuffer->setData(&payload, static_cast<uint32_t>(sizeof(payload)), 0);

	constexpr uint32_t zero = 0;
	m_counterBuffer->setData(&zero, static_cast<uint32_t>(sizeof(uint32_t)), 0);

	m_shader->bindStorageBuffer(kBindingAabbs, m_aabbsBuffer);
	m_shader->bindStorageBuffer(kBindingFrustum, m_frustumBuffer);
	m_shader->bindStorageBuffer(kBindingCounter, m_counterBuffer);
	m_shader->bindStorageBuffer(kBindingCommands, m_commandsBuffer);

	const uint32_t workgroups = paddedCount / kWorkgroupSize;
	m_shader->dispatch(workgroups, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

auto FrustumCullingPass::extractFrustumPlanes(const math::mat4& iViewProj) -> std::array<math::vec4, 6> {
	const auto m = [&](size_t iRow, size_t iCol) -> float { return iViewProj(iRow, iCol); };

	std::array<math::vec4, 6> planes{};
	// Left: row3 + row0
	planes[0] = math::vec4{m(3, 0) + m(0, 0), m(3, 1) + m(0, 1), m(3, 2) + m(0, 2), m(3, 3) + m(0, 3)};
	// Right: row3 - row0
	planes[1] = math::vec4{m(3, 0) - m(0, 0), m(3, 1) - m(0, 1), m(3, 2) - m(0, 2), m(3, 3) - m(0, 3)};
	// Bottom: row3 + row1
	planes[2] = math::vec4{m(3, 0) + m(1, 0), m(3, 1) + m(1, 1), m(3, 2) + m(1, 2), m(3, 3) + m(1, 3)};
	// Top: row3 - row1
	planes[3] = math::vec4{m(3, 0) - m(1, 0), m(3, 1) - m(1, 1), m(3, 2) - m(1, 2), m(3, 3) - m(1, 3)};
	// Near: row3 + row2
	planes[4] = math::vec4{m(3, 0) + m(2, 0), m(3, 1) + m(2, 1), m(3, 2) + m(2, 2), m(3, 3) + m(2, 3)};
	// Far: row3 - row2
	planes[5] = math::vec4{m(3, 0) - m(2, 0), m(3, 1) - m(2, 1), m(3, 2) - m(2, 2), m(3, 3) - m(2, 3)};
	for (auto& plane: planes) plane = normalisePlane(plane);
	return planes;
}

}// namespace owl::renderer::utils
