/**
 * @file VertexArray.h
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Buffer.h"

namespace owl::renderer::opengl {
/**
 * @brief Class for OpenGL vertex array.
 */
class VertexArray {
public:
	VertexArray();
	VertexArray(const VertexArray&) = default;
	VertexArray(VertexArray&&) = default;
	auto operator=(const VertexArray&) -> VertexArray& = default;
	auto operator=(VertexArray&&) -> VertexArray& = default;

	using vertexBuf = shared<VertexBuffer>;
	using vertBufs = std::vector<vertexBuf>;
	using indexBuf = shared<IndexBuffer>;

	/**
	 * @brief Destructor.
	 */
	~VertexArray();

	/**
	 * @brief Activate the buffer in the GPU.
	 */
	void bind() const;
	/**
	 * @brief Deactivate the buffer in the GPU.
	 */
	static void unbind();

	/**
	 * @brief Append a vertex buffer to the list.
	 * @param[in] iVertexBuffer Vertex buffer to add.
	 */
	void addVertexBuffer(const vertexBuf& iVertexBuffer);

	/**
	 * @brief Define the Index buffer.
	 * @param[in] iIndexBuffer New Index Buffer.
	 */
	void setIndexBuffer(const indexBuf& iIndexBuffer);

	/**
	 * @brief Access to vertex buffers.
	 * @return Vertex buffers.
	 */
	[[nodiscard]] auto getVertexBuffers() const -> const vertBufs& { return m_vertexBuffers; }

	/**
	 * @brief Access to index buffer.
	 * @return Index buffer.
	 */
	[[nodiscard]] auto getIndexBuffer() const -> const indexBuf& { return m_indexBuffer; }

private:
	/// Id in the OpenGL context.
	uint32_t m_rendererId = 0;
	/// Offset on index.
	uint32_t m_vertexBufferIndex = 0;
	/// The vertex buffers.
	vertBufs m_vertexBuffers;
	/// The index buffer.
	indexBuf m_indexBuffer;
};
}// namespace owl::renderer::opengl
