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
	 *  Stub constructor. Allocates a host-side buffer of `iSize` bytes so
	 *  `setData` + `getData` can round-trip — lets headless tests exercise
	 *  the SSBO upload/readback contract without a real GPU.
	 * @param[in] iSize Buffer size in bytes.
	 * @param[in] iBinding Binding slot (unused).
	 */
	StorageBuffer(uint32_t iSize, [[maybe_unused]] uint32_t iBinding);

	/**
	 * @brief
	 *  Stub destructor (defined out-of-line in the .cpp to anchor the vtable).
	 */
	~StorageBuffer() override;

	/**
	 * @brief
	 *  Copy bytes into the host-side mirror so a later `getData` can read
	 *  them back. No GPU activity.
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 * @param[in] iOffset Destination offset.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Copy bytes from the host-side mirror into `oData`. Range outside
	 *  the buffer is left untouched. No GPU activity.
	 * @param[out] oData Destination host buffer.
	 * @param[in] iSize Byte count to read.
	 * @param[in] iOffset Source offset in bytes inside the SSBO.
	 */
	void getData(void* oData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  No-op bind.
	 */
	void bind() override;

private:
	/// Host-side mirror — tests round-trip through it.
	std::vector<std::uint8_t> m_data;
};

}// namespace owl::renderer::gpu::null
