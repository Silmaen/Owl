/**
 * @file StorageBuffer.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::renderer::gpu {
/**
 * @brief
 *  Shader Storage Buffer Object (SSBO). Larger than UBOs and writable from
 *  compute shaders — the foundation for the GPU-offload items (per-column
 *  raycast DDA, world-transform pre-pass, sprite Z-sort, compute culling).
 *
 * Backed by `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` on Vulkan and
 * `GL_SHADER_STORAGE_BUFFER` on OpenGL. Bound to a specific binding slot
 * the compute / graphics pipeline expects.
 */
class OWL_API StorageBuffer {
public:
	StorageBuffer() = default;

	StorageBuffer(const StorageBuffer&) = default;

	StorageBuffer(StorageBuffer&&) = default;

	auto operator=(const StorageBuffer&) -> StorageBuffer& = default;

	auto operator=(StorageBuffer&&) -> StorageBuffer& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~StorageBuffer();

	/**
	 * @brief
	 *  Push data from the CPU into the storage buffer.
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 * @param[in] iOffset Destination offset in bytes.
	 */
	virtual void setData(const void* iData, uint32_t iSize, uint32_t iOffset) = 0;

	/**
	 * @brief
	 *  Bind the SSBO to the shader binding slot it was created with.
	 */
	virtual void bind() = 0;

	/**
	 * @brief
	 *  Create a backend-specific SSBO bound to a binding slot.
	 * @param[in] iSize Buffer size in bytes.
	 * @param[in] iBinding Shader binding slot.
	 * @param[in] iRenderer Owning renderer name (for descriptor namespacing).
	 * @return New StorageBuffer instance.
	 */
	static auto create(uint32_t iSize, uint32_t iBinding, const std::string& iRenderer) -> shared<StorageBuffer>;
};

}// namespace owl::renderer::gpu
