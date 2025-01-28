/**
 * @file DrawData.h
 * @author Silmaen
 * @date 04/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/Buffer.h"

namespace owl::renderer {
/**
 * @brief Abstract class representing what is required for a draw.
 */
class OWL_API DrawData {
public:
	DrawData(const DrawData&) = default;
	DrawData(DrawData&&) = default;
	auto operator=(const DrawData&) -> DrawData& = default;
	auto operator=(DrawData&&) -> DrawData& = default;
	DrawData() = default;
	/**
	 * @brief Destructor.
	 */
	virtual ~DrawData();

	/**
	 * @brief Initialize the draw data.
	 * @param[in] iLayout Layout of the vertex attributes.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iIndices List of vertex indices.
	 * @param[in] iShaderName The shader name.
	 */
	virtual void init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					  const std::string& iShaderName) = 0;

	/**
	 * @brief Bind this draw data.
	 */
	virtual void bind() const = 0;

	/**
	 * @brief Unbind the draw data.
	 */
	virtual void unbind() const = 0;

	/**
	 * @brief Push Vertices data  to the draw buffer.
	 * @param[in] iData The raw vertices data
	 * @param[in] iSize The size of the raw data.
	 */
	virtual void setVertexData(const void* iData, uint32_t iSize) = 0;

	/**
	 * @brief Get the number of vertex to draw.
	 * @return Number of vertex to draw
	 */
	[[nodiscard]] virtual auto getIndexCount() const -> uint32_t = 0;

	/**
	 * @brief Create a new drax data buffer.
	 * @return Pointer to the created buffer.
	 */
	static auto create() -> shared<DrawData>;

	/**
	 * @brief Define the shader for this object.
	 * @param[in] iShaderName The shader name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	virtual void setShader(const std::string& iShaderName, const std::string& iRenderer) = 0;
};

}// namespace owl::renderer
