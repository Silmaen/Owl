/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::renderer {

/**
 * @brief Class UniformBuffer.
 */
class OWL_API UniformBuffer {
public:
	UniformBuffer() = default;
	UniformBuffer(const UniformBuffer &) = default;
	UniformBuffer(UniformBuffer &&) = default;
	UniformBuffer &operator=(const UniformBuffer &) = default;
	UniformBuffer &operator=(UniformBuffer &&) = default;

	/**
	 * @brief Destructor.
	 */
	virtual ~UniformBuffer();

	/**
	 * @brief Push Data to GPU.
	 * @param data The data.
	 * @param size The data size.
	 * @param offset The offset to start.
	 */
	virtual void setData(const void *data, uint32_t size, uint32_t offset) = 0;

	/**
	 * @brief bind this uniform buffer.
	 */
	virtual void bind() = 0;

	/**
	 * @brief Create a new instance of UniformBuffer.
	 * @param size The buffer size.
	 * @param binding The binding.
	 * @param renderer Name of the shader's related renderer.
	 * @return New instance of UniformBuffer.
	 */
	static shared<UniformBuffer> create(uint32_t size, uint32_t binding, const std::string &renderer);
};

}// namespace owl::renderer
