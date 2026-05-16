/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "StorageBuffer.h"

namespace owl::renderer::gpu::null {

StorageBuffer::~StorageBuffer() = default;

void StorageBuffer::setData([[maybe_unused]] const void* iData, [[maybe_unused]] const uint32_t iSize,
							[[maybe_unused]] const uint32_t iOffset) {}

void StorageBuffer::bind() {}

}// namespace owl::renderer::gpu::null
