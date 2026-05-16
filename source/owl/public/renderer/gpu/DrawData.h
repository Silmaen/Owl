/**
 * @file DrawData.h
 * @author Silmaen
 * @date 04/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Buffer.h"

namespace owl::renderer::gpu {
/**
 * @brief
 *  Abstract class representing what is required for a draw.
 */
class OWL_API DrawData {
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
	virtual ~DrawData();

	/**
	 * @brief
	 *  Initialize the draw data.
	 * @param[in] iLayout Layout of the vertex attributes.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iIndices List of vertex indices.
	 * @param[in] iShaderName The shader name.
	 */
	virtual void init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					  const std::string& iShaderName) = 0;

	/**
	 * @brief
	 *  Initialize the draw data for instanced rendering with a second
	 *  per-instance vertex buffer alongside the per-vertex buffer.
	 *
	 *  Per-vertex attributes use locations 0..(N-1) where N is the number of
	 *  attributes in `iVertexLayout`. Per-instance attributes use locations
	 *  N..(N+M-1) where M is the number of attributes in `iInstanceLayout`
	 *  and `glVertexAttribDivisor(loc, 1)` / `VK_VERTEX_INPUT_RATE_INSTANCE`
	 *  are applied to advance the attribute once per instance.
	 * @param[in] iVertexLayout Per-vertex attribute layout.
	 * @param[in] iInstanceLayout Per-instance attribute layout.
	 * @param[in] iVertexCapacity Capacity (number of vertices) of the
	 *  per-vertex VBO.
	 * @param[in] iInstanceCapacity Maximum number of instances the
	 *  per-instance VBO must accommodate.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iIndices List of vertex indices used by the per-vertex VBO.
	 * @param[in] iShaderName The shader name.
	 */
	virtual void initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout,
							   uint32_t iVertexCapacity, uint32_t iInstanceCapacity, const std::string& iRenderer,
							   std::vector<uint32_t>& iIndices, const std::string& iShaderName) {
		// Default implementation: backend doesn't support instancing. Fall
		// back to the non-instanced path so the call site still produces a
		// valid (if slower) draw.
		static_cast<void>(iInstanceLayout);
		static_cast<void>(iVertexCapacity);
		static_cast<void>(iInstanceCapacity);
		init(iVertexLayout, iRenderer, iIndices, iShaderName);
	}

	/**
	 * @brief
	 *  Bind this draw data.
	 */
	virtual void bind() const = 0;

	/**
	 * @brief
	 *  Unbind the draw data.
	 */
	virtual void unbind() const = 0;

	/**
	 * @brief
	 *  Push Vertices data  to the draw buffer.
	 * @param[in] iData The raw vertices data.
	 * @param[in] iSize The size of the raw data.
	 */
	virtual void setVertexData(const void* iData, uint32_t iSize) = 0;

	/**
	 * @brief
	 *  Push instance data to the per-instance buffer (instanced draws only).
	 *  No-op on draw data that wasn't initialised with `initInstanced`.
	 * @param[in] iData The raw per-instance bytes.
	 * @param[in] iSize Size of the raw data in bytes.
	 */
	virtual void setInstanceData([[maybe_unused]] const void* iData, [[maybe_unused]] uint32_t iSize) {}

	/**
	 * @brief
	 *  Get the number of vertex to draw.
	 * @return Number of vertex to draw.
	 */
	[[nodiscard]] virtual auto getIndexCount() const -> uint32_t = 0;

	/**
	 * @brief
	 *  Create a new drax data buffer.
	 * @return Pointer to the created buffer.
	 */
	static auto create() -> shared<DrawData>;

	/**
	 * @brief
	 *  Define the shader for this object.
	 * @param[in] iShaderName The shader name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	virtual void setShader(const std::string& iShaderName, const std::string& iRenderer) = 0;
};

}// namespace owl::renderer::gpu
