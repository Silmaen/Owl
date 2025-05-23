/**
 * @file DrawData.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/DrawData.h"
#include "Buffer.h"
#include "Shader.h"

namespace owl::renderer::vulkan {
/**
 * @brief Class representing what is required for a draw.
 */
class OWL_API DrawData final : public owl::renderer::DrawData {
public:
	DrawData(const DrawData&) = default;
	DrawData(DrawData&&) = default;
	auto operator=(const DrawData&) -> DrawData& = default;
	auto operator=(DrawData&&) -> DrawData& = default;
	DrawData() = default;
	/**
	 * @brief Destructor.
	 */
	~DrawData() override;

	/**
	 * @brief Initialize the draw data.
	 * @param[in] iLayout Layout of the vertex attributes.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iIndices List of vertex indices.
	 * @param[in] iShaderName The shader name.
	 */
	void init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
			  const std::string& iShaderName) override;

	/**
	 * @brief Bind this draw data.
	 */
	void bind() const override;

	/**
	 * @brief Unbind the draw data.
	 */
	void unbind() const override;

	/**
	 * @brief Push Vertices data  to the draw buffer.
	 * @param[in] iData The raw vertices data
	 * @param[in] iSize The size of the raw data.
	 */
	void setVertexData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief Get the number of vertex to draw.
	 * @return Number of vertex to draw
	 */
	[[nodiscard]] auto getIndexCount() const -> uint32_t override;

	/**
	 * @brief Define the shader for this object.
	 * @param[in] iShaderName The shader name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	void setShader(const std::string& iShaderName, const std::string& iRenderer) override;

	[[nodiscard]] auto getName() const -> std::string { return std::format("{}_{}", m_renderer, m_shaderName); }

private:
	/// index of the pipeline.
	int32_t m_pipelineId = -1;
	/// Pointer to the shader/pipeline.
	shared<Shader> mp_shader;
	/// Pointer to the vertex buffer.
	shared<VertexBuffer> mp_vertexBuffer;
	/// Pointer to the index buffer.
	shared<IndexBuffer> mp_indexBuffer;
	/// Name of the shader
	std::string m_shaderName;
	/// Name of the renderer
	std::string m_renderer;
};
}// namespace owl::renderer::vulkan
