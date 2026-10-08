/**
 * @file Buffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"

#include "internal/VulkanHandler.h"
#include "internal/utils.h"

#include <cstring>

namespace owl::renderer::gpu::vulkan {

namespace {
auto shaderDataTypeToVulkanFormat(const ShaderDataType& iType) -> VkFormat {
	switch (iType) {
		case ShaderDataType::None:
			return VK_FORMAT_UNDEFINED;
		case ShaderDataType::Float:
			return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::Float2:
			return VK_FORMAT_R32G32_SFLOAT;
		case ShaderDataType::Float3:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case ShaderDataType::Float4:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
			return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::Int:
			return VK_FORMAT_R32_SINT;
		case ShaderDataType::Int2:
			return VK_FORMAT_R32G32_SINT;
		case ShaderDataType::Int3:
			return VK_FORMAT_R32G32B32_SINT;
		case ShaderDataType::Int4:
			return VK_FORMAT_R32G32B32A32_SINT;
		case ShaderDataType::Bool:
			return VK_FORMAT_R8_UINT;
	}
	return VK_FORMAT_UNDEFINED;
}

}// namespace

VertexBuffer::VertexBuffer(const uint32_t iSize) { createBuffer(nullptr, iSize); }

VertexBuffer::VertexBuffer(const float* iVertices, const uint32_t iSize) { createBuffer(iVertices, iSize); }

VertexBuffer::~VertexBuffer() { release(); }

void VertexBuffer::release() {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to delete vertex buffer after VulkanHandler release...")
		return;
	}
	internal::freeBuffer(m_buffer);
}

void VertexBuffer::bind() const { bindAtBinding(0); }

void VertexBuffer::bindAtBinding(const uint32_t iBinding) const {
	const auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to bind vertex buffer after VulkanHandler release...")
		return;
	}
	const VkBuffer vertexBuffers[] = {m_buffer.buffer};
	constexpr VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(vkh.getCurrentCommandBuffer(), iBinding, 1, vertexBuffers, offsets);
}

void VertexBuffer::unbind() const {}

void VertexBuffer::setData(const void* iData, const uint32_t iSize) {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to set vertex buffer data after VulkanHandler release...")
		return;
	}
	if (iData == nullptr || iSize == 0)
		return;
	if (iSize > m_buffer.size) {
		OWL_CORE_WARN("Vulkan vertex buffer: setData of {} bytes beyond the capacity of {}.", iSize, m_buffer.size)
		return;
	}
	internal::uploadToDeviceBuffer(m_buffer.buffer, iData, iSize);
}

auto VertexBuffer::getBindingDescription(const uint32_t iBinding, const bool iPerInstance) const
		-> VkVertexInputBindingDescription {
	return {.binding = iBinding,
			.stride = getLayout().getStride(),
			.inputRate = iPerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX};
}

auto VertexBuffer::getAttributeDescriptions(const uint32_t iBinding, const uint32_t iStartLocation) const
		-> std::vector<VkVertexInputAttributeDescription> {
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	auto layout = getLayout();
	uint32_t id = iStartLocation;
	for (const auto& element: layout) {
		attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
				.location = id,
				.binding = iBinding,
				.format = shaderDataTypeToVulkanFormat(element.type),
				.offset = element.offset,
		});
		++id;
	}
	return attributeDescriptions;
}

void VertexBuffer::createBuffer(const float* iData, const uint32_t iSize) {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to set vertex buffer data after VulkanHandler release...")
		return;
	}
	release();
	m_buffer = internal::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
									  internal::MemoryUsage::Device, "vertexBuffer");
	setData(iData, iSize);
}

IndexBuffer::IndexBuffer(const uint32_t* iIndices, const uint32_t iSize) : m_count(iSize) {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan index buffer: Trying to create index buffer data after VulkanHandler release...")
		return;
	}
	const VkDeviceSize bufferSize = sizeof(uint32_t) * iSize;
	m_buffer = internal::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
									  internal::MemoryUsage::Device, "indexBuffer");
	if (iIndices != nullptr && m_buffer.buffer != nullptr)
		internal::uploadToDeviceBuffer(m_buffer.buffer, iIndices, bufferSize);
}

IndexBuffer::~IndexBuffer() { release(); }

void IndexBuffer::release() {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to delete vertex buffer after VulkanHandler release...")
		return;
	}
	internal::freeBuffer(m_buffer);
	m_count = 0;
}

void IndexBuffer::bind() const {
	const auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to bind vertex buffer after VulkanHandler release...")
		return;
	}
	vkCmdBindIndexBuffer(vkh.getCurrentCommandBuffer(), m_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void IndexBuffer::unbind() const {}

}// namespace owl::renderer::gpu::vulkan
