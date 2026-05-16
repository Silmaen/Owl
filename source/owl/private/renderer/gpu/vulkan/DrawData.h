/**
 * @file DrawData.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Buffer.h"
#include "Shader.h"
#include "renderer/gpu/DrawData.h"

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Class representing what is required for a draw.
 */
class OWL_API DrawData final : public owl::renderer::gpu::DrawData {
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
	 *  Initialise an instanced draw — creates two `VertexBuffer`s (one per-vertex,
	 *  one per-instance) and a pipeline with two `VkVertexInputBindingDescription`s
	 *  (binding 0 per-vertex, binding 1 per-instance).
	 * @param[in] iVertexLayout Per-vertex layout.
	 * @param[in] iInstanceLayout Per-instance layout.
	 * @param[in] iVertexCapacity Number of vertices the per-vertex VBO must hold.
	 * @param[in] iInstanceCapacity Number of instances the per-instance VBO must hold.
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
	 *  Push per-instance data to the per-instance VBO. No-op for
	 *  non-instanced draws.
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

	/**
	 * @brief
	 *  Get the name.
	 * @return The name.
	 */
	[[nodiscard]] auto getName() const -> std::string { return std::format("{}_{}", m_renderer, m_shaderName); }

private:
	/// index of the pipeline.
	int32_t m_pipelineId = -1;
	/// Pointer to the shader/pipeline.
	shared<Shader> mp_shader;
	/// Pointer to the vertex buffer.
	shared<VertexBuffer> mp_vertexBuffer;
	/// Pointer to the per-instance vertex buffer (`initInstanced` path only).
	shared<VertexBuffer> mp_instanceBuffer;
	/// Pointer to the index buffer.
	shared<IndexBuffer> mp_indexBuffer;
	/// Name of the shader
	std::string m_shaderName;
	/// Name of the renderer
	std::string m_renderer;
};
}// namespace owl::renderer::gpu::vulkan
