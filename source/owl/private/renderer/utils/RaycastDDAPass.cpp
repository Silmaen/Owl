/**
 * @file RaycastDDAPass.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "RaycastDDAPass.h"

#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer::utils {

namespace {
constexpr const char* kRenderer = "raycast_dda";
constexpr const char* kShaderName = "raycast_dda";
constexpr uint32_t kBindingParams = 0;
constexpr uint32_t kBindingGrid = 1;
constexpr uint32_t kBindingMeta = 2;
constexpr uint32_t kBindingHitCount = 3;
constexpr uint32_t kBindingZBuffer = 4;
constexpr uint32_t kBindingColumnHits = 5;
}// namespace

void RaycastDDAPass::init() {
	if (m_ready)
		return;
	m_shader = gpu::ComputeShader::create(kShaderName, kRenderer);
	if (m_shader == nullptr) {
		OWL_CORE_ERROR("RaycastDDAPass: failed to create compute shader.")
		return;
	}
	m_paramsBuffer = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(Params)), kBindingParams, kRenderer);
	if (m_paramsBuffer == nullptr) {
		OWL_CORE_ERROR("RaycastDDAPass: failed to create params SSBO.")
		m_shader.reset();
		return;
	}
	m_ready = true;
}

void RaycastDDAPass::shutdown() {
	m_shader.reset();
	m_paramsBuffer.reset();
	m_gridBuffer.reset();
	m_metaBuffer.reset();
	m_hitCountBuffer.reset();
	m_zBufferBuffer.reset();
	m_columnHitsBuffer.reset();
	m_gridCapacity = 0;
	m_metaCapacity = 0;
	m_columnCapacity = 0;
	m_ready = false;
}

void RaycastDDAPass::update(std::span<const int32_t> iTileGrid, const uint32_t iWidth, const uint32_t iHeight,
							std::span<const TileMeta> iTileMeta) {
	if (!m_ready)
		return;
	const uint32_t cellCount = iWidth * iHeight;
	if (cellCount == 0 || iTileGrid.size() < cellCount) {
		OWL_CORE_WARN("RaycastDDAPass: update with empty / undersized grid (cells={}, span={}).", cellCount,
					  iTileGrid.size())
		return;
	}
	if (cellCount > m_gridCapacity) {
		m_gridBuffer =
				gpu::StorageBuffer::create(cellCount * static_cast<uint32_t>(sizeof(int32_t)), kBindingGrid, kRenderer);
		if (m_gridBuffer == nullptr) {
			OWL_CORE_ERROR("RaycastDDAPass: failed to grow grid SSBO to {} cells.", cellCount)
			return;
		}
		m_gridCapacity = cellCount;
	}
	m_gridBuffer->setData(iTileGrid.data(), cellCount * static_cast<uint32_t>(sizeof(int32_t)), 0);

	const uint32_t metaCount = std::max<uint32_t>(1, static_cast<uint32_t>(iTileMeta.size()));
	if (metaCount > m_metaCapacity) {
		m_metaBuffer = gpu::StorageBuffer::create(metaCount * static_cast<uint32_t>(sizeof(TileMeta)), kBindingMeta,
												  kRenderer);
		if (m_metaBuffer == nullptr) {
			OWL_CORE_ERROR("RaycastDDAPass: failed to grow meta SSBO to {} entries.", metaCount)
			return;
		}
		m_metaCapacity = metaCount;
	}
	if (!iTileMeta.empty())
		m_metaBuffer->setData(iTileMeta.data(), static_cast<uint32_t>(iTileMeta.size() * sizeof(TileMeta)), 0);
}

void RaycastDDAPass::dispatch(const Params& iParams) {
	if (!m_ready)
		return;
	if (iParams.numRays == 0 || m_gridBuffer == nullptr || m_metaBuffer == nullptr) {
		OWL_CORE_WARN("RaycastDDAPass: dispatch with numRays=0 or missing grid/meta — skipped.")
		return;
	}

	if (iParams.numRays > m_columnCapacity) {
		const uint32_t hitBytes = iParams.numRays * static_cast<uint32_t>(sizeof(uint32_t));
		const uint32_t zBytes = iParams.numRays * static_cast<uint32_t>(sizeof(float));
		const uint32_t colBytes = iParams.numRays * kMaxHitsPerColumn * static_cast<uint32_t>(sizeof(ColumnHit));
		m_hitCountBuffer = gpu::StorageBuffer::create(hitBytes, kBindingHitCount, kRenderer);
		m_zBufferBuffer = gpu::StorageBuffer::create(zBytes, kBindingZBuffer, kRenderer);
		m_columnHitsBuffer = gpu::StorageBuffer::create(colBytes, kBindingColumnHits, kRenderer);
		if (m_hitCountBuffer == nullptr || m_zBufferBuffer == nullptr || m_columnHitsBuffer == nullptr) {
			OWL_CORE_ERROR("RaycastDDAPass: failed to grow output buffers for {} columns.", iParams.numRays)
			return;
		}
		m_columnCapacity = iParams.numRays;
	}

	m_paramsBuffer->setData(&iParams, static_cast<uint32_t>(sizeof(Params)), 0);

	m_shader->bindStorageBuffer(kBindingParams, m_paramsBuffer);
	m_shader->bindStorageBuffer(kBindingGrid, m_gridBuffer);
	m_shader->bindStorageBuffer(kBindingMeta, m_metaBuffer);
	m_shader->bindStorageBuffer(kBindingHitCount, m_hitCountBuffer);
	m_shader->bindStorageBuffer(kBindingZBuffer, m_zBufferBuffer);
	m_shader->bindStorageBuffer(kBindingColumnHits, m_columnHitsBuffer);

	const uint32_t workgroups = (iParams.numRays + kWorkgroupSize - 1) / kWorkgroupSize;
	m_shader->dispatch(workgroups, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

}// namespace owl::renderer::utils
