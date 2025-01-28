/**
 * @file Buffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"

#include "internal/VulkanHandler.h"
#include "internal/utils.h"

namespace owl::renderer::vulkan {

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
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to delete vertex buffer after VulkanHander release...")
		return;
	}
	const auto& vkc = internal::VulkanCore::get();
	if (m_vertexBuffer != nullptr)
		vkDestroyBuffer(vkc.getLogicalDevice(), m_vertexBuffer, nullptr);
	m_vertexBuffer = nullptr;
	if (m_vertexBufferMemory != nullptr)
		vkFreeMemory(vkc.getLogicalDevice(), m_vertexBufferMemory, nullptr);
	m_vertexBufferMemory = nullptr;
}

void VertexBuffer::bind() const {
	const auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to bind vertex buffer after VulkanHander release...")
		return;
	}
	const VkBuffer vertexBuffers[] = {m_vertexBuffer};
	constexpr VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(vkh.getCurrentCommandBuffer(), 0, 1, vertexBuffers, offsets);
}

void VertexBuffer::unbind() const {}

void VertexBuffer::setData(const void* iData, const uint32_t iSize) {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to set vertex buffer data after VulkanHander release...")
		return;
	}

	if (iData != nullptr) {
		VkBuffer stagingBuffer{nullptr};
		VkDeviceMemory stagingBufferMemory{nullptr};
		internal::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							   stagingBuffer, stagingBufferMemory);
		const auto& vkc = internal::VulkanCore::get();

		void* dataInternal = nullptr;
		vkMapMemory(vkc.getLogicalDevice(), stagingBufferMemory, 0, iSize, 0, &dataInternal);
		memcpy(dataInternal, iData, iSize);
		vkUnmapMemory(vkc.getLogicalDevice(), stagingBufferMemory);

		internal::copyBuffer(stagingBuffer, m_vertexBuffer, iSize);

		vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
		vkFreeMemory(vkc.getLogicalDevice(), stagingBufferMemory, nullptr);
	}
}

auto VertexBuffer::getBindingDescription() const -> VkVertexInputBindingDescription {
	return {.binding = 0, .stride = getLayout().getStride(), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
}

auto VertexBuffer::getAttributeDescriptions() const -> std::vector<VkVertexInputAttributeDescription> {
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	auto layout = getLayout();
	uint32_t id = 0;
	for (const auto& element: layout) {
		attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
				.location = id,
				.binding = 0,
				.format = shaderDataTypeToVulkanFormat(element.type),
				.offset = element.offset,
		});
		++id;
	}
	return attributeDescriptions;
}

void VertexBuffer::createBuffer(const float* iData, const uint32_t iSize) {

	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to set vertex buffer data after VulkanHander release...")
		return;
	}
	release();
	internal::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
						   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);
	setData(iData, iSize);
}


IndexBuffer::IndexBuffer(const uint32_t* iIndices, const uint32_t iSize) : m_count(iSize) {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan index buffer: Trying to create index buffer data after VulkanHander release...")
		return;
	}
	const VkDeviceSize bufferSize = sizeof(uint16_t) * iSize;
	internal::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
						   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);

	if (iIndices != nullptr) {
		VkBuffer stagingBuffer{nullptr};
		VkDeviceMemory stagingBufferMemory{nullptr};
		internal::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							   stagingBuffer, stagingBufferMemory);
		void* dataInternal{nullptr};
		const auto& vkc = internal::VulkanCore::get();

		vkMapMemory(vkc.getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &dataInternal);
		memcpy(dataInternal, iIndices, bufferSize);
		vkUnmapMemory(vkc.getLogicalDevice(), stagingBufferMemory);
		internal::copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
		vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
		vkFreeMemory(vkc.getLogicalDevice(), stagingBufferMemory, nullptr);
	}
}

IndexBuffer::~IndexBuffer() { release(); }

void IndexBuffer::release() {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to delete vertex buffer after VulkanHander release...")
		return;
	}
	const auto& vkc = internal::VulkanCore::get();

	if (m_indexBuffer != nullptr)
		vkDestroyBuffer(vkc.getLogicalDevice(), m_indexBuffer, nullptr);
	m_indexBuffer = nullptr;
	if (m_indexBufferMemory != nullptr)
		vkFreeMemory(vkc.getLogicalDevice(), m_indexBufferMemory, nullptr);
	m_indexBufferMemory = nullptr;
	m_count = 0;
}

void IndexBuffer::bind() const {
	const auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan vertex buffer: Trying to bind vertex buffer after VulkanHander release...")
		return;
	}
	vkCmdBindIndexBuffer(vkh.getCurrentCommandBuffer(), m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void IndexBuffer::unbind() const {}

}// namespace owl::renderer::vulkan
