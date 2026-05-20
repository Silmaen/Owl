/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UniformBuffer.h"

#include "internal/Descriptors.h"
#include "internal/RendererDescriptors.h"
#include "internal/VulkanHandler.h"

namespace owl::renderer::gpu::vulkan {

UniformBuffer::UniformBuffer(const uint32_t iSize, const uint32_t iBinding, const std::string& iRenderer)
	: m_renderer{iRenderer}, m_binding{iBinding} {
	if (auto* const rd = internal::RendererDescriptors::getForRenderer(iRenderer); rd != nullptr) {
		rd->registerUniform(iBinding, iSize);
		rd->updateDescriptors();
		return;
	}
	auto& vkd = internal::Descriptors::get();
	vkd.registerUniform(iBinding, iSize);
	vkd.updateDescriptors();
}

UniformBuffer::~UniformBuffer() = default;

void UniformBuffer::setData(const void* iData, const uint32_t iSize, uint32_t) {
	if (const auto* const rd = internal::RendererDescriptors::getForRenderer(m_renderer); rd != nullptr) {
		rd->setUniformData(m_binding, iData, iSize);
		return;
	}
	const auto& vkd = internal::Descriptors::get();
	vkd.setUniformData(m_binding, iData, iSize);
}

}// namespace owl::renderer::gpu::vulkan
