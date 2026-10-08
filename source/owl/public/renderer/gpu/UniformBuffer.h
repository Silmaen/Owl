/**
 * @file UniformBuffer.h
 * @author Silmaen
 * @date 02/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <string>

namespace owl::renderer::gpu {
/**
 * @brief
 *  Abstract class for managing uniform buffer.
 */
class OWL_API UniformBuffer {
public:
	UniformBuffer() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	UniformBuffer(const UniformBuffer&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	UniformBuffer(UniformBuffer&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const UniformBuffer&) -> UniformBuffer& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(UniformBuffer&&) -> UniformBuffer& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~UniformBuffer();

	/**
	 * @brief
	 *  Push Data to GPU.
	 * @param[in] iData The data.
	 * @param[in] iSize The data size.
	 * @param[in] iOffset The offset to start.
	 */
	virtual void setData(const void* iData, uint32_t iSize, uint32_t iOffset) = 0;

	/**
	 * @brief
	 *  bind this uniform buffer.
	 */
	virtual void bind() = 0;

	/**
	 * @brief
	 *  Create a new instance of UniformBuffer.
	 * @param[in] iSize The buffer size.
	 * @param[in] iBinding The binding.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @return New instance of UniformBuffer.
	 */
	static auto create(uint32_t iSize, uint32_t iBinding, const std::string& iRenderer) -> shared<UniformBuffer>;
};

}// namespace owl::renderer::gpu
