/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "renderer/UniformBuffer.h"

namespace owl::renderer::null {
/**
 * @brief Specialized class for managing null uniform buffer.
 */
class UniformBuffer final : public ::owl::renderer::UniformBuffer {
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
	 */
	UniformBuffer(uint32_t iSize, uint32_t iBinding);

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

};
}// namespace owl::renderer::null
