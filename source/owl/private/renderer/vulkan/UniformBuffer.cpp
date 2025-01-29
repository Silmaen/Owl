/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UniformBuffer.h"

#include "internal/Descriptors.h"
#include "internal/VulkanHandler.h"

namespace owl::renderer::vulkan {

UniformBuffer::UniformBuffer(const uint32_t iSize, uint32_t, const std::string &) {
	auto &vkd = internal::Descriptors::get();
	vkd.registerUniform(iSize);
	vkd.updateDescriptors();
}

UniformBuffer::~UniformBuffer() = default;

void UniformBuffer::setData(const void *iData, const uint32_t iSize, uint32_t) {
	auto &vkd = internal::Descriptors::get();
	vkd.setUniformData(iData, iSize);
}


}// namespace owl::renderer::vulkan
