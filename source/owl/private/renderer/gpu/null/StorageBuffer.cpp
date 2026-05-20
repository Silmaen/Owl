/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "StorageBuffer.h"

#include <cstring>

namespace owl::renderer::gpu::null {

StorageBuffer::StorageBuffer(const uint32_t iSize, [[maybe_unused]] const uint32_t iBinding) : m_data(iSize, 0u) {}

StorageBuffer::~StorageBuffer() = default;

void StorageBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	if (iData == nullptr || iSize == 0)
		return;
	if (static_cast<size_t>(iOffset) + static_cast<size_t>(iSize) > m_data.size())
		return;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	std::memcpy(m_data.data() + iOffset, iData, iSize);
	OWL_DIAG_POP
}

void StorageBuffer::getData(void* oData, const uint32_t iSize, const uint32_t iOffset) {
	if (oData == nullptr || iSize == 0)
		return;
	if (static_cast<size_t>(iOffset) + static_cast<size_t>(iSize) > m_data.size())
		return;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	std::memcpy(oData, m_data.data() + iOffset, iSize);
	OWL_DIAG_POP
}

void StorageBuffer::bind() {}

}// namespace owl::renderer::gpu::null
