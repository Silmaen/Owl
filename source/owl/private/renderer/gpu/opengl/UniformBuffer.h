/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 02/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/gpu/UniformBuffer.h"

namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  Specialized class for managing OpenGL uniform buffer.
 */
class UniformBuffer final : public renderer::gpu::UniformBuffer {
public:
	UniformBuffer() = delete;

	UniformBuffer(const UniformBuffer&) = default;

	UniformBuffer(UniformBuffer&&) = default;

	auto operator=(const UniformBuffer&) -> UniformBuffer& = default;

	auto operator=(UniformBuffer&&) -> UniformBuffer& = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Buffer's size.
	 * @param[in] iBinding Buffer's binding.
	 */
	UniformBuffer(uint32_t iSize, uint32_t iBinding);

	/**
	 * @brief
	 *  Destructor.
	 */
	~UniformBuffer() override;

	/**
	 * @brief
	 *  Re-bind this uniform buffer to its binding point.
	 *
	 * OpenGL uniform-buffer bindings are global state shared by every renderer:
	 * several renderers create a UBO at the same binding (e.g. 0), so the last
	 * one constructed wins. Renderers must call this before their draws to make
	 * their own UBO current at draw time.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Push Data to GPU.
	 * @param[in] iData The data.
	 * @param[in] iSize The data size.
	 * @param[in] iOffset The offset to start.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

private:
	/// The renderer's ID.
	uint32_t m_rendererId = 0;
	/// Uniform-buffer binding point this UBO targets.
	uint32_t m_binding = 0;
};
}// namespace owl::renderer::gpu::opengl
