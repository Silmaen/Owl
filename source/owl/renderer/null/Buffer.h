/**
 * @file Buffer.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../Buffer.h"

namespace owl::renderer::null {
/**
 * @brief Class VertexBuffer.
 */
class VertexBuffer final : public renderer::VertexBuffer {
public:
	VertexBuffer(const VertexBuffer &) = delete;
	VertexBuffer(VertexBuffer &&) = delete;
	VertexBuffer &operator=(const VertexBuffer &) = delete;
	VertexBuffer &operator=(VertexBuffer &&) = delete;
	/**
	 * @brief Constructor.
	 * @param[in] iSize The buffer size.
	 */
	explicit VertexBuffer(uint32_t iSize);

	/**
	 * @brief Default constructor.
	 * @param[in] iVertices The Vertices.
	 * @param[in] iSize The buffer size.
	 */
	VertexBuffer(float *iVertices, uint32_t iSize);

	/**
	 * @brief Destructor.
	 */
	~VertexBuffer() override;

	/**
	 * @brief Activate the buffer in the GPU.
	 */
	void bind() const override;

	/**
	 * @brief Deactivate the buffer in the GPU.
	 */
	void unbind() const override;

	/**
	 * @brief Defines the data of the vertex buffer.
	 * @param[in] iData The raw data.
	 * @param[in] iSize Number of data.
	 */
	void setData(const void *iData, uint32_t iSize) override;
};

/**
 * @brief Class IndexBuffer.
 */
class IndexBuffer final : public renderer::IndexBuffer {
public:
	IndexBuffer(const IndexBuffer &) = delete;
	IndexBuffer(IndexBuffer &&) = delete;
	IndexBuffer &operator=(const IndexBuffer &) = delete;
	IndexBuffer &operator=(IndexBuffer &&) = delete;
	/**
	 * @brief Default constructor.
	 * @param[in] iIndices Array of indices.
	 * @param[in] iCount Number of indices in the array.
	 */
	IndexBuffer(uint32_t *iIndices, uint32_t iCount);

	/**
	 * @brief Destructor.
	 */
	~IndexBuffer() override;

	/**
	 * @brief Activate the buffer in the GPU.
	 */
	void bind() const override;

	/**
	 * @brief Deactivate the buffer in the GPU.
	 */
	void unbind() const override;

	/**
	 * @brief Get the number of element in the buffer.
	 * @return Number of element in the buffer.
	 */
	[[nodiscard]] uint32_t getCount() const override { return m_count; }

private:
	/// Number of elements.
	uint32_t m_count = 0;
};
}// namespace owl::renderer::null
