/**
 * @file StorageBuffer.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::gpu::opengl {

/**
 * @brief
 *  OpenGL-backed Shader Storage Buffer Object — wraps a
 *  `GL_SHADER_STORAGE_BUFFER` named buffer bound to its binding slot via
 *  `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ...)`.
 */
class StorageBuffer final : public renderer::gpu::StorageBuffer {
public:
	StorageBuffer(const StorageBuffer&) = delete;

	StorageBuffer(StorageBuffer&&) = delete;

	auto operator=(const StorageBuffer&) -> StorageBuffer& = delete;

	auto operator=(StorageBuffer&&) -> StorageBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Buffer size in bytes.
	 * @param[in] iBinding Shader binding slot.
	 */
	StorageBuffer(uint32_t iSize, uint32_t iBinding);

	/**
	 * @brief
	 *  Destructor.
	 */
	~StorageBuffer() override;

	/**
	 * @brief
	 *  Upload data from the CPU into the SSBO.
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 * @param[in] iOffset Destination offset in bytes.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Rebind the SSBO to its binding slot.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Get the underlying GL handle.
	 * @return GL named buffer id.
	 */
	[[nodiscard]] auto getHandle() const -> uint32_t { return m_rendererId; }

private:
	/// GL named buffer id.
	uint32_t m_rendererId = 0;
	/// Shader binding slot.
	uint32_t m_binding = 0;
};

}// namespace owl::renderer::gpu::opengl
