/**
 * @file DrawData.h
 * @author Silmaen
 * @date 05/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Buffer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "renderer/gpu/DrawData.h"

namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  Class representing what is required for a draw.
 */
class OWL_API DrawData final : public renderer::gpu::DrawData {
public:
	DrawData(const DrawData&) = default;

	DrawData(DrawData&&) = default;

	auto operator=(const DrawData&) -> DrawData& = default;

	auto operator=(DrawData&&) -> DrawData& = default;

	DrawData() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~DrawData() override;

	/**
	 * @brief
	 *  Initialize the draw data.
	 * @param[in] iLayout Layout of the vertex attributes.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iIndices List of vertex indices.
	 * @param[in] iShaderName The shader name.
	 */
	void init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
			  const std::string& iShaderName) override;

	/**
	 * @brief
	 *  Instanced init — adds a per-instance vertex buffer alongside the
	 *  per-vertex one. Both buffers are attached to the same VAO; the
	 *  per-instance attributes get `glVertexAttribDivisor(loc, 1)`.
	 * @param[in] iVertexLayout Per-vertex layout.
	 * @param[in] iInstanceLayout Per-instance layout.
	 * @param[in] iVertexCapacity Per-vertex capacity (vertices).
	 * @param[in] iInstanceCapacity Per-instance capacity (instances).
	 * @param[in] iRenderer Shader renderer name.
	 * @param[in] iIndices Index list.
	 * @param[in] iShaderName Shader file name.
	 */
	void initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout, uint32_t iVertexCapacity,
					   uint32_t iInstanceCapacity, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					   const std::string& iShaderName) override;

	/**
	 * @brief
	 *  Bind this draw data.
	 */
	void bind() const override;

	/**
	 * @brief
	 *  Unbind the draw data.
	 */
	void unbind() const override;

	/**
	 * @brief
	 *  Push Vertices data  to the draw buffer.
	 * @param[in] iData The raw vertices data.
	 * @param[in] iSize The size of the raw data.
	 */
	void setVertexData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Push per-instance data to the per-instance VBO. No-op when
	 *  `initInstanced` wasn't called.
	 * @param[in] iData Raw per-instance bytes.
	 * @param[in] iSize Size in bytes.
	 */
	void setInstanceData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Get the number of vertex to draw.
	 * @return Number of vertex to draw.
	 */
	[[nodiscard]] auto getIndexCount() const -> uint32_t override;

	/**
	 * @brief
	 *  Define the shader for this object.
	 * @param[in] iShaderName The shader name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	void setShader(const std::string& iShaderName, const std::string& iRenderer) override;

private:
	/// Pointer to the vertex array.
	shared<VertexArray> mp_vertexArray;
	/// Pointer to the vertex buffer.
	shared<VertexBuffer> mp_vertexBuffer;
	/// Pointer to the per-instance vertex buffer (`initInstanced` path only).
	shared<VertexBuffer> mp_instanceBuffer;
	/// Pointer to the shader.
	shared<Shader> mp_shader;
};
}// namespace owl::renderer::gpu::opengl
