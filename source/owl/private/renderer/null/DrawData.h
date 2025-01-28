/**
 * @file DrawData.h
 * @author Silmaen
 * @date 05/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/DrawData.h"

namespace owl::renderer::null {
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
	void init([[maybe_unused]] const BufferLayout& iLayout, [[maybe_unused]] const std::string& iRenderer,
			  [[maybe_unused]] std::vector<uint32_t>& iIndices,
			  [[maybe_unused]] const std::string& iShaderName) override {}

	/**
	 * @brief Bind this draw data.
	 */
	void bind() const override {}

	/**
	 * @brief Unbind the draw data.
	 */
	void unbind() const override {}

	/**
	 * @brief Push Vertices data  to the draw buffer.
	 * @param[in] iData The raw vertices data
	 * @param[in] iSize The size of the raw data.
	 */
	void setVertexData([[maybe_unused]] const void* iData, [[maybe_unused]] uint32_t iSize) override {}

	/**
	 * @brief Get the number of vertex to draw.
	 * @return Number of vertex to draw
	 */
	[[nodiscard]] auto getIndexCount() const -> uint32_t override { return 0; }

	/**
	 * @brief Define the shader for this object.
	 * @param[in] iShaderName The shader name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	void setShader([[maybe_unused]] const std::string& iShaderName,
				   [[maybe_unused]] const std::string& iRenderer) override {}
};
}// namespace owl::renderer::null
