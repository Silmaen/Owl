/**
 * @file StorageBuffer.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::gpu::null {

/**
 * @brief
 *  Null-backend SSBO stub — all operations are no-ops. Used by headless tests
 *  and the renderer fallback path when no GPU API is available.
 */
class StorageBuffer final : public renderer::gpu::StorageBuffer {
public:
	StorageBuffer(const StorageBuffer&) = default;

	StorageBuffer(StorageBuffer&&) = default;

	auto operator=(const StorageBuffer&) -> StorageBuffer& = default;

	auto operator=(StorageBuffer&&) -> StorageBuffer& = default;

	/**
	 * @brief
	 *  Stub constructor.
	 * @param[in] iSize Buffer size (unused).
	 * @param[in] iBinding Binding slot (unused).
	 */
	StorageBuffer([[maybe_unused]] uint32_t iSize, [[maybe_unused]] uint32_t iBinding) {}

	/**
	 * @brief
	 *  Stub destructor (defined out-of-line in the .cpp to anchor the vtable).
	 */
	~StorageBuffer() override;

	/**
	 * @brief
	 *  No-op setData.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  No-op bind.
	 */
	void bind() override;
};

}// namespace owl::renderer::gpu::null
