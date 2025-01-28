/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UniformBuffer.h"

namespace owl::renderer::null {

UniformBuffer::UniformBuffer(uint32_t, uint32_t) {}

UniformBuffer::~UniformBuffer() = default;

void UniformBuffer::setData(const void *, uint32_t, uint32_t) {}


}// namespace owl::renderer::null
