/**
 * @file DrawData.h
 * @author Silmaen
 * @date 05/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/DrawData.h"
#include "Buffer.h"
#include "Shader.h"
#include "VertexArray.h"

namespace owl::renderer::opengl {
/**
 * @brief Class representing what is required for a draw.
 */
class OWL_API DrawData final : public renderer::DrawData {
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

private:
	/// Pointer to the vertex array.
	shared<VertexArray> mp_vertexArray;
	/// Pointer to the vertex buffer.
	shared<VertexBuffer> mp_vertexBuffer;
	/// Pointer to the shader.
	shared<Shader> mp_shader;
};
}// namespace owl::renderer::opengl
