/**
 * @file Buffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/Buffer.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class for managing vulkan vertex buffer.
 */
class VertexBuffer final : public renderer::gpu::VertexBuffer {
public:
	VertexBuffer(const VertexBuffer&) = delete;

	VertexBuffer(VertexBuffer&&) = delete;

	auto operator=(const VertexBuffer&) -> VertexBuffer& = delete;

	auto operator=(VertexBuffer&&) -> VertexBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize The buffer size.
	 */
	explicit VertexBuffer(uint32_t iSize);

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iVertices The vertices.
	 * @param[in] iSize Number of data.
	 */
	VertexBuffer(const float* iVertices, uint32_t iSize);

	/**
	 * @brief
	 *  Destructor.
	 */
	~VertexBuffer() override;

	/**
	 * @brief
	 *  Release the memory buffer.
	 */
	void release();

	/**
	 * @brief
	 *  Activate the buffer in the GPU at binding 0.
	 */
	void bind() const override;

	/**
	 * @brief
	 *  Activate the buffer at a specific Vulkan binding index. Used by
	 *  instanced draws where the per-instance buffer sits on binding 1
	 *  alongside the per-vertex buffer on binding 0.
	 * @param[in] iBinding Vulkan binding index.
	 */
	void bindAtBinding(uint32_t iBinding) const;

	/**
	 * @brief
	 *  Deactivate the buffer in the GPU.
	 */
	void unbind() const override;

	/**
	 * @brief
	 *  Defines the data of the vertex buffer.
	 * @param[in] iData The raw data.
	 * @param[in] iSize Number of data.
	 */
	void setData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Get the binding description.
	 * @param[in] iBinding Binding index (defaults to 0 for the per-vertex buffer).
	 * @param[in] iPerInstance Set `VK_VERTEX_INPUT_RATE_INSTANCE` when true so this
	 *  buffer advances once per instance instead of once per vertex.
	 * @return The bining description.
	 */
	[[nodiscard]] auto getBindingDescription(uint32_t iBinding = 0, bool iPerInstance = false) const
			-> VkVertexInputBindingDescription;

	/**
	 * @brief
	 *  Get The attribute description.
	 * @param[in] iBinding Binding index this buffer is bound to.
	 * @param[in] iStartLocation Shader location of the first attribute in this layout
	 *  (per-instance attributes follow the per-vertex ones).
	 * @return The attribute description.
	 */
	[[nodiscard]] auto getAttributeDescriptions(uint32_t iBinding = 0, uint32_t iStartLocation = 0) const
			-> std::vector<VkVertexInputAttributeDescription>;

private:
	/// The vulkan vertex buffer.
	VkBuffer m_vertexBuffer{nullptr};
	/// The vulkan vertex buffer memory.
	VkDeviceMemory m_vertexBufferMemory{nullptr};

	void createBuffer(const float* iData, uint32_t iSize);
};

/**
 * @brief
 *  Specialized class for managing vulkan index buffer.
 */
class IndexBuffer final : public renderer::gpu::IndexBuffer {
public:
	IndexBuffer(const IndexBuffer&) = delete;

	IndexBuffer(IndexBuffer&&) = delete;

	auto operator=(const IndexBuffer&) -> IndexBuffer& = delete;

	auto operator=(IndexBuffer&&) -> IndexBuffer& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iIndices Array of indices.
	 * @param[in] iSize Number of indices in the array.
	 */
	IndexBuffer(const uint32_t* iIndices, uint32_t iSize);

	/**
	 * @brief
	 *  Destructor.
	 */
	~IndexBuffer() override;

	/**
	 * @brief
	 *  Release the memory buffer.
	 */
	void release();

	/**
	 * @brief
	 *  Activate the buffer in the GPU.
	 */
	void bind() const override;

	/**
	 * @brief
	 *  Deactivate the buffer in the GPU.
	 */
	void unbind() const override;

	/**
	 * @brief
	 *  Get the number of element in the buffer.
	 * @return Number of element in the buffer.
	 */
	[[nodiscard]] auto getCount() const -> uint32_t override { return m_count; }

private:
	/// Number of elements.
	uint32_t m_count = 0;
	/// Vulkan index buffer.
	VkBuffer m_indexBuffer{nullptr};
	/// Vulkan memory buffer.
	VkDeviceMemory m_indexBufferMemory{nullptr};
};
}// namespace owl::renderer::gpu::vulkan
