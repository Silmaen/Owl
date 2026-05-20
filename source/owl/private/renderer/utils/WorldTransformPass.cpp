/**
 * @file WorldTransformPass.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "WorldTransformPass.h"

#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer::utils {

namespace {
// Renderer/shader directory under `engine_assets/shaders/`.
constexpr const char* kRenderer = "world_transform";
// Shader name (file `engine_assets/shaders/world_transform/slang/world_transform.slang`).
constexpr const char* kShaderName = "world_transform";

constexpr uint32_t kBindingLocals = 0;
constexpr uint32_t kBindingParents = 1;
constexpr uint32_t kBindingWorlds = 2;
}// namespace

void WorldTransformPass::init() {
	if (m_ready)
		return;
	m_shader = gpu::ComputeShader::create(kShaderName, kRenderer);
	if (m_shader == nullptr) {
		OWL_CORE_ERROR("WorldTransformPass: failed to create compute shader.")
		return;
	}
	m_ready = true;
}

void WorldTransformPass::shutdown() {
	m_shader.reset();
	m_localsBuffer.reset();
	m_parentsBuffer.reset();
	m_worldBuffer.reset();
	m_paddedCapacity = 0;
	m_entryCount = 0;
	m_ready = false;
}

void WorldTransformPass::compute(std::span<const Entry> iEntries) {
	if (!m_ready)
		return;
	m_entryCount = static_cast<uint32_t>(iEntries.size());
	if (m_entryCount == 0)
		return;

	const uint32_t paddedCount = ((m_entryCount + kWorkgroupSize - 1) / kWorkgroupSize) * kWorkgroupSize;

	if (paddedCount > m_paddedCapacity) {
		const uint32_t localsBytes = paddedCount * static_cast<uint32_t>(sizeof(math::mat4));
		const uint32_t parentsBytes = paddedCount * static_cast<uint32_t>(sizeof(int32_t));
		m_localsBuffer = gpu::StorageBuffer::create(localsBytes, kBindingLocals, kRenderer);
		m_parentsBuffer = gpu::StorageBuffer::create(parentsBytes, kBindingParents, kRenderer);
		m_worldBuffer = gpu::StorageBuffer::create(localsBytes, kBindingWorlds, kRenderer);
		m_paddedCapacity = paddedCount;
	}

	thread_local std::vector<math::mat4> localsHost;
	thread_local std::vector<int32_t> parentsHost;
	localsHost.resize(paddedCount);
	parentsHost.resize(paddedCount);
	for (uint32_t i = 0; i < m_entryCount; ++i) {
		localsHost[i] = iEntries[i].local;
		parentsHost[i] = iEntries[i].parentIdx;
	}
	for (uint32_t i = m_entryCount; i < paddedCount; ++i) {
		localsHost[i] = math::identity<float, 4>();
		parentsHost[i] = -1;
	}

	m_localsBuffer->setData(localsHost.data(), m_entryCount * static_cast<uint32_t>(sizeof(math::mat4)), 0);
	if (paddedCount > m_entryCount) {
		const uint32_t tailOffset = m_entryCount * static_cast<uint32_t>(sizeof(math::mat4));
		const uint32_t tailBytes = (paddedCount - m_entryCount) * static_cast<uint32_t>(sizeof(math::mat4));
		m_localsBuffer->setData(localsHost.data() + m_entryCount, tailBytes, tailOffset);
	}
	m_parentsBuffer->setData(parentsHost.data(), paddedCount * static_cast<uint32_t>(sizeof(int32_t)), 0);

	m_shader->bindStorageBuffer(kBindingLocals, m_localsBuffer);
	m_shader->bindStorageBuffer(kBindingParents, m_parentsBuffer);
	m_shader->bindStorageBuffer(kBindingWorlds, m_worldBuffer);

	const uint32_t workgroups = paddedCount / kWorkgroupSize;
	m_shader->dispatch(workgroups, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

}// namespace owl::renderer::utils
