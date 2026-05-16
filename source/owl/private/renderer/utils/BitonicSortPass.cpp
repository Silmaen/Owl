/**
 * @file BitonicSortPass.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "BitonicSortPass.h"

#include "renderer/gpu/RenderCommand.h"

#include <limits>

namespace owl::renderer::utils {

namespace {
constexpr const char* kRenderer = "bitonic_sort";
constexpr const char* kShaderName = "bitonic_sort";
constexpr uint32_t kBindingItems = 0;
}// namespace

void BitonicSortPass::init() {
	if (m_ready)
		return;
	m_shader = gpu::ComputeShader::create(kShaderName, kRenderer);
	if (m_shader == nullptr) {
		OWL_CORE_ERROR("BitonicSortPass: failed to create compute shader.")
		return;
	}
	m_buffer = gpu::StorageBuffer::create(kSortSize * static_cast<uint32_t>(sizeof(Item)), kBindingItems, kRenderer);
	if (m_buffer == nullptr) {
		OWL_CORE_ERROR("BitonicSortPass: failed to create SSBO.")
		m_shader.reset();
		return;
	}
	m_ready = true;
}

void BitonicSortPass::shutdown() {
	m_shader.reset();
	m_buffer.reset();
	m_itemCount = 0;
	m_ready = false;
}

void BitonicSortPass::sort(std::span<const Item> iItems) {
	if (!m_ready)
		return;
	m_itemCount = std::min(static_cast<uint32_t>(iItems.size()), kSortSize);
	if (iItems.size() > kSortSize) {
		OWL_CORE_WARN("BitonicSortPass: {} items > capacity {}; truncating.", iItems.size(), kSortSize)
	}

	thread_local std::vector<Item> staging;
	staging.resize(kSortSize);
	for (uint32_t i = 0; i < m_itemCount; ++i) staging[i] = iItems[i];
	for (uint32_t i = m_itemCount; i < kSortSize; ++i)
		staging[i] = Item{.key = std::numeric_limits<float>::infinity(), .value = 0u};

	m_buffer->setData(staging.data(), kSortSize * static_cast<uint32_t>(sizeof(Item)), 0);
	m_shader->bindStorageBuffer(kBindingItems, m_buffer);
	m_shader->dispatch(1, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

}// namespace owl::renderer::utils
