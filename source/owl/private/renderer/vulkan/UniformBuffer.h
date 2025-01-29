/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/UniformBuffer.h"

namespace owl::renderer::vulkan {
/**
 * @brief Specialized class for managing vulkan uniform buffer.
 */
class UniformBuffer final : public renderer::UniformBuffer {
public:
	UniformBuffer() = delete;
	UniformBuffer(const UniformBuffer&) = default;
	UniformBuffer(UniformBuffer&&) = default;
	auto operator=(const UniformBuffer&) -> UniformBuffer& = default;
	auto operator=(UniformBuffer&&) -> UniformBuffer& = default;

	/**
	 * @brief Constructor.
	 * @param[in] iSize Buffer's size.
	 * @param[in] iBinding Buffer's binding.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 */
	UniformBuffer(uint32_t iSize, uint32_t iBinding, const std::string& iRenderer);

	/**
	 * @brief Destructor.
	 */
	~UniformBuffer() override;

	/**
	 * @brief bind this uniform buffer.
	 */
	void bind() override {}

	/**
	 * @brief Push Data to GPU.
	 * @param[in] iData The data.
	 * @param[in] iSize The data size.
	 * @param[in] iOffset The offset to start.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

private:
	/// The renderer name.
	std::string m_renderer;
};
}// namespace owl::renderer::vulkan
