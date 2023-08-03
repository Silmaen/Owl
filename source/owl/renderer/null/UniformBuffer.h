/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "../UniformBuffer.h"

namespace owl::renderer::null {

/**
 * @brief Class UniformBuffer.
 */
class UniformBuffer : public ::owl::renderer::UniformBuffer {
public:
	UniformBuffer() = delete;
	UniformBuffer(const UniformBuffer &) = default;
	UniformBuffer(UniformBuffer &&) = default;
	UniformBuffer &operator=(const UniformBuffer &) = default;
	UniformBuffer &operator=(UniformBuffer &&) = default;

	/**
	 * @brief Constructor.
	 * @param size Buffer's size.
	 * @param binding Buffer's binding.
	 */
	UniformBuffer(uint32_t size, uint32_t binding);
	/**
	 * @brief Destructor.
	 */
	~UniformBuffer() override;
	/**
	 * @brief Push Data to GPU.
	 * @param data The data.
	 * @param size The data size.
	 * @param offset The offset to start.
	 */
	void setData(const void *data, uint32_t size, uint32_t offset = 0) override;

private:
};

}// namespace owl::renderer::null