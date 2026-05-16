/**
 * @file ComputeShader.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "ComputeShader.h"

namespace owl::renderer::gpu::null {

ComputeShader::~ComputeShader() = default;

void ComputeShader::bindStorageBuffer([[maybe_unused]] const uint32_t iBinding,
									  [[maybe_unused]] const shared<renderer::gpu::StorageBuffer>& iBuffer) {}

void ComputeShader::dispatch([[maybe_unused]] const uint32_t iGroupsX, [[maybe_unused]] const uint32_t iGroupsY,
							 [[maybe_unused]] const uint32_t iGroupsZ) {}

}// namespace owl::renderer::gpu::null
